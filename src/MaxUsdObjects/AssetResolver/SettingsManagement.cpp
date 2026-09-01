//
// Copyright 2025 Autodesk
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

#include "SettingsManagement.h"

#include "AssetResolverApplicationHost.h"

#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>

#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/OptionUtils.h>
#include <MaxUsd/Utilities/VtDictionaryUtils.h>

#ifdef ADSK_ASSET_RESOLVER_ENABLED
#include <AssetResolverExtensions/PathDialog/PathDialog.h>
#include <AssetResolverExtensions/Settings/AssetResolverSettings.h>
#include <AssetResolverExtensions/Settings/AssetResolverSettingsManagement.h>
#endif // ADSK_ASSET_RESOLVER_ENABLED

#include <pxr/base/vt/dictionary.h>

#include <Qt/QmaxDockWidget.h>
#include <Qt/QmaxMainWindow.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <maxapi.h>
#include <qmenubar.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace AssetResolverSettingsManagement {

#ifdef ADSK_ASSET_RESOLVER_ENABLED
namespace {

// QmaxMainWindow::raiseDockWidget only exists in an Autodesk patched version of Qt.
// However, no public build is available outside of Autodesk at the moment.
// Invoking raiseDockWidget through the meta objects allows to build the solution with a
// standard Qt distribution.
void RaiseDockWidget(QMainWindow* mainWindow, QDockWidget* dockWidget)
{
    if (mainWindow->metaObject()->indexOfMethod("raiseDockWidget(QDockWidget*)") != -1) {
        std::ignore = QMetaObject::invokeMethod(
            mainWindow, "raiseDockWidget", Q_ARG(QDockWidget*, dockWidget));
    }
}

Adsk::AssetResolverPathDialog* CreateUsdPathDialog(QWidget* parent)
{
    Adsk::AssetResolverPathDialog* usdPathDialog = new Adsk::AssetResolverPathDialog(parent);

    usdPathDialog->setSettingsAppliedFunctor(AssetResolverSettingsManagement::SaveSettings);
    usdPathDialog->setGetStagesFunctor([]() {
        std::vector<UsdStageRefPtr> stages;
        for (const auto& stageObject : StageObjectMap::GetInstance()->GetAllStageObjects()) {
            if (auto stage = stageObject->GetUSDStage()) {
                if (stage->GetRootLayer()) {
                    stages.push_back(stage);
                }
            }
        }
        return stages;
    });

    return usdPathDialog;
}

MaxSDK::Util::Path GetPathToUsdArSettings()
{
    auto pathToUsdSettings = MaxUsd::OptionUtils::GetPathToUSDSettings();
    pathToUsdSettings.Append(_T("\\usdArSettings.json"));
    return pathToUsdSettings;
}

} // namespace

VtDictionary LoadSettings()
{
    QFile       file(GetPathToUsdArSettings().GetString());
    QJsonObject json;

    VtDictionary guide = Adsk::AssetResolverSettings::GetDefaultSettings();

    if (file.exists()) {
        if (!MaxUsd::OptionUtils::ReadJsonFile(json, file, GetPathToUsdArSettings().GetCStr())) {
            return guide;
        }

        VtDictionary  dict;
        QJsonDocument doc(json);
        QString       strJson(doc.toJson());
        auto          stdStr = strJson.toStdString();
        MaxUsd::DictUtils::VtDictFromString(stdStr, dict);
        MaxUsd::DictUtils::CoerceDictToGuideType(dict, guide);
        return VtDictionaryOver(dict, guide);
    }
    return guide;
}

void SaveSettings(const Adsk::AssetResolverSettings& options)
{
    // update the options instance
    // the copy clears env search paths as they are not saved
    // those are only used to display the paths in the dialog
    Adsk::AssetResolverSettings::GetInstance() = options;

    // save options to disk
    QJsonObject json;
    MaxUsd::DictUtils::VtDictToJson(Adsk::AssetResolverSettings::GetInstance().GetSettings(), json);

    QFile file(GetPathToUsdArSettings().GetString());
    MaxUsd::OptionUtils::WriteJsonFile(
        file, QJsonDocument(json).toJson().toStdString(), GetPathToUsdArSettings().GetCStr());
}

