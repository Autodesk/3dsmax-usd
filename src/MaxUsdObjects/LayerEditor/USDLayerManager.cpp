//
// Copyright 2024 Autodesk
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

#include "USDLayerManager.h"

#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>
#include <MaxUsdObjects/Views/SaveUSDOptionsDialog.h>

#include <MaxUsd/Utilities/ListenerUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/UiUtils.h>

#include <UsdLayerEditor/batchSaveLayersUIDelegate.h>

#include <Qt/QmaxMainWindow.h>

#include <max.h>
#include <notify.h>
#include <util/MainThreadTaskManager.h>

std::unique_ptr<USDLayerManager> USDLayerManager::instance;

USDLayerManager* USDLayerManager::Instance()
{
    if (!instance) {
        instance = std::unique_ptr<USDLayerManager>(new USDLayerManager);
    }
    return instance.get();
}

USDLayerManager::USDLayerManager()
{
    RegisterNotification(NotifyFileSave, this, NOTIFY_FILE_PRE_SAVE);
    RegisterNotification(NotifyFileSave, this, NOTIFY_FILE_POST_SAVE);
    RegisterNotification(NotifyFileSave, this, NOTIFY_FILE_CHECK_STATUS);
    RegisterNotification(NotifyFileSave, this, NOTIFY_FILE_POST_OPEN);
}

USDLayerManager::~USDLayerManager()
{
    UnRegisterNotification(NotifyFileSave, this, NOTIFY_FILE_PRE_SAVE);
    UnRegisterNotification(NotifyFileSave, this, NOTIFY_FILE_POST_SAVE);
    UnRegisterNotification(NotifyFileSave, this, NOTIFY_FILE_CHECK_STATUS);
    UnRegisterNotification(NotifyFileSave, this, NOTIFY_FILE_POST_OPEN);
}

std::unordered_map<USDStageObject*, std::vector<pxr::SdfLayerHandle>>
USDLayerManager::GetDirtyLayersToSave()
{

    auto getAllSublayersFromLayer
        = [](const SdfLayerRefPtr& layer, std::set<SdfLayerRefPtr>& layerRefs) {
              // Add the layer itself
              layerRefs.insert(layer);

              // Queue to process by depth
              std::deque<SdfLayerRefPtr> processing;
              processing.push_back(layer);

              while (!processing.empty()) {
                  auto layerToProcess = processing.front();
                  processing.pop_front();

                  SdfSubLayerProxy sublayerPaths = layerToProcess->GetSubLayerPaths();
                  for (auto path : sublayerPaths) {
                      SdfLayerRefPtr sublayer = SdfLayer::FindRelativeToLayer(layer, path);
                      if (sublayer) {
                          layerRefs.insert(sublayer);
                          processing.push_back(sublayer);
                      }
                  }
              }
          };

    std::unordered_map<USDStageObject*, std::vector<pxr::SdfLayerHandle>> dirtyLayers;

    for (const auto& stageObject : StageObjectMap::GetInstance()->GetAllStageObjects()) {

        // Stage object not referenced in the scene -> nothing to save...
        const auto nodes = MaxUsd::GetReferencingNodes(stageObject);
        if (nodes.Count() == 0) {
            continue;
        }

        auto usdStage = stageObject->GetUSDStage();
        if (!usdStage) {
            continue;
        }

        std::set<SdfLayerRefPtr> foundLayers;
        // Start from the root and the session layer, find all the layers
        // in their local layer stacks
        // Note: we do this because the regular USD APIs for getting
        // layers, such as "GetLayerStack()" do not return muted layers
        getAllSublayersFromLayer(usdStage->GetRootLayer(), foundLayers);
        getAllSublayersFromLayer(usdStage->GetSessionLayer(), foundLayers);

        // Find the used layers of the stage -- this will have overlap with the above
        // but also include non-local layers.
        // Note: muted layers will not be returned by this function
        const auto& usedLayers = usdStage->GetUsedLayers(true);

        foundLayers.insert(usedLayers.begin(), usedLayers.end());

        // Now check if the layers are either dirty OR anonymous
        // (don't include session layer unless it's dirty).
        for (const auto& layer : foundLayers) {
            auto isSessionLayer = usdStage->GetSessionLayer() == layer;
            if (layer->IsDirty() || (layer->IsAnonymous() && !isSessionLayer)) {
                dirtyLayers[stageObject].push_back(layer);
            }
        }
    }

    return dirtyLayers;
}

