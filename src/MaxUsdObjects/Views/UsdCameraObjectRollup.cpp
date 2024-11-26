//
// Copyright 2024 Autodesk
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
#include "UsdCameraObjectRollup.h"

#include "ui_UsdCameraObjectRollup.h"

#include <MaxUsdObjects/Objects/USDCameraObject.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <Qt/QmaxToolClips.h>
#include <maxscript/maxscript.h>

#include <iparamb2.h>
#include <maxicon.h>

UsdCameraObjectRollup::UsdCameraObjectRollup(ReferenceMaker& owner, IParamBlock2& paramBlock)
    : ui(new Ui::UsdCameraObjectRollup)
{
    SetParamBlock(&owner, &paramBlock);
    cameraObject = static_cast<USDCameraObject*>(&owner);
    ui->setupUi(this);

    ui->PathLabel->setMinimumHeight(MaxSDK::UIScaled(20));
    ui->PathLabel->setAlignment({ Qt::AlignHCenter | Qt::AlignVCenter });
    // 3dsMax tooltip styling doesn't handle long strings very well.
    MaxSDK::QmaxToolClips::disableToolClip(ui->PathLabel);
}

void UsdCameraObjectRollup::SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    cameraObject = static_cast<USDCameraObject*>(owner);
}

void UsdCameraObjectRollup::UpdateUI(const TimeValue t)
{
    static QString NA = QObject::tr("N/A");

    auto fillWithNA = [this]() {
        ui->PathLabel->setText(NA);
        ui->ProjectionValue->setText(NA);
        ui->HorizontalApertureValue->setText(NA);
        ui->HorizontalApertureOffsetValue->setText(NA);
        ui->VerticalApertureValue->setText(NA);
        ui->VerticalApertureOffsetValue->setText(NA);
        ui->FocalLengthValue->setText(NA);
        ui->ClippingNearValue->setText(NA);
        ui->ClippingFarValue->setText(NA);
        ui->FStopValue->setText(NA);
        ui->FocusDistanceValue->setText(NA);
        ui->StereoRoleValue->setText(NA);
        ui->ShutterOpenValue->setText(NA);
        ui->ShutterCloseValue->setText(NA);
        ui->ExposureValue->setText(NA);
    };

    INode*   stageNode = nullptr;
    Interval valid = FOREVER;
    paramBlock->GetValue(
        USDCameraParams_USDStage, GetCOREInterface()->GetTime(), stageNode, valid, 0);
    if (!stageNode) {
        fillWithNA();
        return;
    }

    const auto stageObject = dynamic_cast<USDStageObject*>(stageNode->GetObjectRef());
    if (!stageObject) {
        fillWithNA();
        return;
    }

    const auto timeCode = stageObject->ResolveRenderTimeCode(t);

    const auto usdCamera = cameraObject->GetUsdCamera();
    const auto prim = usdCamera.GetPrim();
    if (!prim.IsValid()) {
        fillWithNA();
        return;
    }

    const auto pathStr = QString::fromStdString(prim.GetPath().GetString());
    ui->PathLabel->setText(pathStr); // Might be elided on long path, so also set as tooltip.
    ui->PathLabel->setToolTip(pathStr);

    pxr::TfToken projection;
    usdCamera.GetProjectionAttr().Get(&projection, timeCode);
    ui->ProjectionValue->setText(QString::fromStdString(projection.GetString()));

    float horizontalAperture = 0.f;
    usdCamera.GetHorizontalApertureAttr().Get(&horizontalAperture, timeCode);
    ui->HorizontalApertureValue->setText(QString::number(horizontalAperture));

    float horizontalApertureOffset = 0.f;
    usdCamera.GetHorizontalApertureOffsetAttr().Get(&horizontalApertureOffset, timeCode);
    ui->HorizontalApertureOffsetValue->setText(QString::number(horizontalApertureOffset));

    float verticalAperture = 0.f;
    usdCamera.GetVerticalApertureAttr().Get(&verticalAperture, timeCode);
    ui->VerticalApertureValue->setText(QString::number(verticalAperture));

    float verticalApertureOffset = 0.f;
    usdCamera.GetVerticalApertureOffsetAttr().Get(&verticalApertureOffset, timeCode);
    ui->VerticalApertureOffsetValue->setText(QString::number(verticalApertureOffset));

    float focalLength = 0.f;
    usdCamera.GetFocalLengthAttr().Get(&focalLength, timeCode);
    ui->FocalLengthValue->setText(QString::number(focalLength));

    pxr::GfVec2f clipRange;
    usdCamera.GetClippingRangeAttr().Get(&clipRange, timeCode);
    ui->ClippingNearValue->setText(QString::number(clipRange[0], 'f', 2));
    ui->ClippingFarValue->setText(QString::number(clipRange[1], 'f', 2));

    float fStop = 0.f;
    usdCamera.GetFStopAttr().Get(&fStop, timeCode);
    ui->FStopValue->setText(QString::number(fStop));

    float focusDistance = 0.f;
    usdCamera.GetFocusDistanceAttr().Get(&focusDistance, timeCode);
    ui->FocusDistanceValue->setText(QString::number(focusDistance));

    pxr::TfToken stereoRole;
    usdCamera.GetStereoRoleAttr().Get(&stereoRole, timeCode);
    ui->StereoRoleValue->setText(QString::fromStdString(stereoRole.GetString()));

    float shutterOpen = 0.f;
    usdCamera.GetShutterOpenAttr().Get(&shutterOpen, timeCode);
    ui->ShutterOpenValue->setText(QString::number(shutterOpen));

    float shutterClose = 0.f;
    usdCamera.GetShutterCloseAttr().Get(&shutterClose, timeCode);
    ui->ShutterCloseValue->setText(QString::number(shutterClose));

    float exposure = 0.f;
    usdCamera.GetExposureAttr().Get(&exposure, timeCode);
    ui->ExposureValue->setText(QString::number(exposure));
}