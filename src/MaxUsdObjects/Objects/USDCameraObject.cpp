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

#include "USDCameraObject.h"

#include "CreateCallbacks/CreateAtPosition.h"
#include "USDStageObject.h"
#include "UsdCameraObjectclassDesc.h"

#include <MaxUsdObjects/DLLEntry.h>
#include <MaxUsdObjects/resource.h>

#include <MaxUsd/CameraConversion/CameraConverter.h>
#include <MaxUsd/Utilities/UsdLinkUtils.h>

#include <iparamm2.h>

#define PBLOCK_REF 0

Class_ID USDCAMERAOBJECT_CLASS_ID(0x56fb5fb2, 0x7687774f);

// clang-format off
ParamBlockDesc2 usdCameraParamblockDesc(PBLOCK_REF, 
	_M("USDCameraObject"),
	IDS_USDCAMERA_ROLL_OUT,
	GetUSDCameraObjectClassDesc(), 
	P_AUTO_CONSTRUCT | P_AUTO_UI_QT | P_MULTIMAP,
	PBLOCK_REF,
	1,
	USDCameraMapID_General,
	// Parameters
	USDCameraParams_USDStage, _M("USDStage"), TYPE_INODE, 0, IDS_USDCAMERA_USDSTAGE_PARAM, p_end, 
	USDCameraParams_PrimPath,_M("CameraPrimPath"), TYPE_STRING, 0, IDS_USDCAMERA_CAMERAPATH_PARAM,p_default, L"", p_end, 
	p_end);
// clang-format on

USDCameraObject::USDCameraObject()
{
    GetUSDCameraObjectClassDesc()->MakeAutoParamBlocks(this);
    conversionOptions.SetDefaults();
    internalCamera = static_cast<IPhysicalCamera*>(
        GetCOREInterface17()->CreateInstance(CAMERA_CLASS_ID, GetClassID()));

    // Register ourselves as a listener for USD stage change notifications. If the camera is
    // changed externally, will need to reconvert.
    pxr::TfWeakPtr<USDCameraObject> me(this);
    onStageChangeNotice = pxr::TfNotice::Register(me, &USDCameraObject::OnStageChange);
}

USDCameraObject::~USDCameraObject()
{
    // Camera is only known to us, it needs to be deleted manually.
    internalCamera->MaybeAutoDelete();
    pxr::TfNotice::Revoke(onStageChangeNotice);
}

void USDCameraObject::SetReference(int i, RefTargetHandle rtarg)
{
    paramBlock = dynamic_cast<IParamBlock2*>(rtarg);
}

int USDCameraObject::NumRefs() { return 1; }

ReferenceTarget* USDCameraObject::GetReference(int i) { return i == 0 ? paramBlock : nullptr; }

int USDCameraObject::NumParamBlocks() { return 1; }

IParamBlock2* USDCameraObject::GetParamBlock(int i) { return i == 0 ? paramBlock : nullptr; }

IParamBlock2* USDCameraObject::GetParamBlockByID(BlockID id)
{
    if (paramBlock && paramBlock->ID() == id) {
        return paramBlock;
    }
    return nullptr;
}

int USDCameraObject::NumSubs() { return 1; }

Animatable* USDCameraObject::SubAnim(int i) { return paramBlock; }

TSTR USDCameraObject::SubAnimName(int i, bool localized)
{
    return localized ? GetString(IDS_PARAMS) : _T("Parameters");
}

int USDCameraObject::SubNumToRefNum(int subNum)
{
    if (subNum == PBLOCK_REF) {
        return subNum;
    }
    return -1;
}

RefTargetHandle USDCameraObject::Clone(RemapDir& remap)
{
    USDCameraObject* newCamera = new USDCameraObject();
    newCamera->ReplaceReference(PBLOCK_REF, remap.CloneRef(paramBlock));
    BaseClone(this, newCamera, remap);
    return newCamera;
}

void USDCameraObject::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
    IPhysicalCamera::BeginEditParams(ip, flags, prev);
    GetUSDCameraObjectClassDesc()->BeginEditParams(ip, this, flags, prev);
}

void USDCameraObject::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
    IPhysicalCamera::EndEditParams(ip, flags, next);
    GetUSDCameraObjectClassDesc()->EndEditParams(ip, this, flags, next);
}

