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

#include <QtWidgets/QWidget>

namespace Ui {
class UsdExportAnimationRollup;
}

class QCheckBox;

class UsdExportAnimationRollup : public QWidget
{
    Q_OBJECT
public:
    explicit UsdExportAnimationRollup(
        MaxUsd::USDSceneBuilderOptions& buildOptions,
        QWidget*                        parent = nullptr);
    virtual ~UsdExportAnimationRollup();

    // called when closing the dialog to properly set the build option values
    // from the user selections; specifically for the frame range selections
    void SaveDialogState();

public Q_SLOTS:
    /// Qt callback functions based on named widgets in the associated .ui file
    void on_AnimationRangeRadioButton_clicked(bool checked);
    void on_CurrentFrameRadioButton_clicked(bool checked);
    void on_FrameNumberRadioButton_clicked(bool checked);
    void on_FrameRangeRadioButton_clicked(bool checked);

    void on_FrameNumberDoubleSpinBox_valueChanged(double value);
    void on_SamplePerFrameDoubleSpinBox_valueChanged(double value);

    void on_SkinCheckBox_stateChanged(int state);
    void on_MorpherCheckBox_stateChanged(int state);

    // made those explicit instead of relying on the automatically generated connection
    // base on the widgets names; the 'setMinimum' emits 'valueChanged' on those otherwise
    void OnFrameRangeStartValueChanged(double value);
    void OnFrameRangeEndValueChanged(double value);

private:
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdExportAnimationRollup> ui {
        std::make_unique<Ui::UsdExportAnimationRollup>()
    };

    /// member of USDExportDialog
    MaxUsd::USDSceneBuilderOptions& buildOptions;

    // dialog state; not all elements can be ruled thru the builder options
    MaxUsd::USDSceneBuilderOptions::AnimationRollupData animationRollupData;

    void SetWidgetsState();
};