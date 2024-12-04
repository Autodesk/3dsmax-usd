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

#include "USDTransformControllers.h"

#include "USDStageObject.h"
#include "USDTransformControllersClassDesc.h"

#include <MaxUsdObjects/DLLEntry.h>
#include <MaxUsdObjects/resource.h>

#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>
#include <MaxUsd/Utilities/UsdLinkUtils.h>

#include <decomp.h>
#include <inode.h>
#include <iparamm2.h>

#define PBLOCK_REF 0

Class_ID USDXFORMABLECONTROLLER_CLASS_ID(0x1a855cd0, 0x7b2c3bd2);

// clang-format off
ParamBlockDesc2 xformableControllerParamblockDesc(PBLOCK_REF, 
	_M("USDXformControllerParamBlock"),
	IDS_USDXFORMCONTROLLER_CLASS_NAME,
	GetUSDXformableControllerClassDesc(), 
	P_AUTO_CONSTRUCT | P_AUTO_UI_QT | P_MULTIMAP,
	PBLOCK_REF,
	1,
	USDTransformControllerMapID_General,
	// Parameters
	USDControllerParams_USDStage, _M("USDStage"), TYPE_INODE, 0, IDS_USDCONTROLLER_USDSTAGE_PARAM, p_end, 
	USDControllerParams_Path,_M("XformablePrimPath"), TYPE_STRING, 0, IDS_USDXFORMCONTROLLER_XFORMABLEPATH_PARAM,p_default, L"", p_end, 
	USDControllerParams_PreventNodeDeletion,_M("PreventsNodeDeletion"), TYPE_BOOL, 0, IDS_USDXFORMCONTROLLER_PREVENTSNODEDELETION_PARAM ,p_default, FALSE, p_end,
	p_end);
// clang-format on

USDXformableController::USDXformableController()
{
    GetUSDXformableControllerClassDesc()->MakeAutoParamBlocks(this);
}

Class_ID USDXformableController::ClassID() { return USDXFORMABLECONTROLLER_CLASS_ID; }

SClass_ID USDXformableController::SuperClassID() { return CTRL_MATRIX3_CLASS_ID; }

void USDXformableController::GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method)
{
    if (!stageNode || !xformable.GetPrim().IsValid()) {
        return;
    }
    const auto matrix = static_cast<Matrix3*>(val);
    *matrix = USDStageObject::GetMaxScenePrimTransform(stageNode, xformable.GetPrim(), t, true);
    valid = Interval(t, t);
}

void USDXformableController::GetClassName(MSTR& className, bool localized) const
{
    className
        = localized ? GetString(IDS_USDXFORMCONTROLLER_CLASS_NAME) : _T("USD Xformable Controller");
}

RefTargetHandle USDXformableController::Clone(RemapDir& remap)
{
    USDXformableController* newXform = new USDXformableController();
    newXform->ReplaceReference(PBLOCK_REF, remap.CloneRef(paramBlock));
    BaseClone(this, newXform, remap);
    return newXform;
}

bool USDXformableController::IsSourceObjectValid() const { return xformable.GetPrim().IsValid(); }

ClassDesc2* USDXformableController::GetControllerClassDesc() const
{
    return GetUSDXformableControllerClassDesc();
}

bool USDXformableController::UpdateSource(IParamBlock2* pb)
{
    return MaxUsd::UpdateUsdSource<pxr::UsdGeomXformable>(
        stageNode, xformable, pb, USDControllerParams_USDStage, USDControllerParams_Path);
}

const pxr::UsdGeomXformable& USDXformableController::GetXformable() { return xformable; }

BOOL USDXformableController::PreventNodeDeletion()
{
    BOOL     value = false;
    Interval valid;
    paramBlock->GetValue(
        USDControllerParams_PreventNodeDeletion, GetCOREInterface()->GetTime(), value, valid);
    return static_cast<bool>(value);
}

void USDPRSController::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
    // Work around 3dsMax issue where the flag is not properly set in version <= 2025.
    const auto removeUi = flags | END_EDIT_REMOVEUI;
    USDXformableController::EndEditParams(ip, removeUi, next);
}

bool USDPRSController::IsSourceObjectValid() const
{
    return attribute.IsValid() || USDXformableController::IsSourceObjectValid();
}

bool USDPRSController::UpdateSource(IParamBlock2* pb)
{
    bool attrChanged = MaxUsd::UpdateUsdSourceAttr(
        stageNode, attribute, paramBlock, USDControllerParams_USDStage, USDControllerParams_Path);

    return attrChanged || USDXformableController::UpdateSource(pb);
}

const pxr::UsdAttribute& USDPRSController::GetAttr() const { return attribute; }

Class_ID USDPOSITIONCONTROLLER_CLASS_ID(0x48bd4ade, 0x23b00d7);

