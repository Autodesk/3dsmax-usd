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
}

USDLayerManager::~USDLayerManager()
{
    UnRegisterNotification(NotifyFileSave, this, NOTIFY_FILE_PRE_SAVE);
    UnRegisterNotification(NotifyFileSave, this, NOTIFY_FILE_POST_SAVE);
    UnRegisterNotification(NotifyFileSave, this, NOTIFY_FILE_CHECK_STATUS);
}

std::unordered_map<USDStageObject*, std::vector<pxr::SdfLayerHandle>>
USDLayerManager::GetDirtyLayersToSave()
{
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

        const auto allLayers = usdStage->GetUsedLayers(true);
        for (const auto& layer : allLayers) {
            // TODO LE-EXTRACT Save anonymous layers..
            if (!layer->IsAnonymous() && layer->IsDirty()) {
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
        if (dialog->GetSaveMode() == SaveUSDOptionsDialog::SaveMode::SaveAll) {

            std::vector<UsdLayerEditor::StageSavingInfo> stagesToSave;
            for (const auto& entry : stagesDirtyLayers) {

                const auto        stage = entry.first->GetUSDStage();
                const std::string stageName = MaxUsd::Ui::GetStageLabel(stage);

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
    // In quiet mode - warn the user. Scripters are expected to figure out how to save
    // the USD content themselves.
    else {
        MaxUsd::Listener::Write(L"Warning : Saving the 3dsMax scene in quiet mode will not "
                                L"save the following dirty USD layers :");

        for (const auto& entry : stagesDirtyLayers) {
            // At this point we know for sure at least one node is referencing this stage object.
            const auto node = GetReferencingNodes(entry.first)[0];
            auto       stageObjectMsg = node->GetName() + std::wstring(L":");
            MaxUsd::Listener::Write(stageObjectMsg.data());
            for (const auto layer : entry.second) {
                std::wstring layerMsg = std::wstring(L"  -")
                    + MaxUsd::UsdStringToMaxString(layer->GetDisplayName()).data();
                MaxUsd::Listener::Write(layerMsg.data());
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
    }
}