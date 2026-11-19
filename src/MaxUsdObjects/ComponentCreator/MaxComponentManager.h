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
#include <MaxUsdObjects/MaxUsdObjectsAPI.h>

#include <pxr/usd/usd/stage.h>

#include <AdskUsdComponentCreator/ComponentManager.h>
#include <memory>

struct NotifyInfo;

class MaxComponentManager : public AdskUsdComponentCreator::ComponentManager
{
public:
    MaxUSDObjectsAPI static MaxComponentManager* Instance();
    MaxUSDObjectsAPI static MaxComponentManager*
                                 CreateForTesting(); // no NOTIFY_SCENE_* registration
    MaxUSDObjectsAPI static void Shutdown();

    void AddComponent(const pxr::UsdStageRefPtr& stage) override;
    void RemoveComponent(const pxr::UsdStageRefPtr& stage) override;
    void SaveComponent(const pxr::UsdStageRefPtr& stage) override;
    void ReloadComponent(const pxr::UsdStageRefPtr& stage) override;

private:
    explicit MaxComponentManager(bool registerCallbacks);
    ~MaxComponentManager();

    static void OnNodeAdded(void* param, NotifyInfo* info);
    static void OnStageLoadStateChanged(void* param, NotifyInfo* info);
    void        NotifyVariantEditor(const pxr::UsdStageRefPtr& stage);

    bool callbacksRegistered = false;

    friend struct std::default_delete<MaxComponentManager>;
};