// clang-format off
ParamBlockDesc2 positionControllerParamblockDesc(PBLOCK_REF, 
	_M("USDPositionControllerParamBlock"),
	IDS_USDPOSITIONCONTROLLER_CLASS_NAME,
	GetUSDPositionControllerClassDesc(), 
	P_AUTO_CONSTRUCT | P_AUTO_UI_QT | P_MULTIMAP,
	PBLOCK_REF,
	1,
	USDTransformControllerMapID_General,
	// Parameters
	USDControllerParams_USDStage, _M("USDStage"), TYPE_INODE, 0, IDS_USDCONTROLLER_USDSTAGE_PARAM, p_end, 
	USDControllerParams_Path,_M("Path"), TYPE_STRING, 0, IDS_USDTRANSFORMCONTROLLER_PATH_PARAM,p_default, L"", p_end,
	p_end);
// clang-format on

USDPositionController::USDPositionController()
{
    GetUSDPositionControllerClassDesc()->MakeAutoParamBlocks(this);
}

Class_ID USDPositionController::ClassID() { return USDPOSITIONCONTROLLER_CLASS_ID; }

SClass_ID USDPositionController::SuperClassID() { return CTRL_POSITION_CLASS_ID; }

void USDPositionController::GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method)
{
    if (!stageNode) {
        return;
    }

    const auto  xformable = GetXformable();
    const auto& attr = GetAttr();

    Point3 translate;
    // Case where the translation is driven by a Xformable prim.
    if (xformable.GetPrim().IsValid()) {
        auto fullTransform = USDStageObject::GetMaxScenePrimTransform(
            stageNode, GetXformable().GetPrim(), t, true);
        translate = fullTransform.GetTrans();
    }
    // Case where the translation is driven by an attribute prim.
    else if (attr.IsValid()) {
        auto value = MaxUsd::GetAttrValue(stageNode, attr, t);
        if (value.IsEmpty()) {
            return;
        }
        if (value.CanCast<pxr::GfVec3f>()) {
            translate = MaxUsd::ToMax(value.Cast<pxr::GfVec3f>().Get<pxr::GfVec3f>());
        } else {
            return;
        }
    } else {
        return;
    }

    // PRS controllers have two GetValue() modes, relative and absolute.
    // In relative mode, a transform is given that needs to be pre-multiplied.
    // In absolute mode, the translation is returned directly.
    if (method == CTRL_RELATIVE) {
        const auto matrix = static_cast<Matrix3*>(val);
        matrix->PreTranslate(translate);
    }
    // CTRL_ABSOLUTE
    else {
        const auto position = static_cast<Point3*>(val);
        *position = translate;
    }
    valid = Interval(t, t);
}

void USDPositionController::GetClassName(MSTR& className, bool localized) const
{
    className = localized ? GetString(IDS_USDPOSITIONCONTROLLER_CLASS_NAME) : _T("USD Position");
}

ClassDesc2* USDPositionController::GetControllerClassDesc() const
{
    return GetUSDPositionControllerClassDesc();
}

Class_ID USDSCALECONTROLLER_CLASS_ID(0x8b63fef, 0x6eba1867);

// clang-format off
ParamBlockDesc2 scaleControllerParamblockDesc(PBLOCK_REF, 
	_M("USDScaleControllerParamBlock"),
	IDS_USDSCALECONTROLLER_CLASS_NAME,
	GetUSDScaleControllerClassDesc(), 
	P_AUTO_CONSTRUCT | P_AUTO_UI_QT | P_MULTIMAP,
	PBLOCK_REF,
	1,
	USDTransformControllerMapID_General,
	// Parameters
	USDControllerParams_USDStage, _M("USDStage"), TYPE_INODE, 0, IDS_USDCONTROLLER_USDSTAGE_PARAM, p_end, 
	USDControllerParams_Path,_M("Path"), TYPE_STRING, 0, IDS_USDTRANSFORMCONTROLLER_PATH_PARAM,p_default, L"", p_end,
	p_end);
// clang-format on

USDScaleController::USDScaleController()
{
    GetUSDScaleControllerClassDesc()->MakeAutoParamBlocks(this);
}

Class_ID USDScaleController::ClassID() { return USDSCALECONTROLLER_CLASS_ID; }

SClass_ID USDScaleController::SuperClassID() { return CTRL_SCALE_CLASS_ID; }

