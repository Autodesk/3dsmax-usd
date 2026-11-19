//
// Copyright 2026 Autodesk
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
#include "CreateComponentFromNodesCmd.h"

#include "../MaxComponentManager.h"
#include "../MaxVariantEditor.h"
#include "../UtilComponentCreator.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/ExportToStageCommand.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include <ufe/undoableCommandMgr.h>

#include <AdskUsdComponentCreator/Commands/CreateFromStageCommand.h>
#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/ComponentFileNames.h>
#include <AdskUsdComponentCreator/Containers.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <GetCOREInterface.h>
#include <Tab.h>
#include <algorithm>
#include <hold.h>
#include <inode.h>
#include <max.h>

CreateComponentFromNodesCmd::CreateComponentFromNodesCmd(
    std::vector<INode*>              selectedNodes,
    AdskUsdComponentCreator::Options options,
    MaxUsd::USDSceneBuilderOptions   exportOptions,
    bool                             multiVariant)
    : nodes(std::move(selectedNodes))
    , options(std::move(options))
    , exportOptions(std::move(exportOptions))
    , multiVariant(multiVariant)
{
    // Keep only top-level nodes so a selected node and its descendant don't become separate
    // variants / double-included (mirrors the prim-source filter below).
    nodes = MaxUsd::RemoveDescendantINodes(nodes);
}

CreateComponentFromNodesCmd::CreateComponentFromNodesCmd(
    std::vector<MaxUsd::ComponentCreator::UsdPrimSource> sources,
    AdskUsdComponentCreator::Options                     options,
    bool                                                 multiVariant)
    : primSources(std::move(sources))
    , options(std::move(options))
    , multiVariant(multiVariant)
{
    // Keep only top-level prims per source so a selected prim and its descendant don't
    // contribute the same geometry twice (mirrors RemoveDescendantINodes for the node case).
    for (auto& src : primSources) {
        src.primPaths = MaxUsd::RemoveDescendantPaths(src.primPaths);
    }
}

namespace {
// The stage object a node holds, or null if the node is gone or holds something else.
USDStageObject* StageObjectFromNode(INode* node)
{
    if (!node || !node->GetObjectRef()) {
        return nullptr;
    }
    Object* base = node->GetObjectRef()->FindBaseObject();
    if (!base || base->ClassID() != USDSTAGEOBJECT_CLASS_ID) {
        return nullptr;
    }
    return static_cast<USDStageObject*>(base);
}
} // namespace

CreateComponentFromNodesCmd::~CreateComponentFromNodesCmd()
{
    if (!createdStageNode && !tempFolder.empty()) {
        MaxUsd::ComponentCreator::RemoveComponentTempFolder(tempFolder);
    }
}

void CreateComponentFromNodesCmd::redo()
{
    if (componentRootLayerPath.empty()) {
        execute();
        return;
    }
    // 3ds Max restores the node it recorded on create, so the same node and stage object come
    // back; only the component-side state has to be re-applied.
    USDStageObject* stageObj = StageObjectFromNode(createdStageNode);
    if (!stageObj) {
        return;
    }
    componentStage = stageObj->GetUSDStage();
    finalizeNewComponent(stageObj, /*setDefaultTarget=*/true);
}

std::string CreateComponentFromNodesCmd::commandString() const
{
    return "Create Component from Selection";
}

void CreateComponentFromNodesCmd::execute()
{
    tempFolder = MaxUsd::ComponentCreator::AllocateComponentTempFolder();

    // Multi-variant registers the component mid-creation, so it runs its own flow.
    if (multiVariant) {
        if (primSources.empty()) {
            executeMultiVariantFromINodes();
        } else {
            executeMultiVariantFromPrims();
        }
        return;
    }

    std::shared_ptr<AdskUsdComponentCreator::CreatorInfo> info
        = primSources.empty() ? createSingleVariantComponent() : createComponentFromPrims();
    if (!info) {
        MaxUsd::ComponentCreator::RemoveComponentTempFolder(tempFolder);
        throw std::runtime_error("CreateComponentFromNodesCmd: failed to create component");
    }
    const auto infoStage = info->GetStage();
    if (!infoStage) {
        MaxUsd::ComponentCreator::RemoveComponentTempFolder(tempFolder);
        throw std::runtime_error("CreateComponentFromNodesCmd: component stage is null");
    }

    USDStageObject* stageObj
        = createStageObjectForComponent(infoStage->GetRootLayer()->GetRealPath());
    componentStage = stageObj->GetUSDStage();
    finalizeNewComponent(stageObj, /*setDefaultTarget=*/true);
}

