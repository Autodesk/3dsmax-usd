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
#include "UsdStageViewportPerformanceRollup.h"

#include "Ui_UsdStageViewportPerformanceRollup.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <GetCOREInterface.h>
#include <iparamb2.h>
#include <maxapi.h>

using namespace MaxSDK;

UsdStageViewportPerformanceRollup::UsdStageViewportPerformanceRollup(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock)
    : ui(new Ui::UsdStageViewportPerformanceRollup)
{
    SetParamBlock((ReferenceMaker*)&owner, (IParamBlock2*)&paramBlock);

    ui->setupUi(this);
    modelObj = static_cast<USDStageObject*>(&owner);

    ui->gridLayout_2->setContentsMargins(QMargins(0, MaxSDK::UIScaled(11), 0, MaxSDK::UIScaled(9)));

    ui->MeshMergeMode->addItem(tr("Static"), static_cast<int>(HdMaxConsolidator::Strategy::Static));
    ui->MeshMergeMode->addItem(
        tr("Dynamic"), static_cast<int>(HdMaxConsolidator::Strategy::Dynamic));
    ui->MeshMergeMode->addItem(tr("Off"), static_cast<int>(HdMaxConsolidator::Strategy::Off));
}

UsdStageViewportPerformanceRollup::~UsdStageViewportPerformanceRollup() { }

void UsdStageViewportPerformanceRollup::SetParamBlock(
    ReferenceMaker*     owner,
    IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    modelObj = static_cast<USDStageObject*>(owner);
}

void UsdStageViewportPerformanceRollup::UpdateUI(const TimeValue t)
{
    UpdateParameterUI(GetCOREInterface()->GetTime(), MeshMergeMode, t);
}

void UsdStageViewportPerformanceRollup::UpdateParameterUI(
    const TimeValue t,
    const ParamID   paramId,
    const int /*tabIndex*/)
{
    if (MeshMergeMode == paramId) {

        int      value = 0;
        Interval valid;
        paramBlock->GetValue(paramId, GetCOREInterface()->GetTime(), value, valid);
        ui->MeshMergeMode->setCurrentIndex(value);
    }
}