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

#include "USDLayerEditor.h"

#include <MaxUsdObjects/LayerEditor/MaxLayerEditorWindow.h>
#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>
#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/UiUtils.h>

#include <UsdLayerEditor/layerLocking.h>
#include <UsdLayerEditor/layerMuting.h>
#include <usdUfe/ufe/Global.h>
#include <usdUfe/ufe/StagesSubject.h>

#include <Qt/QmaxDockWidget.h>
#include <ufe/pathString.h>

#include <QtWidgets/QApplication>
#include <max.h>
#include <qpointer.h>

std::unique_ptr<USDLayerEditor> USDLayerEditor::instance;

namespace {

/**
 * Builds (if needed) and returns a 3dsMax dock widget with QMaxMainWindow behavior
 * containing the USD Layer Editor.
 * @return the doc widget.
 */
MaxSDK::QmaxDockWidget* getLayerEditorDockWidget()
{

    static MaxSDK::QmaxDockWidget* dockWidget = [] {
        auto maxMainWindow = GetCOREInterface()->GetQmaxMainWindow();

        auto layerEditorDockWidget = new MaxSDK::QmaxDockWidget(
            "USD Layer Editor", QObject::tr("USD Layer Editor"), maxMainWindow);
        layerEditorDockWidget->setProperty("QmaxDockMinMaximizable", true);

        auto layerEditorMainWindow
            = new MaxLayerEditorWindow("USD Layer Editor", layerEditorDockWidget, Qt::Widget);

        layerEditorDockWidget->setWidget(layerEditorMainWindow);

        layerEditorDockWidget->setFocusProxy(layerEditorMainWindow->centralWidget());
        layerEditorDockWidget->setFocusPolicy(Qt::StrongFocus);

        // Workaround to trick 3dsmax into properly docking this widget.
        maxMainWindow->addDockWidget(Qt::RightDockWidgetArea, layerEditorDockWidget);

        // We want our dock-widget to float with native window behavior
        layerEditorDockWidget->setFloating(true);
        // Arbitrary default size...
        layerEditorDockWidget->resize(MaxSDK::UIScaled(280), MaxSDK::UIScaled(440));

        // Set back default size when un-docking.
        QSize floatingSize = layerEditorDockWidget->size();
        QObject::connect(
            layerEditorDockWidget,
            &MaxSDK::QmaxDockWidget::topLevelChanged,
            [floatingSize, layerEditorDockWidget](bool topLevel) {
                if (topLevel) {
                    layerEditorDockWidget->resize(floatingSize);
                }
            });

        // We want the 3dsMax hotkeys to work while we are focused on the layer treeView.
        auto treeViews = layerEditorDockWidget->findChildren<QTreeView*>();
        for (const auto& treeView : treeViews) {
            MaxUsd::Ui::DisableMaxAcceleratorsOnFocus(treeView, false);
        }

        return layerEditorDockWidget;
    }();

    return dockWidget;
}
} // namespace

USDLayerEditor* USDLayerEditor::Instance()
{
    if (!instance) {
        instance = std::unique_ptr<USDLayerEditor>(new USDLayerEditor);

        // One time configuration some USDLayerEditor DCC hooks..

        UsdLayerEditor::setLockedLayersSaveFunction([](const std::string& stageObjectPath) {
            const auto ufePath = Ufe::PathString::path(stageObjectPath);
            const auto object = StageObjectMap::GetInstance()->Get(ufePath);
            object->SetLockedLayersState(UsdLayerEditor::getLockedLayersIdentifiers());
        });

        UsdLayerEditor::setMutedLayersSaveFunction([](const std::string& stageObjectPath) {
            const auto ufePath = Ufe::PathString::path(stageObjectPath);
            const auto object = StageObjectMap::GetInstance()->Get(ufePath);
            if (auto stage = object->GetUSDStage()) {
                object->SetMutedLayersState(stage->GetMutedLayers());
            }
        });
    }
    return instance.get();
}

USDLayerEditor::USDLayerEditor()
{
    RegisterNotification(&USDLayerEditor::OnSceneReset, this, NOTIFY_POST_SCENE_RESET);
}

USDLayerEditor::~USDLayerEditor()
{
    UnRegisterNotification(&USDLayerEditor::OnSceneReset, this, NOTIFY_POST_SCENE_RESET);
}

void USDLayerEditor::Open()
{
    const auto dock = getLayerEditorDockWidget();
    dock->setWindowState(dock->windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
    dock->show();
    dock->raise();
}

void USDLayerEditor::Close()
{
    const auto host = getLayerEditorDockWidget();
    host->hide();
}

void USDLayerEditor::OpenStage(USDStageObject* stageObject)
{
    if (!stageObject) {
        Open();
        return;
    }

    const auto stage = stageObject->GetUSDStage();
    if (!stage) {
        return;
    }
    const auto dock = getLayerEditorDockWidget();
    auto       layerEditor = static_cast<MaxLayerEditorWindow*>(dock->widget());

    layerEditor->selectDccObject(MaxUsd::ufe::getUsdStageObjectPath(stageObject).string().c_str());

    dock->show();
    dock->setWindowState(dock->windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
    dock->raise();
}

void USDLayerEditor::OnSceneReset(void* param, NotifyInfo* info)
{
    // Make sure we don't hold onto locked or muted layers now that the
    // 3dsMax scene is reset.
    UsdLayerEditor::forgetMutedLayers();
    UsdLayerEditor::forgetLockedLayers();
    UsdLayerEditor::forgetSystemLockedLayers();
}