void CreateComponentFromNodesCmd::undo()
{
    // The node is not touched here: 3ds Max recorded its creation, so its own restore object
    // deletes it and keeps it alive for the redo.

    // The Variant Manager is cleared here rather than left to the node-deleted notification it
    // normally reacts to: the root-layer restore in this same entry puts the stage object back on
    // its pre-SetRootLayer stage before the node goes away, so by then the notification carries a
    // stage that no longer matches the component being displayed.
    if (MaxVariantEditor::IsCreated() && componentStage
        && MaxVariantEditor::Instance()->GetCurrentStage() == componentStage) {
        MaxVariantEditor::Instance()->Clear();
    }
    componentStage = pxr::UsdStageRefPtr();
}

USDStageObject*
CreateComponentFromNodesCmd::createStageObjectForComponent(const std::string& rootLayerRealPath)
{
    componentRootLayerPath = rootLayerRealPath;
    // Recorded by 3ds Max (no hold suspension): the node create and SetRootLayer become part of
    // this command's undo entry, so Max owns the node's lifetime and restores the same node on
    // redo. redo() therefore does not create one.
    auto* stageObj = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, USDSTAGEOBJECT_CLASS_ID));
    createdStageNode = GetCOREInterface()->CreateObjectNode(stageObj);
    const auto rootLayerPath = MaxUsd::UsdStringToMaxString(rootLayerRealPath);
    // SetRootLayer (not SetValue) loads the stage and notifies the viewport/Layer Editor/Explorer.
    stageObj->SetRootLayer(rootLayerPath.data(), L"/", /*payloadsLoaded=*/true);
    return stageObj;
}

void CreateComponentFromNodesCmd::finalizeNewComponent(
    USDStageObject* stageObj,
    bool            setDefaultTarget)
{
    // Target the default variant of each variant set so subsequent edits land in it.
    if (setDefaultTarget && componentStage) {
        if (auto desc = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(
                componentStage)) {
            for (auto& vs : desc->GetVariantSets()) {
                const auto&        vsDesc = vs.second;
                const std::string& defaultName = vsDesc->GetDefaultVariant();
                if (defaultName.empty()) {
                    continue;
                }
                if (auto variantDesc = vsDesc->GetVariantDescription(defaultName)) {
                    AdskUsdComponentCreator::ComponentAPI::SetVariantTarget(
                        desc, { vsDesc, variantDesc });
                }
            }
        }
    }

    // The component is authored into its own layers, so the exported scene's units and up axis do
    // not come across with the prims and have to be restated here.
    MaxUsd::ComponentCreator::ApplySceneMetadataToComponent(componentStage, exportOptions);

    // Skinned meshes bind through their nearest enclosing SkelRoot; the exported nodes are flat, so
    // the component prim is what covers both the meshes and their skeleton.
    if (hasSkinnedContent && componentStage) {
        MaxUsd::ComponentCreator::MakeComponentPrimSkelRoot(componentStage);
    }

    // Via the stage object, so the IsOpenInExplorer flag is set too : reloading the stage (as the
    // repoint after a save does) only re-opens the Explorer when that flag is set.
    stageObj->OpenInUsdExplorer();

    // Open the variant editor on the new component.
    if (componentStage) {
        MaxVariantEditor::Instance()->OpenFor(componentStage);
    }
}

AdskUsdComponentCreator::Options CreateComponentFromNodesCmd::makeBaseComponentOptions() const
{
    // The library writes <folder>/<name>/<name>; the temp folder's parent + its own name keep
    // the component in tempFolder with no extra subfolder.
    AdskUsdComponentCreator::Options opts = options;
    const std::string                name = tempFolder.filename().string();
    opts.SetComponentFolder(tempFolder.parent_path().string());
    opts.SetComponentName(name);
    opts.SetComponentFilename(name);
    return opts;
}

std::shared_ptr<AdskUsdComponentCreator::CreatorInfo>
CreateComponentFromNodesCmd::createSingleVariantComponent()
{
    namespace cc = AdskUsdComponentCreator;
    auto srcStage = MaxUsd::ComponentCreator::ExportINodesToSourceStage(nodes, exportOptions);
    if (!srcStage) {
        return nullptr;
    }
    hasSkinnedContent |= MaxUsd::ComponentCreator::SourceStageHasSkinnedContent(srcStage);

    cc::Options compOpts = makeBaseComponentOptions();

    std::vector<std::string> nodeNames;
    for (INode* n : nodes) {
        if (n) {
            nodeNames.push_back(MaxUsd::MaxStringToUsdString(n->GetName()));
        }
    }
    const std::string vsName = cc::GenerateVariantSetNameFromObjectName(compOpts, "", {});
    const std::string vName = cc::GenerateVariantNameFromObjectName(
        compOpts, nodeNames.empty() ? std::string() : nodeNames.front(), nodeNames, {});
    compOpts.SetComponentVariants({ { vsName, vName } });
    compOpts.SetIsDefaultVariant(true);
    compOpts.SetIncludedPaths(
        MaxUsd::ComponentCreator::IncludedPathsForFlatExport(srcStage, compOpts, exportOptions));

    // CreateFromStage reuses the open export stage; CreateFromFile would re-open the layer and
    // clash with the one already in the registry.
    return cc::ComponentAPI::CreateFromStage(srcStage, compOpts);
}