void USDScaleController::GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method)
{
    if (!stageNode) {
        return;
    }

    const auto  xformable = GetXformable();
    const auto& attr = GetAttr();

    Point3 scaling;
    // Case where the scaling is driven by a Xformable prim.
    if (xformable.GetPrim().IsValid()) {
        const auto fullTransform = USDStageObject::GetMaxScenePrimTransform(
            stageNode, GetXformable().GetPrim(), t, true);
        AffineParts ap;
        decomp_affine(fullTransform, &ap);
        scaling = ap.k;
    }
    // Case where the scaling is driven by an attribute prim.
    else if (attr.IsValid()) {
        auto value = MaxUsd::GetAttrValue(stageNode, attr, t);
        if (value.IsEmpty()) {
            return;
        }
        if (value.CanCast<pxr::GfVec3f>()) {
            scaling = MaxUsd::ToMax(value.Cast<pxr::GfVec3f>().Get<pxr::GfVec3f>());
        } else {
            return;
        }
    } else {
        return;
    }

    // PRS controllers have two GetValue() modes, relative and absolute.
    // In relative mode, a transform is given that needs to be pre-multiplied.
    // In absolute mode, the scale is returned directly.
    if (method == CTRL_RELATIVE) {
        const auto matrix = static_cast<Matrix3*>(val);
        matrix->PreScale(scaling);
    }
    // CTRL_ABSOLUTE
    else {
        const auto scaleValue = static_cast<ScaleValue*>(val);
        *scaleValue = scaling;
    }
    valid = Interval(t, t);
}

void USDScaleController::GetClassName(MSTR& className, bool localized) const
{
    className = localized ? GetString(IDS_USDSCALECONTROLLER_CLASS_NAME) : _T("USD Scale");
}

ClassDesc2* USDScaleController::GetControllerClassDesc() const
{
    return GetUSDScaleControllerClassDesc();
}

Class_ID USDROTATIONCONTROLLER_CLASS_ID(0x4a4f675a, 0x57a314b6);

// clang-format off
ParamBlockDesc2 rotationControllerParamblockDesc(PBLOCK_REF, 
	_M("USDRotationControllerParamBlock"),
	IDS_USDROTATIONCONTROLLER_CLASS_NAME,
	GetUSDRotationControllerClassDesc(), 
	P_AUTO_CONSTRUCT | P_AUTO_UI_QT | P_MULTIMAP,
	PBLOCK_REF,
	1,
	USDTransformControllerMapID_General,
	// Parameters
	USDControllerParams_USDStage, _M("USDStage"), TYPE_INODE, 0, IDS_USDCONTROLLER_USDSTAGE_PARAM, p_end, 
	USDControllerParams_Path,_M("Path"), TYPE_STRING, 0, IDS_USDTRANSFORMCONTROLLER_PATH_PARAM,p_default, L"", p_end,
	p_end);
// clang-format on

USDRotationController::USDRotationController()
{
    GetUSDRotationControllerClassDesc()->MakeAutoParamBlocks(this);
}

Class_ID USDRotationController::ClassID() { return USDROTATIONCONTROLLER_CLASS_ID; }

SClass_ID USDRotationController::SuperClassID() { return CTRL_ROTATION_CLASS_ID; }

void USDRotationController::GetValue(TimeValue t, void* val, Interval& valid, GetSetMethod method)
{
    if (!stageNode) {
        return;
    }

    const auto  xformable = GetXformable();
    const auto& attr = GetAttr();

    Quat rotation;
    // Case where the rotation is driven by a Xformable prim.
    if (xformable.GetPrim().IsValid()) {
        const auto fullTransform = USDStageObject::GetMaxScenePrimTransform(
            stageNode, GetXformable().GetPrim(), t, true);
        AffineParts ap;
        decomp_affine(fullTransform, &ap);
        rotation = ap.q;
    }
    // Case where the rotation is driven by an attribute prim.
    else if (attr.IsValid()) {
        auto value = MaxUsd::GetAttrValue(stageNode, attr, t);
        if (value.IsEmpty()) {
            return;
        }

        bool quatConverted = MaxUsd::ToMaxQuat<pxr::GfQuatf>(value, rotation)
            || MaxUsd::ToMaxQuat<pxr::GfQuatd>(value, rotation)
            || MaxUsd::ToMaxQuat<pxr::GfQuath>(value, rotation);
        if (!quatConverted) {
            return;
        }
    } else {
        return;
    }

    // PRS controllers have two GetValue() modes, relative and absolute.
    // In relative mode, a transform is given that needs to be pre-multiplied.
    // In absolute mode, the rotation is returned directly.
    if (method == CTRL_RELATIVE) {
        const auto matrix = static_cast<Matrix3*>(val);
        PreRotateMatrix(*matrix, rotation);
    }
    // CTRL_ABSOLUTE
    else {
        const auto quatValue = static_cast<Quat*>(val);
        *quatValue = rotation;
    }
    valid = Interval(t, t);
}

void USDRotationController::GetClassName(MSTR& className, bool localized) const
{
    className = localized ? GetString(IDS_USDROTATIONCONTROLLER_CLASS_NAME) : _T("USD Rotation");
}

ClassDesc2* USDRotationController::GetControllerClassDesc() const
{
    return GetUSDRotationControllerClassDesc();
}

// Disable obscure warning only occurring for 2022 (pixar usd version: 21.11):
// no definition for inline function : pxr::DefaultValueHolder Was not able to identify what is
// triggering this.
#pragma warning(disable : 4506)