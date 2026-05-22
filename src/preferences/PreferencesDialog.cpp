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

#include "PreferenceApplicationHost.h"
#include "PreferencesManagement.h"
#include "ui_PreferencesDialog.h"

#include <MaxUsd/Utilities/PluginUtils.h>
#include <MaxUsd/Utilities/UiUtils.h>

#include <Qt/QmaxMainWindow.h>
#include <Qt/QmaxToolClips.h>

#ifdef IS_MAX2026_OR_GREATER
#include <AssetResolverPreferences/AssetResolverSettingsManagement.h>
#include <AssetResolverPreferences/USDAssetResolverSettingsWidget.h>
#endif
#include <QStyle>
#include <maxapi.h>

UsdPreferencesDialog::UsdPreferencesDialog(QWidget* parent)
    : QDialog { parent }
{
    setWindowFlags(windowFlags());
    ui->setupUi(this);
    setParent(parent, windowFlags());

    QPixmap headerIcon = style()->standardPixmap(QStyle::SP_MessageBoxInformation);
    ui->left_label->setPixmap(headerIcon);

    ui->version_label->setText(QString::fromStdString(MaxUsd::GetPluginDisplayVersion()));

#ifdef IS_MAX2026_OR_GREATER
    auto margins = ui->MainLayout->contentsMargins();
    ui->MainLayout->setContentsMargins(
        margins.left(),
        Adsk::ApplicationHost::instance().pm(Adsk::ApplicationHost::PixelMetric::ItemHeight),
        margins.right(),
        margins.bottom());

    // asset resolver group and widgets
    auto assetResolverGroup = new QGroupBox(this);
    assetResolverSettingsWidget = new Adsk::USDAssetResolverSettingsWidget(
        Adsk::AssetResolverSettingsManagement::FillSettingsWithExtensions(), parent);
    assetResolverGroup->setTitle(tr("Asset Resolver"));
    auto layout = new QHBoxLayout(assetResolverGroup);
    layout->addWidget(assetResolverSettingsWidget);
    ui->MainLayout->addWidget(assetResolverGroup);
#endif

    QWidget*     saveAndCloseWidget = new QWidget(this);
    QHBoxLayout* saveAndCloselayout = new QHBoxLayout(saveAndCloseWidget);
    QPushButton* saveButton = new QPushButton(tr("Save && Close"), this);
    saveButton->setDefault(false);
    saveButton->setAutoDefault(false);
    saveButton->setFocusPolicy(Qt::NoFocus);
    saveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    saveButton->setToolTip(tr("Save settings"));
    saveAndCloselayout->addWidget(saveButton);
    QPushButton* closeButton = new QPushButton(tr("Close"), this);
    closeButton->setDefault(false);
    closeButton->setAutoDefault(false);
    closeButton->setFocusPolicy(Qt::NoFocus);
    closeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    saveAndCloselayout->addWidget(closeButton);
    ui->MainLayout->addWidget(saveAndCloseWidget);

    // Connect only the action signals (save and close)
    QObject::connect(saveButton, &QPushButton::clicked, this, &QDialog::accept);
    QObject::connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);

    // 3ds Max toolclips do not behave so well (linger and do not disappear or move with the
    // dialog). Disable until these issues are fixed.
    MaxUsd::Ui::DisableMaxToolClipsRecursively(this);
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

#ifdef IS_MAX2026_OR_GREATER
const Adsk::AssetResolverSettings UsdPreferencesDialog::getOptions() const
{
    return assetResolverSettingsWidget->getSettings();
}
#endif
