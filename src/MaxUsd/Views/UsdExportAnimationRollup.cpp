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
#include "UsdExportAnimationRollup.h"

#include "ui_UsdExportAnimationRollup.h"

#include <MaxUsd/Translators/ShadingModeRegistry.h>

#include <QtWidgets/QCheckBox>
#include <units.h>

UsdExportAnimationRollup::UsdExportAnimationRollup(
    MaxUsd::USDSceneBuilderOptions& buildOptions,
    QWidget*                        parent /* = nullptr */)
    : ui(new Ui::UsdExportAnimationRollup)
    , buildOptions(buildOptions)
{
    ui->setupUi(this);

    ui->SkinCheckBox->setChecked(buildOptions.GetTranslateSkin());
    ui->MorpherCheckBox->setChecked(buildOptions.GetTranslateMorpher());

    const auto timeMode = buildOptions.GetTimeMode();
    switch (timeMode) {
    case MaxUsd::USDSceneBuilderOptions::TimeMode::AnimationRange:
        ui->AnimationRangeRadioButton->setChecked(true);
        break;
    case MaxUsd::USDSceneBuilderOptions::TimeMode::CurrentFrame:
        ui->CurrentFrameRadioButton->setChecked(true);
        break;
    case MaxUsd::USDSceneBuilderOptions::TimeMode::ExplicitFrame:
        ui->FrameNumberRadioButton->setChecked(true);
        break;
    case MaxUsd::USDSceneBuilderOptions::TimeMode::FrameRange:
        ui->FrameRangeRadioButton->setChecked(true);
        break;
    }

    ui->FrameNumberDoubleSpinBox->setMinimum(-DBL_MAX);
    ui->FrameNumberDoubleSpinBox->setMaximum(DBL_MAX);
    ui->FrameRangeStartDoubleSpinBox->setMinimum(-DBL_MAX);
    ui->FrameRangeStartDoubleSpinBox->setMaximum(DBL_MAX);
    ui->FrameRangeEndDoubleSpinBox->setMinimum(-DBL_MAX);
    ui->FrameRangeEndDoubleSpinBox->setMaximum(DBL_MAX);

    // USDSceneBuilderOptions::TimeMode::AnimationRange:
    double startFrame = GetCOREInterface()->GetAnimRange().Start() / GetTicksPerFrame();
    double endFrame = GetCOREInterface()->GetAnimRange().End() / GetTicksPerFrame();
    ui->AnimationRangeLabel->setText(QString::asprintf("%.0lf - %.0lf", startFrame, endFrame));
    // USDSceneBuilderOptions::TimeMode::CurrentFrame:
    double currentFrame = GetCOREInterface()->GetTime() / GetTicksPerFrame();
    ui->CurrentFrameLabel->setText(QString::asprintf("%.0lf", currentFrame));

    // properly set frame numbers based on scene settings
    // if no user settings were applied already or
    // on build options that might have been set thru scripting
    const auto timeConfig = buildOptions.GetResolvedTimeConfig();
    buildOptions.FetchAnimationRollupData(animationRollupData);

    if (animationRollupData.frameNumberDefault) {
        if (timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::ExplicitFrame) {
            // the default time mode is CurrentFrame
            // if the dialog initializes to ExplicitFrame without applied defaults
            // those most have been set thru scripting
            animationRollupData.frameNumberDefault = false;
            animationRollupData.frameNumber = timeConfig.GetStartFrame();
        } else {
            animationRollupData.frameNumber = currentFrame;
        }
    }
    if (animationRollupData.frameRangeDefault) {
        if (timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::FrameRange) {
            // the default time mode is CurrentRange
            // if the dialog initializes to FrameRange without applied defaults
            // those most have been set thru scripting
            animationRollupData.frameRangeDefault = false;
            animationRollupData.frameRangeStart = timeConfig.GetStartFrame();
            animationRollupData.frameRangeEnd = timeConfig.GetEndFrame();
        } else {
            animationRollupData.frameRangeStart = startFrame;
            animationRollupData.frameRangeEnd = endFrame;
        }
    }
    // in case the settings were badly set by scripting
    // make sure the frame range is a valid one
    if (timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::FrameRange
        && animationRollupData.frameRangeEnd < animationRollupData.frameRangeStart) {
        animationRollupData.frameRangeEnd = animationRollupData.frameRangeStart;
    }
    // USDSceneBuilderOptions::TimeMode::ExplicitFrame
    ui->FrameNumberDoubleSpinBox->setValue(animationRollupData.frameNumber);
    ui->FrameNumberDoubleSpinBox->setResetValue(currentFrame); // for ctrl-RMB
    // USDSceneBuilderOptions::TimeMode::FrameRange:
    ui->FrameRangeStartDoubleSpinBox->setValue(animationRollupData.frameRangeStart);
    ui->FrameRangeStartDoubleSpinBox->setResetValue(startFrame); // for ctrl-RMB
    ui->FrameRangeEndDoubleSpinBox->setValue(animationRollupData.frameRangeEnd);
    ui->FrameRangeEndDoubleSpinBox->setResetValue(endFrame); // for ctrl-RMB
    ui->FrameRangeEndDoubleSpinBox->setMinimum(animationRollupData.frameRangeStart);

    ui->SamplePerFrameDoubleSpinBox->setValue(buildOptions.GetSamplesPerFrame());
    ui->SamplePerFrameDoubleSpinBox->setResetValue(
        buildOptions.GetSamplesPerFrame()); // for ctrl-RMB
    SetWidgetsState();

    // made those explicit instead of relying on the automatically generated connection
    // base on the widgets names; the 'setMinimum' emits 'valueChanged' on those otherwise
    connect(
        ui->FrameRangeStartDoubleSpinBox,
        static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
        this,
        &UsdExportAnimationRollup::OnFrameRangeStartValueChanged);
    connect(
        ui->FrameRangeEndDoubleSpinBox,
        static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
        this,
        &UsdExportAnimationRollup::OnFrameRangeEndValueChanged);
}

