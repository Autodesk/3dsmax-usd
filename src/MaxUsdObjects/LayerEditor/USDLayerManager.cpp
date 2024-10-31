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

#include <Qt/QmaxMainWindow.h>

#include <max.h>
#include <notify.h>

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
}

USDLayerManager::~USDLayerManager()
{
    UnRegisterNotification(NotifyFileSave, this, NOTIFY_FILE_PRE_SAVE);
    UnRegisterNotification(NotifyFileSave, this, NOTIFY_FILE_POST_SAVE);
}

bool USDLayerManager::HandleMaxSceneSave()
{
    // Only prompt & save layers on the first call withing a scene save operation.
    if (!mustHandleSave) {
        return true;
    }
    mustHandleSave = false;

    // Get all dirty layers currently in use in USDStageObjects.
    std::unordered_map<std::string, pxr::SdfLayerHandle> dirtyLayers;

    const bool quietMode = GetCOREInterface()->GetQuietMode();
    // For listener display in quiet mode.
    std::unordered_map<INode*, std::vector<std::string>> objectDirtyLayers;

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
        for (const auto layer : allLayers) {
            // TODO LE-EXTRACT Save anonymous layers..
            if (!layer->IsAnonymous() && layer->IsDirty()) {
                dirtyLayers.insert({ layer->GetIdentifier(), layer });
                // Collect information for the listener..
                if (quietMode) {
                    objectDirtyLayers[nodes[0]].push_back(layer->GetDisplayName());
                }
            }
        }
    }

    if (!dirtyLayers.empty()) {

        if (!quietMode) {
            auto dialog = new SaveUSDOptionsDialog { GetCOREInterface()->GetQmaxMainWindow() };
            if (dialog->exec() != QDialog::Accepted) {
                // User cancelled the save.
                return false;
            }
            if (dialog->GetSaveMode() == SaveUSDOptionsDialog::SaveMode::SaveAll) {
                for (const auto& layer : dirtyLayers) {
                    layer.second->Save();
                }
            }
        }
        // In quiet mode - warn the user. Scripters are expected to figure out how to save
        // the USD content themselves.
        else {
            MaxUsd::Listener::Write(L"Warning : Saving the 3dsMax scene in quiet mode will not "
                                    L"save the following dirty USD layers :");
            for (const auto& entry : objectDirtyLayers) {
                auto stageObjectMsg = entry.first->GetName() + std::wstring(L":");
                MaxUsd::Listener::Write(stageObjectMsg.data());
                for (const auto layerName : entry.second) {
                    std::wstring layerMsg
                        = std::wstring(L"  -") + MaxUsd::UsdStringToMaxString(layerName).data();
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
    case NOTIFY_FILE_PRE_SAVE: layerManager->mustHandleSave = true; break;
    case NOTIFY_FILE_POST_SAVE: layerManager->mustHandleSave = false; break;
    }
}