RefResult USDCameraObject::NotifyRefChanged(
    const Interval& changeInt,
    RefTargetHandle hTarget,
    PartID&         partID,
    RefMessage      message,
    BOOL            propagate)
{
    switch (message) {
    case REFMSG_CHANGE: {
        const auto pb = dynamic_cast<IParamBlock2*>(hTarget);
        if (!pb) {
            return REF_DONTCARE;
        }
        int     tabIndex = -1;
        ParamID pID = pb->LastNotifyParamID(tabIndex);
        switch (pID) {
        case USDCameraParams_USDStage:
        case USDCameraParams_PrimPath:
            if (!MaxUsd::UpdateUsdSource<pxr::UsdGeomCamera>(
                    stageNode, usdCamera, pb, USDCameraParams_USDStage, USDCameraParams_PrimPath)) {
                return REF_SUCCEED;
            }
            const auto map = pb->GetMap(USDCameraMapID_General);
            if (map) {
                map->UpdateUI(GetCOREInterface()->GetTime());
            }
            break;
        }
        break;
    }
    }
    return REF_SUCCEED;
}

void USDCameraObject::GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box)
{
    Update(t);
    internalCamera->GetWorldBoundBox(t, inode, vp, box);
}

void USDCameraObject::GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box)
{
    Update(t);
    internalCamera->GetLocalBoundBox(t, inode, vp, box);
}

Class_ID USDCameraObject::ClassID() { return USDCAMERAOBJECT_CLASS_ID; }

int USDCameraObject::IsRenderable() { return internalCamera->IsRenderable(); }

Interval USDCameraObject::ObjectValidity(TimeValue time)
{
    // Always use an "instant" interval, we want to update at every frame.
    return Interval(time, time);
}

RefResult USDCameraObject::EvalCameraState(TimeValue time, Interval& valid, CameraState* cs)
{
    Update(time);
    return internalCamera->EvalCameraState(time, valid, cs);
}

ObjectState USDCameraObject::Eval(TimeValue t)
{
    Update(t);
    return ObjectState(this);
}

void USDCameraObject::InitNodeName(MSTR& name) { name = L"UsdCamera"; }

const wchar_t* USDCameraObject::GetObjectName(bool localized) const { return _M("UsdCamera"); }

CreateMouseCallBack* USDCameraObject::GetCreateMouseCallBack()
{
    // USD Camera object is not meant to be user created.
    return nullptr;
}

void USDCameraObject::SetOrtho(BOOL b)
{
    // Unsupported, Camera driven from USD.
}

BOOL USDCameraObject::IsOrtho() { return internalCamera->IsOrtho(); }

void USDCameraObject::SetFOV(TimeValue t, float f)
{
    // Unsupported, Camera driven from USD.
}

float USDCameraObject::GetFOV(TimeValue t, Interval& valid)
{
    Update(t);
    return internalCamera->GetFOV(t, valid);
}

void USDCameraObject::SetTDist(TimeValue t, float f)
{
    // Unsupported, Camera driven from USD.
}

float USDCameraObject::GetTDist(TimeValue t, Interval& valid)
{
    Update(t);
    return internalCamera->GetTDist(t, valid);
}

int USDCameraObject::GetManualClip() { return internalCamera->GetManualClip(); }

void USDCameraObject::SetManualClip(int onOff)
{
    // Unsupported, Camera driven from USD.
}

float USDCameraObject::GetClipDist(TimeValue t, int which, Interval& valid)
{
    Update(t);
    return internalCamera->GetClipDist(t, which, valid);
}

void USDCameraObject::SetClipDist(TimeValue t, int which, float val)
{
    // Unsupported, Camera driven from USD.
}

void USDCameraObject::SetEnvRange(TimeValue time, int which, float f)
{
    // Unsupported, Camera driven from USD.
}

float USDCameraObject::GetEnvRange(TimeValue t, int which, Interval& valid)
{
    Update(t);
    return internalCamera->GetEnvRange(t, which, valid);
}

void USDCameraObject::SetEnvDisplay(BOOL b, int notify)
{
    // Unsupported, Camera driven from USD.
}

BOOL USDCameraObject::GetEnvDisplay() { return internalCamera->GetEnvDisplay(); }

void USDCameraObject::RenderApertureChanged(TimeValue t)
{
    internalCamera->RenderApertureChanged(t);
}

GenCamera* USDCameraObject::NewCamera(int type) { return internalCamera->NewCamera(type); }

void USDCameraObject::SetConeState(int s)
{
    // Unsupported, Camera driven from USD.
}

