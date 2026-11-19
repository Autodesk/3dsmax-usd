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
#include "MaxHost.h"

#include "ComponentUfeCommand.h"
#include "MaxVariantEditor.h"
#include "UtilComponentCreator.h"

#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/USDIOController.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <usdUfe/undo/UsdUndoManager.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/usd/sdf/layer.h>

#include <ufe/undoableCommandMgr.h>

#include <AdskUsdComponentCreator/Commands/ComponentCommand.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <GetCOREInterface.h>
#include <max.h>

namespace {
std::shared_ptr<MaxHost> g_instance;

// The component description and the scene's stage object can hold different UsdStage instances
// over the same component file: the creator opens one, USDStageObject::SetRootLayer opens
// another, and only the latter is in the stage-keyed map. Layers are interned by identifier, so
// the root layer identifies the component across both.
USDStageObject* FindStageObjectForComponent(const pxr::UsdStageRefPtr& componentStage)
{
    auto* map = StageObjectMap::GetInstance();
    if (auto* stageObject = map->Get(componentStage)) {
        return stageObject;
    }
    const pxr::SdfLayerHandle rootLayer = componentStage->GetRootLayer();
    if (!rootLayer) {
        return nullptr;
    }
    for (auto* stageObject : map->GetAllStageObjects()) {
        const auto stage = stageObject->GetUSDStage();
        if (stage && stage->GetRootLayer() == rootLayer) {
            return stageObject;
        }
    }
    return nullptr;
}
} // namespace

std::shared_ptr<MaxHost> MaxHost::Instance()
{
    if (!g_instance) {
        g_instance = std::shared_ptr<MaxHost>(new MaxHost());
    }
    return g_instance;
}

void MaxHost::HandleRenamedStage(
    const std::string& oldRootLayerFilename,
    const std::string& newRootLayerFilename)
{
    MaxUsd::ComponentCreator::RepointStageRootLayer(oldRootLayerFilename, newRootLayerFilename);
}

std::vector<pxr::SdfLayerRefPtr> MaxHost::GetSessionLayersForFilename(const std::string& filename)
{
    // Find the USDStageObject whose root layer real path matches the given filename,
    // and return its session layer. This is used by SetVariantTemporarySelection to
    // write variant selections into the stage's session layer so the viewport updates.
    // USD returns forward-slash paths; make_preferred() normalises separators, and the
    // comparison is case-insensitive because Windows paths are.
    namespace fs = std::filesystem;
    const std::string normFilename = fs::path(filename).make_preferred().string();

    std::vector<pxr::SdfLayerRefPtr> result;
    for (auto* stageObj : StageObjectMap::GetInstance()->GetAllStageObjects()) {
        if (!stageObj) {
            continue;
        }
        const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
        if (!stage) {
            continue;
        }
        auto rootLayer = stage->GetRootLayer();
        if (!rootLayer) {
            continue;
        }
        const std::string realPath = fs::path(rootLayer->GetRealPath()).make_preferred().string();
        // TODO: a root-layer path does not uniquely identify a stage -- several stage
        // objects can share one root layer while keeping distinct session layers. This filename
        // lookup is therefore ambiguous, so a variant temporary selection can be written to the
        // wrong stage's session layer. Needs a stage-identity key instead of the path.
        if (_stricmp(realPath.c_str(), normFilename.c_str()) == 0) {
            auto sessionLayer = stage->GetSessionLayer();
            if (sessionLayer) {
                result.push_back(sessionLayer);
            }
        }
    }
    return result;
}

bool MaxHost::ExecuteWithUndo(
    const std::shared_ptr<AdskUsdComponentCreator::ComponentCommand>& command)
{
    if (!command) {
        return false;
    }
    auto cmd = std::make_shared<ComponentUfeCommand>(command);
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
    GetCOREInterface()->RedrawViews(GetCOREInterface()->GetTime());
    return command->IsSuccessful();
}

bool MaxHost::HasSceneSelection() const { return GetCOREInterface()->GetSelNodeCount() > 0; }

bool MaxHost::AddSelectionToComponent(
    const AdskUsdComponentCreator::ComponentDescription::Ptr& componentDesc,
    const AdskUsdComponentCreator::VariantSelections&         variantSelections,
    bool                                                      isReplacing,
    const std::string&                                        purpose)
{
    if (!componentDesc) {
        return false;
    }

    std::vector<INode*> nodes = MaxUsd::GetSelectedNodes();
    if (nodes.empty()) {
        TF_WARN("MaxHost::AddSelectionToComponent: no nodes selected");
        return false;
    }

    const MaxUsd::USDSceneBuilderOptions exportOptions
        = MaxUsd::GetUSDIOController()->GetExportUIOptions(
            MaxUsd::USDSceneBuilderOptions::Type::ToStage);

    // Honour "Inherit Stage Object Transform", so the added nodes keep their placement relative
    // to the component's stage object rather than landing in world space.
    Matrix3 rootTransform;
    bool    allowOverwrite = false;
    if (const auto componentStage
        = AdskUsdComponentCreator::ComponentDescription::GetStageFromComponentDescription(
            componentDesc)) {
        if (auto* stageObject = FindStageObjectForComponent(componentStage)) {
            const INodeTab stageNodes = MaxUsd::GetReferencingNodes(stageObject);
            if (stageNodes.Count() > 0) {
                MaxUsd::GetUSDIOController()->GetUIExportToStageExtraOptions(
                    stageNodes[0], allowOverwrite, rootTransform);
                if (allowOverwrite) {
                    TF_WARN("Prim overwrite is not supported when adding to a component.");
                }
            }
        }
    }

    const bool result = MaxUsd::ComponentCreator::AddINodesToComponent(
        componentDesc, nodes, variantSelections, isReplacing, exportOptions, purpose,
        rootTransform);

    if (result && MaxVariantEditor::IsCreated()) {
        MaxVariantEditor::Instance()->NotifyComponentChanged(componentDesc);
    }

    return result;
}

bool MaxHost::IsPinned() const
{
    return MaxVariantEditor::IsCreated() && MaxVariantEditor::Instance()->IsPinned();
}

bool MaxHost::SetPinned(bool isPinned)
{
    MaxVariantEditor::Instance()->SetPinned(isPinned);
    return true;
}

double MaxHost::GetUiScaleFactor() const { return static_cast<double>(MaxSDK::GetUIScaleFactor()); }

double MaxHost::ScaleForUi(double value) const { return value * GetUiScaleFactor(); }

void MaxHost::TrackLayerChanges(const pxr::SdfLayerRefPtr& layer)
{
    // Register the layer with the USD undo manager so edits made within a UsdUndoBlock (see
    // ComponentUfeCommand) are captured and can be undone/redone. Called by ComponentCommand::
    // TrackStageChanges() for every layer the command may touch.
    if (layer) {
        UsdUfe::UsdUndoManager::instance().trackLayerStates(layer);
    }
}
