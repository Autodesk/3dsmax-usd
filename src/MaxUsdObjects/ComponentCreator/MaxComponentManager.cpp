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
#include "MaxComponentManager.h"

#include "MaxVariantEditor.h"

#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>
#include <MaxUsdObjects/Objects/MaxUsdNotify.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Interfaces/IUSDStageProvider.h>

#include <UsdLayerEditor/layerLocking.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/usd/sdf/layer.h>

#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <inode.h>
#include <notify.h>

namespace {
std::unique_ptr<MaxComponentManager> g_instance;
} // namespace

MaxComponentManager* MaxComponentManager::Instance()
{
    if (!g_instance) {
        g_instance = std::unique_ptr<MaxComponentManager>(new MaxComponentManager(true));
    }
    return g_instance.get();
}

MaxComponentManager* MaxComponentManager::CreateForTesting()
{
    g_instance = std::unique_ptr<MaxComponentManager>(new MaxComponentManager(false));
    return g_instance.get();
}

void MaxComponentManager::Shutdown() { g_instance.reset(); }

MaxComponentManager::MaxComponentManager(bool registerCallbacks)
    : callbacksRegistered(registerCallbacks)
{
    if (registerCallbacks) {
        RegisterNotification(OnNodeAdded, this, NOTIFY_SCENE_ADDED_NODE);
        // Deliberately no NOTIFY_SCENE_PRE_DELETED_NODE hook: untracking on delete would drop the
        // manager's strong refs to the component's unsaved (dirty) layers, so undoing the deletion
        // would lose those edits. A component falls away when its root layer becomes unreferenced.
        RegisterNotification(OnStageLoadStateChanged, this, NOTIFY_STAGE_LOAD_STATE_CHANGED);
    }
}

MaxComponentManager::~MaxComponentManager()
{
    if (callbacksRegistered) {
        UnRegisterNotification(OnNodeAdded, this, NOTIFY_SCENE_ADDED_NODE);
        UnRegisterNotification(OnStageLoadStateChanged, this, NOTIFY_STAGE_LOAD_STATE_CHANGED);
    }
}

void MaxComponentManager::AddComponent(const pxr::UsdStageRefPtr& stage)
{
    ComponentManager::AddComponent(stage);
    if (auto root = stage->GetRootLayer()) {
        UsdLayerEditor::lockLayer(std::string {}, root, UsdLayerEditor::LayerLock_Locked, false);
    }
    // Default the edit target to the session layer: component edits are authored
    // into the session and the edit-forwarding system routes them to the targeted
    // variant.
    if (auto session = stage->GetSessionLayer()) {
        stage->SetEditTarget(session);
    }
}

void MaxComponentManager::RemoveComponent(const pxr::UsdStageRefPtr& stage)
{
    if (auto root = stage->GetRootLayer()) {
        UsdLayerEditor::lockLayer(std::string {}, root, UsdLayerEditor::LayerLock_Unlocked, false);
    }
    ComponentManager::RemoveComponent(stage);
}

void MaxComponentManager::SaveComponent(const pxr::UsdStageRefPtr& stage)
{
    ComponentManager::SaveComponent(stage);
    NotifyVariantEditor(stage);
}

void MaxComponentManager::ReloadComponent(const pxr::UsdStageRefPtr& stage)
{
    ComponentManager::ReloadComponent(stage);
    NotifyVariantEditor(stage);
}

void MaxComponentManager::NotifyVariantEditor(const pxr::UsdStageRefPtr& stage)
{
    if (MaxVariantEditor::IsCreated()) {
        MaxVariantEditor::Instance()->NotifyComponentChanged(stage);
    }
}

void MaxComponentManager::OnNodeAdded(void* param, NotifyInfo* info)
{
    auto*  self = static_cast<MaxComponentManager*>(param);
    INode* node = GetNotifyParam<NOTIFY_SCENE_ADDED_NODE>(info);

    if (!node) {
        return;
    }
    auto* provider = dynamic_cast<MaxUsd::IUSDStageProvider*>(
        node->GetObjectRef()->GetInterface(IUSDStageProvider_ID));
    if (!provider) {
        return;
    }
    const pxr::UsdStageRefPtr stage = provider->GetUSDStage();
    if (!stage) {
        return;
    }
    try {
        auto desc = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(stage);
        if (desc) {
            self->AddComponent(stage);
        }
    } catch (...) {
    }
}

void MaxComponentManager::OnStageLoadStateChanged(void* param, NotifyInfo* info)
{
    // Fires when a USDStageObject loads or reloads its stage (e.g. after SetRootLayer).
    // This covers nodes that were added to the scene before their root layer was set.
    auto* self = static_cast<MaxComponentManager*>(param);
    auto* stageObj = GetNotifyParam<NOTIFY_STAGE_LOAD_STATE_CHANGED>(info);

    if (!stageObj) {
        return;
    }
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
    if (!stage) {
        return;
    }
    auto rootLayer = stage->GetRootLayer();
    if (!rootLayer || !rootLayer->PermissionToEdit()) {
        // Already tracked: AddComponent sets PermissionToEdit=false on the root layer.
        // TODO: use ComponentManager::IsComponentTracked(stage) once exposed, instead of the
        // PermissionToEdit proxy.
        return;
    }
    try {
        auto desc = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(stage);
        if (desc) {
            self->AddComponent(stage);
        }
    } catch (...) {
    }
}