int USDCameraObject::GetConeState() { return internalCamera->GetConeState(); }

void USDCameraObject::SetHorzLineState(int s)
{
    // Unsupported, Camera driven from USD.
}

int USDCameraObject::GetHorzLineState() { return internalCamera->GetHorzLineState(); }

void USDCameraObject::Enable(int enab) { internalCamera->Enable(enab); }

BOOL USDCameraObject::SetFOVControl(Control* c)
{
    // Unsupported, Camera driven from USD.
    return FALSE;
}

void USDCameraObject::SetFOVType(int ft)
{
    // Unsupported, Camera driven from USD.
}

int USDCameraObject::GetFOVType() { return internalCamera->GetFOVType(); }

Control* USDCameraObject::GetFOVControl() { return internalCamera->GetFOVControl(); }

int USDCameraObject::Type() { return internalCamera->Type(); }

void USDCameraObject::SetType(int tp)
{
    // Unsupported, Camera driven from USD.
}

float USDCameraObject::GetFilmWidth(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetFilmWidth(t, validity);
}

float USDCameraObject::GetEffectiveLensFocalLength(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetEffectiveLensFocalLength(t, validity);
}

float USDCameraObject::GetCropZoomFactor(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetCropZoomFactor(t, validity);
}

float USDCameraObject::GetLensApertureRadius(
    const TimeValue t,
    Interval&       validity,
    const bool      adjustForBlades) const
{
    return internalCamera->GetLensApertureRadius(t, validity, adjustForBlades);
}

float USDCameraObject::GetLensApertureFNumber(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetLensApertureFNumber(t, validity);
}

bool USDCameraObject::GetMotionBlurEnabled(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetMotionBlurEnabled(t, validity);
}

float USDCameraObject::GetShutterDurationInFrames(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetShutterDurationInFrames(t, validity);
}

float USDCameraObject::GetShutterOffsetInFrames(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetShutterOffsetInFrames(t, validity);
}

float USDCameraObject::GetFocusDistance(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetFocusDistance(t, validity);
}

float USDCameraObject::GetEffectiveISO(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetEffectiveISO(t, validity);
}

float USDCameraObject::GetEffectiveEV(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetEffectiveEV(t, validity);
}

Color USDCameraObject::GetWhitePoint(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetWhitePoint(t, validity);
}

float USDCameraObject::GetExposureVignettingAmount(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetExposureVignettingAmount(t, validity);
}

bool USDCameraObject::GetDOFEnabled(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetDOFEnabled(t, validity);
}

MaxSDK::IPhysicalCamera::BokehShape
USDCameraObject::GetBokehShape(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetBokehShape(t, validity);
}

int USDCameraObject::GetBokehNumberOfBlades(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetBokehNumberOfBlades(t, validity);
}

float USDCameraObject::GetBokehBladesRotationDegrees(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetBokehBladesRotationDegrees(t, validity);
}

Texmap* USDCameraObject::GetBokehTexture(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetBokehTexture(t, validity);
}

bool USDCameraObject::GetBokehTextureAffectExposure(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetBokehTextureAffectExposure(t, validity);
}

float USDCameraObject::GetBokehOpticalVignetting(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetBokehOpticalVignetting(t, validity);
}

float USDCameraObject::GetBokehCenterBias(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetBokehCenterBias(t, validity);
}

float USDCameraObject::GetBokehAnisotropy(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetBokehAnisotropy(t, validity);
}

MaxSDK::IPhysicalCamera::LensDistortionType
USDCameraObject::GetLensDistortionType(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetLensDistortionType(t, validity);
}

float USDCameraObject::GetLensDistortionCubicAmount(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetLensDistortionCubicAmount(t, validity);
}

Texmap* USDCameraObject::GetLensDistortionTexture(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetLensDistortionTexture(t, validity);
}

Point2 USDCameraObject::GetFilmPlaneOffset(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetFilmPlaneOffset(t, validity);
}

Point2 USDCameraObject::GetTiltCorrection(const TimeValue t, Interval& validity) const
{
    return internalCamera->GetTiltCorrection(t, validity);
}

int USDCameraObject::HitTest(
    TimeValue t,
    INode*    inode,
    int       type,
    int       crossing,
    int       flags,
    IPoint2*  p,
    ViewExp*  vpt)
{
    return internalCamera->HitTest(t, inode, type, crossing, flags, p, vpt);
}

