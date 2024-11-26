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
#pragma once

#include <MaxUsdObjects/MaxUsdObjectsAPI.h>

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>

#include <pxr/usd/usdGeom/camera.h>

#include <Scene/IPhysicalCamera.h>

extern Class_ID USDCAMERAOBJECT_CLASS_ID;

enum USDCameraParams
{
    USDCameraParams_USDStage,
    USDCameraParams_PrimPath
};

enum USDCameraParamMapID
{
    USDCameraMapID_General
};

class MaxUSDObjectsAPI USDCameraObject
    : public MaxSDK::IPhysicalCamera
    , public pxr::TfWeakBase // Required for this to listen to stage events.
{
public:
    /**
     * \brief Constructor
     */
    USDCameraObject();

    /**
     * \brief Destructor.
     */
    ~USDCameraObject() override;

    // Object/Animatable overrides.
    ObjectState          Eval(TimeValue t) override;
    void                 InitNodeName(MSTR& name) override;
    const MCHAR*         GetObjectName(bool localized) const override;
    CreateMouseCallBack* GetCreateMouseCallBack() override;
    void                 SetReference(int i, RefTargetHandle rtarg) override;
    int                  NumRefs() override;
    ReferenceTarget*     GetReference(int i) override;
    int                  NumParamBlocks() override;
    IParamBlock2*        GetParamBlock(int i) override;
    IParamBlock2*        GetParamBlockByID(BlockID id) override;
    int                  NumSubs() override;
    Animatable*          SubAnim(int i) override;
    TSTR                 SubAnimName(int i, bool localized) override;
    int                  SubNumToRefNum(int subNum) override;
    RefTargetHandle      Clone(RemapDir& remap);
    void                 BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev) override;
    void                 EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override;
    RefResult            NotifyRefChanged(
                   const Interval& changeInt,
                   RefTargetHandle hTarget,
                   PartID&         partID,
                   RefMessage      message,
                   BOOL            propagate) override;
    void     GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    void     GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;
    Class_ID ClassID() override;
    int      IsRenderable() override;
    Interval ObjectValidity(TimeValue time) override;
    int
    HitTest(TimeValue t, INode* inode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt)
        override;
    void Snap(TimeValue t, INode* inode, SnapInfo* snap, IPoint2* p, ViewExp* vpt) override;
    void SetExtendedDisplay(int flags) override;
    int  Display(TimeValue t, INode* inode, ViewExp* vpt, int flags) override;
    unsigned long GetObjectDisplayRequirement() const override;
    bool
    PrepareDisplay(const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext) override;
    bool UpdatePerNodeItems(
        const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
        MaxSDK::Graphics::UpdateNodeContext&          nodeContext,
        MaxSDK::Graphics::IRenderItemContainer&       targetRenderItemContainer) override;

    // GenCamera / IPhysicalCamera overrides.
    RefResult  EvalCameraState(TimeValue time, Interval& valid, CameraState* cs) override;
    void       SetOrtho(BOOL b) override;
    BOOL       IsOrtho() override;
    void       SetFOV(TimeValue t, float f) override;
    float      GetFOV(TimeValue t, Interval& valid) override;
    void       SetTDist(TimeValue t, float f) override;
    float      GetTDist(TimeValue t, Interval& valid) override;
    int        GetManualClip() override;
    void       SetManualClip(int onOff) override;
    float      GetClipDist(TimeValue t, int which, Interval& valid) override;
    void       SetClipDist(TimeValue t, int which, float val) override;
    void       SetEnvRange(TimeValue time, int which, float f) override;
    float      GetEnvRange(TimeValue t, int which, Interval& valid) override;
    void       SetEnvDisplay(BOOL b, int notify) override;
    BOOL       GetEnvDisplay() override;
    void       RenderApertureChanged(TimeValue t) override;
    GenCamera* NewCamera(int type) override;
    void       SetConeState(int s) override;
    int        GetConeState() override;
    void       SetHorzLineState(int s) override;
    int        GetHorzLineState() override;
    void       Enable(int enab) override;
    BOOL       SetFOVControl(Control* c) override;
    void       SetFOVType(int ft) override;
    int        GetFOVType() override;
    Control*   GetFOVControl() override;
    int        Type() override;
    void       SetType(int tp) override;
    float      GetFilmWidth(const TimeValue t, Interval& validity) const override;
    float      GetEffectiveLensFocalLength(const TimeValue t, Interval& validity) const override;
    float      GetCropZoomFactor(const TimeValue t, Interval& validity) const override;
    float GetLensApertureRadius(const TimeValue t, Interval& validity, const bool adjustForBlades)
        const override;
    float      GetLensApertureFNumber(const TimeValue t, Interval& validity) const override;
    bool       GetMotionBlurEnabled(const TimeValue t, Interval& validity) const override;
    float      GetShutterDurationInFrames(const TimeValue t, Interval& validity) const override;
    float      GetShutterOffsetInFrames(const TimeValue t, Interval& validity) const override;
    float      GetFocusDistance(const TimeValue t, Interval& validity) const override;
    float      GetEffectiveISO(const TimeValue t, Interval& validity) const override;
    float      GetEffectiveEV(const TimeValue t, Interval& validity) const override;
    Color      GetWhitePoint(const TimeValue t, Interval& validity) const override;
    float      GetExposureVignettingAmount(const TimeValue t, Interval& validity) const override;
    bool       GetDOFEnabled(const TimeValue t, Interval& validity) const override;
    BokehShape GetBokehShape(const TimeValue t, Interval& validity) const override;
    int        GetBokehNumberOfBlades(const TimeValue t, Interval& validity) const override;
    float      GetBokehBladesRotationDegrees(const TimeValue t, Interval& validity) const override;
    Texmap*    GetBokehTexture(const TimeValue t, Interval& validity) const override;
    bool       GetBokehTextureAffectExposure(const TimeValue t, Interval& validity) const override;
    float      GetBokehOpticalVignetting(const TimeValue t, Interval& validity) const override;
    float      GetBokehCenterBias(const TimeValue t, Interval& validity) const override;
    float      GetBokehAnisotropy(const TimeValue t, Interval& validity) const override;
    LensDistortionType GetLensDistortionType(const TimeValue t, Interval& validity) const override;
    float   GetLensDistortionCubicAmount(const TimeValue t, Interval& validity) const override;
    Texmap* GetLensDistortionTexture(const TimeValue t, Interval& validity) const override;
    Point2  GetFilmPlaneOffset(const TimeValue t, Interval& validity) const override;
    Point2  GetTiltCorrection(const TimeValue t, Interval& validity) const override;

    /**
     * \brief Returns the USD Camera used as source for the camera.
     * \return The USD camera. Can be invalid.
     */
    const pxr::UsdGeomCamera& GetUsdCamera();

