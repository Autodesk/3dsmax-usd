//
// Copyright 2023 Autodesk
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
#include "UsdExportMaterialsRollup.h"

#include "ui_UsdExportMaterialsRollup.h"

#include <MaxUsd/Translators/ShadingModeRegistry.h>
#include <MaxUsd/USDCore.h>

#include <pxr/usdImaging/usdImaging/tokens.h>

#include <Qt/QMaxMessageBox.h>
#include <Qt/QmaxToolClips.h>

#include <IPathConfigMgr.h>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>

UsdExportMaterialsRollup::UsdExportMaterialsRollup(
    MaxUsd::USDSceneBuilderOptions& buildOptions,
    QWidget*                        parent /* = nullptr */)
    : ui(new Ui::UsdExportMaterialsRollup)
    , buildOptions(buildOptions)
{
    ui->setupUi(this);

    ui->ExportMaterialsGroupBox->setChecked(buildOptions.GetTranslateMaterials());
    MaxSDK::QmaxToolClips::disableToolClip(ui->ExportMaterialsGroupBox);

    auto               allMaterialConversions = buildOptions.GetAllMaterialConversions();
    pxr::TfTokenVector materials = pxr::MaxUsdShadingModeRegistry::ListMaterialConversions();
    for (const auto& material : materials) {
        if (!(material == pxr::UsdImagingTokens->UsdPreviewSurface || material == "MaterialX")) {
            continue;
        }
        const pxr::MaxUsdShadingModeRegistry::ConversionInfo& conversionInfo
            = pxr::MaxUsdShadingModeRegistry::GetMaterialConversionInfo(material);
        QCheckBox* materialSelector = new QCheckBox(conversionInfo.niceName.GetText());
        materialSelector->setToolTip(conversionInfo.exportDescription.GetText());
        ui->ConvertToMaterialsLayout->addWidget(materialSelector);
        materialConversionMap[materialSelector] = material;
        materialSelector->setChecked(
            allMaterialConversions.find(material) != allMaterialConversions.end());
        connect(
            materialSelector,
            &QCheckBox::stateChanged,
            this,
            &UsdExportMaterialsRollup::OnMaterialConversionStateChanged);
    }

#ifdef IS_MAX2024_OR_GREATER
    connect(
        ui->MaterialSwitcherExportStyleComboBox,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        &UsdExportMaterialsRollup::OnMaterialSwitcherExportStyleComboBoxChanged);

    QStandardItemModel* exportStyleModel
        = dynamic_cast<QStandardItemModel*>(ui->MaterialSwitcherExportStyleComboBox->model());
    exportStyleModel->item(0)->setToolTip(
        tr("All material inputs are exported as material variants."));
    exportStyleModel->item(1)->setToolTip(
        tr("Only exports the active material and binds it to the prim."));
    if (buildOptions.GetMtlSwitcherExportStyle()
        == MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::AsVariantSets) {
        ui->MaterialSwitcherExportStyleComboBox->setCurrentIndex(0);
    } else {
        ui->MaterialSwitcherExportStyleComboBox->setCurrentIndex(1);
    }
#else
    ui->MaterialSwitcherOptionWidget->hide();
#endif

    ui->materialLayerPath->setText(QString::fromStdString(buildOptions.GetMaterialLayerPath()));
    OnMaterialLayerPathChanged();
    connect(
        ui->materialLayerPath,
        &QLineEdit::editingFinished,
        this,
        &UsdExportMaterialsRollup::OnMaterialLayerPathChanged);

    ui->separateMaterialLayer->setChecked(buildOptions.GetUseSeparateMaterialLayer());
    ui->materialLayerPath->setEnabled(buildOptions.GetUseSeparateMaterialLayer());
    ui->layerPathLabel->setEnabled(buildOptions.GetUseSeparateMaterialLayer());
    ui->materialLayerPicker->setEnabled(buildOptions.GetUseSeparateMaterialLayer());
    connect(
        ui->separateMaterialLayer,
        &QCheckBox::clicked,
        this,
        &UsdExportMaterialsRollup::OnSeparateMaterialChanged);

    ui->materialPrimPath->setText(
        QString::fromStdString(buildOptions.GetMaterialPrimPath().GetString()));
    connect(
        ui->materialPrimPath,
        &QLineEdit::editingFinished,
        this,
        &UsdExportMaterialsRollup::OnMaterialPrimPathChanged);

    connect(
        ui->materialLayerPicker,
        &QToolButton::clicked,
        this,
        &UsdExportMaterialsRollup::OnMaterialLayerClicked);
}