void InitializeSettings()
{
    // Add ApplicationHost for the USD Preferences dialog
    AssetResolverApplicationHost::CreateInstance(GetCOREInterface()->GetQmaxMainWindow());
    // Load USD Preference options to ensure the Adsk Asset Resolver works as configured
    Adsk::AssetResolverSettings::GetInstance().SetSettings(LoadSettings());
    Adsk::AssetResolverSettingsManagement::ApplySettings(
        Adsk::AssetResolverSettings(), Adsk::AssetResolverSettings::GetInstance());
}

void ShowDialog(const Adsk::AssetResolverPathDialog::Tab& tab, UsdStageRefPtr stage)
{
    auto mainWindow = GetCOREInterface()->GetQmaxMainWindow();

    static QPointer<MaxSDK::QmaxDockWidget> s_dockWidget = nullptr;

    if (s_dockWidget) {
        if (auto usdPathDialog
            = dynamic_cast<Adsk::AssetResolverPathDialog*>(s_dockWidget->widget())) {
            usdPathDialog->setCurrentTab(tab);
            if (stage) {
                usdPathDialog->setCurrentStage(stage);
            }
        }
        RaiseDockWidget(mainWindow, s_dockWidget);
        s_dockWidget->raise();
        if (s_dockWidget->isMinimized()) {
            s_dockWidget->showNormal();
        } else {
            s_dockWidget->show();
        }
        return;
    }

    // Restore the dock widget's saved state from the 3dsMax workspace layout.
    // If no saved state is found (first-time use), fall back to floating
    // with a default size.
    const QSize defaultFloatingSize(MaxSDK::UIScaled(800), MaxSDK::UIScaled(400));

    auto dockWidget
        = new MaxSDK::QmaxDockWidget("USD Path Editor", QObject::tr("USD Path Editor"), mainWindow);
    dockWidget->setAttribute(Qt::WA_DeleteOnClose);
    dockWidget->setAllowedAreas(Qt::AllDockWidgetAreas);

    // this also will show the "native title bar" when the dock widget is
    // floating, which gives a better experience for modal dialogs.
    dockWidget->setProperty("QmaxDockMinMaximizable", true);

    auto usdPathDialog = CreateUsdPathDialog(mainWindow);
    usdPathDialog->setCurrentTab(tab);
    if (stage) {
        usdPathDialog->setCurrentStage(stage);
    }
    // usdPathDialog->setWindowFlags(Qt::Widget);
    dockWidget->setWidget(usdPathDialog);
    usdPathDialog->setContentsMargins(0, 0, 0, 0);

    // TODO: cleanup the asset resolver layouts, one rainy day...
    auto menu_bar = usdPathDialog->findChild<QMenuBar*>();
    if (menu_bar) {
        menu_bar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        menu_bar->adjustSize();
        menu_bar->setFixedWidth(menu_bar->sizeHint().width());
        menu_bar->resize(menu_bar->sizeHint());
    }

    if (!mainWindow->restoreDockWidget(dockWidget)) {
        mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
        dockWidget->setFloating(true);
        dockWidget->resize(defaultFloatingSize);
    }

    QObject::connect(
        usdPathDialog, &QDialog::finished, [dockWidget, usdPathDialog]() { dockWidget->close(); });

    // Reset to the default size when un-docking.
    QObject::connect(
        dockWidget,
        &MaxSDK::QmaxDockWidget::topLevelChanged,
        [defaultFloatingSize, dockWidget](bool topLevel) {
            if (topLevel) {
                dockWidget->resize(defaultFloatingSize);
            }
        });

    RaiseDockWidget(mainWindow, dockWidget);
    dockWidget->raise();
    dockWidget->show();

    s_dockWidget = dockWidget;
}
#endif // ADSK_ASSET_RESOLVER_ENABLED
} // namespace AssetResolverSettingsManagement
