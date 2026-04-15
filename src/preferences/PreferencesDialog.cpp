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
// limitations under the License.
//

#include "PreferencesDialog.h"

#include "AssetResolverPreferences/USDAssetResolverSettingsWidget.h"
#include "PreferenceApplicationHost.h"
#include "PreferencesOptions.h"
#include "ui_PreferencesDialog.h"

#include <MaxUsd/Utilities/PluginUtils.h>
#include <MaxUsd/Utilities/UiUtils.h>

#include <Qt/QmaxMainWindow.h>
#include <Qt/QmaxToolClips.h>

#include <QStyle>
#include <maxapi.h>

UsdPreferencesDialog::UsdPreferencesDialog(const UsdPreferenceOptions& options, QWidget* parent)
    : QDialog { parent }
{
    setWindowFlags(windowFlags());
    ui->setupUi(this);
    setParent(parent, windowFlags());

    QPixmap headerIcon = style()->standardPixmap(QStyle::SP_MessageBoxInformation);
    ui->left_label->setPixmap(headerIcon);

    ui->version_label->setText(QString::fromStdString(MaxUsd::GetPluginDisplayVersion()));

    auto margins = ui->MainLayout->contentsMargins();
    ui->MainLayout->setContentsMargins(
        margins.left(),
        Adsk::ApplicationHost::instance().pm(Adsk::ApplicationHost::PixelMetric::ItemHeight),
        margins.right(),
        margins.bottom());

    // asset resolver group and widgets
    auto assetResolverGroup = new QGroupBox(this);
    assetResolverSettingsWidget = new Adsk::USDAssetResolverSettingsWidget(this);
    assetResolverGroup->setTitle(tr("Asset Resolver"));
    auto layout = new QHBoxLayout(assetResolverGroup);
    layout->addWidget(assetResolverSettingsWidget);
    ui->MainLayout->insertWidget(0, assetResolverGroup, 1);

    // 3ds Max toolclips do not behave so well (linger and do not disappear or move with the
    // dialog). Disable until these issues are fixed.
    MaxUsd::Ui::DisableMaxToolClipsRecursively(this);

    loadOptions(options);
}

UsdPreferencesDialog::~UsdPreferencesDialog() { }

void UsdPreferencesDialog::moveEvent(QMoveEvent* event)
{
    QDialog::moveEvent(event);
    Q_EMIT geometryChanged(geometry());
}

void UsdPreferencesDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    Q_EMIT geometryChanged(geometry());
}

void UsdPreferencesDialog::loadOptions(const UsdPreferenceOptions& options)
{
    if (assetResolverSettingsWidget) {
        assetResolverSettingsWidget->setIncludeProjectTokens(options.IsUsingProjectTokens());
        assetResolverSettingsWidget->setMappingFilePath(
            QString::fromStdString(options.GetMappingFile().string()));

        assetResolverSettingsWidget->setUserPathsFirst(options.IsUsingUserSearchPathsFirst());
        assetResolverSettingsWidget->setUserPathsOnly(!options.IsIncludingEnvironmentSearchPaths());
        QStringList qUserPaths;
        for (const auto& path : options.GetUserSearchPaths()) {
            qUserPaths.append(QString::fromStdString(path));
        }
        assetResolverSettingsWidget->setUserPaths(qUserPaths);

        QStringList qEnvPaths;
        for (const auto& path : options.GetEnvironmentSearchPaths()) {
            qEnvPaths.append(QString::fromStdString(path));
        }
        assetResolverSettingsWidget->setExtAndEnvPaths(qEnvPaths);
    }
}

const UsdPreferenceOptions UsdPreferencesDialog::getOptions() const
{
    UsdPreferenceOptions options;

    if (assetResolverSettingsWidget) {
        options.SetUsingProjectTokens(assetResolverSettingsWidget->includeProjectTokens());
        options.SetMappingFile(assetResolverSettingsWidget->mappingFilePath().toStdString());

        std::vector<std::string> userPaths;
        for (auto qPath : assetResolverSettingsWidget->userPaths()) {
            userPaths.push_back(qPath.toStdString());
        }
        options.SetUserSearchPaths(userPaths);
        options.SetUsingUserSearchPathsFirst(assetResolverSettingsWidget->userPathsFirst());
        options.SetIncludingEnvironmentSearchPaths(!assetResolverSettingsWidget->userPathsOnly());
    }

    return options;
}