UsdExportMaterialsRollup::~UsdExportMaterialsRollup() { }

void UsdExportMaterialsRollup::on_ExportMaterialsGroupBox_toggled(bool state)
{
    buildOptions.SetTranslateMaterials(state);

    // disable/enable groupbox's content (which we do not know in advance)
    auto layout = ui->ExportMaterialsGroupBox->layout();
    int  nbLayoutItems = layout->count();
    for (int i = 0; i < nbLayoutItems; ++i) {
        auto item = layout->itemAt(i);
        auto widget = item->widget();
        // spacer items are not reported as widgets
        if (widget) {
            widget->setEnabled(state);
        }
    }
}

void UsdExportMaterialsRollup::OnMaterialConversionStateChanged(bool /*state*/)
{
    std::set<pxr::TfToken> allMaterialConversions;
    for (const auto checkboxItem : materialConversionMap) {
        if (checkboxItem.first->isChecked()) {
            allMaterialConversions.insert(checkboxItem.second);
        }
    }

    buildOptions.SetAllMaterialConversions(allMaterialConversions);
}

void UsdExportMaterialsRollup::OnSeparateMaterialChanged(bool checked)
{
    buildOptions.SetUseSeparateMaterialLayer(checked);
    ui->materialLayerPath->setEnabled(checked);
    ui->layerPathLabel->setEnabled(checked);
    ui->materialLayerPicker->setEnabled(checked);
}

void UsdExportMaterialsRollup::OnMaterialPrimPathChanged()
{
    const std::string value = ui->materialPrimPath->text().toStdString();
    std::string       err;
    if (pxr::SdfPath::IsValidPathString(value, &err)) {
        pxr::SdfPath primPath(value);
        if (primPath.IsAbsoluteRootOrPrimPath()) {
            buildOptions.SetMaterialPrimPath(primPath);
            return;
        } else {
            MaxSDK::QmaxMessageBox(
                this,
                QString("Invalid Prim Path, it will not be used for the export."),
                "Invalid Prim Path",
                MB_ICONEXCLAMATION | MB_OK | MB_SYSTEMMODAL);
            return;
        }
    } else {
        MaxSDK::QmaxMessageBox(
            this,
            QString("Invalid Prim Path, it will not be used for the export.\n")
                + QString::fromStdString(err),
            "Invalid Prim Path",
            MB_ICONEXCLAMATION | MB_OK | MB_SYSTEMMODAL);
    }
}

void UsdExportMaterialsRollup::OnMaterialLayerPathChanged()
{
    const QString value = ui->materialLayerPath->text();
    const auto    path = USDCore::sanitizedFilename(value.toStdString(), ".usda");
    buildOptions.SetMaterialLayerPath(path.string());
}

void UsdExportMaterialsRollup::OnMaterialLayerClicked()
{
    const TCHAR* exportDir
        = MaxSDKSupport::GetString(IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_EXPORT_DIR));
    QString qDir = QString::fromStdString(MaxUsd::MaxStringToUsdString(exportDir));
    QString materialFile = QFileDialog::getSaveFileName(
        this, tr("Select file to export materials"), qDir, tr("USD (*.usd *.usdc *.usda)"));
    if (!materialFile.isEmpty()) {
        ui->materialLayerPath->setText(materialFile);
        OnMaterialLayerPathChanged();
    }
}

#ifdef IS_MAX2024_OR_GREATER
void UsdExportMaterialsRollup::OnMaterialSwitcherExportStyleComboBoxChanged(int index)
{
    switch (index) {
    case 0:
        buildOptions.SetMtlSwitcherExportStyle(
            MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::AsVariantSets);
        break;
    case 1:
        buildOptions.SetMtlSwitcherExportStyle(
            MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::ActiveMaterialOnly);
        break;
    default:
        DbgAssert(
            false && "Invalid USD export Material Switcher export style - this should not be hit!");
        break;
    }
}
#endif