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
#include "SunPositionerWriter.h"

#include <MaxUsd/Translators/primWriter.h>
#include <MaxUsd/Translators/writeJobContext.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/shadowAPI.h>

#include <DayLightSimulation/IPhysicalSunSky.h>
#include <DayLightSimulation/ISunPositioner.h>
#include <linshape.h>
#include <toneOp.h>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdSunPositionerWriter::MaxUsdSunPositionerWriter(
    const MaxUsdWriteJobContext& jobCtx,
    INode*                       node)
    : MaxUsdPrimWriter(jobCtx, node)
{
}

MaxUsdPrimWriter::ContextSupport
MaxUsdSunPositionerWriter::CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    if (!exportArgs.GetTranslateLights()) {
        return ContextSupport::Unsupported;
    }
    const auto object = node->EvalWorldState(exportArgs.GetResolvedTimeConfig().GetStartTime()).obj;
    if (object->IsSubClassOf(MaxSDK::ISunPositioner::GetClassID())) {
        return ContextSupport::Fallback;
    }
    return ContextSupport::Unsupported;
}

MaxUsd::XformSplitRequirement MaxUsdSunPositionerWriter::RequiresXformPrim()
{
    return MaxUsd::XformSplitRequirement::Always;
}

Interval MaxUsdSunPositionerWriter::GetValidityInterval(const TimeValue& time)
{
    // The object validity on the SunPositioner is not properly reporting on the
    // validity interval of the node if the sun positioner is set up
    // not to use a weather file (aka using the 'date, time & location').
    // We need to manually combine those validity intervals.
    INode* sourceNode = GetNode();

    // Start from the object validity interval..
    const auto obj = sourceNode->EvalWorldState(time).obj;
    auto       validityInterval = obj->ObjectValidity(time);

    Interval      sunSkyEnvVal = FOREVER;
    Texmap* const envmap = GetCOREInterface()->GetEnvironmentMap();
    // If the sun positioner object is not using the default shader, this will map to nullptr
    MaxSDK::IPhysicalSunSky* sunskyenv = dynamic_cast<MaxSDK::IPhysicalSunSky*>(envmap);
    if (sunskyenv) {
        sunskyenv->InstantiateShader(time, sunSkyEnvVal);
    }
    MaxSDK::ISunPositioner* maxSunPositioner = static_cast<MaxSDK::ISunPositioner*>(obj);
    maxSunPositioner->GetSunDirection(time, sunSkyEnvVal);

    // Intersect the intervals..
    validityInterval &= sunSkyEnvVal;
    return validityInterval;
}

bool MaxUsdSunPositionerWriter::Write(
    UsdPrim&                  targetPrim,
    bool                      applyOffsetTransform,
    const MaxUsd::ExportTime& time)
{
    INode* sourceNode = GetNode();

    const auto& timeVal = time.GetMaxTime();
    const auto& usdTimeCode = time.GetUsdTime();

    const auto obj = sourceNode->EvalWorldState(timeVal).obj;

    MaxSDK::ISunPositioner* maxSunPositioner = dynamic_cast<MaxSDK::ISunPositioner*>(obj);
    if (maxSunPositioner == nullptr) {
        return false;
    }

    auto stage = targetPrim.GetStage();
    auto primPath = targetPrim.GetPath();

    // TODO - the SunPositioner would translate better into a pxr:UsdRiPxrEnvDayLight but it does
    // not work for now It is a render delegate support issue. RenderMan 23 render delegate is not
    // supporting this for now.
    // https://graphics.pixar.com/usd/docs/api/class_usd_ri_pxr_env_day_light.html
    pxr::UsdLuxDistantLight usdDistantLightPrim = pxr::UsdLuxDistantLight::Define(stage, primPath);

    Texmap* const envmap = GetCOREInterface()->GetEnvironmentMap();
    // if the sun positioner object is not using the default shader, this will map to nullptr
    MaxSDK::IPhysicalSunSky* sunskyenv = dynamic_cast<MaxSDK::IPhysicalSunSky*>(envmap);

    // On the first frame, translate some non-animatable properties.
    if (time.IsFirstFrame()) {

        if (sunskyenv) {
            // replace the default intensity for distant light
            // important to note, here, the color attribute has an intensity component to it (see
            // sun shader below) the value for intensity is a multiplier required based on trials
            // and errors
            usdDistantLightPrim.CreateIntensityAttr().Set(3.f, pxr::UsdTimeCode::Default());
        } else {
            MaxUsd::Log::Warn(
                L"The SunPositioner '{0}' is not using an environment map derived from "
                L"MaxSDK::IPhysicalSunSky. "
                L"The light color and intensity will not properly be exported.",
                sourceNode->GetName());
        }
    }

    pxr::UsdGeomXformable xformable(usdDistantLightPrim.GetPrim());

    // Write animatable properties.

    // properly align distant light along the direction of the sun for the z-axis
    // direction towards the sun - a unit vector pointing towards the sun, in object space
    Interval valid = FOREVER;
    auto     sunDirection = maxSunPositioner->GetSunDirection(timeVal, valid);
    Point3   x = Point3(0.f, 1.f, 0.f) ^ sunDirection;
    x.Normalize();
    Point3 y = sunDirection ^ x;
    y.Normalize();
    Matrix3 sunOrientation(x, y, sunDirection, Point3());

    // Only define the xformOp once.
    if (!usdGeomXFormOp.IsDefined()) {
        usdGeomXFormOp = xformable.AddXformOp(
            pxr::UsdGeomXformOp::TypeTransform,
            pxr::UsdGeomXformOp::PrecisionDouble,
            pxr::TfToken("sunDirection"));
    }
    usdGeomXFormOp.Set(MaxUsd::ToUsd(sunOrientation), usdTimeCode);

    std::unique_ptr<MaxSDK::IPhysicalSunSky::IShader> sunShader;
    if (sunskyenv) {
        Interval interval = FOREVER;
        sunShader = sunskyenv->InstantiateShader(timeVal, interval);
    }

    // sun color with an intensity component part of its value
    if (sunShader) {
        Color                        sunColor = sunShader->Evaluate(sunDirection);
        ToneOperatorInterface* const toneOpInt
            = dynamic_cast<ToneOperatorInterface*>(GetCOREInterface(TONE_OPERATOR_INTERFACE));
        ToneOperator* const toneOp
            = (toneOpInt != nullptr) ? toneOpInt->GetToneOperator() : nullptr;
        if (toneOp != nullptr) {
            toneOp->ScaledToRGB(sunColor);
        }
        pxr::GfVec3f usdLightColor { sunColor[0], sunColor[1], sunColor[2] };
        usdDistantLightPrim.CreateColorAttr().Set(usdLightColor, usdTimeCode);
    }

    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE