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
#include "UsdExportIncludeRollup.h"

#include "USDExportCustomChannelMappingsDialog.h"
#include "ui_UsdExportIncludeRollup.h"

#include <MaxUsd/Translators/ShadingModeRegistry.h>
#include <MaxUsd/Widgets/TooltipEventFilter.h>

#include <Qt/QmaxToolClips.h>

#include <QtHelp/QtHelp>
#include <QtWidgets/QCheckBox>
#include <maxicon.h>

UsdExportIncludeRollup::UsdExportIncludeRollup(
    MaxUsd::USDSceneBuilderOptions& buildOptions,
    QWidget*                        parent /* = nullptr */)
    : ui(new Ui::UsdExportIncludeRollup)
    , buildOptions(buildOptions)
    , customChannelMappings()
{
    ui->setupUi(this);

    ui->CamerasCheckBox->setChecked(buildOptions.GetTranslateCameras());
    ui->LightsCheckBox->setChecked(buildOptions.GetTranslateLights());
    ui->ShapesCheckBox->setChecked(buildOptions.GetTranslateShapes());
    ui->UsdStagesCheckBox->setChecked(buildOptions.GetUsdStagesAsReferences());

    ui->GeometryGroupBox->setChecked(buildOptions.GetTranslateMeshes());

    QStandardItemModel* meshModel
        = dynamic_cast<QStandardItemModel*>(ui->MeshFormatComboBox->model());
    meshModel->item(0)->setToolTip(tr("Exports meshes as they are in the scene."));
    meshModel->item(1)->setToolTip(tr("Converts to polygonal meshes at export."));
    meshModel->item(2)->setToolTip(tr("Converts to triangulated mesh at export."));
    ui->MeshFormatComboBox->setCurrentIndex(
        static_cast<int>(buildOptions.GetMeshConversionOptions().GetMeshFormat()));
    ui->MeshFormatComboBox->setToolTip(
        meshModel->item(static_cast<int>(buildOptions.GetMeshConversionOptions().GetMeshFormat()))
            ->toolTip());

    ui->PreserveEdgeOrientationCheckBox->setChecked(
        buildOptions.GetMeshConversionOptions().GetPreserveEdgeOrientation());
    ui->BakeOffsetTransformCheckBox->setChecked(
        buildOptions.GetMeshConversionOptions().GetBakeObjectOffsetTransform());

    QStandardItemModel* normalsModel
        = dynamic_cast<QStandardItemModel*>(ui->NormalsComboBox->model());
    normalsModel->item(0)->setToolTip(
        tr("Exports normals as USD primvars (primitive variables). This interpolates "
           "attribute values over the surface/volume of a prim."));
    normalsModel->item(1)->setToolTip(tr("Exports surface normals as attributes."));
    normalsModel->item(2)->setToolTip(tr("Surface normals are not exported."));
    ui->NormalsComboBox->setCurrentIndex(
        static_cast<int>(buildOptions.GetMeshConversionOptions().GetNormalMode()));
    ui->NormalsComboBox->setToolTip(
        normalsModel
            ->item(static_cast<int>(buildOptions.GetMeshConversionOptions().GetNormalMode()))
            ->toolTip());

    IdentifyCurrentVertexChannelMapping();
    ui->VertexChannelsComboBox->setCurrentIndex(static_cast<int>(currentChannelMappingType));
    if (currentChannelMappingType == ChannelMappingType::Custom) {
        customChannelMappings = buildOptions.GetMeshConversionOptions().GetChannelMappings();
    } else {
        MaxUsd::MaxMeshConversionOptions defaultOptions;
        customChannelMappings = defaultOptions.GetChannelMappings();
    }
    ui->VertexChannelsToolButton->setEnabled(
        currentChannelMappingType == ChannelMappingType::Custom);
    ui->VertexChannelsToolButton->setIcon(MaxSDK::LoadMaxMultiResIcon("Common/Settings"));

    ui->HiddenObjectsGroupBox->setChecked(buildOptions.GetTranslateHidden());
    MaxSDK::QmaxToolClips::disableToolClip(ui->HiddenObjectsGroupBox);
    ui->UseUsdVisibilityCheckBox->setChecked(buildOptions.GetUseUSDVisibility());

    // filter tootips for the following ui elements
    auto tooltipFilterFunction = [this](QObject* o) { QToolTip::hideText(); };
    tooltipFilter = std::make_unique<MaxUsd::TooltipEventFilter>(tooltipFilterFunction);
    ui->VertexChannelsComboBox->installEventFilter(tooltipFilter.get());
    ui->VertexChannelsToolButton->installEventFilter(tooltipFilter.get());
}

UsdExportIncludeRollup::~UsdExportIncludeRollup() { }

void UsdExportIncludeRollup::on_CamerasCheckBox_stateChanged(int state)
{
    buildOptions.SetTranslateCameras(state == Qt::Checked);
}

void UsdExportIncludeRollup::on_LightsCheckBox_stateChanged(int state)
{
    buildOptions.SetTranslateLights(state == Qt::Checked);
}

void UsdExportIncludeRollup::on_ShapesCheckBox_stateChanged(int state)
{
    buildOptions.SetTranslateShapes(state == Qt::Checked);
}

void UsdExportIncludeRollup::on_SkinCheckBox_stateChanged(int state)
{
    buildOptions.SetTranslateSkin(state == Qt::Checked);
}

void UsdExportIncludeRollup::on_UsdStagesCheckBox_stateChanged(int state)
{
    buildOptions.SetUsdStagesAsReferences(state == Qt::Checked);
}

void UsdExportIncludeRollup::on_GeometryGroupBox_toggled(bool state)
{
    buildOptions.SetTranslateMeshes(state);

    // disable/enable groupbox's content
    ui->MeshFormatLabel->setEnabled(state);
    ui->MeshFormatComboBox->setEnabled(state);
    ui->PreserveEdgeOrientationCheckBox->setEnabled(state);
    ui->BakeOffsetTransformCheckBox->setEnabled(state);
    ui->NormalsLabel->setEnabled(state);
    ui->NormalsComboBox->setEnabled(state);
    ui->VertexChannelsLabel->setEnabled(state);
    ui->VertexChannelsComboBox->setEnabled(state);
}

void UsdExportIncludeRollup::on_MeshFormatComboBox_currentIndexChanged(int index)
{
    buildOptions.SetMeshFormat(static_cast<MaxUsd::MaxMeshConversionOptions::MeshFormat>(index));
    if (auto* meshModel = dynamic_cast<QStandardItemModel*>(ui->MeshFormatComboBox->model())) {
        ui->MeshFormatComboBox->setToolTip(meshModel->item(index)->toolTip());
    }
}

void UsdExportIncludeRollup::on_PreserveEdgeOrientationCheckBox_stateChanged(int state)
{
    auto meshOptions = buildOptions.GetMeshConversionOptions();
    meshOptions.SetPreserveEdgeOrientation(state == Qt::Checked);
    buildOptions.SetMeshConversionOptions(meshOptions);
}

void UsdExportIncludeRollup::on_BakeOffsetTransformCheckBox_stateChanged(int state)
{
    auto meshOptions = buildOptions.GetMeshConversionOptions();
    meshOptions.SetBakeObjectOffsetTransform(state == Qt::Checked);
    buildOptions.SetMeshConversionOptions(meshOptions);
}

void UsdExportIncludeRollup::on_NormalsComboBox_currentIndexChanged(int index)
{
    buildOptions.SetNormalsMode(static_cast<MaxUsd::MaxMeshConversionOptions::NormalsMode>(index));
    if (auto* normalsModel = dynamic_cast<QStandardItemModel*>(ui->NormalsComboBox->model())) {
        ui->NormalsComboBox->setToolTip(normalsModel->item(index)->toolTip());
    }
}

void UsdExportIncludeRollup::on_VertexChannelsComboBox_currentIndexChanged(int index)
{
    // when leaving custom vertex channel type
    // save custom channel mappings for later
    if (currentChannelMappingType == ChannelMappingType::Custom) {
        customChannelMappings = buildOptions.GetMeshConversionOptions().GetChannelMappings();
    }

    auto meshOptions = buildOptions.GetMeshConversionOptions();
    switch (index) {
    case 0: // all
    {
        meshOptions.SetDefaultChannelPrimvarMappings();
        break;
    }
    case 1: // none
    {
        const MaxUsd::MaxMeshConversionOptions defaultOptions;
        for (int channel = -NUM_HIDDENMAPS; channel < MAX_MESHMAPS; ++channel) {
            meshOptions.SetChannelPrimvarConfig(
                channel,
                { pxr::TfToken(),
                  defaultOptions.GetChannelPrimvarConfig(channel).GetPrimvarType() });
        }
        break;
    }
    case 2: // custom
    {
        if (!customChannelMappings.empty()) {
            meshOptions.SetChannelMappings(customChannelMappings);
        }
        break;
    }
    }
    buildOptions.SetMeshConversionOptions(meshOptions);
    currentChannelMappingType = static_cast<ChannelMappingType>(index);
    ui->VertexChannelsToolButton->setEnabled(
        currentChannelMappingType == ChannelMappingType::Custom);
}

void UsdExportIncludeRollup::on_VertexChannelsToolButton_clicked()
{
    USDExportCustomChannelMappingsDialog mapDetailsDialog { this->buildOptions };
}

void UsdExportIncludeRollup::on_HiddenObjectsGroupBox_toggled(bool state)
{
    buildOptions.SetTranslateHidden(state);
    // disable/enable groupbox's content
    ui->UseUsdVisibilityCheckBox->setEnabled(state);
}

void UsdExportIncludeRollup::on_UseUsdVisibilityCheckBox_stateChanged(int state)
{
    buildOptions.SetUseUSDVisibility(state == Qt::Checked);
}

void UsdExportIncludeRollup::IdentifyCurrentVertexChannelMapping()
{
    // Figure out which channel mapping is current.
    // 1) Options exactly defaults -> "All".
    // 2) Options exactly defaults, except all disabled -> "None".
    // 3) Anything else -> "Custom".
    MaxUsd::MaxMeshConversionOptions defaultOptions;
    auto                             currentOptions = buildOptions.GetMeshConversionOptions();

    bool isAll = true;
    bool isNone = true;

    for (const auto& mapping : defaultOptions.GetChannelMappings()) {
        int  chanelID = std::stoi(mapping.first);
        auto defaultConfig = defaultOptions.GetChannelPrimvarConfig(chanelID);
        auto config = currentOptions.GetChannelPrimvarConfig(chanelID);
        if (config.GetPrimvarName().IsEmpty()) {
            isAll = false;
        } else {
            isNone = false;
            // Make sure the target primvar name is the default...
            if (config.GetPrimvarName() != defaultConfig.GetPrimvarName()) {
                isAll = false;
            }
        }

        // For both "All" and "None", the primvar type must be the default one.
        if (config.GetPrimvarType() != defaultConfig.GetPrimvarType()) {
            isAll = false;
            isNone = false;
        }

        if (!isAll && !isNone) {
            break; // Means we need to use custom.
        }
    }

    currentChannelMappingType
        = (isAll        ? ChannelMappingType::All
               : isNone ? ChannelMappingType::None
                        : ChannelMappingType::Custom);
}
