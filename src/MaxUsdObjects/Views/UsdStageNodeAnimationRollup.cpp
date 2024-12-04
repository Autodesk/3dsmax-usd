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
#include "UsdStageNodeAnimationRollup.h"

#include "ui_UsdStageNodeAnimationRollup.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/MathUtils.h>

#include <Qt/QmaxToolClips.h>

using namespace MaxSDK;

UsdStageNodeAnimationRollup::UsdStageNodeAnimationRollup(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock)
    : ui(new Ui::UsdStageNodeAnimationRollup)
{
    SetParamBlock((ReferenceMaker*)&owner, (IParamBlock2*)&paramBlock);

    ui->setupUi(this);
    modelObj = static_cast<USDStageObject*>(&owner);

    // Disable 3dsMax's tooltips for group boxes because the tooltips still not working properly
    MaxSDK::QmaxToolClips::disableToolClip(ui->PlaybackTypeGroupBox);
    MaxSDK::QmaxToolClips::disableToolClip(ui->MaxAnimationDataGroupBox);
    MaxSDK::QmaxToolClips::disableToolClip(ui->SourceAnimationDataGroupBox);

    ui->AnimationMode->addItem(tr("Original Range"), OriginalRange);
    ui->AnimationMode->addItem(tr("Custom Start & Speed"), CustomStartAndSpeed);
    ui->AnimationMode->addItem(tr("Custom Range"), CustomRange);
    ui->AnimationMode->addItem(tr("Custom TimeCode Playback"), CustomTimeCodePlayback);

    connect(
        ui->AnimationMode,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        [this](int index) {
            // The following condition depends on the fact that the combobox widget entries
            // are ordered in the same way as the AnimationMode enum
            if (index == AnimationMode::OriginalRange) {
                ui->StartFrameLabel->setEnabled(false);
                ui->CustomAnimationStartFrame->setEnabled(false);
                ui->SpeedLabel->setEnabled(false);
                ui->CustomAnimationSpeed->setEnabled(false);
                ui->EndFrameLabel->setEnabled(false);
                ui->CustomAnimationEndFrame->setEnabled(false);
                ui->UsdTimecodeLabel->setEnabled(false);
                ui->CustomAnimationPlaybackTimecode->setEnabled(false);
                ui->ClampFramesLabel->setEnabled(false);
                ui->ClampFrames->setEnabled(false);
            } else if (index == AnimationMode::CustomStartAndSpeed) {
                ui->StartFrameLabel->setEnabled(true);
                ui->CustomAnimationStartFrame->setEnabled(true);
                ui->SpeedLabel->setEnabled(true);
                ui->CustomAnimationSpeed->setEnabled(true);
                ui->EndFrameLabel->setEnabled(false);
                ui->CustomAnimationEndFrame->setEnabled(false);
                ui->UsdTimecodeLabel->setEnabled(false);
                ui->CustomAnimationPlaybackTimecode->setEnabled(false);
                ui->ClampFramesLabel->setEnabled(false);
                ui->ClampFrames->setEnabled(false);
            } else if (index == AnimationMode::CustomRange) {
                ui->StartFrameLabel->setEnabled(true);
                ui->CustomAnimationStartFrame->setEnabled(true);
                ui->SpeedLabel->setEnabled(false);
                ui->CustomAnimationSpeed->setEnabled(false);
                ui->EndFrameLabel->setEnabled(true);
                ui->CustomAnimationEndFrame->setEnabled(true);
                ui->UsdTimecodeLabel->setEnabled(false);
                ui->CustomAnimationPlaybackTimecode->setEnabled(false);
                ui->ClampFramesLabel->setEnabled(false);
                ui->ClampFrames->setEnabled(false);
            } else if (index == AnimationMode::CustomTimeCodePlayback) {
                ui->StartFrameLabel->setEnabled(false);
                ui->CustomAnimationStartFrame->setEnabled(false);
                ui->SpeedLabel->setEnabled(false);
                ui->CustomAnimationSpeed->setEnabled(false);
                ui->EndFrameLabel->setEnabled(false);
                ui->CustomAnimationEndFrame->setEnabled(false);
                ui->UsdTimecodeLabel->setEnabled(true);
                ui->CustomAnimationPlaybackTimecode->setEnabled(true);
                ui->ClampFramesLabel->setEnabled(true);
                ui->ClampFrames->setEnabled(true);
            }
        });
}