void CreateComponentFromNodesCmd::executeMultiVariantFromINodes()
{
    namespace cc = AdskUsdComponentCreator;

    std::vector<INode*> validNodes;
    for (INode* n : nodes) {
        if (n) {
            validNodes.push_back(n);
        }
    }
    if (validNodes.empty()) {
        MaxUsd::ComponentCreator::RemoveComponentTempFolder(tempFolder);
        throw std::runtime_error("CreateComponentFromNodesCmd: no nodes to create component from");
    }
    // Reverse-alphabetical: USD orders variants alphabetically, so creating the alpha-first node
    // last makes it the default (targeted in finalizeNewComponent).
    std::sort(validNodes.begin(), validNodes.end(), [](INode* a, INode* b) {
        return MaxUsd::MaxStringToUsdString(a->GetName())
            > MaxUsd::MaxStringToUsdString(b->GetName());
    });

    cc::Options baseOpts = makeBaseComponentOptions();

    // The first node (null description) creates the component; the rest add to it. The component
    // is registered (stage object created) after the first variant so the manager tracks the rest.
    cc::ComponentDescription::Ptr componentDesc;
    USDStageObject*               stageObj = nullptr;
    for (INode* node : validNodes) {
        const std::string nodeName = MaxUsd::MaxStringToUsdString(node->GetName());
        const std::string vsName = cc::GenerateVariantSetNameFromObjectName(baseOpts, "", {});
        const std::string vName = cc::GenerateVariantNameFromObjectName(baseOpts, nodeName, {}, {});
        baseOpts.SetComponentVariants({ { vsName, vName } });
        baseOpts.SetReplaceVariantContent(true);

        auto srcStage
            = MaxUsd::ComponentCreator::ExportINodesToSourceStage({ node }, exportOptions);
        if (srcStage) {
            hasSkinnedContent |= MaxUsd::ComponentCreator::SourceStageHasSkinnedContent(srcStage);
            baseOpts.SetIncludedPaths(
                MaxUsd::ComponentCreator::IncludedPathsForFlatExport(
                    srcStage, baseOpts, exportOptions));
        }

        cc::UnlockComponentContext unlock(componentDesc);
        auto cmd = srcStage ? cc::CreateFromStageCommand::Create(componentDesc, srcStage, baseOpts)
                            : nullptr;
        const auto info = (cmd && cmd->Execute()) ? cmd->GetOutputs().creatorInfo : nullptr;
        if (!info || !info->GetStage()) {
            if (!stageObj) {
                MaxUsd::ComponentCreator::RemoveComponentTempFolder(tempFolder);
            }
            throw std::runtime_error("CreateComponentFromNodesCmd: failed to create component");
        }
        componentDesc = cmd->GetComponentDescription();
        if (!stageObj) {
            stageObj
                = createStageObjectForComponent(info->GetStage()->GetRootLayer()->GetRealPath());
            componentStage = stageObj->GetUSDStage();
        }
    }

    finalizeNewComponent(stageObj, /*setDefaultTarget=*/true);
}

std::shared_ptr<AdskUsdComponentCreator::CreatorInfo>
CreateComponentFromNodesCmd::createComponentFromPrims()
{
    namespace cc = AdskUsdComponentCreator;

    cc::Options compOpts = makeBaseComponentOptions();

    std::vector<std::string> primNames;
    for (const auto& src : primSources) {
        for (const auto& primPath : src.primPaths) {
            primNames.push_back(primPath.GetName());
        }
    }
    const std::string vsName = cc::GenerateVariantSetNameFromObjectName(compOpts, "", {});
    const std::string vName = cc::GenerateVariantNameFromObjectName(
        compOpts, primNames.empty() ? std::string() : primNames.front(), primNames, {});
    compOpts.SetComponentVariants({ { vsName, vName } });
    compOpts.SetIsDefaultVariant(true);

    // All source prims go into the single default variant: the first stage replaces its content,
    // the rest add to it.
    std::shared_ptr<cc::CreatorInfo> info;
    bool                             first = true;
    for (const auto& src : primSources) {
        if (!src.stage || src.primPaths.empty()) {
            continue;
        }
        compOpts.SetIncludedPaths(cc::PathsSet(src.primPaths.begin(), src.primPaths.end()));
        compOpts.SetReplaceVariantContent(first);
        info = cc::ComponentAPI::CreateFromStage(src.stage, compOpts);
        if (!info) {
            return nullptr;
        }
        first = false;
    }
    return info;
}

