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
#pragma once
#include <MaxUsdObjects/ComponentCreator/UtilComponentCreator.h>
#include <MaxUsdObjects/MaxUsdObjectsAPI.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>

#include <ufe/undoableCommand.h>

#include <AdskUsdComponentCreator/Infos/CreatorInfo.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <filesystem>
#include <memory>
#include <vector>

class INode;
class USDStageObject;

class CreateComponentFromNodesCmd : public Ufe::UndoableCommand
{
public:
    // When multiVariant is true, each selected top-level node becomes its own variant in the
    // component's first variant set (a node's descendants are folded into that node's variant,
    // not made separate variants). Otherwise all nodes go into a single default variant.
    CreateComponentFromNodesCmd(
        std::vector<INode*>              selectedNodes,
        AdskUsdComponentCreator::Options options,
        MaxUsd::USDSceneBuilderOptions   exportOptions = MaxUsd::USDSceneBuilderOptions(),
        bool                             multiVariant = false);

    // Create the component from existing USD prims (one or more source stages) instead
    // of Max nodes.
    CreateComponentFromNodesCmd(
        std::vector<MaxUsd::ComponentCreator::UsdPrimSource> primSources,
        AdskUsdComponentCreator::Options                     options,
        bool                                                 multiVariant = false);

    ~CreateComponentFromNodesCmd() override;

    void        execute() override;
    void        undo() override;
    void        redo() override;
    std::string commandString() const override;

private:
    // Component creation options (options) seeded with the output location derived from
    // tempFolder. Shared by every create path.
    AdskUsdComponentCreator::Options makeBaseComponentOptions() const;

    // Build the component on disk (in tempFolder) and return its CreatorInfo. execute()
    // then wraps it as a USD Stage Object. Returns null on failure.
    std::shared_ptr<AdskUsdComponentCreator::CreatorInfo> createSingleVariantComponent();
    std::shared_ptr<AdskUsdComponentCreator::CreatorInfo> createComponentFromPrims();

    // Multi-variant create: each top-level node/prim becomes a variant (descendants are folded
    // into their ancestor's variant). The component is registered after the first variant so the
    // manager tracks the rest; reverse-alpha order makes the alphabetically-first the default.
    void executeMultiVariantFromINodes();
    void executeMultiVariantFromPrims();

    // Create the USD Stage Object that wraps the component at the given root-layer path (which
    // registers the component with the manager via the OnNodeAdded callback). Sets
    // createdStageNode and componentRootLayerPath. Returns the stage object.
    USDStageObject* createStageObjectForComponent(const std::string& rootLayerRealPath);
    // Refresh the Explorer and open the Variant Editor. When setDefaultTarget is true, also target
    // the default variant of each set (only wanted on first create -- on redo the recorded history
    // restores the target/selection, so re-applying the default here double-selects it).
    void finalizeNewComponent(USDStageObject* stageObj, bool setDefaultTarget);

    std::vector<INode*>                                  nodes;
    std::vector<MaxUsd::ComponentCreator::UsdPrimSource> primSources;
    AdskUsdComponentCreator::Options                     options;
    MaxUsd::USDSceneBuilderOptions                       exportOptions;
    bool                                                 multiVariant = false;
    bool                                                 hasSkinnedContent = false;
    std::filesystem::path                                tempFolder;
    INode*                                               createdStageNode = nullptr;
    pxr::UsdStageRefPtr                                  componentStage;
    std::string                                          componentRootLayerPath;
};

namespace MaxUsd::ComponentCreator {

// Create a USD component from the given Max nodes: exports them and wraps the
// result as a structured component, adding a new USD Stage Object to the scene.
// No-op when nodes is empty. This is the single entry point behind the "Create
// USD Component" menu items; tests drive it directly.
MaxUSDObjectsAPI void CreateComponentFromNodes(
    const std::vector<INode*>&              nodes,
    const AdskUsdComponentCreator::Options& options,
    const MaxUsd::USDSceneBuilderOptions&   exportOptions = MaxUsd::USDSceneBuilderOptions());

// Like CreateComponentFromNodes, but creates one variant per selected top-level node (each
// node's subtree exported into its own variant; descendants of a selected node are not made
// separate variants). No-op when nodes is empty.
MaxUSDObjectsAPI void CreateMultiVariantsComponentFromNodes(
    const std::vector<INode*>&              nodes,
    const AdskUsdComponentCreator::Options& options,
    const MaxUsd::USDSceneBuilderOptions&   exportOptions = MaxUsd::USDSceneBuilderOptions());

// Create a USD component from existing USD prims (the included prims of one or more
// source stages) rather than Max nodes. No-op when sources is empty.
MaxUSDObjectsAPI void CreateComponentFromUsdPrims(
    const std::vector<UsdPrimSource>&       sources,
    const AdskUsdComponentCreator::Options& options);

// Like CreateComponentFromUsdPrims, but each selected top-level prim becomes its own variant in
// the component's first variant set (a prim's descendants are folded into that prim's variant).
// No-op when sources is empty.
MaxUSDObjectsAPI void CreateMultiVariantsComponentFromUsdPrims(
    const std::vector<UsdPrimSource>&       sources,
    const AdskUsdComponentCreator::Options& options);

} // namespace MaxUsd::ComponentCreator