UsdStageNodeAnimationRollup::~UsdStageNodeAnimationRollup() { }

void UsdStageNodeAnimationRollup::SetParamBlock(
    ReferenceMaker*     owner,
    IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    modelObj = static_cast<USDStageObject*>(owner);
}

void UsdStageNodeAnimationRollup::UpdateUI(const TimeValue t)
{
    auto stage = modelObj->GetUSDStage();
    if (stage) {
        float    fVal = 0;
        Interval valid = FOREVER;
        paramBlock->GetValue(
            PBParameterIds::MaxAnimationStartFrame, GetCOREInterface()->GetTime(), fVal, valid);
        ui->MaxAnimationStartFrame->setText(
            QString::number(MaxUsd::MathUtils::RoundToSignificantDigit(fVal, 4)));

        paramBlock->GetValue(
            PBParameterIds::MaxAnimationEndFrame, GetCOREInterface()->GetTime(), fVal, valid);
        ui->MaxAnimationEndFrame->setText(
            QString::number(MaxUsd::MathUtils::RoundToSignificantDigit(fVal, 4)));

        ui->MaxAnimationFPS->setText(QString::number(GetFrameRate()));

        paramBlock->GetValue(
            PBParameterIds::SourceAnimationStartTimeCode,
            GetCOREInterface()->GetTime(),
            fVal,
            valid);
        ui->SourceAnimationStartTimeCode->setText(
            QString::number(MaxUsd::MathUtils::RoundToSignificantDigit(fVal, 4)));

        paramBlock->GetValue(
            PBParameterIds::SourceAnimationEndTimeCode, GetCOREInterface()->GetTime(), fVal, valid);
        ui->SourceAnimationEndTimeCode->setText(
            QString::number(MaxUsd::MathUtils::RoundToSignificantDigit(fVal, 4)));

        paramBlock->GetValue(
            PBParameterIds::SourceAnimationTPS, GetCOREInterface()->GetTime(), fVal, valid);
        ui->SourceAnimationTPS->setText(
            QString::number(MaxUsd::MathUtils::RoundToSignificantDigit(fVal, 4)));
    } else {
        ui->MaxAnimationStartFrame->setText("N/A");
        ui->MaxAnimationEndFrame->setText("N/A");
        ui->MaxAnimationFPS->setText("N/A");
        ui->SourceAnimationStartTimeCode->setText("N/A");
        ui->SourceAnimationEndTimeCode->setText("N/A");
        ui->SourceAnimationTPS->setText("N/A");
    }
    UpdateParameterUI(GetCOREInterface()->GetTime(), AnimationMode, t);
}

void UsdStageNodeAnimationRollup::UpdateParameterUI(
    const TimeValue t,
    const ParamID   paramId,
    const int /*tabIndex*/)
{
    if (AnimationMode == paramId) {
        int      value = 0;
        Interval valid;
        paramBlock->GetValue(paramId, GetCOREInterface()->GetTime(), value, valid);
        ui->AnimationMode->setCurrentIndex(value);
    } else if (MaxAnimationEndFrame == paramId) {
        float    value = 0;
        Interval valid = FOREVER;
        paramBlock->GetValue(paramId, GetCOREInterface()->GetTime(), value, valid);

        double endFrame = MaxUsd::MathUtils::RoundToSignificantDigit(value, 4);
        ui->MaxAnimationEndFrame->setText(QString::number(endFrame));
        ui->MaxAnimationFPS->setText(QString::number(GetFrameRate()));
    } else if (MaxAnimationStartFrame == paramId) {
        float    value = 0;
        Interval valid = FOREVER;
        paramBlock->GetValue(paramId, GetCOREInterface()->GetTime(), value, valid);

        double startFrame = MaxUsd::MathUtils::RoundToSignificantDigit(value, 4);
        ui->MaxAnimationStartFrame->setText(QString::number(startFrame));
        ui->MaxAnimationFPS->setText(QString::number(GetFrameRate()));
    }
}