bool USDLayerManager::HandleMaxSceneSave()
{
    // Only prompt & save layers on the first call withing a scene save operation.
    if (!mustHandleSave) {
        return true;
    }
    mustHandleSave = false;

    auto stagesDirtyLayers = GetDirtyLayersToSave();
    if (stagesDirtyLayers.empty()) {
        return true;
    }

    if (!GetCOREInterface()->GetQuietMode()) {
        auto dialog = new SaveUSDOptionsDialog { GetCOREInterface()->GetQmaxMainWindow() };
        if (dialog->exec() != QDialog::Accepted) {
            // User cancelled the save.
            return false;
        }

        if (saveMode == SaveMode::SaveAll) {

            std::vector<UsdLayerEditor::StageSavingInfo> stagesToSave;
            for (const auto& entry : stagesDirtyLayers) {
                const auto stage = entry.first->GetUSDStage();

                // Note: we are looping against "stagesDirtyLayers = GetDirtyLayersToSave();"
                // which is based on StageObjects, which are nodes in the scene, so there should
                // always be a referencing node in the scene to get the node name from.
                auto              referencingNodes = MaxUsd::GetReferencingNodes(entry.first);
                const std::string stageName
                    = MaxUsd::MaxStringToUsdString(referencingNodes[0]->NodeName().data());

                UsdLayerEditor::StageSavingInfo si { stage, stageName, true, false };
                stagesToSave.push_back(si);
            }

            const auto result = batchSaveLayersUIDelegate(stagesToSave, false);

            // The batch save dialog returns partially completed - meaning some layers might already
            // have been saved - but the caller is still responsible to save file-backed layers.
            if (result == UsdLayerEditor::kPartiallyCompleted) {
                for (const auto& entry : stagesDirtyLayers) {
                    for (const auto& layer : entry.second) {
                        layer->Save();
                    }
                }
            } else {
                return false;
            }
        }
    }
    // In quiet mode
    else {
        // if SaveAllEditsMax is not set - warn the user. Scripters are expected
        // to figure out how to save the USD content themselves.
        // (USDStageObjectclassDesc::Save function handles saving when SaveAllEditsMax is set)
        if (saveMode != SaveMode::SaveAllEditsMax) {
            MaxUsd::Listener::Write(L"Warning : Saving the 3dsMax scene in quiet mode will not "
                                    L"save the following dirty USD layers:");

            for (const auto& entry : stagesDirtyLayers) {
                // At this point we know for sure at least one node is referencing this stage
                // object.
                const auto node = GetReferencingNodes(entry.first)[0];
                auto       stageObjectMsg = node->GetName() + std::wstring(L":");
                MaxUsd::Listener::Write(stageObjectMsg.data());
                for (const auto layer : entry.second) {
                    auto         layerDisplayName = layer->GetDisplayName();
                    std::wstring layerMsg = std::wstring(L"  -")
                        + MaxUsd::UsdStringToMaxString(
                              !layerDisplayName.empty() ? layerDisplayName : layer->GetIdentifier())
                              .data();
                    MaxUsd::Listener::Write(layerMsg.data());
                }
            }
        }
    }
    return true;
}

void USDLayerManager::NotifyFileSave(void* param, NotifyInfo* info)
{
    auto layerManager = static_cast<USDLayerManager*>(param);

    switch (info->intcode) {
    case NOTIFY_FILE_PRE_SAVE: layerManager->mustHandleSave = !layerManager->isAutoSave; break;
    case NOTIFY_FILE_POST_SAVE: {
        layerManager->mustHandleSave = false;
        // Assume auto-save except when during saves where we received a NOTIFY_FILE_CHECK_STATUS
        layerManager->isAutoSave = true;

        // If there were dirty layers and the user decided against saving them, we need
        // to flag the UsdStageObject's as still "dirty" so that the users can be prompted,
        // for example when closing 3dsMax.
        auto dirty = layerManager->GetDirtyLayersToSave();
        if (!dirty.empty()) {
            // Annoyingly, NOTIFY_FILE_POST_SAVE is sent out before the "SaveRequiredFlag"
            // gets set to false by 3dsMax, so we cannot set it here directly.
            // To work around this - queue a main thread task. Which will executed
            // from the main 3dsmax loop - after the save operation fully completes.
            IMainThreadTaskManager* taskMgr = IMainThreadTaskManager::GetInstance();
            if (taskMgr) {
                class NeedSaveRequest : public MainThreadTask
                {
                public:
                    NeedSaveRequest() = default;

                private:
                    void Execute() override { SetSaveRequiredFlag(TRUE); }
                };
                taskMgr->PostTask(new NeedSaveRequest());
            }
        }
        break;
    }
    case NOTIFY_FILE_CHECK_STATUS: layerManager->isAutoSave = false; break;
    case NOTIFY_FILE_POST_OPEN: layerManager->ClearLoadedLayersMap(); break;
    }
}

SaveMode USDLayerManager::GetSaveMode() { return saveMode; }

void USDLayerManager::SetSaveMode(SaveMode saveMode) { this->saveMode = saveMode; }

const std::unordered_map<std::string, pxr::SdfLayerRefPtr>& USDLayerManager::GetLoadedLayerMap()
{
    return loadedLayerMap;
}

void USDLayerManager::AddLoadedLayerMapping(std::string& oldId, pxr::SdfLayerRefPtr& newLayer)
{
    loadedLayerMap[oldId] = newLayer;
}

void USDLayerManager::ClearLoadedLayersMap() { loadedLayerMap.clear(); }