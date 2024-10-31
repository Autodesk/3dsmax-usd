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

#include "USDAttrControllers.h"

#include "USDAttrControllerClassDescs.h"
#include "USDStageObject.h"

#include <MaxUsdObjects/DLLEntry.h>
#include <MaxUsdObjects/Views/UsdControllerWidget.h>
#include <MaxUsdObjects/resource.h>

#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>
#include <MaxUsd/Utilities/UsdLinkUtils.h>

#include <QT/QmaxMainWindow.h>

#include <inode.h>
#include <iparamm2.h>

#define PBLOCK_REF 0

USDAttrController::~USDAttrController()
{
    if (dialog) {
        // This will also delete the dialog as the "delete on close" flag is set.
        dialog->close();
    }
}

pxr::VtValue USDAttrController::GetAttrValue(const TimeValue& time) const
{
    const auto& attr = GetAttribute();
    if (!stageNode || !attr.IsValid()) {
        return {};
    }
    const auto stageObject = dynamic_cast<USDStageObject*>(stageNode->GetObjectRef());
    if (!stageObject) {
        return {};
    }

    const auto   timeCode = stageObject->ResolveRenderTimeCode(time);
    pxr::VtValue value;
    attr.Get(&value, timeCode);
    return value;
}

void USDAttrController::EditTrackParams(
    TimeValue           t,
    ParamDimensionBase* dim,
    const TCHAR*        pname,
    HWND                hParent,
    IObjParam*          ip,
    DWORD               flags)
{
    if (!dialog) {
        dialog = new QDialog { GetCOREInterface()->GetQmaxMainWindow() };
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        controllerWidget = new UsdControllerWidget { *this, *paramBlock };
        layout->addWidget(controllerWidget);
        controllerWidget->SetLabel(QObject::tr("Attribute Path:"));
        controllerWidget->SetLabelTooltip(QObject::tr("The path of the attribute used as source."));
        controllerWidget->SetPickButtonTooltip(
            QObject::tr("Select the USD stage node that contains the source attribute."));
        SetupDialog(dialog, controllerWidget);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    dialog->show();
    dialog->raise();
}

int USDAttrController::TrackParamsType() { return TRACKPARAMS_WHOLE; }

bool USDAttrController::IsSourceObjectValid() const { return attribute.IsValid(); }

bool USDAttrController::UpdateSource(IParamBlock2* pb)
{
    bool changed = MaxUsd::UpdateUsdSourceAttr(
        stageNode, attribute, pb, USDControllerParams_USDStage, USDControllerParams_Path);
    if (controllerWidget) {
        controllerWidget->UpdateUI(GetCOREInterface()->GetTime());
    }
    return changed;
}

const pxr::UsdAttribute& USDAttrController::GetAttribute() const { return attribute; }

Class_ID USDFLOATCONTROLLER_CLASS_ID(0x24a91d51, 0x25de5c1c);

// clang-format off
ParamBlockDesc2 floatControllerParamblockDesc(PBLOCK_REF, 
	_M("USDFloatControllerParamBlock"),
	IDS_USDFLOATCONTROLLER_CLASS_NAME,
	GetUSDFloatControllerClassDesc(), 
	P_AUTO_CONSTRUCT | P_AUTO_UI_QT,
	PBLOCK_REF,
	// Parameters
	USDControllerParams_USDStage, _M("USDStage"), TYPE_INODE, 0, IDS_USDCONTROLLER_USDSTAGE_PARAM, p_end, 
	USDControllerParams_Path,_M("AttrPath"), TYPE_STRING, 0, IDS_USDATTRCONTROLLER_PATH_PARAM,p_default, L"", p_end, 
	p_end);
// clang-format on

USDFloatController::USDFloatController()
{
    GetUSDFloatControllerClassDesc()->MakeAutoParamBlocks(this);
}

Class_ID USDFloatController::ClassID() { return USDFLOATCONTROLLER_CLASS_ID; }

SClass_ID USDFloatController::SuperClassID() { return CTRL_FLOAT_CLASS_ID; }

void USDFloatController::GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method)
{
    auto       value = GetAttrValue(t);
    const auto floatVal = static_cast<float*>(val);
    if (value.IsEmpty()) {
        // With raw types, need to initialize to the default.
        *floatVal = 0.f;
        return;
    }
    if (value.CanCast<float>()) {
        *floatVal = value.Cast<float>().Get<float>();
    }
    valid = Interval(t, t);
}