void USDCameraObject::Snap(TimeValue t, INode* inode, SnapInfo* snap, IPoint2* p, ViewExp* vpt)
{
    return internalCamera->Snap(t, inode, snap, p, vpt);
}

void USDCameraObject::SetExtendedDisplay(int flags)
{
    return internalCamera->SetExtendedDisplay(flags);
}

int USDCameraObject::Display(TimeValue t, INode* inode, ViewExp* vpt, int flags)
{
    return internalCamera->Display(t, inode, vpt, flags);
}

unsigned long USDCameraObject::GetObjectDisplayRequirement() const
{
    return internalCamera->GetObjectDisplayRequirement();
}

bool USDCameraObject::PrepareDisplay(
    const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext)
{
    return internalCamera->PrepareDisplay(prepareDisplayContext);
}

bool USDCameraObject::UpdatePerNodeItems(
    const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
    MaxSDK::Graphics::UpdateNodeContext&          nodeContext,
    MaxSDK::Graphics::IRenderItemContainer&       targetRenderItemContainer)
{
    return internalCamera->UpdatePerNodeItems(
        updateDisplayContext, nodeContext, targetRenderItemContainer);
}

const pxr::UsdGeomCamera& USDCameraObject::GetUsdCamera() { return usdCamera; }

void USDCameraObject::Update(TimeValue time)
{
    if (!stageNode || !usdCamera.GetPrim().IsValid()) {
        return;
    }

    const auto stageObject = dynamic_cast<USDStageObject*>(stageNode->GetObjectRef());
    if (!stageObject) {
        return;
    }

    const auto stage = stageObject->GetUSDStage();
    if (!stage) {
        return;
    }

    const auto timeCode = stageObject->ResolveRenderTimeCode(time);

    // Already have a valid converted camera on that time, nothing to do.
    if (conversionTimeCode == timeCode) {
        return;
    }

    if (conversionTimeCode != pxr::UsdTimeCode::Default()) {
        // If we have a camera converted at time, and we know that none of the attributes
        // are animated, we can bail.
        bool cameraMightBeAnimated = false;
        for (const auto& attrName : usdCamera.GetSchemaAttributeNames()) {
            const auto& attr = usdCamera.GetPrim().GetAttribute(attrName);
            if (attr.ValueMightBeTimeVarying()) {
                cameraMightBeAnimated = true;
                break;
            }
        }
        if (!cameraMightBeAnimated) {
            return;
        }
    }

    // Setup the conversion options - the only thing we care about for cameras, is the time
    // at which the conversion takes place.
    conversionOptions.SetTimeMode(MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::CustomRange);
    conversionOptions.SetStartTimeCode(timeCode.GetValue());
    conversionOptions.SetEndTimeCode(timeCode.GetValue());
    const pxr::MaxUsdReadJobContext readJobContext { conversionOptions,
                                                     stageObject->GetUSDStage() };

    MaxUsd::CameraConverter::ToPhysicalCamera(usdCamera, internalCamera, readJobContext);
    conversionTimeCode = timeCode;
}

void USDCameraObject::OnStageChange(pxr::UsdNotice::ObjectsChanged const& notice)
{
    const auto cameraPrim = usdCamera.GetPrim();
    if (!stageNode || !cameraPrim.IsValid()) {
        return;
    }

    if (notice.GetStage() != cameraPrim.GetStage()) {
        return;
    }

    // Here, we only need to care about the camera prim and its properties for the
    // purpose of conversion. If the camera is culled completely from the stage, the
    // object will be removed, but that does not concern conversion.

    // AffectedObject won't tell us if properties of the camera have changed (as attributes are
    // also objects with their own paths).
    auto propertiesChanged = [&notice, &cameraPrim]() {
        for (const auto& path : notice.GetChangedInfoOnlyPaths()) {
            if (path.IsPropertyPath() || path.HasPrefix(cameraPrim.GetPath())) {
                return true;
            }
        }
        return false;
    };

    if (notice.AffectedObject(cameraPrim) || propertiesChanged()) {
        conversionTimeCode = pxr::UsdTimeCode::Default();
        Interval valid = FOREVER;
        this->ForceNotify(valid);
    }
}

bool USDCameraObject::SetEvaluatingRenderTransform(const bool evaluatingRenderTransform)
{
    const bool oldValue = evaluateRenderTransform;
    evaluateRenderTransform = evaluatingRenderTransform;
    return oldValue;
}