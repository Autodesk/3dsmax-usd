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
#include "CameraConverter.h"

#include <MaxUsd/Translators/ShadingModeExporterContext.h>
#include <MaxUsd/Translators/ShadingModeRegistry.h>
#include <MaxUsd/Translators/TranslatorUtils.h>
#include <MaxUsd/Utilities/MathUtils.h>

namespace MAXUSD_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

void CameraConverter::ToPhysicalCamera(
    const pxr::UsdGeomCamera&   usdCamera,
    MaxSDK::IPhysicalCamera*    maxCamera,
    const MaxUsdReadJobContext& readContext)
{
    const auto prim = usdCamera.GetPrim();

    TimeValue defaultTimeValue
        = MaxUsd::GetMaxTimeValueFromUsdTimeCode(prim.GetStage(), UsdTimeCode::Default());

    maxCamera->SetHorzLineState(TRUE);
    maxCamera->Enable(TRUE);

    // Projection type:
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdCamera.GetProjectionAttr(),
        [maxCamera](const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
            TfToken projectionType = value.Get<TfToken>();
            maxCamera->SetOrtho(projectionType == UsdGeomTokens->orthographic);
            return true;
        },
        readContext,
        false /* if not authored, the projection type default will be applied */);

    // Clipping range:
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdCamera.GetClippingRangeAttr(),
        [maxCamera](const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
            GfVec2f clippingRange = value.Get<GfVec2f>();
            float   nearDistance = std::min(clippingRange[0], clippingRange[1]);
            float   farDistance = std::max(clippingRange[0], clippingRange[1]);

            maxCamera->SetManualClip(TRUE);
            maxCamera->SetClipDist(timeValue, CAM_HITHER_CLIP, nearDistance);
            maxCamera->SetClipDist(timeValue, CAM_YON_CLIP, farDistance);
            return true;
        },
        readContext);

    // Focus Distance
    MaxUsdTranslatorUtil::ReadUsdAttribute(
			usdCamera.GetFocusDistanceAttr(),
			[maxCamera, prim](const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
				float focus = value.Get<float>();

				// the Focus Distance shall not be set to 0.f
				if (focus == 0.0)
				{
					// set a default Focus Distance for the camera to properly work
					// based on the default Free Camera setting
					focus = 160.f;
					MaxUsd::Log::Warn("Focus Distance is set to '0.0f' for camera '{0}'. Setting value to '160.f' to "
									  "get a minimal working camera.",
							prim.GetName().GetString());
				}
				maxCamera->SetTDist(timeValue, focus);
				return true;
			},
			readContext,
			false /* if not authored, 'focus' will be set to 0 and a default will be applied by this method */);

    // USD Cameras are not targeted.
    auto camParamBlock = maxCamera->GetParamBlock(0);
    camParamBlock->SetValueByName(L"targeted", false, 0);

    // Aperture horizontal (and its dependencies0
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdCamera.GetHorizontalApertureAttr(),
        [camParamBlock, usdCamera, prim, defaultTimeValue](
            const VtValue& value, const UsdTimeCode& timeCode, const TimeValue& timeValue) {
            float horizontalAperture = value.Get<float>();

            // Setting pb_film_width_mm in the physical camera is slow, because it loads presets, in
            // order to set pb_film_preset to the "custom" preset. We can speed this up by checking
            // if the preset is already on "Custom", and if so, directly assigning the value,
            // instead of going through the setter. The setter also invalidates the camera
            // internally - which this approach wont do. But the next call, to setup the length
            // breathing will, so we should be ok.
            const MCHAR* presetNameStr = nullptr;
            Interval     valid = FOREVER;
            camParamBlock->GetValue(
                55 /*pb_film_preset*/, GetCOREInterface()->GetTime(), presetNameStr, valid);
            const auto preset = std::wstring(presetNameStr);
            if (L"Custom" != preset) {
                camParamBlock->SetValue(4 /*pb_film_width_mm*/, timeValue, horizontalAperture);
            } else {
                PB2Value& raw = camParamBlock->GetPB2Value(4, timeValue);
                raw.f = horizontalAperture;
            }

            // the lens breathing multiplier shall be set to 0 in order to
            // keep the EffectiveLensFocalLength (used at export) equal to FilmWidth
            camParamBlock->SetValue(54 /*pb_lens_breathing_amount*/, defaultTimeValue, 0.0f);

            // horizontal offset
            float horizontalApertureOffset = 0.0f;
            if (usdCamera.GetHorizontalApertureOffsetAttr().IsAuthored()
                && usdCamera.GetHorizontalApertureOffsetAttr().Get(
                    &horizontalApertureOffset, timeCode)) {
                // the value is stored has a percentage of the aperture size
                camParamBlock->SetValue(
                    39 /*pb_lens_horizontal_shift*/,
                    timeValue,
                    -(horizontalApertureOffset / horizontalAperture));
            }

            // vertical
            float verticalAperture = 0.0f;
            if (usdCamera.GetVerticalApertureAttr().IsAuthored()
                && usdCamera.GetVerticalApertureAttr().Get(&verticalAperture, timeCode)) {
                float aspect = horizontalAperture / verticalAperture;
                if (GetCOREInterface()->GetRendImageAspect() != aspect) {
                    MaxUsd::Log::Warn(
                        "Vertical aperture is not imported for cameras. The aspect ratio ({0}) on "
                        "'{1}' cannot stay the same in 3ds Max.",
                        aspect,
                        prim.GetName().GetString());
                }

                // vertical offset
                float verticalApertureOffset = 0.0f;
                if (usdCamera.GetVerticalApertureOffsetAttr().IsAuthored()
                    && usdCamera.GetVerticalApertureOffsetAttr().Get(
                        &verticalApertureOffset, timeCode)) {
                    // the value is stored has a percentage of the aperture size
                    camParamBlock->SetValue(
                        40 /*pb_lens_vertical_shift*/,
                        timeValue,
                        -(verticalApertureOffset / verticalAperture / aspect));
                }
            }
            return true;
        },
        readContext);

    // Focal Length
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdCamera.GetFocalLengthAttr(),
        [maxCamera, camParamBlock, defaultTimeValue](
            const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
            // the focal length is exported using the FOV and the Horizontal Aperture values
            // both the focal length and zoom factor have an influence on the FOV value
            // on import, only the focal length is kept has a the retained value
            // zoom multiplier has no influence
            camParamBlock->SetValue(
                19 /*pb_fov_specify*/, defaultTimeValue, 0); // force uncheck 'Specify FOV'
            camParamBlock->SetValue(7 /*pb_lens_zoom*/, defaultTimeValue, 1.0f);
            camParamBlock->SetValue(5 /*pb_focal_length_mm*/, timeValue, value.Get<float>());
            return true;
        },
        readContext);

    // Lens Aperture
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdCamera.GetFStopAttr(),
        [camParamBlock,
         prim](const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
            float fstop = value.Get<float>();
            // define default value in case attribute is set to 0.f
            if (fstop == 0.0f) {
                fstop = 8.f;
                MaxUsd::Log::Warn(
                    "FStop is set to '0.0' for camera '{0}'. Setting value to '8', the default "
                    "value "
                    "on a Physical camera, to let the camera see something.",
                    prim.GetName().GetString());
            }
            camParamBlock->SetValue(6 /*pb_f_stop*/, timeValue, fstop);
            return true;
        },
        readContext);

    // shutter
    // default shutter unit type
    camParamBlock->SetValue(
        12 /*pb_shutter_unit_type*/, defaultTimeValue, 3 /*PBShutterType_Frames*/);
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdCamera.GetShutterOpenAttr(),
        [camParamBlock, usdCamera, prim, defaultTimeValue](
            const VtValue& value, const UsdTimeCode& timeCode, const TimeValue& timeValue) {
            double shutterOffset = value.Get<double>();
            camParamBlock->SetValue(
                16 /*pb_shutter_offset_relative*/,
                timeValue,
                MaxUsd::GetMaxFrameFromUsdFrameTime(prim.GetStage(), shutterOffset));
            camParamBlock->SetValue(
                17 /*pb_shutter_offset_enabled*/, defaultTimeValue, 1 /*true/enabled*/);

            double shutterClose = 0.0f;
            if (usdCamera.GetShutterCloseAttr().IsAuthored()
                && usdCamera.GetShutterCloseAttr().Get(&shutterClose, timeCode)) {
                float maxShutterClose = MaxUsd::GetMaxFrameFromUsdFrameTime(
                    prim.GetStage(), shutterClose - shutterOffset);
                // define default value in case attribute is set to 0.f
                if (shutterClose == 0.0f) {
                    maxShutterClose = 0.5f;
                    MaxUsd::Log::Warn(
                        "Shutter Close attribute is set to '0.0' for camera '{0}'. Setting value "
                        "to "
                        "'0.5', the default value on a Physical camera, to let the camera see "
                        "something.",
                        prim.GetName().GetString());
                }
                camParamBlock->SetValue(
                    15 /*pb_shutter_length_relative*/, timeValue, maxShutterClose);
            }
            return true;
        },
        readContext);

    // exposure
    // TODO - missing 'Install Exposure Control' to be enabled
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdCamera.GetExposureAttr(),
        [camParamBlock, prim, defaultTimeValue](
            const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
            float exposure = value.Get<float>();
            // define default value in case attribute is set to 0.f
            if (exposure == 0.0f) {
                exposure = 6.f;
                MaxUsd::Log::Warn(
                    "Exposure attribute is set to '0.0' for camera '{0}'. Setting value to "
                    "'6' EV, the default "
                    "value on a Physical camera, to prevent rendering the scene all white.",
                    prim.GetName().GetString());
            }
            camParamBlock->SetValue(24 /*pb_exposure_value*/, timeValue, exposure);
            camParamBlock->SetValue(
                22 /*pb_exposure_gain_type*/, defaultTimeValue, 1 /*PBExposureGainType_EV*/);
            return true;
        },
        readContext);
}
} // namespace MAXUSD_NS_DEF
// Disable obscure warning only occurring for 2022 (pixar usd version: 21.11):
// no definition for inline function : pxr::DefaultValueHolder Was not able to identify what is
// triggering this.
#pragma warning(disable : 4506)