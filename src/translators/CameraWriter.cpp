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

#include "splineUtils.h"

#include <MaxUsd/Translators/primWriter.h>
#include <MaxUsd/Translators/writeJobContext.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>

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

#ifdef USD_CURVES_SUPPORTED
        if (GetExportArgs().GetAnimationType()
                != MaxUsd::USDSceneBuilderOptions::AnimationType::TimeSamples
            && time.IsFirstFrame()) {
            MaxUsd::Log::Warn(
                L"Clipping range is not supported in USD Splines. Defaulting to export Clipping "
                L"Range as time samples for node {0}",
                sourceNode->GetName());
        }
#endif
    }

#ifdef USD_CURVES_SUPPORTED
    const bool exportCurves = GetExportArgs().GetAnimationType()
            != MaxUsd::USDSceneBuilderOptions::AnimationType::TimeSamples
        && time.IsFirstFrame();
    const bool exportTimeSamples = GetExportArgs().GetAnimationType()
        != MaxUsd::USDSceneBuilderOptions::AnimationType::Curves;
#endif

    const auto maxPhysicalCamera = dynamic_cast<MaxSDK::IPhysicalCamera*>(obj);
    if (maxPhysicalCamera) {
        const auto camParamBlock = maxPhysicalCamera->GetParamBlock(0);
        Interval   valid = FOREVER;

        auto MaxFrameToUSDTime = [&stage](double time) {
            DbgAssert(stage->GetTimeCodesPerSecond() != 0);
            return time * stage->GetTimeCodesPerSecond() / (GetTicksPerFrame() / 4800.f);
        };

        // Focus Distance
        {
            const bool specifyFocus
                = camParamBlock->GetInt(10 /*pb_specify_focus*/, timeVal, valid);
            auto focusDistanceAttr = usdCamera.CreateFocusDistanceAttr();
#ifdef USD_CURVES_SUPPORTED
            if (exportCurves) {
                if (specifyFocus) {
                    MaxSDKSupport::WriteSplineAttribute<float>(
                        stage,
                        camParamBlock->GetControllerByID(9 /*pb_focus_distance*/),
                        targetPrim,
                        focusDistanceAttr);
                } else {
                    // if the focus distance is not specified, we use the target distance
                    // which is not animated in max
                    float targetDistance = maxCamera->GetTDist(timeVal);
                    focusDistanceAttr.Set(targetDistance);
                }
            }
            if (exportTimeSamples)
#endif
            {
                float focusDistance = specifyFocus
                    ? camParamBlock->GetFloat(9 /*pb_focus_distance*/, timeVal, valid)
                    : maxCamera->GetTDist(timeVal);
                focusDistanceAttr.Set(focusDistance, usdTimeCode);
            }
        }

        // Focal Length
        // The use of the effective lens focal length would counteract lens breathing
        // Perspective focal length in tenths of a scene unit
        {
            auto focalLengthAttribute = usdCamera.CreateFocalLengthAttr();
#ifdef USD_CURVES_SUPPORTED
            if (exportCurves) {
                if (auto focalLengthController
                    = camParamBlock->GetControllerByID(5 /*pb_focal_length_mm*/)) {
                    // This param block doesn't match the "effective focal length" function
                    // call. Thus, create the spline then replace the knot values with the
                    // "correct" one.
                    TsSpline focalLengthSpline = MaxSDKSupport::CreateSplineFromControl<float>(
                        stage, focalLengthController);
                    auto focalLengthKnots = focalLengthSpline.GetKnots();
                    for (auto& knot : focalLengthKnots) {
                        auto knotTime = MaxUsd::GetTimeValueFromFrame(knot.GetTime());
                        knot.SetValue(
                            maxPhysicalCamera->GetEffectiveLensFocalLength(knotTime, valid) * 10.f);
                    }

                    if (!focalLengthKnots.empty()) {
                        focalLengthSpline.SetKnots(focalLengthKnots);
                        focalLengthAttribute.SetSpline(focalLengthSpline);
                    }
                } else {
                    focalLengthAttribute.Set(
                        maxPhysicalCamera->GetEffectiveLensFocalLength(timeVal, valid) * 10.f);
                }
            }

            if (exportTimeSamples)
#endif
            {
                float focal = maxPhysicalCamera->GetEffectiveLensFocalLength(timeVal, valid) * 10.f;
                focalLengthAttribute.Set(focal, usdTimeCode);
            }
        }

        // Aperture
        // in order to compensate for the possible zoom factor
        // the aperture width is changed and is not based on film width
        //    w != maxPhysicalCamera->GetFilmWidth(context.maxTimeCode, FOREVER) *
        //    MaxSDKSupport::units::GetSystemUnitScale(UNITS_MILLIMETERS);
        // if the zoom factor is 1.0, the aperture width is equivalent to film width as expected
        const auto aspect = GetCOREInterface()->GetRendImageAspect();
#ifdef USD_CURVES_SUPPORTED
        TsSpline horizontalApertureSpline, verticalApertureSpline;
        {
            if (exportCurves) {
                if (auto fovController = camParamBlock->GetControllerByID(20 /*pb_fov*/)) {
                    TsSpline focalLengthSpline = MaxSDKSupport::CreateSplineFromControl<float>(
                        stage, camParamBlock->GetControllerByID(5 /*pb_focal_length_mm*/));
                    horizontalApertureSpline
                        = MaxSDKSupport::CreateSplineFromControl<float>(stage, fovController);
                    TsKnotMap verticalApertureKnots;
                    auto      horizontalApertureKnots = horizontalApertureSpline.GetKnots();
                    for (auto& knot : horizontalApertureKnots) {
                        auto  knotTime = MaxUsd::GetTimeValueFromFrame(knot.GetTime());
                        float focalLength;
                        focalLengthSpline.Eval(knotTime, &focalLength);
                        auto aperture
                            = tan(maxCamera->GetFOV(MaxUsd::GetTimeValueFromFrame(knotTime)) / 2.f)
                            * focalLength * 2.f;
                        knot.SetValue(aperture);

                        auto verticalKnot = knot;
                        verticalKnot.SetValue(aperture / aspect);
                        verticalApertureKnots.insert(verticalKnot);
                    }

                    if (!horizontalApertureKnots.empty()) {
                        horizontalApertureSpline.SetKnots(horizontalApertureKnots);
                        usdCamera.CreateHorizontalApertureAttr().SetSpline(
                            horizontalApertureSpline);
                    } else {
                        usdCamera.CreateHorizontalApertureAttr().Set(
                            tan(maxCamera->GetFOV(timeVal) / 2.0f)
                            * maxPhysicalCamera->GetEffectiveLensFocalLength(timeVal, valid) * 10.f
                            * 2.0f);
                    }

                    if (!verticalApertureKnots.empty()) {
                        verticalApertureSpline.SetKnots(verticalApertureKnots);
                        usdCamera.CreateVerticalApertureAttr().SetSpline(verticalApertureSpline);
                    } else {
                        usdCamera.CreateVerticalApertureAttr().Set(
                            tan(maxCamera->GetFOV(timeVal) / 2.0f)
                            * maxPhysicalCamera->GetEffectiveLensFocalLength(timeVal, valid) * 10.f
                            * 2.0f / aspect);
                    }
                }
            }

            if (exportTimeSamples)
#endif
            {
                float focal = maxPhysicalCamera->GetEffectiveLensFocalLength(timeVal, valid) * 10.f;
                float w = tan(maxCamera->GetFOV(timeVal) / 2.0f) * focal * 2.0f;
                usdCamera.CreateHorizontalApertureAttr().Set(w, usdTimeCode);
                float v = w / aspect;
                usdCamera.CreateVerticalApertureAttr().Set(v, usdTimeCode);
            }
#ifdef USD_CURVES_SUPPORTED
        }
#endif

        // Lens Aperture
        {
            auto fStopAttribute = usdCamera.CreateFStopAttr();
#ifdef USD_CURVES_SUPPORTED
            if (exportCurves) {
                if (auto fStopController = camParamBlock->GetControllerByID(6 /*pb_f_stop*/)) {
                    MaxSDKSupport::WriteSplineAttribute<float>(
                        stage, fStopController, targetPrim, fStopAttribute);
                } else {
                    fStopAttribute.Set(maxPhysicalCamera->GetLensApertureFNumber(timeVal, valid));
                }
            }

            if (exportTimeSamples)
#endif
            {
                float fstop = maxPhysicalCamera->GetLensApertureFNumber(timeVal, valid);
                fStopAttribute.Set(fstop, usdTimeCode);
            }
        }

        // only set the open attribute of the property if the camera attribute is enabled
        // otherwise let the shutter offset be set to 0 (default)
        {
            bool offsetEnabled
                = camParamBlock->GetInt(17 /*shutter_offset_enabled*/, timeVal, valid) == 1;
#ifdef USD_CURVES_SUPPORTED
            if (exportCurves) {

                TsSpline shutterOffsetSpline(TfType::Find<double>()),
                    shutterDurationSpline(TfType::Find<double>());
                switch (camParamBlock->GetInt(12 /*pb_shutter_unit_type*/, timeVal, valid)) {
                case 2:   // PBShutterType_Degrees
                case 3: { // PBShutterType_Frames
                    if (offsetEnabled) {
                        shutterOffsetSpline = MaxSDKSupport::CreateSplineFromControl<double>(
                            stage,
                            camParamBlock->GetControllerByID(16 /*pb_shutter_offset_relative*/),
                            [MaxFrameToUSDTime](double shutterOffset) {
                                return MaxFrameToUSDTime(shutterOffset);
                            });
                    }
                    shutterDurationSpline = MaxSDKSupport::CreateSplineFromControl<double>(
                        stage,
                        camParamBlock->GetControllerByID(15 /*pb_shutter_length_relative*/),
                        [MaxFrameToUSDTime](double shutterLength) {
                            return MaxFrameToUSDTime(shutterLength);
                        });
                    break;
                }
                case 0: // PBShutterType_OneOverSeconds
                case 1: // PBShutterType_Seconds
                default: {
                    if (offsetEnabled) {
                        shutterOffsetSpline = MaxSDKSupport::CreateSplineFromControl<double>(
                            stage,
                            camParamBlock->GetControllerByID(14 /*pb_shutter_offset_absolute*/),
                            [MaxFrameToUSDTime](double shutterOffset) {
                                return MaxFrameToUSDTime(
                                    shutterOffset * static_cast<double>(GetFrameRate()));
                            });
                    }
                    shutterDurationSpline = MaxSDKSupport::CreateSplineFromControl<double>(
                        stage,
                        camParamBlock->GetControllerByID(13 /*pb_shutter_length_absolute*/),
                        [MaxFrameToUSDTime](double duration) {
                            return MaxFrameToUSDTime(
                                duration * static_cast<double>(GetFrameRate()));
                        });
                    break;
                }
                }

                if (!shutterOffsetSpline.GetKnots().empty()) {
                    usdCamera.CreateShutterOpenAttr().SetSpline(shutterOffsetSpline);
                }

                if (!shutterDurationSpline.GetKnots().empty()) {
                    usdCamera.CreateShutterCloseAttr().SetSpline(
                        MaxSDKSupport::CombineSplines<double>(
                            shutterOffsetSpline,
                            shutterDurationSpline,
                            [](double v1, double v2) -> double { return v1 + v2; }));
                }
            }

            if (exportTimeSamples)
#endif
            {
                double shutterOffset = offsetEnabled
                    ? maxPhysicalCamera->GetShutterOffsetInFrames(timeVal, valid)
                    : 0.0;
                double shutterDuration
                    = maxPhysicalCamera->GetShutterDurationInFrames(timeVal, valid);
                if (offsetEnabled) {
                    usdCamera.CreateShutterOpenAttr().Set(
                        MaxFrameToUSDTime(shutterOffset), usdTimeCode);
                }
                usdCamera.CreateShutterCloseAttr().Set(
                    MaxFrameToUSDTime(shutterOffset + shutterDuration), usdTimeCode);
            }
        }

        // exposure
        {
            auto exposureAttribute = usdCamera.CreateExposureAttr();
#ifdef USD_CURVES_SUPPORTED
            if (exportCurves) {
                if (auto exposureController
                    = camParamBlock->GetControllerByID(24 /*pb_exposure_value*/)) {
                    MaxSDKSupport::WriteSplineAttribute<float>(
                        stage, exposureController, targetPrim, exposureAttribute);
                } else {
                    exposureAttribute.Set(maxPhysicalCamera->GetEffectiveEV(timeVal, valid));
                }
            }

            if (exportTimeSamples)
#endif
            {
                exposureAttribute.Set(
                    maxPhysicalCamera->GetEffectiveEV(timeVal, valid), usdTimeCode);
            }
        }

        // Aperture offset
        {
#ifdef USD_CURVES_SUPPORTED
            if (exportCurves) {
                auto multiplySplines = [](TsSpline& spline1, const TsSpline& spline2) {
                    auto knots = spline1.GetKnots();
                    for (auto& knot : knots) {
                        float value1;
                        knot.GetValue(&value1);

                        float value2 = 0;
                        spline2.Eval(knot.GetTime(), &value2);
                        knot.SetValue(value1 * value2);
                    }

                    if (!knots.empty()) {
                        spline1.SetKnots(knots);
                    }
                };

                auto lensHorizontalShiftSpline = MaxSDKSupport::CreateSplineFromControl<float>(
                    stage,
                    camParamBlock->GetControllerByID(39 /*pb_lens_horizontal_shift*/),
                    [](float horizontalShift) { return -horizontalShift; });
                multiplySplines(lensHorizontalShiftSpline, horizontalApertureSpline);
                if (!lensHorizontalShiftSpline.GetKnots().empty()) {
                    usdCamera.CreateHorizontalApertureOffsetAttr().SetSpline(
                        lensHorizontalShiftSpline);
                }

                auto lensVerticalShiftSpline = MaxSDKSupport::CreateSplineFromControl<float>(
                    stage,
                    camParamBlock->GetControllerByID(40 /*pb_lens_vertical_shift*/),
                    [](float verticalShift) { return -verticalShift; });
                multiplySplines(lensVerticalShiftSpline, horizontalApertureSpline);
                if (!lensVerticalShiftSpline.GetKnots().empty()) {
                    usdCamera.CreateVerticalApertureOffsetAttr().SetSpline(lensVerticalShiftSpline);
                }
            }
            if (exportTimeSamples)
#endif
            {
                Point2 offset = maxPhysicalCamera->GetFilmPlaneOffset(timeVal, valid);
                if (offset != Point2(0.0f, 0.0f)) {
                    // The offset value we get is a percentage from the film width
                    // The negative sign is the offset direction USD applies on the camera
                    float focal
                        = maxPhysicalCamera->GetEffectiveLensFocalLength(timeVal, valid) * 10.f;
                    float w = tan(maxCamera->GetFOV(timeVal) / 2.0f) * focal * 2.0f;
                    offset[0] = -(offset[0] * w);
                    offset[1] = -(offset[1] * w);
                    usdCamera.CreateHorizontalApertureOffsetAttr().Set(offset[0], usdTimeCode);
                    usdCamera.CreateVerticalApertureOffsetAttr().Set(offset[1], usdTimeCode);
                }
            }
        }

        Point2 tilt = maxPhysicalCamera->GetTiltCorrection(timeVal, valid);
        if (tilt != Point2(0.0f, 0.0f)) {
            MaxUsd::Log::Warn(
                L"The tilt correction applied to '{0}' is not supported by USD, and will not "
                L"get "
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
                    L"exported at timeCode {1}.",
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
                    L"Lens distortion settings of '{0}' is not supported by USD, and will not "
                    L"get exported at timeCode {1}.",
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
        // Not taking into account the Multi-Pass Focal Depth that could be specified by the
        // user
        if (maxCamera->GetMultiPassEffectEnabled(timeVal, valid)) {
            MaxUsd::Log::Warn(
                L"The Multi-Pass Effect on '{0}' will not get exported at timeCode {1}.",
                sourceNode->GetName(),
                usdTimeCode.GetValue());
        }

#ifdef USD_CURVES_SUPPORTED
        if (GetExportArgs().GetAnimationType()
                != MaxUsd::USDSceneBuilderOptions::AnimationType::TimeSamples
            && time.IsFirstFrame()) {
            // Focal Length
            {
                float w = GetCOREInterface()->GetRendApertureWidth();
                auto  calculateFocal = [w](float tanFov) {
                    if (tanFov != 0) {
                        return float((0.5f * w) / tanFov);
                    }
                    return FLT_MAX; // Focal length is infinite
                };
                if (auto focalLengthController = maxCamera->GetFOVControl()) {

                    TsSpline focalLengthSpline = MaxSDKSupport::CreateSplineFromControl<float>(
                        stage, focalLengthController);
                    auto focalLengthKnots = focalLengthSpline.GetKnots();
                    for (auto& knot : focalLengthKnots) {
                        auto knotTime = MaxUsd::GetTimeValueFromFrame(knot.GetTime());
                        knot.SetValue(calculateFocal(tan(maxCamera->GetFOV(knotTime) / 2.0f)));
                    }

                    if (!focalLengthKnots.empty()) {
                        focalLengthSpline.SetKnots(focalLengthKnots);
                        usdCamera.CreateFocalLengthAttr().SetSpline(focalLengthSpline);
                    }
                } else {
                    usdCamera.CreateFocalLengthAttr().Set(
                        calculateFocal(tan(maxCamera->GetFOV(timeVal) / 2.0f)));
                }
            }
        }

        if (GetExportArgs().GetAnimationType()
            != MaxUsd::USDSceneBuilderOptions::AnimationType::Curves) {
#endif
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
#ifdef USD_CURVES_SUPPORTED
        }
#endif
        // Aperture
        {
            // Not frame dependent and not animated, not need to set multiple times
            if (time.IsFirstFrame()) {
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
    }
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
