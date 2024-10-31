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
#pragma once

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>

#include <MaxUsd.h>
#include <QtWidgets/QWidget>

namespace Ui {
class UsdExportIncludeRollup;
}

namespace MAXUSD_NS_DEF {
class TooltipEventFilter;
}

class UsdExportIncludeRollup : public QWidget
{
    Q_OBJECT
public:
    explicit UsdExportIncludeRollup(
        MaxUsd::USDSceneBuilderOptions& buildOptions,
        QWidget*                        parent = nullptr);
    virtual ~UsdExportIncludeRollup();

public Q_SLOTS:
    /// Qt callback functions based on named widgets in the associated .ui file
    void on_CamerasCheckBox_stateChanged(int state);
    void on_LightsCheckBox_stateChanged(int state);
    void on_ShapesCheckBox_stateChanged(int state);
    void on_SkinCheckBox_stateChanged(int state);
    void on_UsdStagesCheckBox_stateChanged(int state);

    void on_GeometryGroupBox_toggled(bool state);
    void on_MeshFormatComboBox_currentIndexChanged(int index);
    void on_PreserveEdgeOrientationCheckBox_stateChanged(int state);
    void on_BakeOffsetTransformCheckBox_stateChanged(int state);
    void on_NormalsComboBox_currentIndexChanged(int index);
    void on_VertexChannelsComboBox_currentIndexChanged(int index);
    void on_VertexChannelsToolButton_clicked();

    void on_HiddenObjectsGroupBox_toggled(bool state);
    void on_UseUsdVisibilityCheckBox_stateChanged(int state);

private:
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdExportIncludeRollup> ui {
        std::make_unique<Ui::UsdExportIncludeRollup>()
    };

    /// member of USDExportDialog
    MaxUsd::USDSceneBuilderOptions& buildOptions;

    std::unique_ptr<MaxUsd::TooltipEventFilter> tooltipFilter;

    /// Simple enum to keep track of the current channel mapping
    /// used to act to specific changes and properly handle custom settings
    enum class ChannelMappingType
    {
        All,
        None,
        Custom
    };
    ChannelMappingType currentChannelMappingType;
    /// Custom channel mapping setup via the "Configure..." button. We keep this around in case the
    /// the user toggles back and forth, so the the customization is not lost.
    pxr::VtDictionary customChannelMappings;

    void IdentifyCurrentVertexChannelMapping();
};