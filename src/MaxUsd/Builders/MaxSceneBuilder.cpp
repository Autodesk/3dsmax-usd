//
// Copyright 2023 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "MaxSceneBuilder.h"

#include <MaxUsd/Chaser/ImportChaserRegistry.h>
#include <MaxUsd/DLLEntry.h>
#include <MaxUsd/MaxTokens.h>
#include <MaxUsd/Translators/PrimReaderRegistry.h>
#include <MaxUsd/Translators/ShadingModeImporter.h>
#include <MaxUsd/Translators/ShadingModeRegistry.h>
#include <MaxUsd/Translators/TranslatorXformable.h>
#include <MaxUsd/Utilities/MaxProgressBar.h>
#include <MaxUsd/Utilities/MetaDataUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>
#include <MaxUsd/Utilities/UiUtils.h>
#include <MaxUsd/resource.h>

#include <iInstanceMgr.h> // for IInstanceMgr::GetAutoMtlPropagation()

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAXUSD_NS_DEF {

MaxSceneBuilder::MaxSceneBuilder() = default;

bool MaxSceneBuilder::ExcludedPrimNode(UsdPrimRange::iterator& primIt)
{
    return primIt->IsA<pxr::UsdGeomSubset>() || primIt->IsA<pxr::UsdShadeMaterial>()
        || primIt->IsA<pxr::UsdShadeShader>() || primIt->IsA<pxr::UsdShadeNodeGraph>();
}

void MaxSceneBuilder::DoImportPrimIt(
    UsdPrimRange::iterator& primIt,
    MaxUsdReadJobContext&   readCtx,
    PrimReaderMap&          primReaderMap)
{
    const UsdPrim& prim = *primIt;

    // The iterator will hit each prim twice. IsPostVisit tells us if
    // this is the pre-visit (Read) step or post-visit (PostReadSubtree)
    // step.
    if (primIt.IsPostVisit()) {
        // This is the PostReadSubtree step, if the PrimReader has specified one.
        auto primReaderIt = primReaderMap.find(prim.GetPath());
        if (primReaderIt != primReaderMap.end()) {
            if (primReaderIt->second->HasPostReadSubtree()) {
                primReaderIt->second->PostReadSubtree();
            }
        }
    } else {
        // This is the normal Read step (pre-visit).
        TfToken typeName = prim.GetTypeName();
        if (MaxUsdPrimReaderRegistry::ReaderFactoryFn factoryFn
            = MaxUsdPrimReaderRegistry::FindOrFallback(typeName, readCtx.GetArgs(), prim)) {
            MaxUsdPrimReaderSharedPtr primReader = factoryFn(prim, readCtx);
            if (primReader) {
                primReader->Read();
                primReaderMap[prim.GetPath()] = primReader;
            }
            // has the last PrimReader took care of its children, then prune rest of tree branch
            if (readCtx.GetPruneChildren()) {
                primIt.PruneChildren();
                readCtx.SetPruneChildren(false);
            }
        }
    }
}

void MaxSceneBuilder::DoImportInstanceIt(
    UsdPrimRange::iterator& primIt,
    MaxUsdReadJobContext&   readCtx,
    PrototypeLookupMaps&    prototypeLookupMaps,
    bool                    insidePrototype)
{
    if (primIt.IsPostVisit()) {
        return;
    }

    const UsdPrim& prim = *primIt;
    const UsdPrim  prototype = prim.GetPrototype();
    if (!prototype) {
        return;
    }

    // get instance prototype path
    const SdfPath prototypePath = prototype.GetPath();
    // was the prototype already imported previously
    INode* prototypeNode = readCtx.GetMaxNode(prototypePath, false);
    if (!prototypeNode) {
        ImportPrototype(prototype, readCtx, prototypeLookupMaps);
        prototypeNode = readCtx.GetMaxNode(prototypePath, false);
        if (!prototypeNode) {
            MaxUsd::Log::Error(
                "The prototype node ({0}) could not be found and will not be instanciated. Import "
                "issue "
                "should be resolved.",
                prototypePath.GetString().c_str());
            return;
        }
    }

    // clone prototype as instance
    INodeTab inputTab, sourceTab, outputTab;
    Point3   offset(0, 0, 0);
    inputTab.AppendNode(prototypeNode);
    GetCOREInterface()->CloneNodes(inputTab, offset, true, NODE_INSTANCE, &sourceTab, &outputTab);

    // Rename the node to remove the automatically prepended number by 3dsMax on clone.
    INode* createdNode = outputTab[0];
    createdNode->SetName(MaxUsd::UsdStringToMaxString(prim.GetName()).data());
    prototypeLookupMaps.prototypeReaderMap[prototypePath]->InstanceCreated(prim, createdNode);

    // if instancing from within a prototype, map the cloned nodes to the reader they originate from
    if (insidePrototype) {
        prototypeLookupMaps.nodeToPrototypeMap[createdNode] = prim.GetPath();
        prototypeLookupMaps.prototypeReaderMap[prim.GetPath()]
            = prototypeLookupMaps
                  .prototypeReaderMap[prototypeLookupMaps.nodeToPrototypeMap[sourceTab[0]]];
    }

    // Add duplicate node to registry.
    readCtx.RegisterNewMaxRefTargetHandle(prim.GetPath(), createdNode);

    INode* parentNode = readCtx.GetMaxNode(prim.GetParent().GetPath(), false);
    if (parentNode) {
        parentNode->AttachChild(createdNode);
    }

    // Read xformable attributes from the
    // UsdPrim on to the transform node.
    MaxUsdTranslatorXformable::Read(prim, createdNode, readCtx);

    // process the cloned node's children
    // - rename nodes to prototype name
    // - add nodes to created nodes list
    // - call InstanceCreated using the proper prototype reader that was used
    // - keep track of nodes created and their original prototype
    bool hideCloned = !prototype.IsHidden() && createdNode->IsHidden();
    int  nbClones = sourceTab.Count();
    for (int i = nbClones - 1; i > 0;
         --i) // clones are listed in reverse order traversal - we need depth first
    {
        INode* sourceChildNode = sourceTab[i];
        INode* clonedChildNode = outputTab[i];
        if (hideCloned) {
            clonedChildNode->Hide(true);
        }
        // for the instance prototype to know from which prototype it comes from
        auto subInstancePath = prototypeLookupMaps.nodeToPrototypeMap[sourceChildNode];
        auto instancePrimPath = subInstancePath.ReplacePrefix(prototypePath, primIt->GetPath());

        clonedChildNode->SetName(sourceChildNode->GetName());
        readCtx.RegisterNewMaxRefTargetHandle(instancePrimPath, clonedChildNode);

        // Read xformable attributes from the
        // UsdPrim on to the transform node.
        MaxUsdTranslatorXformable::Read(
            readCtx.GetStage()->GetPrimAtPath(instancePrimPath), clonedChildNode, readCtx);

        // if instancing from within a prototype, map the cloned nodes to the reader they originate
        // from
        if (insidePrototype) {
            prototypeLookupMaps.nodeToPrototypeMap[clonedChildNode] = instancePrimPath;
            prototypeLookupMaps.prototypeReaderMap[instancePrimPath]
                = prototypeLookupMaps
                      .prototypeReaderMap[prototypeLookupMaps.nodeToPrototypeMap[sourceChildNode]];
        }
        prototypeLookupMaps
            .prototypeReaderMap[prototypeLookupMaps.nodeToPrototypeMap[sourceChildNode]]
            ->InstanceCreated(readCtx.GetStage()->GetPrimAtPath(instancePrimPath), clonedChildNode);
    }
}

void MaxSceneBuilder::ImportPrototype(
    const UsdPrim&        prototype,
    MaxUsdReadJobContext& readCtx,
    PrototypeLookupMaps&  prototypeLookupMaps)
{
    PrimReaderMap      primReaderMap;
    const UsdPrimRange range = UsdPrimRange::PreAndPostVisit(prototype);
    for (auto primIt = range.begin(); primIt != range.end(); ++primIt) {
        if (ExcludedPrimNode(primIt)) {
            continue;
        }
        const UsdPrim& prim = *primIt;
        if (prim.IsInstance()) {
            DoImportInstanceIt(primIt, readCtx, prototypeLookupMaps, true);
        } else {
            DoImportPrimIt(primIt, readCtx, primReaderMap);
        }
    }
    // add to the prototypeReaderMap, the readers that were used to load the prototype
    prototypeLookupMaps.prototypeReaderMap.insert(primReaderMap.begin(), primReaderMap.end());
    for (const auto& element : primReaderMap) {
        prototypeLookupMaps.nodeToPrototypeMap[readCtx.GetMaxNode(element.first, false)]
            = element.first;
    }
}

int MaxSceneBuilder::Build(
    INode*                        node,
    const pxr::UsdPrim&           prim,
    const MaxSceneBuilderOptions& buildOptions,
    const fs::path&               filename)
{
    pxr::UsdStageRefPtr stage = prim.GetStage();

    // Insert the stage in the global cache for the time of the import. Useful so it can be accessed
    // from callbacks. Removed from the cache using RAII.
    const MaxUsd::StageCacheScopeGuard stageCacheGuard { stage };

    const auto timeConfig = buildOptions.GetResolvedTimeConfig(stage);
    const auto startTime = buildOptions.GetStartTimeCode();
    const auto endTime = buildOptions.GetEndTimeCode();

    std::string timeCodeLogMessage = "Importing at ";
    switch (buildOptions.GetTimeMode()) {
    case MaxSceneBuilderOptions::ImportTimeMode::AllRange: {
        if (startTime != endTime || startTime != 0) {
            MaxUsd::Log::Warn("A non-default TimeCode is specified, but will be ignored, as the "
                              "TimeMode property is "
                              "configured as #AllRange.");
        }
        timeCodeLogMessage.append(
            std::string("#AllRange timeCode : ") + std::to_string(timeConfig.GetStartTimeCode())
            + " " + std::to_string(timeConfig.GetEndTimeCode()));
        break;
    }
    case MaxSceneBuilderOptions::ImportTimeMode::CustomRange: {
        timeCodeLogMessage.append(
            std::string("#CustomRange timeCode : ") + std::to_string(timeConfig.GetStartTimeCode())
            + " " + std::to_string(timeConfig.GetEndTimeCode()));
        break;
    }
    case MaxSceneBuilderOptions::ImportTimeMode::StartTime: {
        timeCodeLogMessage.append(
            std::string("#StartTime timeCode : ") + std::to_string(timeConfig.GetStartTimeCode()));
        break;
    }
    case MaxSceneBuilderOptions::ImportTimeMode::EndTime: {
        timeCodeLogMessage.append(
            std::string("#EndTime timeCode : ") + std::to_string(timeConfig.GetEndTimeCode()));
        break;
    }
    default: DbgAssert(_T("Unhandled TimeMode mode type. Importing using default values.")); break;
    }
    MaxUsd::Log::Info(timeCodeLogMessage);

    MaxUsdReadJobContext context(buildOptions, stage);

    // We want both pre- and post- visit iterations over the prims in this
    // method. To do so, iterate over all the root prims of the input range,
    // and create new PrimRanges to iterate over their subtrees.
    PrimReaderMap     primReaderMap;
    auto              predicates = !pxr::UsdPrimIsAbstract && pxr::UsdPrimIsDefined;
    pxr::UsdPrimRange newPrimRange = pxr::UsdPrimRange::PreAndPostVisit(prim, predicates);

    // Prepare 3ds Max to expose information to the User about the progress of the import:
    int            currentPrimIndex = 0;
    MaxProgressBar progressBar(
        GetString(IDS_IMPORT_PROGRESS_MESSAGE),
        std::distance(newPrimRange.cbegin(), newPrimRange.cend()));
    progressBar.SetEnabled(buildOptions.GetUseProgressBar());
    progressBar.Start();

    IInstanceMgr* pInstanceMgr = IInstanceMgr::GetInstanceMgr();
    const bool    autoMtlPropagation = pInstanceMgr && pInstanceMgr->GetAutoMtlPropagation();
    if (autoMtlPropagation) {
        // temporarily disable auto material propagation
        pInstanceMgr->SetAutoMtlPropagation(false);
    }

    PrototypeLookupMaps prototypeLookupMaps;
    for (auto primIt = newPrimRange.begin(); primIt != newPrimRange.end(); ++primIt) {
        // Stop the import in its current state if the User chose to cancel it.
        //
        // NOTE: This will result in partially-loaded content, which may require additional handling
        // to make sure the User understands that this may cause side-effects. All the geometry
        // content should be removed.  However, some non-geometry can still have been imported
        // (materials, textures, etc.)
        if (coreInterface->GetCancel()) {
            bool cancelImport = true;

            // Avoid displaying a blocking dialog if 3ds Max is running in Quiet Mode:
            if (!coreInterface->GetQuietMode()) {
                cancelImport = MaxUsd::Ui::AskYesNoQuestion(
                    GetStdWString(IDS_IMPORT_CANCEL_TEXT),
                    GetStdWString(IDS_IMPORT_CANCEL_CAPTION));
            }

            if (cancelImport) {
                progressBar.Stop();
                // delete created nodes
                {
                    std::vector<INode*> nodes;
                    context.GetAllCreatedNodes(nodes);
                    INodeTab newNodes;
                    newNodes.Insert(0, static_cast<int>(nodes.size()), nodes.data());
                    GetCOREInterface17()->DeleteNodes(newNodes);
                }
                MaxUsd::Log::Info("USD import canceled.");
                return IMPEXP_CANCEL;
            }
            coreInterface->SetCancel(false);
        }

        if (primIt->IsPseudoRoot() || ExcludedPrimNode(primIt)) {
            continue;
        }

        if (primIt->IsInstance()) {
            DoImportInstanceIt(primIt, context, prototypeLookupMaps);
        } else {
            DoImportPrimIt(primIt, context, primReaderMap);
        }

        // Update the progress bar displayed by 3ds Max to notify the User about the status of the
        // operation:
        progressBar.UpdateProgress(currentPrimIndex++);
    }

    // delete prototype nodes that are now useless
    std::function<void(INode*)> deleteNode = [&](INode* node) {
        while (node->NumChildren() > 0) {
            deleteNode(node->GetChildNode(0));
        }
        context.RemoveNode(node);
        GetCOREInterface17()->DeleteNode(node, false);
    };
    auto prototypes = context.GetStage()->GetPrototypes();
    for (const auto& prototype : prototypes) {
        const SdfPath prototypePath = prototype.GetPath();
        INode*        prototypeNode = context.GetMaxNode(prototypePath, false);
        if (prototypeNode) {
            deleteNode(prototypeNode);
        }
    }

    context.RescaleRegisteredNodes();

    if (autoMtlPropagation) {
        pInstanceMgr->SetAutoMtlPropagation(true);
    }

    // Report that we are running chasers...
    progressBar.UpdateProgress(
        progressBar.GetTotal(), false, GetString(IDS_IMPORT_CHASERS_PROGRESS_MESSAGE));

    // call chasers
    // populate the chasers and run post import
    std::vector<std::pair<std::string, pxr::MaxUsdImportChaserRefPtr>> chasers;
    pxr::MaxUsdImportChaserRegistry::FactoryContext ctx(predicates, context, filename);
    // for available chasers to load if not done already
    pxr::MaxUsdImportChaserRegistry::GetAllRegisteredChasers();

    for (const std::string& chaserName : buildOptions.GetChaserNames()) {
        if (pxr::MaxUsdImportChaserRefPtr fn
            = pxr::MaxUsdImportChaserRegistry::Create(chaserName, ctx)) {
            chasers.push_back(std::make_pair(chaserName, fn));
        } else {
            MaxUsd::Log::Error("Failed to create chaser: {0}", chaserName.c_str());
        }
    }

    for (const auto& chaser : chasers) {
        if (chaser.second->PostImport()) {
            MaxUsd::Log::Info("Successfully executed PostImport() for {0}", chaser.first);
        } else {
            MaxUsd::Log::Error("Failed executing PostImport() for {0}", chaser.first);
        }
    }

    return IMPEXP_SUCCESS;
}

} // namespace MAXUSD_NS_DEF