private:
    /**
     * \brief Updates the camera for the given time if necessary.
     * \param time The time to update to.
     */
    void Update(TimeValue time);

    /**
     * \brief OnStageChange event handler.
     * \param notice The stage objects changed notice - contains a reference to the changed stage.
     */
    void OnStageChange(pxr::UsdNotice::ObjectsChanged const& notice);

    // IPhysical camera override. Taken as-is from the PhysicalCamera implementation.
    // We cant just forward it to the internal camera, as the function is private.
    bool SetEvaluatingRenderTransform(const bool evaluatingRenderTransform) override;
    bool evaluateRenderTransform = true;

    /// A concrete 3dsMax physical camera that we use to represent the USD camera.
    IPhysicalCamera* internalCamera;
    /// Paramblock, configuring the source stage and camera path.
    IParamBlock2* paramBlock;
    /// The stage node this camera sources its data from.
    INode* stageNode = nullptr;
    /// The USD camera this camera sources its data from.
    pxr::UsdGeomCamera usdCamera;
    /// The last time at which the camera conversion from USD took place.
    pxr::UsdTimeCode conversionTimeCode = pxr::UsdTimeCode::Default();
    // Initializing options takes some time. Keep an instance on which we will only
    // need to update the conversion time.
    MaxUsd::MaxSceneBuilderOptions conversionOptions;
    // Notice to react to changed to the stage - might need to refresh the camera.
    pxr::TfNotice::Key onStageChangeNotice;
};
