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
#include "UsdStageViewportDisplayRollup.h"

#include "Ui_UsdStageViewportDisplayRollup.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <iparamb2.h>
#include <maxicon.h>

using namespace MaxSDK;

UsdStageViewportDisplayRollup::UsdStageViewportDisplayRollup(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock)
    : ui(new Ui::UsdStageViewportDisplayRollup)
{
    SetParamBlock((ReferenceMaker*)&owner, (IParamBlock2*)&paramBlock);

    ui->setupUi(this);
    modelObj = static_cast<USDStageObject*>(&owner);

    ui->DisplayMode->addItem(
        tr("3ds Max Wire Color"), HdMaxDisplaySettings::DisplayMode::WireColor);
    ui->DisplayMode->addItem(
        tr("USD Display Color"), HdMaxDisplaySettings::DisplayMode::USDDisplayColor);
    ui->DisplayMode->addItem(
        tr("USD Preview Surface"), HdMaxDisplaySettings::DisplayMode::USDPreviewSurface);

    ui->PointInstancesDrawModesRebuildButton->setIcon(
        MaxSDK::LoadMaxMultiResIcon(MSTR(_T("StateSets\\Refresh.png"))));
    const int iconSize = MaxSDK::UIScaled(16);
    ui->PointInstancesDrawModesRebuildButton->setIconSize(QSize(iconSize, iconSize));

    ui->PointInstancesDrawMode->addItem(tr("Default"), 0);
    ui->PointInstancesDrawMode->addItem(tr("Cards (Box)"), 1);
    ui->PointInstancesDrawMode->addItem(tr("Cards (Cross)"), 2);
}

UsdStageViewportDisplayRollup::~UsdStageViewportDisplayRollup() { }

void UsdStageViewportDisplayRollup::SetParamBlock(
    ReferenceMaker*     owner,
    IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    modelObj = static_cast<USDStageObject*>(owner);
}

void UsdStageViewportDisplayRollup::UpdateUI(const TimeValue t)
{
    UpdateParameterUI(GetCOREInterface()->GetTime(), DisplayMode, t);
}

void UsdStageViewportDisplayRollup::UpdateParameterUI(
    const TimeValue t,
    const ParamID   paramId,
    const int /*tabIndex*/)
{
    if (DisplayMode == paramId) {
        int      value = 0;
        Interval valid;
        paramBlock->GetValue(paramId, GetCOREInterface()->GetTime(), value, valid);
        ui->DisplayMode->setCurrentIndex(value);
    }

    if (PointInstancesDrawMode == paramId) {
        int      value = 0;
        Interval valid;
        paramBlock->GetValue(paramId, GetCOREInterface()->GetTime(), value, valid);
        ui->PointInstancesDrawMode->setCurrentIndex(value);
    }
}

void UsdStageViewportDisplayRollup::on_InvertProxyRenderButton_clicked()
{
    // Toggle the checked state of the DisplayProxy and DisplayRender checkboxes
    theHold.SuperBegin();
    ui->DisplayProxy->setChecked(!ui->DisplayProxy->isChecked());
    ui->DisplayRender->setChecked(!ui->DisplayRender->isChecked());
    theHold.SuperAccept(_T("Invert Display Purpose"));
}

void UsdStageViewportDisplayRollup::on_PointInstancesDrawModesRebuildButton_clicked() const
{
    modelObj->GenerateDrawModes();
}