void USDFloatController::GetClassName(MSTR& className, bool localized) const
{
    className
        = localized ? GetString(IDS_USDFLOATCONTROLLER_CLASS_NAME) : _T("USD Float Controller");
}

void USDFloatController::SetupDialog(
    const QPointer<QDialog>&             dialog,
    const QPointer<UsdControllerWidget>& widget)
{
    dialog->setWindowTitle(QObject::tr("USD Float Controller"));
    widget->SetPathErrorMessage(QObject::tr("Invalid Attribute path for a float value : "));
}

RefTargetHandle USDFloatController::Clone(RemapDir& remap)
{
    USDFloatController* floatCtrl = new USDFloatController();
    floatCtrl->ReplaceReference(PBLOCK_REF, remap.CloneRef(paramBlock));
    BaseClone(this, floatCtrl, remap);
    return floatCtrl;
}

ClassDesc2* USDFloatController::GetControllerClassDesc() const
{
    return GetUSDFloatControllerClassDesc();
}

Class_ID USDPOINT3CONTROLLER_CLASS_ID(0x2346548e, 0x1a1e57e8);

// clang-format off
ParamBlockDesc2 point3ControllerParamblockDesc(PBLOCK_REF, 
	_M("USDPoint3ControllerParamBlock"),
	IDS_USDPOINT3CONTROLLER_CLASS_NAME,
	GetUSDPoint3ControllerClassDesc(), 
	P_AUTO_CONSTRUCT | P_AUTO_UI_QT,
	PBLOCK_REF,
	// Parameters
	USDControllerParams_USDStage, _M("USDStage"), TYPE_INODE, 0, IDS_USDCONTROLLER_USDSTAGE_PARAM, p_end, 
	USDControllerParams_Path,_M("AttrPath"), TYPE_STRING, 0, IDS_USDATTRCONTROLLER_PATH_PARAM,p_default, L"", p_end, 
	p_end);
// clang-format on

USDPoint3Controller::USDPoint3Controller()
{
    GetUSDPoint3ControllerClassDesc()->MakeAutoParamBlocks(this);
}

Class_ID USDPoint3Controller::ClassID() { return USDPOINT3CONTROLLER_CLASS_ID; }

SClass_ID USDPoint3Controller::SuperClassID() { return CTRL_POINT3_CLASS_ID; }

void USDPoint3Controller::GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method)
{
    auto value = GetAttrValue(t);
    if (value.IsEmpty()) {
        return;
    }
    const auto point3Val = static_cast<Point3*>(val);
    if (value.CanCast<pxr::GfVec3f>()) {
        *point3Val = MaxUsd::ToMax(value.Cast<pxr::GfVec3f>().Get<pxr::GfVec3f>());
    }
    // Allow two-dimensional values to be read into Point3 as well.
    else if (value.CanCast<pxr::GfVec2f>()) {
        auto         vec2 = value.Cast<pxr::GfVec2f>().Get<pxr::GfVec2f>();
        pxr::GfVec3f vec3 = { vec2[0], vec2[1], 0.f };
        *point3Val = MaxUsd::ToMax(vec3);
    }
    valid = Interval(t, t);
}

void USDPoint3Controller::GetClassName(MSTR& className, bool localized) const
{
    className
        = localized ? GetString(IDS_USDPOINT3CONTROLLER_CLASS_NAME) : _T("USD Point3 Controller");
}

void USDPoint3Controller::SetupDialog(
    const QPointer<QDialog>&             dialog,
    const QPointer<UsdControllerWidget>& widget)
{
    dialog->setWindowTitle(QString::fromStdWString(GetString(IDS_USDPOINT3CONTROLLER_CLASS_NAME)));
    widget->SetPathErrorMessage(QObject::tr("Invalid Attribute path to read a Point3 value : "));
}

