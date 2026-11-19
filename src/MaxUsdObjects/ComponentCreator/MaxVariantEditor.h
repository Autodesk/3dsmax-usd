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

#include <ufe/observer.h>

#include <memory>

namespace AdskVariantEditor {
class VariantEditorWidget;
}
namespace AdskUsdComponentCreator {
class ComponentDescription;
}
namespace MaxSDK {
class QmaxDockWidget;
}
struct NotifyInfo;

class MaxUSDObjectsAPI MaxVariantEditor
{
public:
    static MaxVariantEditor* Instance();
    static void              Shutdown();
    static bool              IsCreated();

    void Open();
    void OpenFor(const pxr::UsdStageRefPtr& stage);
    void NotifyComponentChanged(const pxr::UsdStageRefPtr& stage);
    void NotifyComponentChanged(
        const std::shared_ptr<AdskUsdComponentCreator::ComponentDescription>& desc);
    void RefreshCurrent();
    void Clear();
    void RepointStage(const pxr::UsdStageRefPtr& oldStage, const pxr::UsdStageRefPtr& newStage);
    bool IsPinned() const;
    void SetPinned(bool pinned);
    pxr::UsdStageRefPtr GetCurrentStage() const;

    // Show whatever the current USD selection points at. Called by the selection observer; pinning
    // stops the editor from following the selection.
    void HandleSelectionChanged();

private:
    MaxVariantEditor();
    ~MaxVariantEditor();
    void EnsureDockWidget();

    static void OnNodePreDeleted(void* param, NotifyInfo* info);
    static void OnSceneReset(void* param, NotifyInfo* info);
    static void OnSelectionSetChanged(void* param, NotifyInfo* info);

    MaxSDK::QmaxDockWidget*                 dock = nullptr;
    AdskVariantEditor::VariantEditorWidget* widget = nullptr;
    pxr::UsdStageRefPtr                     currentStage;
    bool                                    pinned = false;
    std::shared_ptr<Ufe::Observer>          selectionObserver;

    // Allow unique_ptr's default deleter to call the private destructor.
    friend struct std::default_delete<MaxVariantEditor>;
};
