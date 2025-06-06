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
#include "UsdStageMetadataRollup.h"

#include "ui_UsdStageMetadataRollup.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/MathUtils.h>

// max sdk
#include <Qt/QmaxToolClips.h>

#include <GetCOREInterface.h>
#include <iparamb2.h>
#include <maxapi.h>

using namespace MaxSDK;

UsdStageMetadataRollup::UsdStageMetadataRollup(ReferenceMaker& owner, IParamBlock2& paramBlock)
    : ui(new Ui::UsdStageMetadataRollup)
{
    SetParamBlock((ReferenceMaker*)&owner, (IParamBlock2*)&paramBlock);

    ui->setupUi(this);

    modelObj = static_cast<USDStageObject*>(&owner);
}

void UsdStageMetadataRollup::SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    modelObj = static_cast<USDStageObject*>(owner);
}

void UsdStageMetadataRollup::UpdateUI(const TimeValue t)
{
    auto stage = modelObj->GetUSDStage();
    if (stage) {
        FLOAT    sourceMPU = 0;
        Interval valid = FOREVER;
        paramBlock->GetValue(
            PBParameterIds::SourceMetersPerUnit, GetCOREInterface()->GetTime(), sourceMPU, valid);
        double mpu = MaxUsd::MathUtils::RoundToSignificantDigit(sourceMPU, 5);
        ui->SourceMetersPerUnit->setText(QString::number(mpu));
    } else {
        ui->SourceMetersPerUnit->setText("N/A");
    }
}

void UsdStageMetadataRollup::UpdateParameterUI(
    const TimeValue t,
    const ParamID   paramId,
    const int /*tabIndex*/)
{
    if (paramId == SourceMetersPerUnit) {
        FLOAT    sourceMPU = 0;
        Interval valid = FOREVER;
        paramBlock->GetValue(
            PBParameterIds::SourceMetersPerUnit, GetCOREInterface()->GetTime(), sourceMPU, valid);

        double mpu = MaxUsd::MathUtils::RoundToSignificantDigit(sourceMPU, 5);
        ui->SourceMetersPerUnit->setText(QString::number(mpu));
    }
}