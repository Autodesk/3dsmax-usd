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

#include "MaxLayerEditor.h"

#include "USDLayerManager.h"

#include <MaxUsdObjects/LayerEditor/MaxLayerEditorWindow.h>
#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>
#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ListenerUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/UiUtils.h>

#include <UsdLayerEditor/layerLocking.h>
#include <UsdLayerEditor/layerMuting.h>
#include <UsdLayerEditor/utilFileSystem.h>
#include <UsdLayerEditor/utilSerialization.h>
#include <UsdLayerEditor/utilUI.h>
#include <usdUfe/ufe/Global.h>
#include <usdUfe/ufe/StagesSubject.h>

#include <pxr/usd/usd/stageCache.h>
#include <pxr/usd/usd/stageCacheContext.h>

#include <Qt/QmaxDockWidget.h>
#include <ufe/pathString.h>

#include <IPathConfigMgr.h>
#include <QtWidgets/QApplication>
#include <max.h>
#include <qpointer.h>

#if _MSVC_LANG > 201402L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

std::unique_ptr<MaxLayerEditor> MaxLayerEditor::instance;

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

MaxLayerEditor* MaxLayerEditor::Instance()
{
    if (!instance) {
        instance = std::unique_ptr<MaxLayerEditor>(new MaxLayerEditor);
    }
    return instance.get();
}

MaxLayerEditor::MaxLayerEditor()
{
    RegisterNotification(&MaxLayerEditor::OnSceneReset, this, NOTIFY_POST_SCENE_RESET);
}

MaxLayerEditor::~MaxLayerEditor()
{
    UnRegisterNotification(&MaxLayerEditor::OnSceneReset, this, NOTIFY_POST_SCENE_RESET);
}

void MaxLayerEditor::Initialize()
{
    UsdLayerEditor::initializeQtUtils();
    UsdLayerEditor::getQtUtils()->setDpiScale(static_cast<double>(MaxSDK::GetUIScaleFactor()));

    UsdLayerEditor::FileSystem::setFileWriteAccessFunction([](const std::string& path) {
        const auto filePath = MaxUsd::UsdStringToMaxString(path);
        // Check for the file's existence, and write permission.
        if (_taccess(filePath, 0) == 0 && _taccess(filePath, 2) == -1) {
            return false;
        }
        return true;
    });

    UsdLayerEditor::Serialization::setUpdateDCCObjectRootLayerFunction(
        [](const std::string& stageObjectPath, const std::string& rootLayerPath) {
            const auto ufePath = Ufe::PathString::path(stageObjectPath);
            const auto object = StageObjectMap::GetInstance()->Get(ufePath);
            const auto rootPath = MaxUsd::UsdStringToMaxString(rootLayerPath);

            // Keep track of the muted layers of the stage object:
            // we need to reset them to muted because we are recreating
            // a new stage object and they won't carry over implicitly.
            const auto mutedLayers = object->GetUSDStage()->GetMutedLayers();

            IParamBlock2* pb = object->GetParamBlock(0);
            BOOL          payloadsLoaded = false;
            Interval      valid;
            pb->GetValue(
                PBParameterIds::LoadPayloads, GetCOREInterface()->GetTime(), payloadsLoaded, valid);

            SdfLayerRefPtr layerPtr = SdfLayer::FindOrOpen(rootLayerPath);
            auto           updatedStage = UsdStage::UsdStage::Open(
                layerPtr,
                object->GetUSDStage()->GetSessionLayer(),
                payloadsLoaded ? UsdStage::InitialLoadSet::LoadAll
                                         : UsdStage::InitialLoadSet::LoadNone);

            object->GetParamBlock(0)->SetValue(StageFile, GetCOREInterface()->GetTime(), rootPath);
            object->GetParamBlock(0)->SetValue(StageMask, GetCOREInterface()->GetTime(), L"/");
            object->SetUSDStage(updatedStage);

            // Set the muted layers
            UsdLayerEditor::LayerNameMap nameMap;
            UsdLayerEditor::loadLayerMuteState(mutedLayers, nameMap, *updatedStage);

            // Reset the AnonRootId to "", so that when we persist this param,
            // on load, the code in USDAssetAccessor.h will not attempt to load
            // an anonymous layer that may be serialized in the .max scene file
            // (since this param is what drives reloading of anon root layers
            // from the .max scenes).
            // NOTE: the reason we reset it here is because this callback
            // is called when an anonymous root layer is saved from the layer
            // editor.
            if (pb) {
                pb->SetValue(PBParameterIds::AnonRootId, 0, L"");
            }
        });

    UsdLayerEditor::FileSystem::setDCCSceneLocationFunc([]() -> std::string {
        auto path = GetCOREInterface()->GetCurFilePath();
        if (path == 0) {
            return "";
        }

        return fs::path(MaxUsd::MaxStringToUsdString(path)).parent_path().string();
    });

    UsdLayerEditor::FileSystem::setDCCWorkspaceSceneLocationFunc([]() -> std::string {
        const MSTR sceneDir
            = MaxSDKSupport::GetString(IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_SCENE_DIR));
        return MaxUsd::MaxStringToUsdString(sceneDir);
    });

    UsdLayerEditor::UIUtils::setErrorDisplayCallbackFunction([](std::string str) {
        MaxUsd::Listener::Write(MaxUsd::UsdStringToMaxString(str).data(), true);
    });

    // Force initialize the instance - hooks up to 3dsmax notifications.
    USDLayerManager::Instance();
}

void MaxLayerEditor::Open()
{
    if (GetCOREInterface()->GetQuietMode()) {
        return;
    }

    const auto dock = getLayerEditorDockWidget();
    dock->setWindowState(dock->windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
    dock->show();
    dock->raise();
}

void MaxLayerEditor::Close()
{
    const auto host = getLayerEditorDockWidget();
    host->hide();
}

void MaxLayerEditor::OpenStage(USDStageObject* stageObject)
{
    if (GetCOREInterface()->GetQuietMode()) {
        return;
    }

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

void MaxLayerEditor::OnSceneReset(void* param, NotifyInfo* info)
{
    // Make sure we don't hold onto locked or muted layers now that the
    // 3dsMax scene is reset.
    UsdLayerEditor::forgetMutedLayers();
    UsdLayerEditor::forgetLockedLayers();
    UsdLayerEditor::forgetSystemLockedLayers();
}