RefTargetHandle USDPoint3Controller::Clone(RemapDir& remap)
{
    USDPoint3Controller* float3Ctrl = new USDPoint3Controller();
    float3Ctrl->ReplaceReference(PBLOCK_REF, remap.CloneRef(paramBlock));
    BaseClone(this, float3Ctrl, remap);
    return float3Ctrl;
}

ClassDesc2* USDPoint3Controller::GetControllerClassDesc() const
{
    return GetUSDPoint3ControllerClassDesc();
}

Class_ID USDPOINT4CONTROLLER_CLASS_ID(0x3f955b1d, 0x22ad1d76);

// clang-format off
ParamBlockDesc2 point4ControllerParamblockDesc(PBLOCK_REF, 
	_M("USDPoint4ControllerParamBlock"),
	IDS_USDPOINT4CONTROLLER_CLASS_NAME,
	GetUSDPoint4ControllerClassDesc(), 
	P_AUTO_CONSTRUCT | P_AUTO_UI_QT,
	PBLOCK_REF,
	// Parameters
	USDControllerParams_USDStage, _M("USDStage"), TYPE_INODE, 0, IDS_USDCONTROLLER_USDSTAGE_PARAM, p_end, 
	USDControllerParams_Path,_M("AttrPath"), TYPE_STRING, 0, IDS_USDATTRCONTROLLER_PATH_PARAM,p_default, L"", p_end, 
	p_end);
// clang-format on

USDPoint4Controller::USDPoint4Controller()
{
    GetUSDPoint3ControllerClassDesc()->MakeAutoParamBlocks(this);
}

Class_ID USDPoint4Controller::ClassID() { return USDPOINT4CONTROLLER_CLASS_ID; }

SClass_ID USDPoint4Controller::SuperClassID() { return CTRL_POINT4_CLASS_ID; }

void USDPoint4Controller::GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method)
{
    auto value = GetAttrValue(t);
    if (value.IsEmpty()) {
        return;
    }
    const auto point4Val = static_cast<Point4*>(val);
    if (value.CanCast<pxr::GfVec4f>()) {
        *point4Val = MaxUsd::ToMax(value.Cast<pxr::GfVec4f>().Get<pxr::GfVec4f>());
    }
    // Allow three-dimensional values to be read into Point4 as well.
    else if (value.CanCast<pxr::GfVec3f>()) {
        auto         vec3 = value.Cast<pxr::GfVec3f>().Get<pxr::GfVec3f>();
        pxr::GfVec4f vec4 = { vec3[0], vec3[1], vec3[2], 0.f };
        *point4Val = MaxUsd::ToMax(vec4);
    }
    valid = Interval(t, t);
}

void USDPoint4Controller::GetClassName(MSTR& className, bool localized) const
{
    className
        = localized ? GetString(IDS_USDPOINT4CONTROLLER_CLASS_NAME) : _T("USD Point4 Controller");
}

void USDPoint4Controller::SetupDialog(
    const QPointer<QDialog>&             dialog,
    const QPointer<UsdControllerWidget>& widget)
{
    dialog->setWindowTitle(QString::fromStdWString(GetString(IDS_USDPOINT4CONTROLLER_CLASS_NAME)));
    widget->SetPathErrorMessage(QObject::tr("Invalid Attribute path to read a Point4 value : "));
}

RefTargetHandle USDPoint4Controller::Clone(RemapDir& remap)
{
    USDPoint4Controller* float4Ctrl = new USDPoint4Controller();
    float4Ctrl->ReplaceReference(PBLOCK_REF, remap.CloneRef(paramBlock));
    BaseClone(this, float4Ctrl, remap);
    return float4Ctrl;
}

ClassDesc2* USDPoint4Controller::GetControllerClassDesc() const
{
    return GetUSDPoint4ControllerClassDesc();
}

// Disable obscure warning only occurring for 2022 (pixar usd version: 21.11):
// no definition for inline function : pxr::DefaultValueHolder Was not able to identify what is
// triggering this.
#pragma warning(disable : 4506)