void CreateComponentFromNodesCmd::executeMultiVariantFromPrims()
{
    namespace cc = AdskUsdComponentCreator;

    cc::Options baseOpts = makeBaseComponentOptions();

    // The first prim (null description) creates the component; the rest add to it. The component
    // is registered (stage object created) after the first variant so the manager tracks the rest.
    cc::ComponentDescription::Ptr componentDesc;
    USDStageObject*               stageObj = nullptr;
    std::vector<std::string>      existingVariants;
    for (const auto& src : primSources) {
        if (!src.stage) {
            continue;
        }
        // Reverse-alphabetical (see executeMultiVariantFromINodes).
        std::vector<pxr::SdfPath> primPaths(src.primPaths.begin(), src.primPaths.end());
        std::sort(
            primPaths.begin(), primPaths.end(), [](const pxr::SdfPath& a, const pxr::SdfPath& b) {
                return a.GetName() > b.GetName();
            });
        for (const auto& primPath : primPaths) {
            const std::string vsName = cc::GenerateVariantSetNameFromObjectName(baseOpts, "", {});
            // existingVariants disambiguates same-named prims from different source stages.
            const std::string vName = cc::GenerateVariantNameFromObjectName(
                baseOpts, primPath.GetName(), {}, existingVariants);
            existingVariants.push_back(vName);
            baseOpts.SetComponentVariants({ { vsName, vName } });
            baseOpts.SetReplaceVariantContent(true);
            baseOpts.SetIncludedPaths(cc::PathsSet { primPath });

            cc::UnlockComponentContext unlock(componentDesc);
            auto       cmd = cc::CreateFromStageCommand::Create(componentDesc, src.stage, baseOpts);
            const auto info = (cmd && cmd->Execute()) ? cmd->GetOutputs().creatorInfo : nullptr;
            if (!info || !info->GetStage()) {
                if (!stageObj) {
                    MaxUsd::ComponentCreator::RemoveComponentTempFolder(tempFolder);
                }
                throw std::runtime_error("CreateComponentFromNodesCmd: failed to create component");
            }
            componentDesc = cmd->GetComponentDescription();
            if (!stageObj) {
                stageObj = createStageObjectForComponent(
                    info->GetStage()->GetRootLayer()->GetRealPath());
                componentStage = stageObj->GetUSDStage();
            }
        }
    }
    if (!stageObj) {
        MaxUsd::ComponentCreator::RemoveComponentTempFolder(tempFolder);
        throw std::runtime_error("CreateComponentFromNodesCmd: no prims to create component from");
    }

    finalizeNewComponent(stageObj, /*setDefaultTarget=*/true);
}

namespace MaxUsd::ComponentCreator {

void CreateComponentFromNodes(
    const std::vector<INode*>&              nodes,
    const AdskUsdComponentCreator::Options& options,
    const MaxUsd::USDSceneBuilderOptions&   exportOptions)
{
    if (nodes.empty()) {
        return;
    }
    auto cmd = std::make_shared<CreateComponentFromNodesCmd>(nodes, options, exportOptions);
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
}

void CreateMultiVariantsComponentFromNodes(
    const std::vector<INode*>&              nodes,
    const AdskUsdComponentCreator::Options& options,
    const MaxUsd::USDSceneBuilderOptions&   exportOptions)
{
    if (nodes.empty()) {
        return;
    }
    auto cmd = std::make_shared<CreateComponentFromNodesCmd>(
        nodes, options, exportOptions, /*multiVariant=*/true);
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
}

void CreateComponentFromUsdPrims(
    const std::vector<UsdPrimSource>&       sources,
    const AdskUsdComponentCreator::Options& options)
{
    if (sources.empty()) {
        return;
    }
    auto cmd = std::make_shared<CreateComponentFromNodesCmd>(sources, options);
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
}

void CreateMultiVariantsComponentFromUsdPrims(
    const std::vector<UsdPrimSource>&       sources,
    const AdskUsdComponentCreator::Options& options)
{
    if (sources.empty()) {
        return;
    }
    auto cmd
        = std::make_shared<CreateComponentFromNodesCmd>(sources, options, /*multiVariant=*/true);
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
}

} // namespace MaxUsd::ComponentCreator