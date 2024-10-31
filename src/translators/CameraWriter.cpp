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
#include "CameraWriter.h"

#include <MaxUsd/Translators/primWriter.h>
#include <MaxUsd/Translators/writeJobContext.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

#include <Scene/IPhysicalCamera.h>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdCameraWriter::MaxUsdCameraWriter(const MaxUsdWriteJobContext& jobCtx, INode* node)
    : MaxUsdPrimWriter(jobCtx, node)
{
}

MaxUsdPrimWriter::ContextSupport
MaxUsdCameraWriter::CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    if (!exportArgs.GetTranslateCameras()) {
        return ContextSupport::Unsupported;
    }
    const auto object = node->EvalWorldState(exportArgs.GetResolvedTimeConfig().GetStartTime()).obj;
    return object->SuperClassID() == CAMERA_CLASS_ID ? ContextSupport::Fallback
                                                     : ContextSupport::Unsupported;
}

bool MaxUsdCameraWriter::Write(
    UsdPrim&                  targetPrim,
    bool                      applyOffsetTransform,
    const MaxUsd::ExportTime& time)
{
    INode* sourceNode = GetNode();

    const auto& timeVal = time.GetMaxTime();
    const auto& usdTimeCode = time.GetUsdTime();

    Object* obj = sourceNode->EvalWorldState(timeVal).obj;

    const auto maxCamera = dynamic_cast<GenCamera*>(obj);
    if (maxCamera == nullptr) {
        return false;
    }

    auto stage = targetPrim.GetStage();

    pxr::UsdGeomCamera usdCamera { targetPrim };

    if (time.IsFirstFrame()) {
        // Projection type is not animatable, only need to set that up on the first frame we export.
        pxr::TfToken projectionType = maxCamera->IsOrtho() ? pxr::UsdGeomTokens->orthographic
                                                           : pxr::UsdGeomTokens->perspective;
        usdCamera.CreateProjectionAttr().Set(projectionType, pxr::UsdTimeCode::Default());
    }

    const auto& displayTimeIndependentWarnings = time.IsFirstFrame();

    // Clipping range:
    if (maxCamera->GetManualClip() != 0) {
        float nearDistance = maxCamera->GetClipDist(timeVal, CAM_HITHER_CLIP);
        float farDistance = maxCamera->GetClipDist(timeVal, CAM_YON_CLIP);

        if (nearDistance + FLT_EPSILON > FLT_MIN && farDistance + FLT_EPSILON > FLT_MIN) {
            pxr::GfVec2f clippingRange { nearDistance, farDistance };
            usdCamera.CreateClippingRangeAttr().Set(clippingRange, usdTimeCode);
        }
    }

    const auto maxPhysicalCamera = dynamic_cast<MaxSDK::IPhysicalCamera*>(obj);
    if (maxPhysicalCamera) {
        const auto camParamBlock = maxPhysicalCamera->GetParamBlock(0);
        Interval   valid = FOREVER;
        // Focus Distance
        {
            float focusDistance = camParamBlock->GetInt(10 /*pb_specify_focus*/, timeVal, valid)
                ? camParamBlock->GetFloat(9 /*pb_focus_distance*/, timeVal, valid)
                : maxCamera->GetTDist(timeVal);
            usdCamera.CreateFocusDistanceAttr().Set(focusDistance, usdTimeCode);
        }

        // Focal Length
        // The use of the effective lens focal length would counteract lens breathing
        float focal = maxPhysicalCamera->GetEffectiveLensFocalLength(timeVal, valid)
            * static_cast<float>(GetSystemUnitScale(UNITS_MILLIMETERS));
        usdCamera.CreateFocalLengthAttr().Set(focal, usdTimeCode);

        // Aperture
        // in order to compensate for the possible zoom factor
        // the aperture width is changed and is not based on film width
        //    w != maxPhysicalCamera->GetFilmWidth(context.maxTimeCode, FOREVER) *
        //    MaxSDKSupport::units::GetSystemUnitScale(UNITS_MILLIMETERS);
        // if the zoom factor is 1.0, the aperture width is equivalent to film width as expected
        float w = tan(maxCamera->GetFOV(timeVal) / 2.0f) * focal * 2.0f;
        auto  aspect = GetCOREInterface()->GetRendImageAspect();
        usdCamera.CreateHorizontalApertureAttr().Set(w, usdTimeCode);
        float v = w / aspect;
        usdCamera.CreateVerticalApertureAttr().Set(v, usdTimeCode);

        // Lens Aperture
        float fstop = maxPhysicalCamera->GetLensApertureFNumber(timeVal, valid);
        usdCamera.CreateFStopAttr().Set(fstop, usdTimeCode);

        // shutter open/close (all frame related time)
        // values are converted to USD time frame reference
        auto MaxFrameToUSDTime = [&stage](double time) {
            DbgAssert(stage->GetTimeCodesPerSecond() != 0);
            return time * stage->GetTimeCodesPerSecond() / (GetTicksPerFrame() / 4800.f);
        };

        // only set the open attribute of the property if the camera attribute is enabled
        // otherwise let the shutter offset be set to 0 (default)
        bool offsetEnabled
            = camParamBlock->GetInt(17 /*shutter_offset_enabled*/, timeVal, valid) == 1;
        double shutterOffset
            = offsetEnabled ? maxPhysicalCamera->GetShutterOffsetInFrames(timeVal, valid) : 0.0;
        double shutterDuration = maxPhysicalCamera->GetShutterDurationInFrames(timeVal, valid);
        if (offsetEnabled) {
            usdCamera.CreateShutterOpenAttr().Set(MaxFrameToUSDTime(shutterOffset), usdTimeCode);
        }
        usdCamera.CreateShutterCloseAttr().Set(
            MaxFrameToUSDTime(shutterOffset + shutterDuration), usdTimeCode);

        // exposure
        usdCamera.CreateExposureAttr().Set(
            maxPhysicalCamera->GetEffectiveEV(timeVal, valid), usdTimeCode);

        // Aperture offset
        Point2 offset = maxPhysicalCamera->GetFilmPlaneOffset(timeVal, valid);
        if (offset != Point2(0.0f, 0.0f)) {
            // The offset value we get is a percentage from the film width
            // The negative sign is the offset direction USD applies on the camera
            offset[0] = -(offset[0] * w);
            offset[1] = -(offset[1] * v * aspect);
            usdCamera.CreateHorizontalApertureOffsetAttr().Set(offset[0], usdTimeCode);
            usdCamera.CreateVerticalApertureOffsetAttr().Set(offset[1], usdTimeCode);
        }

        Point2 tilt = maxPhysicalCamera->GetTiltCorrection(timeVal, valid);
        if (tilt != Point2(0.0f, 0.0f)) {
            MaxUsd::Log::Warn(
                L"The tilt correction applied to '{0}' is not supported by USD, and will not get "
                L"exported at timeCode {1}.",
                sourceNode->GetName(),
                usdTimeCode.GetValue());
        }

        // bokeh - depth of field
        // not supported
        {
            if (maxPhysicalCamera->GetBokehShape(timeVal, valid)
                    != MaxSDK::IPhysicalCamera::BokehShape::Circular
                || maxPhysicalCamera->GetBokehCenterBias(timeVal, valid) != 0.0f
                || maxPhysicalCamera->GetBokehOpticalVignetting(timeVal, valid) != 0.0f
                || maxPhysicalCamera->GetBokehAnisotropy(timeVal, valid) != 0.0f) {
                MaxUsd::Log::Warn(
                    L"The Bokeh settings of '{0}' is not supported by USD, and will not get "
                    L"exported "
                    L"at timeCode {1}.",
                    sourceNode->GetName(),
                    usdTimeCode.GetValue());
            }
        }

        // lens distortion
        // not supported
        {
            if (maxPhysicalCamera->GetLensDistortionType(timeVal, valid)
                != MaxSDK::IPhysicalCamera::LensDistortionType::None) {
                MaxUsd::Log::Warn(
                    L"Lens distortion settings of '{0}' is not supported by USD, and will not get "
                    L"exported at timeCode {1}.",
                    sourceNode->GetName(),
                    usdTimeCode.GetValue());
            }
        }
    } else // if not a PhysicalCamera
    {
        MSTR cameraClassName;
        maxCamera->GetClassName(cameraClassName);
        int cameraType = maxCamera->Type();

        if (displayTimeIndependentWarnings) {
            MaxUsd::Log::Warn(
                L"Limited support on '{0}[{1}]' cameras ('{2}'). Use a physical camera to get best "
                L"results.",
                (cameraType == 0
                     ? _T("Free Camera")
                     : (cameraType == 1 ? _T("Target Camera") : _T("Orthographic Camera"))),
                cameraClassName.data(),
                sourceNode->GetName());
        }

        // Focus Distance
        // only set the Focus Distance attribute on target camera
        if (maxCamera->Type() != 0 /* free camera */) {
            // The value returned from camera.GetTDist() doesn't update over animations.
            // Calculate the targetDistance ourselves.
            float      targetDistance;
            const auto target = sourceNode->GetTarget();
            if (!target) {
                if (displayTimeIndependentWarnings) {
                    MaxUsd::Log::Error(
                        L"Unable to recompute the target distance for camera {0}.",
                        sourceNode->GetName());
                }
                targetDistance = maxCamera->GetTDist(timeVal);
            } else {
                const Point3 targetPos = target->GetNodeTM(timeVal).GetTrans();
                const Point3 cameraPos = sourceNode->GetNodeTM(timeVal).GetTrans();
                targetDistance = Length(targetPos - cameraPos);
            }
            usdCamera.CreateFocusDistanceAttr().Set(targetDistance, usdTimeCode);
        }
        Interval valid = FOREVER;
        // Not taking into account the Multi-Pass Focal Depth that could be specified by the user
        if (maxCamera->GetMultiPassEffectEnabled(timeVal, valid)) {
            MaxUsd::Log::Warn(
                L"The Multi-Pass Effect on '{0}' will not get exported at timeCode {1}.",
                sourceNode->GetName(),
                usdTimeCode.GetValue());
        }

        // Focal Length
        {
            // classic FOV equation
            // see maxsdk\samples\objects\camera.h:	float FOVtoMM(float fov);
            // focal and aperture in mm and is not subjected to units translation
            float w = GetCOREInterface()->GetRendApertureWidth();
            float focal;
            float tanFov = tan(maxCamera->GetFOV(timeVal) / 2.0f);
            if (tanFov == 0.0f) {
                focal = FLT_MAX;
            } else {
                focal = float((0.5f * w) / tanFov);
            }
            usdCamera.CreateFocalLengthAttr().Set(focal, usdTimeCode);
        }
        // Aperture
        {
            auto aspect = GetCOREInterface()->GetRendImageAspect();
            // aperture in mm and is not subjected to units translation
            float w = GetCOREInterface()->GetRendApertureWidth();
            usdCamera.CreateHorizontalApertureAttr().Set(w);

            float verticalAperture;
            if (aspect == 0.0f) {
                verticalAperture = FLT_MAX;
            } else {
                verticalAperture = w / aspect;
            }
            usdCamera.CreateVerticalApertureAttr().Set(verticalAperture);
        }
    }
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE