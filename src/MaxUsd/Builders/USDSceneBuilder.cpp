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
#include "USDSceneBuilder.h"

#include <MaxUsd/MaxTokens.h>
#include <MaxUsd/Translators/AnimExportTask.h>
#include <MaxUsd/Translators/PrimWriterRegistry.h>
#include <MaxUsd/Translators/TranslatorMaterial.h>
#include <MaxUsd/Translators/WriteJobContext.h>
#include <MaxUsd/USDCore.h>
#include <MaxUsd/Utilities/Logging.h>
#include <MaxUsd/Utilities/MathUtils.h>
#include <MaxUsd/Utilities/MaxProgressBar.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/MetaDataUtils.h>
#include <MaxUsd/Utilities/PluginUtils.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>
#include <MaxUsd/resource.h>

#include <pxr/usd/kind/registry.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usd/inherits.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/shadowAPI.h>

#include <maxscript/maxwrapper/mxsobjects.h>

#include <Shlwapi.h>
#include <memory>
#include <mesh.h>
#include <modstack.h>
#include <stack>

namespace MAXUSD_NS_DEF {

USDSceneBuilder::USDSceneBuilder() = default;

pxr::UsdStageRefPtr USDSceneBuilder::Build(
    const USDSceneBuilderOptions&               buildOptions,
    bool&                                       cancelled,
    const fs::path&                             filename,
    std::map<std::string, pxr::SdfLayerRefPtr>& editedLayers,
    bool                                        isUSDZ = false)
{
    pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();

    auto& exportOptions = const_cast<USDSceneBuilderOptions&>(buildOptions);

    // Create the write job context - used for shader and prim writers.
    // Also resolve the token that can be in the MaterialLayerPath, so that the full path can be
    // used by the writers through the context.
    pxr::MaxUsdWriteJobContext writeJobContext { stage, filename.string(), exportOptions, isUSDZ };
    exportOptions.SetMaterialLayerPath(
        writeJobContext.ResolveString(buildOptions.GetMaterialLayerPath()));

    if (buildOptions.GetUseSeparateMaterialLayer()) {
        const auto matFilePath
            = USDCore::sanitizedFilename(buildOptions.GetMaterialLayerPath(), ".usda");
        if (matFilePath.empty()) {
            MaxUsd::Log::Error(
                "Invalid material layer path: {0}", buildOptions.GetMaterialLayerPath());
        } else if (matFilePath.extension() == ".usdz") {
            MaxUsd::Log::Error(
                "Invalid material layer path: {0}. USDZ is not a valid file format for material "
                "layers.",
                buildOptions.GetMaterialLayerPath());
        } else {
            auto ext = pxr::SdfFileFormat::FindByExtension(buildOptions.GetMaterialLayerPath());
            const auto identifier = matFilePath.string();

            // The layer could already be in memory...(previous version loaded in a stage)
            const auto matLayer = MaxUsd::CreateOrOverwriteLayer(ext, identifier);
            if (matLayer) {
                writeJobContext.AddUsedLayerIdentifier(matFilePath.string(), matLayer);
            } else {
                MaxUsd::Log::Error("Material Layer for {} failed to be created", identifier);
            }
        }
    }

    // Insert the stage in the global cache for the time of the export. Useful so it can be accessed
    // from callbacks. Removed from the cache using RAII.
    const MaxUsd::StageCacheScopeGuard stageCacheGuard { stage };

    pxr::TfToken upAxis;
    if (buildOptions.GetUpAxis() == USDSceneBuilderOptions::UpAxis::Y) {
        upAxis = pxr::UsdGeomTokens->y;
    } else {
        upAxis = pxr::UsdGeomTokens->z;
    }

    pxr::UsdGeomSetStageUpAxis(stage, upAxis);

    // Export units setup
    double stageScale = GetSystemUnitScale(UNITS_METERS);
    // round float imprecision
    stageScale = MaxUsd::MathUtils::RoundToSignificantDigit(
        stageScale, std::numeric_limits<float>::digits10);
    pxr::UsdGeomSetStageMetersPerUnit(stage, stageScale);

    INode* rootNode = coreInterface->GetRootNode();
    if (rootNode == nullptr) {
        return stage;
    }

    const auto timeConfig = buildOptions.GetResolvedTimeConfig();
    if (timeConfig.IsAnimated()) {
        stage->SetStartTimeCode(timeConfig.GetStartFrame());
        stage->SetEndTimeCode(timeConfig.GetEndFrame());
        // In 3dsMax, one tick is defined as 1/4800th of a second.
        const auto maxFramePerSecond = (4800.0 / double(GetTicksPerFrame()));
        stage->SetTimeCodesPerSecond(maxFramePerSecond);
        // Typically the FramePerSeconds and TimeCodePerSeconds are equal, although they
        // don't need necessarily need to be. According to the docs, FramePerSecond
        // "makes an advisory statement about how the contained data can be most usefully
        // consumed and presented. It's primarily an indication of the expected playback rate
        // for the data, but a timeline editing tool might also want to use this to decide
        // how to scale and label its timeline."
        stage->SetFramesPerSecond(maxFramePerSecond);
    }

    // If we are not exporting the whole scene, build the set of the nodes to export, for easy
    // access later.
    nodesToExportSet.clear();
    if (buildOptions.GetContentSource() == USDSceneBuilderOptions::ContentSource::NodeList) {
        const auto nodesToExport = buildOptions.GetNodesToExport();
        for (int i = 0; i < nodesToExport.Count(); ++i) {
            nodesToExportSet.emplace(nodesToExport[i]);
        }
    } else if (
        buildOptions.GetContentSource() == USDSceneBuilderOptions::ContentSource::Selection) {
        for (int i = 0; i < GetCOREInterface()->GetSelNodeCount(); ++i) {
            nodesToExportSet.emplace(GetCOREInterface()->GetSelNode(i));
        }
    }

    // If we are only exporting a set of nodes, and it is empty, we are done!
    // In practice, a user would most likely get stopped before reaching this point if trying to
    // export from an empty selection or an empty list of nodes.
    if (buildOptions.GetContentSource() != USDSceneBuilderOptions::ContentSource::RootNode
        && nodesToExportSet.empty()) {
        return stage;
    }

    MaxUsd::UniqueNameGenerator primNameGenerator;

    // Keep track of each prim's source node. To be able to notify users if needed.
    pxr::MaxUsdExportChaserRegistry::FactoryContext::PrimToNodeMap primsToNodes;
    pxr::TfHashSet<pxr::SdfPath, pxr::SdfPath::Hash>               primsToMaterialBind;

    static const std::wstring progressBarTitle = GetString(IDS_EXPORT_PROGRESS_TITLE);
    MaxProgressBar            progressBar(progressBarTitle.c_str());
    progressBar.SetEnabled(buildOptions.GetUseProgressBar());
    static const std::wstring completionMsg = GetString(IDS_EXPORT_PROGRESS_COMPLETED_MESSAGE);
    const auto                progressScopeGuard = MaxUsd::MakeScopeGuard(
        [&progressBar]() { progressBar.Start(); },
        [&progressBar]() { progressBar.Stop(false, completionMsg.c_str()); });

    if (!BuildStageFromMaxNodes(writeJobContext, primsToNodes, primsToMaterialBind, progressBar)) {
        // export was cancelled
        cancelled = true;
        stage.Reset();
        return stage;
    }

    // Set the first valid prim as default prim.
    if (!stage->HasDefaultPrim()) {
        pxr::UsdPrimSiblingRange allChildren
            = stage->GetPrimAtPath(pxr::SdfPath::AbsoluteRootPath()).GetAllChildren();

        for (const auto& prim : allChildren) {
            // Only prims which originate from 3dsMax nodes should be used as default prims.
            // For example, prototype prims (classes), should not be considered.
            if (primsToNodes.find(prim.GetPath()) == primsToNodes.end() || !prim.IsValid()) {
                continue;
            }
            stage->SetDefaultPrim(prim);
            break;
        }
    }

    if (buildOptions.GetTranslateMaterials()) {
        pxr::MaxUsdTranslatorMaterial::ExportMaterials(
            writeJobContext, primsToMaterialBind, progressBar);
    }

    // Report that we are running chasers...
    progressBar.UpdateProgress(
        progressBar.GetTotal(), false, GetString(IDS_EXPORT_CHASERS_PROGRESS_MESSAGE));

    // call chasers
    // populate the chasers and run post export
    std::vector<std::pair<std::string, pxr::MaxUsdExportChaserRefPtr>> chasers;
    pxr::MaxUsdExportChaserRegistry::FactoryContext                    ctx(
        stage, primsToNodes, buildOptions, filename);
    // for available chasers to load if not done already
    pxr::MaxUsdExportChaserRegistry::GetAllRegisteredChasers();
    for (const std::string& chaserName : buildOptions.GetChaserNames()) {
        if (pxr::MaxUsdExportChaserRefPtr fn
            = pxr::MaxUsdExportChaserRegistry::Create(chaserName, ctx)) {
            chasers.push_back(std::make_pair(chaserName, fn));
        } else {
            MaxUsd::Log::Error("Failed to create chaser: {0}", chaserName.c_str());
        }
    }

    for (const auto& chaser : chasers) {
        if (chaser.second->PostExport()) {
            MaxUsd::Log::Info("Successfully executed PostExport() for {0}", chaser.first);
        } else {
            MaxUsd::Log::Error("Failed executing PostExport() for {0}", chaser.first);
        }
    }

    editedLayers = writeJobContext.GetLayerMap();

    return stage;
}

struct NodeToExportStackItem
{
    INode*                                       nodeToExport;
    std::shared_ptr<MaxUsd::UniqueNameGenerator> nameGenerator;
    pxr::SdfPath                                 parentPrimPath;
    // Keep track of the highest ancester in the hierarchy which is hidden.
    // If none, this wil remain null.
    INode* hiddenAncestor;
};

int USDSceneBuilder::GetNumberOfNodeToExport(const USDSceneBuilderOptions& buildOptions)
{
    switch (buildOptions.GetContentSource()) {
    case USDSceneBuilderOptions::ContentSource::Selection: return coreInterface->GetSelNodeCount();
    case USDSceneBuilderOptions::ContentSource::NodeList:
        return buildOptions.GetNodesToExport().Count();
    case USDSceneBuilderOptions::ContentSource::RootNode: return MaxUsd::GetSceneObjectCount();
    default: DbgAssert(_T("Unhandled content source data type"));
    }

    return 0;
}

bool USDSceneBuilder::ReuseParentPrimForInstancing(const TranslationContext& context)
{
    if (context.parentPrimPath.IsAbsoluteRootPath()) {
        return false;
    }

    INode* parentNode = context.node->GetParentNode();
    if (parentNode == nullptr || parentNode->IsRootNode() || parentNode->NumberOfChildren() > 1) {
        return false;
    }

    Object* parentObject = parentNode->EvalWorldState(context.timeConfig.GetStartTime()).obj;
    if (parentObject->ClassID() != POINTHELPER_TYPE_ID) {
        return false;
    }

    // Check for conflicts on the usd-metadata (usd_kind, usd_hidden, etc.). This meta-data is not
    // animatable, so we can simply look for a conflict at the start frame.
    const auto childObject = context.node->EvalWorldState(context.timeConfig.GetStartTime()).obj;
    if (MaxUsd::MetaData::CheckForConflict(
            childObject, parentObject, context.timeConfig.GetStartTime())) {
        return false;
    }

    return true;
}

bool USDSceneBuilder::BuildStageFromMaxNodes(
    pxr::MaxUsdWriteJobContext&                                     writeJobContext,
    pxr::MaxUsdExportChaserRegistry::FactoryContext::PrimToNodeMap& primToNodeMap,
    pxr::TfHashSet<pxr::SdfPath, pxr::SdfPath::Hash>&               primsToMaterialBind,
    MaxProgressBar&                                                 progress)
{
    std::map<INode*, pxr::SdfPath> exportedNodesToPrims;
    auto rootNamesGenerator = std::make_shared<MaxUsd::UniqueNameGenerator>();

    auto& buildOptions = writeJobContext.GetArgs();

    std::stack<NodeToExportStackItem> nodeToExportStack;
    auto                              pushNodeChildrensToExportStack =
        [&nodeToExportStack, &buildOptions, this, &rootNamesGenerator](
            INode* node, const pxr::SdfPath& parentPrimPath, INode* parentHiddenAncestor) {
            // If exporting the selection or from a node list, nodes that don't have their parents
            // selected end up being exported at the root level. They use the same name generator to
            // ensure that their generated prim paths are unique.
            std::shared_ptr<MaxUsd::UniqueNameGenerator> nameGenerator;
            if (!nodesToExportSet.empty()
                && (node->IsRootNode() || nodesToExportSet.find(node) == nodesToExportSet.end())) {
                nameGenerator = rootNamesGenerator;
            } else {
                nameGenerator = std::make_shared<MaxUsd::UniqueNameGenerator>();
            }

            std::unordered_set<std::wstring> names;
            std::list<INode*>                children;
            std::list<INode*>                childrenWithNameConflict;

            for (int i = 0; i < node->NumberOfChildren(); ++i) {
                const auto child = node->GetChildNode(i);
                const auto name = child->GetName();
                // No conflict -> add to the front of the list.
                if (names.find(name) == names.end()) {
                    names.emplace(name);
                    children.emplace_front(child);
                    continue;
                }
                // Conflict -> add to the end of the list.
                childrenWithNameConflict.emplace_front(child);
            }

            // As we go down the hierarchy, we want to keep track if an ancestor in the
            // node's hierarchy was hidden so we can warn users in case we cannot export
            // the visibility correctly, which may be the case as in USD, visibility is
            // inherited, whereas it is not in 3dsMax. If we are not using USD visibility
            // to match the max node's hidden state, no need to track this. As no USD prim
            // will be flagged as invisible by the exporter.
            INode* hiddenAncestor = nullptr;
            if (buildOptions.GetUseUSDVisibility()) {
                if (parentHiddenAncestor) {
                    hiddenAncestor = parentHiddenAncestor;
                } else {
                    if (node->IsNodeHidden()) {
                        hiddenAncestor = node;
                    }
                }
            }

            // If the name was already used by a sibling, we must use another. We do not need to
            // worry about "stealing" the name of a sibling not traversed yet, as we made sure to
            // process siblings with conflicting names at the very end by pushing them deeper in the
            // stack of node to be exported.
            for (auto it = childrenWithNameConflict.begin(); it != childrenWithNameConflict.end();
                 ++it) {
                NodeToExportStackItem nodeStackItem {
                    *it, nameGenerator, parentPrimPath, hiddenAncestor
                };
                nodeToExportStack.push(nodeStackItem);
            }

            for (auto it = children.begin(); it != children.end(); ++it) {
                NodeToExportStackItem nodeStackItem {
                    *it, nameGenerator, parentPrimPath, hiddenAncestor
                };
                nodeToExportStack.push(nodeStackItem);
            }
        };

    // additional operations to perform on translated prims
    std::vector<USDSceneBuilder::translationPrimConfigurator_t> primConfigurators;
    // 3ds Max Usd Custom Attributes to USD Attributes
    primConfigurators.emplace_back(
        [this](Object* object) { return true; },
        [this](const TranslationContext& context, pxr::UsdPrim& translatedPrim) {
            ConfigureUsdAttributes(context, translatedPrim);
            ConfigureKind(context.node, translatedPrim);
        });

    const int numberOfItemToExport = GetNumberOfNodeToExport(buildOptions);

    pxr::SdfPath rootPath(buildOptions.GetRootPrimPath());
    auto         stage = writeJobContext.GetUsdStage();

    // Unless the absolute root path is used ("/"), we want the root prim to be specified as a
    // Scope.
    if (!rootPath.IsAbsoluteRootPath()) {
        MaxUsd::FetchOrCreatePrim<pxr::UsdGeomXformable>(
            stage, rootPath, pxr::MaxUsdPrimTypeTokens->Xform);
        // The top level primitive from that path should be our default prim.
        auto path = rootPath;
        while (path.GetParentPath() != pxr::SdfPath::AbsoluteRootPath()) {
            path = path.GetParentPath();
        }
        stage->SetDefaultPrim(stage->GetPrimAtPath(path));
    }

    const auto timeConfig = buildOptions.GetResolvedTimeConfig();
    if (buildOptions.GetTimeMode() == USDSceneBuilderOptions::TimeMode::CurrentFrame
        && !MaxUsd::MathUtils::IsAlmostZero(static_cast<float>(timeConfig.GetStartFrame()))) {
        MaxUsd::Log::Warn("The export TimeMode is configured as #current, the specified StartFrame "
                          "will be ignored.");
    }

    if (buildOptions.GetTimeMode() == USDSceneBuilderOptions::TimeMode::FrameRange
        && buildOptions.GetEndFrame() < buildOptions.GetStartFrame()) {
        MaxUsd::Log::Warn(
            "A frame range is exported, but the endFrame is smaller than the startFrame, only the "
            "startFrame will be exported.");
    }

    // 1) Export preview pass :
    // Traverse the 3dsMax scene and export the nodes in "preview" mode. This only simulates the
    // export, figuring out what USD Prims will be created at what paths.

    PrepareExportPass();
    // Used to collect all the USD prims that will be needed to export the scene.
    MaxUsd::PrimDefVector scenePrimDefs;
    scenePrimDefs.reserve(
        numberOfItemToExport); // May get a bit bigger if nodes need multiple prims.

    // Used to collect all translations that we will need to perform to export everything in the
    // scene. Basically a Max node, and where it needs to be exported in the USD Hierarchy.
    std::vector<TranslationContext> translationItems;

    // Traverse the 3dsMax scene, breadth first.
    std::unordered_set<INode*> hiddenAncestorsWithVisibleDescendants;
    INode*                     nodeToConvert = coreInterface->GetRootNode();
    pushNodeChildrensToExportStack(nodeToConvert, rootPath, nullptr);
    while (!nodeToExportStack.empty()) {
        const NodeToExportStackItem& itemToExport = nodeToExportStack.top();
        nodeToConvert = itemToExport.nodeToExport;

        // Warn if visibility cannot be exported correctly (visibility is inherited in USD but not
        // in 3dsMax). Make sure to only warn once per problematic hierarchy.
        if (itemToExport.hiddenAncestor && !nodeToConvert->IsNodeHidden()
            && hiddenAncestorsWithVisibleDescendants.find(itemToExport.hiddenAncestor)
                == hiddenAncestorsWithVisibleDescendants.end()) {
            MaxUsd::Log::Warn(
                L"Node {0} is hidden but has visible descendants. Because in USD visibility is "
                L"inherited, this "
                L"may lead to objects visible in 3dsMax being hidden in USD.",
                itemToExport.hiddenAncestor->GetName());
            hiddenAncestorsWithVisibleDescendants.insert(itemToExport.hiddenAncestor);
        }

        pxr::SdfPath             nodeRootPrimPath;
        MaxUsd::PrimDefVectorPtr nodePrimSpecs;

        // Check if the node should be excluded from export. If nodesToExportSet is empty, it means
        // we want to export the entire scene.
        bool excludeNode = !nodesToExportSet.empty()
            && nodesToExportSet.find(nodeToConvert) == nodesToExportSet.end();

        const std::string primName
            = pxr::TfMakeValidIdentifier(MaxUsd::MaxStringToUsdString(nodeToConvert->GetName()));
        const std::string uniquePrimName = itemToExport.nameGenerator->GetName(primName);
        if (primName != uniquePrimName) {
            MaxUsd::Log::Warn(
                L"Found node name conflict, exporting node {0} as {1} instead.",
                nodeToConvert->GetName(),
                MaxUsd::UsdStringToMaxString(uniquePrimName).data());
        }

        if (!excludeNode) {
            // Preview the export of the node using the "preview" mode.
            // Just figuring out what prims would get created.
            TranslationContext translationContext {
                nodeToConvert, stage, itemToExport.parentPrimPath, uniquePrimName, timeConfig,
                true // Preview mode.
            };

            {
                // Disable logging while exporting nodes in preview mode.
                const auto scopeGuard = MaxUsd::MakeScopeGuard(
                    []() { MaxUsd::Log::Pause(); }, []() { MaxUsd::Log::Resume(); });

                bool                   doMtlAssign;
                MaxUsd::AnimExportTask animTask { buildOptions.GetResolvedTimeConfig() };
                nodePrimSpecs = ProcessNode(
                    translationContext,
                    writeJobContext,
                    primConfigurators,
                    stage,
                    doMtlAssign,
                    animTask);
            }

            // Save this translation item for the second pass, which will actually export the node's
            // data.
            translationContext.preview = false;
            translationItems.push_back(translationContext);

            if (nodePrimSpecs && !nodePrimSpecs->empty()) {
                scenePrimDefs.insert(
                    scenePrimDefs.end(), nodePrimSpecs->begin(), nodePrimSpecs->end());
                // The first prim in the vector is the root prim for the node.
                nodeRootPrimPath = nodePrimSpecs->front().path;
                if (!nodeRootPrimPath.IsEmpty()) {
                    primToNodeMap.insert({ nodeRootPrimPath, nodeToConvert });
                }
                exportedNodesToPrims.emplace(nodeToConvert, nodeRootPrimPath);
            }
        }

        // Remove the node we just exported from the stack
        nodeToExportStack.pop();

        // Push all the node childrens to the stack to be exported.
        pushNodeChildrensToExportStack(
            nodeToConvert,
            nodeRootPrimPath.IsEmpty() ? rootPath : nodeRootPrimPath,
            itemToExport.hiddenAncestor);
    }

    // 2) Prim creation pass :
    // Create all Prims in a single pass. Using Sdf APIs and batching the creation of all prims
    // in a single SdfChangeBlock speeds up the export considerably, as all notifications can be
    // processed at the same time.
    {
        pxr::SdfChangeBlock primBatchCreate;
        for (const auto& primSpec : scenePrimDefs) {
            if (primSpec.path.IsEmpty()) {
                continue;
            }
            auto prim = pxr::SdfCreatePrimInLayer(stage->GetRootLayer(), primSpec.path);
            if (primSpec.type == pxr::MaxUsdPrimTypeTokens->Class) {
                prim->SetSpecifier(pxr::SdfSpecifierClass);
                prim->SetTypeName(primSpec.type);
            } else if (primSpec.type == pxr::MaxUsdPrimTypeTokens->Over) {
                prim->SetSpecifier(pxr::SdfSpecifierOver);
                // No type name for "over" prims.
            } else {
                prim->SetSpecifier(pxr::SdfSpecifierDef);
                prim->SetTypeName(primSpec.type);
            }
        }
    }

    // 3) Export pass :
    // Populate the USD prim properties from the nodes' data.
    // This is where most of the work happens, and where we perform the conversion
    // of Max content to USD content.
    writeJobContext.SetNodeToPrimMap(exportedNodesToPrims);

    PrepareExportPass();

    progress.SetTotal(numberOfItemToExport);

    // As we process each node, we accumulate some work that we need to do for each node->prim
    // translation. Namely, writing the prim attributes, and transforms. Everything else is setup
    // right away as we process the node. The reason we need to delay the write of attributes and
    // transforms is we want to evaluate all object and transforms at a time "t" at the same time,
    // to benefit from 3dsmax's caching of the world state. AnimExportTask accepts work that needs
    // to be run at a certain time, it then makes sure to batch all the work that needs to evaluate
    // max data at the same time together.
    MaxUsd::AnimExportTask timeSamplesExportTask { buildOptions.GetResolvedTimeConfig() };

    const auto prepareExportProgressMsg = GetString(IDS_EXPORT_PREPARING_EXPORT);
    int        progressIndex = 0;
    for (const auto& translationItem : translationItems) {
        // Stop the import in its current state if the User chose to cancel it.
        // NOTE: This will result in partially-loaded content, which may require additional handling
        // to make sure the User understands that this may cause side-effects.
        if (coreInterface->GetCancel()) {
            MaxUsd::Log::Info("USD Export cancelled.");
            return false;
        }
        bool       doMtlAssign = false;
        const auto prims = ProcessNode(
            translationItem,
            writeJobContext,
            primConfigurators,
            stage,
            doMtlAssign,
            timeSamplesExportTask);

        // Report to the caller if the prim associated with this node should be considered for
        // material assignment.
        if (prims && !prims->empty()) {
            // Two cases.
            // 1) The node is not an instance, perform material on the node if ExportNode() tells us
            // we should. 2) The node is an instance, ExportNode() will only return doAssignMaterial
            // = true for the first instances (assuming the translated object should be assigned a
            // material), as the prim writer is only queried / executed for the first instance.
            // Subsequent instances just point to the already created class prim, and the prim
            // writer is not involved. For this reason, in the case of instances, we keep track of
            // the doAssignMaterial material returned for the first instances.

            // A lambda to find the prototype (class) prim used by a given prim. The passed prim is
            // assumed to be the top level prim for a node. Depending on the export scenario (object
            // offset, xform split, etc. the instance prim can be that prim directly, or its first
            // child.
            auto findPrototypePrim = [this, stage](const pxr::SdfPath& primPath) {
                const auto protoIt = instanceToPrototype.find(primPath);
                if (protoIt != instanceToPrototype.end()) {
                    return protoIt->second;
                }
                const auto  prim = stage->GetPrimAtPath(primPath);
                const auto& children = prim.GetChildren();
                if (!children.empty()) {
                    const auto& path = children.front().GetPath();
                    const auto  it = instanceToPrototype.find(path);
                    if (it != instanceToPrototype.end()) {
                        return it->second;
                    }
                }
                // No instancing.
                return pxr::SdfPath {};
            };

            const auto& nodeRoot = prims->front();
            auto        protoPrim = findPrototypePrim(nodeRoot.path);

            // Handle instancing scenario.
            if (!protoPrim.IsEmpty()) {
                // First instance, store info that instances of this prototype should have materials
                // assigned.
                if (doMtlAssign) {
                    prototypeMaterialReq.insert(protoPrim);
                } else {
                    const auto it = prototypeMaterialReq.find(protoPrim);
                    if (it != prototypeMaterialReq.end()) {
                        doMtlAssign = true;
                    }
                }
            }

            if (doMtlAssign) {
                primsToMaterialBind.insert(nodeRoot.path);
            }
        }

        progress.UpdateProgress(++progressIndex, true, prepareExportProgressMsg);
    }

    // Export time sample data.
    timeSamplesExportTask.Execute(progress);

    // 4) Instancing setup pass :
    // Set up instancing properties. This triggers stage notifications, which can dramatically
    // slow down the export, for this reason, we wrap all these in a SdfChangeBlock, so that
    // the stage can process all the notifications at the same time, quickly.
    {
        pxr::SdfChangeBlock instancingSetup;
        for (const auto& pair : instanceToPrototype) {
            auto prim = stage->GetPrimAtPath(pair.first);
            prim.GetInherits().AddInherit(pair.second);
            prim.SetInstanceable(true);
        }
    }

    return true;
}

MaxUsd::PrimDefVectorPtr USDSceneBuilder::ProcessNode(
    const TranslationContext&                                          context,
    const pxr::MaxUsdWriteJobContext&                                  writeJobContext,
    const std::vector<USDSceneBuilder::translationPrimConfigurator_t>& primConfigurators,
    const pxr::UsdStageRefPtr&                                         stage,
    bool&                                                              doAssignMaterial,
    MaxUsd::AnimExportTask&                                            animExportTask)
{
    MaxUsd::PrimDefVectorPtr exportedPrims;

    // Translate the node using the first matching operator:
    bool translationHandled = false;

    // Dummy object evaluation to work around an animation controller issue present in 3dsMax,
    // some controllers will wrongly report an "instantaneous" interval the first time they are
    // queried. This dummy evaluation might be wrong, but the next one will be correct. We only
    // ever need to do this one, so do it here.
    context.node->EvalWorldState(context.timeConfig.GetStartTime() - 1);
    const auto object = context.node->EvalWorldState(context.timeConfig.GetStartTime()).obj;

    const auto& buildOptions = writeJobContext.GetArgs();

    // Check if we need to export the node, if it is hidden.
    if (buildOptions.GetTranslateHidden() || !context.node->IsNodeHidden()) {
        size_t numRegisteredWriters = 0;

        auto primWriter = pxr::MaxUsdPrimWriterRegistry::FindWriter(
            writeJobContext, context.node, numRegisteredWriters);

        if (primWriter) {
            if (numRegisteredWriters > 1) {
                MaxUsd::Log::Info(
                    L"Multiple registered prim writers can support node {0}, using {1}.",
                    context.node->GetName(),
                    primWriter->GetWriterName().data());
            }

            const auto targetRootPath = buildOptions.GetRootPrimPath();

            exportedPrims = WriteNodePrims(
                [this, &primWriter, &doAssignMaterial, &animExportTask, &buildOptions](
                    const TranslationContext& context, bool applyOffsetTransform) {
                    // Ask the writer about the prim's type and name for this node.
                    const auto resolvedPrimName = pxr::TfToken(
                        pxr::TfMakeValidIdentifier(primWriter->GetPrimName(context.primName)));
                    const auto primType = primWriter->GetPrimType();

                    MaxUsd::PrimDef targetPrim {
                        context.parentPrimPath.AppendChild(resolvedPrimName), primType
                    };

                    // In preview, only interested to know where the prim will be exported, and what
                    // it's type is.
                    if (context.preview) {
                        return targetPrim;
                    }

                    // Get the prim that was created for us.
                    auto usdPrim = context.stage->GetPrimAtPath(targetPrim.path);
                    if (!usdPrim.IsValid()) {
                        MaxUsd::Log::Error(
                            L"Unable to write the 3dsMax node \"{0}\" to the prim at {1}. This "
                            L"prim is no "
                            L"longer valid. It may have been pruned by the actions of a prim "
                            L"writer.",
                            context.node->GetName(),
                            MaxUsd::UsdStringToMaxString(targetPrim.path.GetString()).data());
                        return targetPrim;
                    }

                    doAssignMaterial = primWriter->RequiresMaterialAssignment()
                        == MaxUsd::MaterialAssignRequirement::Default;

                    // Before we hand off the prim to the prim writer, apply the object offset
                    // transform. Do this now because if the prim writer needs to add a transform to
                    // the stack part of the object's translation, in most cases it will need to be
                    // added after the object offset, so we are making their job easier. If needed,
                    // the writer can specify that it wants to handle the object offset transform
                    // itself (for example we use this feature when baking the object offset
                    // transform into the geometry). Avoid adding object offset transforms for
                    // groups, special case.
                    const auto writerHandlesOffset = primWriter->HandlesObjectOffsetTransform();
                    if (!writerHandlesOffset && applyOffsetTransform
                        && !context.node->IsGroupHead()) {
                        // The "root" primitive object are exported to, should always be Xformable.
                        if (!usdPrim.IsA<pxr::UsdGeomXformable>()) {
                            MaxUsd::Log::Error(
                                "The prim created for the node \"{0}\" is not an Xformable, "
                                "unable to apply the object offset.");
                        } else {
                            pxr::UsdGeomXformable xformable { usdPrim };
                            MaxUsd::ApplyObjectOffsetTransform(
                                context.node, xformable, context.timeConfig.GetStartTime());
                        }
                    }

                    // If the writer requested to handle the object offset transform, let it know if
                    // it should apply it or not part of this translation (for example, we never
                    // want to apply object offsets to instanced prims directly).
                    const auto requestApplyOffset = applyOffsetTransform && writerHandlesOffset;

                    auto node = context.node;

                    // Queue the work of writing the node properties to the prim - it will be
                    // batched with other translation operations needing to be done at the same
                    // 3dsMax time values (we figure this out from the validity intervals). This
                    // prevents us re-evaluating the same objects multiple times at the same time
                    // values.
                    animExportTask.AddObjectExportOp(
                        [primWriter](TimeValue time) {
                            return primWriter->GetValidityInterval(time);
                        },
                        [primWriter, node, usdPrim, requestApplyOffset](
                            const MaxUsd::ExportTime time) mutable {
                            if (!primWriter->Write(usdPrim, requestApplyOffset, time)) {
                                MaxUsd::Log::Error(
                                    L"Failed to write the prim properties for {0} as time {1}.",
                                    node->GetName(),
                                    std::to_wstring(time.GetUsdTime().GetValue()));
                            }
                        },
                        [primWriter, node, usdPrim]() mutable {
                            if (!primWriter->PostExport(usdPrim)) {
                                MaxUsd::Log::Error(
                                    L"Failed to execute post export for {0}.", node->GetName());
                            }
                        });

                    return targetPrim;
                },
                context,
                primWriter->GetObjectPrimSuffix(),
                primWriter->RequiresXformPrim(),
                primWriter->RequiresInstancing(),
                targetRootPath);

            if (!exportedPrims->empty() && !exportedPrims->front().path.IsEmpty()) {
                translationHandled = true;
            }
        }
    }
    // If the object type was not handled (either it is not supported or it is excluded
    // from the export), we might still need to export it as an Xform if it has children
    // so that any of its exported descendants will have the correct transforms.
    if (!translationHandled) {
        if (HasExportableDescendants(context.node, buildOptions)) {
            MaxUsd::PrimDef primSpec
                = { context.parentPrimPath.AppendChild(pxr::TfToken(context.primName)),
                    pxr::MaxUsdPrimTypeTokens->Xform };

            exportedPrims = std::make_shared<MaxUsd::PrimDefVector>();
            exportedPrims->push_back(primSpec);
            if (context.preview) {
                return exportedPrims;
            }
            MaxUsd::FetchOrCreatePrim<pxr::UsdGeomXformable>(
                stage, primSpec.path, pxr::MaxUsdPrimTypeTokens->Xform);
            translationHandled = true;
            MaxUsd::Log::Info(
                L"Node {0} will be exported to a Xform prim. It is either excluded from export by "
                L"configuration or unsupported, but it has exported descendants.",
                context.node->GetName());
        } else {
            MaxUsd::Log::Info(
                L"Node {0} will be skipped. It is either excluded from export by configuration or "
                L"unsupported.",
                context.node->GetName());
        }
    }

    if (translationHandled) {
        auto nodeRootPrimPath = exportedPrims->front().path;

        pxr::UsdPrim prim = stage->GetPrimAtPath(nodeRootPrimPath);
        if (prim.IsValid()) {
            if (!prim.IsA<pxr::UsdGeomXformable>()) {
                MaxUsd::Log::Error(
                    L"The root primitive created for node {0} is not Xformable. Unable to apply "
                    L"the node's transform.",
                    context.node->GetName());
            } else {

                // Compute object transforms:
                pxr::UsdGeomXformable xFormPrim(prim);

                // Setup the USD visibility, from the Max node's hidden state, if requested.
                if (context.node->IsNodeHidden() && buildOptions.GetUseUSDVisibility()) {
                    xFormPrim.MakeInvisible(pxr::UsdTimeCode::Default());
                }

                // Queue the work of writing the node's transform - it will be batched with other
                // translation operations needing to be done at the same 3dsMax time values (we
                // figure this out from the validity intervals). This prevents us re-evaluating the
                // same objects multiple times at the same time values.
                auto node = context.node;
                animExportTask.AddTransformExportOp(
                    [&buildOptions, node, xFormPrim](
                        const MaxUsd::ExportTime& time, pxr::UsdGeomXformOp& usdGeomXFormOp) {
                        pxr::GfMatrix4d maxTransformMatrix
                            = MaxUsd::ToUsd(node->GetNodeTM(time.GetMaxTime()));
                        MaxUsd::MathUtils::RoundMatrixValues(
                            maxTransformMatrix, std::numeric_limits<float>::digits10);

                        if (buildOptions.GetUpAxis() == USDSceneBuilderOptions::UpAxis::Y) {
                            MaxUsd::MathUtils::ModifyTransformZToYUp(maxTransformMatrix);
                        }

                        // Compute the local transform of the prim. The current transform in the
                        // hierarchy (i.e. the world transform of the parent).
                        pxr::GfMatrix4d parentWorldTransform;
                        parentWorldTransform.SetIdentity();

                        const auto parentNode = node->GetParentNode();
                        if (parentNode && !parentNode->IsRootNode()) {
                            parentWorldTransform = MaxUsd::GetNodeTransform(
                                parentNode,
                                time.GetMaxTime(),
                                buildOptions.GetUpAxis() == USDSceneBuilderOptions::UpAxis::Y);
                        }

                        // The parent transform must be invertible for us to be able to compute the
                        // local transform. A matrix with a non-zero determinant is invertible.
                        if (parentWorldTransform.GetDeterminant() != 0.0) {
                            pxr::GfMatrix4d transformMatrix
                                = maxTransformMatrix * parentWorldTransform.GetInverse();
                            // If exporting a single frame, no need to specify the transform if it
                            // is the identity. When exporting an animation, we need to, as the
                            // transform might change over time. If the frame at the identity was
                            // not exported, the transform at that frame would be interpolated from
                            // other authored frames, which would be wrong.
                            const auto timeConfig = buildOptions.GetResolvedTimeConfig();
                            if (!MaxUsd::MathUtils::IsIdentity(transformMatrix)
                                || timeConfig.IsAnimated()) {
                                bool         resetsXformStack = false;
                                const size_t nbOfOps
                                    = xFormPrim.GetOrderedXformOps(&resetsXformStack).size();
                                if (!usdGeomXFormOp.IsDefined()) {
                                    usdGeomXFormOp = xFormPrim.AddXformOp(
                                        pxr::UsdGeomXformOp::TypeTransform,
                                        pxr::UsdGeomXformOp::PrecisionDouble,
                                        nbOfOps > 0 ? pxr::TfToken("t" + std::to_string(nbOfOps))
                                                    : pxr::TfToken());
                                }
                                usdGeomXFormOp.Set(transformMatrix, time.GetUsdTime());
                            }
                        } else {
                            MaxUsd::Log::Error(
                                std::wstring(L"The parent prim of ") + node->GetName()
                                + std::wstring(L" has a non-invertible world transform matrix. "
                                               L"Unable to compute its local transform at frame ")
                                + std::to_wstring(
                                    double(time.GetMaxTime()) / double(GetTicksPerFrame())));
                        }
                    });
            }

            for (const auto& configuratorStep : primConfigurators) {
                if (configuratorStep.AppliesToObject(object)) {
                    configuratorStep.Execute(context, prim);
                    break;
                }
            }
            MaxUsd::Log::Info(
                L"Exported node {0} to {1}.",
                context.node->GetName(),
                MaxUsd::UsdStringToMaxString(nodeRootPrimPath.GetString()).data());
        }
    }
    return exportedPrims;
}

bool USDSceneBuilder::HasExportableDescendants(
    INode*                        node,
    const USDSceneBuilderOptions& buildOptions)
{
    // Check if we already have the answer in the cache.
    const auto it = hasExportableDescendantsMap.find(node);
    if (it != hasExportableDescendantsMap.end()) {
        return it->second;
    }

    bool exportableHierarchy = false;
    // If we are exporting from a node list, make sure the node should be considered.
    if (nodesToExportSet.empty() || nodesToExportSet.find(node) != nodesToExportSet.end()) {
        // Should the node be ignored because it is hidden?
        if (!node->IsNodeHidden() || buildOptions.GetTranslateHidden()) {
            // Check if any of the translation operations apply to the node's object. If so, it is
            // considered exportable.
            exportableHierarchy = pxr::MaxUsdPrimWriterRegistry::CanBeExported(node, buildOptions);
        }
    }

    // If the given node itself is not exportable, we must check its children, we do so via
    // recursion.
    if (!exportableHierarchy) {
        for (int i = 0; i < node->NumChildren(); ++i) {
            auto childNode = node->GetChildNode(i);
            // If exporting from the a node list, check if the child should be considered. If not,
            // we must stop the recursion, as if any descendants are exported, they will be parented
            // at the root, and so not part of the nodes sub-hierarchy on the USD side. Example :
            // Node A (Selected)
            //    - Node B
            //       - Node C (Selected)
            // Will export to :
            // - Prim Node A
            // - Prim Node C (same level as Node A in the USD hierarchy)
            if (!nodesToExportSet.empty()
                && nodesToExportSet.find(childNode) == nodesToExportSet.end()) {
                continue;
            }

            if (HasExportableDescendants(childNode, buildOptions)) {
                exportableHierarchy = true;
                break;
            }
        }
    }

    hasExportableDescendantsMap.insert({ node, exportableHierarchy });
    return exportableHierarchy;
}

MaxUsd::PrimDefVectorPtr USDSceneBuilder::WriteNodePrims(
    std::function<MaxUsd::PrimDef(const TranslationContext&, bool applyOffsetTransform)>
                                         createObjectPrim,
    const TranslationContext&            context,
    const std::string&                   objectPrimSuffix,
    const MaxUsd::XformSplitRequirement& xformRequirement,
    const MaxUsd::InstancingRequirement& instancingRequirement,
    const pxr::SdfPath&                  rootPrim)
{
    bool isInstanceableNode
        = maxNodeToClassPrimMap.find(context.node) != maxNodeToClassPrimMap.end();

    bool            createdClass = false;
    MaxUsd::PrimDef instanceObjectPrimSpec;
    MaxUsd::PrimDef classPrimSpec;

    INodeTab instanceNodes;

    // Create the class prim if it is the first time we identify this node as being instanceable.
    //  i.e. The node was not previously in the map and we found other node that can be exported as
    //  instance of each other.
    if (instancingRequirement == MaxUsd::InstancingRequirement::Default && !isInstanceableNode
        && MaxUsd::FindInstanceableNodes(context.node, instanceNodes, nodesToExportSet)) {
        isInstanceableNode = true;

        std::string classPrimName
            = "_class_" + classPrimBaseNameGenerator.GetName(context.primName);
        const pxr::SdfPath classPrimPath = rootPrim.AppendChild(pxr::TfToken(classPrimName));

        classPrimSpec = { classPrimPath, pxr::MaxUsdPrimTypeTokens->Class };

        if (!context.preview) {
            context.stage->CreateClassPrim(classPrimPath);
        }

        TranslationContext objectContext { context.node,     context.stage,      classPrimPath,
                                           context.primName, context.timeConfig, context.preview };

        instanceObjectPrimSpec = createObjectPrim(objectContext, false);

        // Populate the map for every instance node we found to avoid searching later when we
        // encounter them.
        for (int i = 0; i < instanceNodes.Count(); i++) {
            maxNodeToClassPrimMap.insert(
                std::pair<INode*, pxr::SdfPath>(instanceNodes[i], classPrimPath));
        }

        createdClass = true;
    }

    pxr::SdfPath currentPrimPath = context.parentPrimPath;
    // This will keep track of all the prims that were created on export to represent the max node.
    MaxUsd::PrimDefVectorPtr exportedPrimPaths = std::make_shared<MaxUsd::PrimDefVector>();
    bool                     applyObjectPrimSuffix = false;

    const Matrix3 offsetTransform = MaxUsd::GetMaxObjectOffsetTransform(context.node);

    // Create a prim for the node's object if necessary. In some cases, it is possible to export
    // a node and its object to a single USD prim. In others we need to keep them separate, most
    // often because of the inheritance rules, indeed only the Node's transform should be inherited.
    bool isIdentityOffset = MaxUsd::MathUtils::IsIdentity(offsetTransform);

    // If a WSM is applied and the object is not at the identity transform, we might need to
    // transform the geometry's points back into local space (with the inverse of the node's
    // transform), so that with the inherited transforms from the USD hierarchy, the overall
    // transforms of the points are correct.
    const bool wsmTransformToLocal
        = MaxUsd::WsmRequiresTransformToLocalSpace(context.node, context.timeConfig.GetStartTime());

    // 1) Prim writers can specify their needs of having a dedicated xform to encode the node's
    // transform. If the requirement from the writer is "ForOffsetObjects", two interesting cases :
    // - If the offset is the identity, no need to create a prim for the offset.
    // - If there is a WSM, the vertices are already transformed to world space, the offset already
    // considered.
    bool objectXformRequiredFromConfig = (xformRequirement == MaxUsd::XformSplitRequirement::Always)
        || (xformRequirement == MaxUsd::XformSplitRequirement::ForOffsetObjects && !isIdentityOffset
            && !wsmTransformToLocal);

    // 2) When instancing a USD prim, it cannot have children, as they would be ignored. And we cant
    // bake the offset into the geometry, as it is reused. therefore if there are any children, or
    // if the offset is not the identity, we must create a separate prim for the object, so that it
    // can be instanced.
    bool objectXFormRequiredFromInstancing
        = isInstanceableNode && (!isIdentityOffset || context.node->NumChildren() > 0);

    // 3) Need an extra Xform for objects exported as guides. In USD, the purpose is inherited, so
    // to avoid any children of the object exported as guides to also be set as guide, we use an
    // extra XForm. Later in this function, some objects (for now, only bones) will be set as
    // guides, to avoid them being rendered unless explicitly requested. Geometry set as
    // non-renderable will also be set as guides.
    bool isGuideObject
        = MaxUsd::IsBoneObject(context.node->EvalWorldState(context.timeConfig.GetStartTime()).obj)
        || context.node->Renderable() == 0;

    if (objectXformRequiredFromConfig || objectXFormRequiredFromInstancing || isGuideObject) {
        pxr::SdfPath xformPrimPath = currentPrimPath.AppendChild(pxr::TfToken(context.primName));

        if (!context.preview) {
            MaxUsd::FetchOrCreatePrim<pxr::UsdGeomXformable>(
                context.stage, xformPrimPath, pxr::MaxUsdPrimTypeTokens->Xform);
        }

        applyObjectPrimSuffix = true;
        currentPrimPath = xformPrimPath;
        exportedPrimPaths->push_back({ xformPrimPath, pxr::MaxUsdPrimTypeTokens->Xform });
    }

    // if it is an instanceable node, find or create the prim that will inherit from the class prim
    if (isInstanceableNode) {
        // Reuse the parent prim if no prim was created for object transform and we identify
        // that the prim should be reused for instancing.
        pxr::SdfPath instancePrimPath;

        if (exportedPrimPaths->empty() && ReuseParentPrimForInstancing(context)) {
            instancePrimPath = context.parentPrimPath;
        } else {
            instancePrimPath = currentPrimPath.AppendChild(pxr::TfToken(pxr::TfMakeValidIdentifier(
                applyObjectPrimSuffix ? context.primName + "_" + objectPrimSuffix
                                      : context.primName)));
        }

        exportedPrimPaths->push_back({ instancePrimPath, pxr::MaxUsdPrimTypeTokens->Xform });

        pxr::SdfPath prototypePrimPath = maxNodeToClassPrimMap[context.node];

        // If we created the class while exporting this instance, add it to the created prims
        // following the order of the hierarchy.
        if (createdClass) {
            exportedPrimPaths->push_back(classPrimSpec);
            exportedPrimPaths->push_back(instanceObjectPrimSpec);
        }

        if (context.preview) {
            return exportedPrimPaths;
        }

        pxr::UsdGeomXformable xformable = MaxUsd::FetchOrCreatePrim<pxr::UsdGeomXformable>(
            context.stage, instancePrimPath, pxr::MaxUsdPrimTypeTokens->Xform);

        // Postpone setting up the instanceable & inherit required on the prim for instancing to
        // avoid triggering notifications on each instance. We will do this all at once within an
        // SdfChangeBlock at the end of the export.
        auto xformPath = xformable.GetPrim().GetPath();
        instanceToPrototype.insert({ xformPath, prototypePrimPath });

        // The timeValue passed is used to check for a WSM, in which case we would not need to apply
        // an offset transform. Whether or not a WSM is applied is not animatable, so we can just
        // consider it at the startFrame.
        MaxUsd::ApplyObjectOffsetTransform(
            context.node, xformable, context.timeConfig.GetStartTime());

        // The root prim created for the node is at .front(). This prim will be where the node
        // object transform will be applied. This should be the instance prim unless we had unbaked
        // offset transform to manage. If this is the case, the front prim should be an xform prim
        // with as children the instance prim which contains the offset transform.
        return exportedPrimPaths;
    }

    TranslationContext objectContext {
        context.node,
        context.stage,
        currentPrimPath,
        pxr::TfMakeValidIdentifier(
            applyObjectPrimSuffix ? context.primName + "_" + objectPrimSuffix : context.primName),
        context.timeConfig,
        context.preview
    };

    MaxUsd::PrimDef createdPrimPath = createObjectPrim(objectContext, true);

    if (isGuideObject && !context.preview) {
        const auto imageable
            = pxr::UsdGeomImageable(context.stage->GetPrimAtPath(createdPrimPath.path));
        imageable.CreatePurposeAttr().Set(pxr::UsdGeomTokens->guide);
    }
    exportedPrimPaths->push_back(createdPrimPath);

    // The root prim created for the node is at .front(). This prim will be where the node object
    // transform will be applied. This should be the prim created by createObjectPrim unless we had
    // unbaked offset transform to manage. If this is the case, the front prim should be an xform
    // prim with as children the prim created by createObjectPrim.
    return exportedPrimPaths;
}

void USDSceneBuilder::PrepareExportPass()
{
    maxNodeToClassPrimMap.clear();
    classPrimBaseNameGenerator.Reset();
}

void USDSceneBuilder::ConfigureUsdAttributes(
    const TranslationContext& translationContext,
    pxr::UsdPrim&             translatedPrim)
{
    const auto    object = translationContext.node->GetObjectRef();
    bool          hiddenFound = false, kindFound = false, purposeFound = false;
    IParamBlock2* usdCustomAttributePb = nullptr;
    // Get non-animatable metadata, we first look at all the modifiers from top to bottom
    // until we find at least one of each of the attributes we're looking for.
    if (object != nullptr && object->SuperClassID() == GEN_DERIVOB_CLASS_ID) {
        const auto derivedObj = dynamic_cast<IDerivedObject*>(object);
        for (int i = 0; i < derivedObj->NumModifiers(); i++) {
            usdCustomAttributePb
                = MaxUsd::MetaData::FindUsdCustomAttributeParamBlock(derivedObj->GetModifier(i));
            if (usdCustomAttributePb != nullptr) {
                if (!hiddenFound) {
                    hiddenFound = MaxUsd::SetPrimHiddenFromCA(usdCustomAttributePb, translatedPrim);
                }
                if (!kindFound) {
                    kindFound = MaxUsd::SetPrimKindFromCA(usdCustomAttributePb, translatedPrim);
                }
                if (!purposeFound) {
                    purposeFound
                        = MaxUsd::SetPrimPurposeFromCA(usdCustomAttributePb, translatedPrim);
                }
            }
            // We found all of them, stop the loop.
            if (hiddenFound && kindFound && purposeFound) {
                return;
            }
        }
    }

    // We didn't find all of the attribute, let's look at the base object
    const auto baseObject = object->FindBaseObject();
    usdCustomAttributePb = MaxUsd::MetaData::FindUsdCustomAttributeParamBlock(baseObject);

    if (usdCustomAttributePb == nullptr) {
        // no paramblock with usd custom attributes found, skip.
        return;
    }

    if (!hiddenFound) {
        MaxUsd::SetPrimHiddenFromCA(usdCustomAttributePb, translatedPrim);
    }
    if (!kindFound) {
        MaxUsd::SetPrimKindFromCA(usdCustomAttributePb, translatedPrim);
    }
    if (!purposeFound) {
        MaxUsd::SetPrimPurposeFromCA(usdCustomAttributePb, translatedPrim);
    }
}

void USDSceneBuilder::ConfigureKind(INode* node, pxr::UsdPrim& translatedPrim)
{
    // Check if kind previously set from custom attributes
    pxr::TfToken kind;
    if (!pxr::UsdModelAPI(translatedPrim).GetKind(&kind) && kind.IsEmpty()) {
        // Set kind to group on export if max group
        if (node->IsGroupHead()) {
            pxr::UsdModelAPI(translatedPrim).SetKind(pxr::KindTokens->group);
        }
    }
}

} // namespace MAXUSD_NS_DEF