UsdExportAnimationRollup::~UsdExportAnimationRollup() { }

void UsdExportAnimationRollup::on_AnimationRangeRadioButton_clicked(bool checked)
{
    buildOptions.SetTimeMode(MaxUsd::USDSceneBuilderOptions::TimeMode::AnimationRange);
    SetWidgetsState();
}

void UsdExportAnimationRollup::on_CurrentFrameRadioButton_clicked(bool checked)
{
    buildOptions.SetTimeMode(MaxUsd::USDSceneBuilderOptions::TimeMode::CurrentFrame);
    SetWidgetsState();
}

void UsdExportAnimationRollup::on_FrameNumberRadioButton_clicked(bool checked)
{
    buildOptions.SetTimeMode(MaxUsd::USDSceneBuilderOptions::TimeMode::ExplicitFrame);
    SetWidgetsState();
}

void UsdExportAnimationRollup::on_FrameRangeRadioButton_clicked(bool checked)
{
    buildOptions.SetTimeMode(MaxUsd::USDSceneBuilderOptions::TimeMode::FrameRange);
    SetWidgetsState();
}

void UsdExportAnimationRollup::on_FrameNumberDoubleSpinBox_valueChanged(double value)
{
    if (value != animationRollupData.frameNumber) {
        animationRollupData.frameNumber = value;
        if (animationRollupData.frameNumberDefault) {
            animationRollupData.frameNumberDefault = false;
        }
    }
}

void UsdExportAnimationRollup::OnFrameRangeStartValueChanged(double value)
{
    if (value != animationRollupData.frameRangeStart) {
        animationRollupData.frameRangeStart = value;
        if (animationRollupData.frameRangeDefault) {
            animationRollupData.frameRangeDefault = false;
        }
    }
    ui->FrameRangeEndDoubleSpinBox->setMinimum(value);
}

void UsdExportAnimationRollup::OnFrameRangeEndValueChanged(double value)
{
    if (value != animationRollupData.frameRangeEnd) {
        animationRollupData.frameRangeEnd = value;
        if (animationRollupData.frameRangeDefault) {
            animationRollupData.frameRangeDefault = false;
        }
    }
}

void UsdExportAnimationRollup::on_SamplePerFrameDoubleSpinBox_valueChanged(double value)
{
    buildOptions.SetSamplesPerFrame(value);
}

void UsdExportAnimationRollup::on_SkinCheckBox_stateChanged(int state)
{
    buildOptions.SetTranslateSkin(state == Qt::Checked);
}

void UsdExportAnimationRollup::on_MorpherCheckBox_stateChanged(int state)
{
    buildOptions.SetTranslateMorpher(state == Qt::Checked);
}

void UsdExportAnimationRollup::SetWidgetsState()
{
    const auto timeMode = buildOptions.GetTimeMode();

    ui->AnimationRangeLabel->setEnabled(
        timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::AnimationRange);
    ui->CurrentFrameLabel->setEnabled(
        timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::CurrentFrame);

    ui->FrameNumberDoubleSpinBox->setEnabled(
        timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::ExplicitFrame);

    ui->FrameRangeStartDoubleSpinBox->setEnabled(
        timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::FrameRange);
    ui->FrameRangeEndDoubleSpinBox->setEnabled(
        timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::FrameRange);

    ui->SamplesPerFrameLabel->setEnabled(
        timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::AnimationRange
        || timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::FrameRange);
    ui->SamplePerFrameDoubleSpinBox->setEnabled(
        timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::AnimationRange
        || timeMode == MaxUsd::USDSceneBuilderOptions::TimeMode::FrameRange);
}

void UsdExportAnimationRollup::SaveDialogState()
{
    buildOptions.SaveAnimationRollupData(animationRollupData);

    const auto timeMode = buildOptions.GetTimeMode();
    switch (timeMode) {
    case MaxUsd::USDSceneBuilderOptions::TimeMode::ExplicitFrame:
        buildOptions.SetStartFrame(animationRollupData.frameNumber);
        break;
    case MaxUsd::USDSceneBuilderOptions::TimeMode::FrameRange:
        buildOptions.SetStartFrame(animationRollupData.frameRangeStart);
        buildOptions.SetEndFrame(animationRollupData.frameRangeEnd);
        break;
    }
}