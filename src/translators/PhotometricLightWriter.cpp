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
#include "PhotometricLightWriter.h"

#include <MaxUsd/Translators/primWriter.h>
#include <MaxUsd/Translators/writeJobContext.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdLux/boundableLightBase.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shadowAPI.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include <linshape.h>
#include <lslights.h>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdPhotometricLightWriter::MaxUsdPhotometricLightWriter(
    const MaxUsdWriteJobContext& jobCtx,
    INode*                       node)
    : MaxUsdPrimWriter(jobCtx, node)
{
}

MaxUsdPrimWriter::ContextSupport MaxUsdPhotometricLightWriter::CanExport(
    INode*                                node,
    const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    if (!exportArgs.GetTranslateLights()) {
        return ContextSupport::Unsupported;
    }
    const auto object = node->EvalWorldState(exportArgs.GetResolvedTimeConfig().GetStartTime()).obj;
    return object->IsSubClassOf(LIGHTSCAPE_LIGHT_CLASS) ? ContextSupport::Fallback
                                                        : ContextSupport::Unsupported;
}

MaxUsd::XformSplitRequirement MaxUsdPhotometricLightWriter::RequiresXformPrim()
{
    const auto object
        = GetNode()->EvalWorldState(GetExportArgs().GetResolvedTimeConfig().GetStartTime()).obj;
    const Class_ID    photometricLightType = object->ClassID();
    LightscapeLight2* maxPhotometricLight
        = dynamic_cast<LightscapeLight2*>(object->ConvertToType(0, photometricLightType));

    // Special case with cylinder lights and, line and plane lights using a uniform
    // spherical light distribution as those get converted to UsdLuxCylinder type.
    // In USD, the expected orientation is on the x-axis, but 3ds Max has it set on the y-axis
    if (maxPhotometricLight
        && ((photometricLightType == LS_CYLINDER_LIGHT_ID
             || photometricLightType == LS_CYLINDER_LIGHT_TARGET_ID)
            || (photometricLightType == LS_LINEAR_LIGHT_ID
                || photometricLightType == LS_LINEAR_LIGHT_TARGET_ID
                || photometricLightType == LS_AREA_LIGHT_ID
                || photometricLightType == LS_AREA_LIGHT_TARGET_ID)
                && maxPhotometricLight->GetDistribution() == LightscapeLight::ISOTROPIC_DIST)) {
        return MaxUsd::XformSplitRequirement::Always;
    }

    return MaxUsd::XformSplitRequirement::ForOffsetObjects;
}

TfToken MaxUsdPhotometricLightWriter::GetPrimType()
{
    const auto        startTime = GetExportArgs().GetResolvedTimeConfig().GetStartTime();
    const auto        object = GetNode()->EvalWorldState(startTime).obj;
    Class_ID          photometricLightType = object->ClassID();
    LightscapeLight2* maxPhotometricLight
        = dynamic_cast<LightscapeLight2*>(object->ConvertToType(startTime, photometricLightType));

    // All Photometric light types have a 'Target' sibling light type
    // It simply acts as a 'lookat' and has no influence on the light by itself

    // Photometric Point and Disk lights using anything but the spherical light (isotropic)
    // distribution type gets translated into a UsdLuxDiskLight.
    if ((photometricLightType == LS_POINT_LIGHT_ID
         || photometricLightType == LS_POINT_LIGHT_TARGET_ID
         || photometricLightType == LS_DISC_LIGHT_ID
         || photometricLightType == LS_DISC_LIGHT_TARGET_ID)
        && maxPhotometricLight->GetDistribution() != LightscapeLight::ISOTROPIC_DIST) {
        return pxr::MaxUsdPrimTypeTokens->DiskLight;
    }
    // Photometric Line and Area (Rectangle) lights using anything but the spherical light
    // (isotropic) distribution type gets translated into a UsdLuxRectangleLight.
    if ((photometricLightType == LS_LINEAR_LIGHT_ID
         || photometricLightType == LS_LINEAR_LIGHT_TARGET_ID
         || photometricLightType == LS_AREA_LIGHT_ID
         || photometricLightType == LS_AREA_LIGHT_TARGET_ID)
        && maxPhotometricLight->GetDistribution() != LightscapeLight::ISOTROPIC_DIST) {
        return pxr::MaxUsdPrimTypeTokens->RectLight;
    }
    // Photometric Sphere light gets translated into a UsdLuxSphereLight
    // Photometric Point and Disk lights using a spherical light distribution type (isotropic)
    // gets translated into a UsdLuxSphereLight.
    if (photometricLightType == LS_SPHERE_LIGHT_ID
        || photometricLightType == LS_SPHERE_LIGHT_TARGET_ID
        || ((photometricLightType == LS_POINT_LIGHT_ID
             || photometricLightType == LS_POINT_LIGHT_TARGET_ID
             || photometricLightType == LS_DISC_LIGHT_ID
             || photometricLightType == LS_DISC_LIGHT_TARGET_ID)
            && maxPhotometricLight->GetDistribution() == LightscapeLight::ISOTROPIC_DIST)) {
        return pxr::MaxUsdPrimTypeTokens->SphereLight;
    }
    // Photometric Cylinder light gets translated into a UsdLuxCylinderLight
    // Photometric Line and Area (Rectangle) lights using a spherical light distribution type
    // (isotropic) gets translated into a UsdLuxCylinderLight.
    if (photometricLightType == LS_CYLINDER_LIGHT_ID
        || photometricLightType == LS_CYLINDER_LIGHT_TARGET_ID
        || ((photometricLightType == LS_LINEAR_LIGHT_ID
             || photometricLightType == LS_LINEAR_LIGHT_TARGET_ID
             || photometricLightType == LS_AREA_LIGHT_ID
             || photometricLightType == LS_AREA_LIGHT_TARGET_ID)
            && maxPhotometricLight->GetDistribution() == LightscapeLight::ISOTROPIC_DIST)) {

        return pxr::MaxUsdPrimTypeTokens->CylinderLight;
    }
    // Should not happpen. Fallback to sphere light.
    return pxr::MaxUsdPrimTypeTokens->SphereLight;
}

bool MaxUsdPhotometricLightWriter::Write(
    UsdPrim&                  targetPrim,
    bool                      applyOffsetTransform,
    const MaxUsd::ExportTime& time)
{
    INode* sourceNode = GetNode();

    auto     object = sourceNode->EvalWorldState(time.GetMaxTime()).obj;
    Class_ID photometricLightType = object->ClassID();

    LightscapeLight2* maxPhotometricLight = dynamic_cast<LightscapeLight2*>(
        object->ConvertToType(time.GetMaxTime(), photometricLightType));

    if (maxPhotometricLight == nullptr) {
        return false;
    }

    auto stage = targetPrim.GetStage();
    auto primPath = targetPrim.GetPath();
    auto targetType = GetPrimType();

    // Write time-independent properties on the first frame only.
    pxr::UsdLuxBoundableLightBase usdLightPrim;
    if (time.IsFirstFrame()) {
        bool applyConversionRotationOffset { false };

        if (targetType == pxr::MaxUsdPrimTypeTokens->DiskLight) {
            pxr::UsdLuxDiskLight discLight = pxr::UsdLuxDiskLight::Define(stage, primPath);
            if (photometricLightType == LS_POINT_LIGHT_ID
                || photometricLightType == LS_POINT_LIGHT_TARGET_ID) {
                // a minimal disk radius needs to be specified in order to emit light
                // value is the same as translated from Arnold MAXtoA
                discLight.CreateRadiusAttr().Set(0.001f, pxr::UsdTimeCode::Default());
            }
            usdLightPrim = discLight;
        } else if (targetType == pxr::MaxUsdPrimTypeTokens->RectLight) {
            pxr::UsdLuxRectLight rectangleLight = pxr::UsdLuxRectLight::Define(stage, primPath);
            if (photometricLightType == LS_LINEAR_LIGHT_ID
                || photometricLightType == LS_LINEAR_LIGHT_TARGET_ID) {
                // a diffuse line light is rendered as a directional line light (a narrow plane)
                // the narrow plane is set to be a '0.1' unit width, fixed in time
                // value is the same as translated from Arnold MAXtoA
                rectangleLight.CreateWidthAttr().Set(0.1f, pxr::UsdTimeCode::Default());
            }
            usdLightPrim = rectangleLight;
        } else if (targetType == pxr::MaxUsdPrimTypeTokens->SphereLight) {
            pxr::UsdLuxSphereLight sphereLight = pxr::UsdLuxSphereLight::Define(stage, primPath);

            bool treatAsPointLight = photometricLightType == LS_POINT_LIGHT_ID
                || photometricLightType == LS_POINT_LIGHT_TARGET_ID;
            if (treatAsPointLight) {
                // TODO: figure out which renderer supports the 'treatAsPoint' attribute
                // For now, set the radius to 0.001 in order to minimally emit light
                // value is the same as translated from Arnold MAXtoA
                sphereLight.CreateRadiusAttr().Set(
                    0.001f, pxr::UsdTimeCode::Default()); // fixed in time
                sphereLight.CreateTreatAsPointAttr().Set(
                    treatAsPointLight, pxr::UsdTimeCode::Default());
            }
            usdLightPrim = sphereLight;
        } else if (targetType == pxr::MaxUsdPrimTypeTokens->CylinderLight) {
            pxr::UsdLuxCylinderLight cylinderLight
                = pxr::UsdLuxCylinderLight::Define(stage, primPath);

            // need to rotate the light on its z-axis 90 degrees for USD
            applyConversionRotationOffset = true;

            bool treatAsLineLight
                = (photometricLightType == LS_LINEAR_LIGHT_ID
                   || photometricLightType == LS_LINEAR_LIGHT_TARGET_ID);
            if (treatAsLineLight) {
                // TODO: figure out which renderer supports the 'treatAsLine' attribute
                // For now, set the radius to 0.001 in order to minimally emit light
                // value is the same as translated from Arnold MAXtoA
                cylinderLight.CreateRadiusAttr().Set(
                    0.001f, pxr::UsdTimeCode::Default()); // fixed in time
                cylinderLight.CreateTreatAsLineAttr().Set(
                    treatAsLineLight, pxr::UsdTimeCode::Default());
            }
            usdLightPrim = cylinderLight;
        }

        if (applyConversionRotationOffset) {
            pxr::UsdGeomXformable xformable(usdLightPrim.GetPrim());
            // Any linear, cylindrical or plane light types are converted to UsdLuxCylinder type.
            // In USD, the expected orientation is on the x-axis, but 3ds Max has it set on the
            // y-axis need to rotate the light on its z-axis 90 degrees for USD
            pxr::UsdGeomXformOp rotationAdjustment = xformable.AddXformOp(
                pxr::UsdGeomXformOp::TypeRotateZ,
                pxr::UsdGeomXformOp::PrecisionDouble,
                pxr::TfToken());
            rotationAdjustment.Set(90.0, pxr::UsdTimeCode::Default());
        }

        // Light color
        // Enable color temperature (Kelvin) if specified
        bool enableColorTemperature = maxPhotometricLight->GetUseKelvin();
        usdLightPrim.CreateEnableColorTemperatureAttr().Set(
            enableColorTemperature, pxr::UsdTimeCode::Default());

        if (!maxPhotometricLight->GetAffectSpecular()) {
            // turn off the effect of this light on the specular response of materials
            usdLightPrim.CreateSpecularAttr().Set(0.0f, pxr::UsdTimeCode::Default());
        } else {
            // TODO: how to compute the specular effect multiplier
            // leave the default value to 1.0f for now
            // usdLightPrim.CreateSpecularAttr().Set(0.0f, pxr::UsdTimeCode::Default());
        }
        if (!maxPhotometricLight->GetAffectDiffuse()) {
            // turn off the effect of this light on the diffuse response of materials
            usdLightPrim.CreateDiffuseAttr().Set(0.0f, pxr::UsdTimeCode::Default());
        } else {
            // TODO: how to compute the diffuse effect multiplier
            // leave the default value to 1.0f for now
            // usdLightPrim.CreateSpecularAttr().Set(0.0f, pxr::UsdTimeCode::Default());
        }

        // Enable shadow casting
        pxr::UsdLuxShadowAPI usdLightShadowProperties(usdLightPrim);
        bool                 shadowEnable = maxPhotometricLight->GetShadow();
        usdLightShadowProperties.CreateShadowEnableAttr().Set(
            shadowEnable, pxr::UsdTimeCode::Default());

        // Normalize light intensity
        // This makes it easier to independently adjust the power and shape of the light,
        // by causing the power to not vary with the area or angular size of the light.
        usdLightPrim.CreateNormalizeAttr().Set(true, pxr::UsdTimeCode::Default());

        // IES distribution is not animatable
        if (maxPhotometricLight->GetDistribution() == LightscapeLight::WEB_DIST) {
            // IES Light Profile file:
            // TODO: Consider exporting the IES File along with the data. For now, this references
            // the IES file as a reference to the original file referenced by the 3ds Max file.
            // TODO: IES files do not get imported with RenderMan render delegate (Prman)
            using namespace MaxSDK::AssetManagement;
            AssetUser asset = maxPhotometricLight->GetWebFile();
            if (asset.GetId() != kInvalidId) {
                pxr::UsdLuxShapingAPI usdLightShape(usdLightPrim);
                pxr::SdfAssetPath     assetFullPath(asset.GetFullFilePath().ToUTF8().data());
                usdLightShape.CreateShapingIesFileAttr().Set(
                    assetFullPath, pxr::UsdTimeCode::Default());
            }
        }
    }

    // Write animatable properties at the requested time.

    // If not on the first frame, we haven't fetched the light yet, do it now.
    if (!usdLightPrim) {
        usdLightPrim = pxr::UsdLuxBoundableLightBase::Get(stage, primPath);
    }

    const auto timeVal = time.GetMaxTime();
    const auto usdTimeCode = time.GetUsdTime();

    // same conditional block statements as above in the light declarations unless specifically
    // noted
    if ((photometricLightType == LS_DISC_LIGHT_ID
         || photometricLightType == LS_DISC_LIGHT_TARGET_ID)
        && maxPhotometricLight->GetDistribution() != LightscapeLight::ISOTROPIC_DIST) {
        // note: since point lights have a fixed radius, there is no need
        // to have those light types treated inside this conditional block

        pxr::UsdLuxDiskLight discLight = (pxr::UsdLuxDiskLight)usdLightPrim;
        float                radius = maxPhotometricLight->GetRadius(timeVal);
        discLight.CreateRadiusAttr().Set(radius, usdTimeCode);
    } else if (
        (photometricLightType == LS_LINEAR_LIGHT_ID
         || photometricLightType == LS_LINEAR_LIGHT_TARGET_ID
         || photometricLightType == LS_AREA_LIGHT_ID
         || photometricLightType == LS_AREA_LIGHT_TARGET_ID)
        && maxPhotometricLight->GetDistribution() != LightscapeLight::ISOTROPIC_DIST) {
        pxr::UsdLuxRectLight rectangleLight = (pxr::UsdLuxRectLight)usdLightPrim;

        if (photometricLightType == LS_AREA_LIGHT_ID
            || photometricLightType == LS_AREA_LIGHT_TARGET_ID) {
            // applies only to area (rectangle) lights as the line lights have fixed width
            float width = maxPhotometricLight->GetWidth(timeVal);
            rectangleLight.CreateWidthAttr().Set(width, usdTimeCode);
        }
        float height = maxPhotometricLight->GetLength(timeVal);
        rectangleLight.CreateHeightAttr().Set(height, usdTimeCode);
    } else if (
        photometricLightType == LS_SPHERE_LIGHT_ID
        || photometricLightType == LS_SPHERE_LIGHT_TARGET_ID
        || ((photometricLightType == LS_DISC_LIGHT_ID
             || photometricLightType == LS_DISC_LIGHT_TARGET_ID)
            && maxPhotometricLight->GetDistribution() == LightscapeLight::ISOTROPIC_DIST)) {
        // note: since point lights have a fixed radius, there is no need
        // to have those light types treated inside this conditional block

        pxr::UsdLuxSphereLight sphereLight = (pxr::UsdLuxSphereLight)usdLightPrim;
        float                  radius = maxPhotometricLight->GetRadius(timeVal);
        sphereLight.CreateRadiusAttr().Set(radius, usdTimeCode);
    } else if (
        photometricLightType == LS_CYLINDER_LIGHT_ID
        || photometricLightType == LS_CYLINDER_LIGHT_TARGET_ID
        || ((photometricLightType == LS_LINEAR_LIGHT_ID
             || photometricLightType == LS_LINEAR_LIGHT_TARGET_ID
             || photometricLightType == LS_AREA_LIGHT_ID
             || photometricLightType == LS_AREA_LIGHT_TARGET_ID)
            && maxPhotometricLight->GetDistribution() == LightscapeLight::ISOTROPIC_DIST)) {
        pxr::UsdLuxCylinderLight cylinderLight = (pxr::UsdLuxCylinderLight)usdLightPrim;
        float                    length = maxPhotometricLight->GetLength(timeVal);
        cylinderLight.CreateLengthAttr().Set(length, usdTimeCode);

        if (photometricLightType == LS_AREA_LIGHT_ID
            || photometricLightType == LS_AREA_LIGHT_TARGET_ID) {
            float radius = maxPhotometricLight->GetWidth(timeVal) / 2.f;
            cylinderLight.CreateRadiusAttr().Set(radius, usdTimeCode);
        } else if (
            photometricLightType == LS_CYLINDER_LIGHT_ID
            || photometricLightType == LS_CYLINDER_LIGHT_TARGET_ID) {
            float radius = maxPhotometricLight->GetRadius(timeVal);
            cylinderLight.CreateRadiusAttr().Set(radius, usdTimeCode);
        }
        // else, line lights have fixed width
    }

    // Light color
    if (maxPhotometricLight->GetUseKelvin()) {
        // USD expects Kelvin range values from 1000 to 10000
        float originalKelvinValue = maxPhotometricLight->GetKelvin(timeVal);
        float clampedKelvinValue = std::min(std::max(1000.f, originalKelvinValue), 10000.f);
        usdLightPrim.CreateColorTemperatureAttr().Set(clampedKelvinValue, usdTimeCode);
        if (originalKelvinValue != clampedKelvinValue) {
            MaxUsd::Log::Warn(
                L"Light '{0}' temperature value was clamped to '{1}' from '{2}' to match USD "
                L"specifications.",
                sourceNode->GetName(),
                clampedKelvinValue,
                originalKelvinValue);
        }

        // add light filter color
        Point3       maxFilteLightColor = maxPhotometricLight->GetRGBFilter(timeVal);
        pxr::GfVec3f usdLightColor { maxFilteLightColor[0],
                                     maxFilteLightColor[1],
                                     maxFilteLightColor[2] };
        usdLightPrim.CreateColorAttr().Set(usdLightColor, usdTimeCode);
    } else {
        // When not using color temperature (Kelvin) to specify light color,
        // light color is then a composition of the specified light and filter color
        Point3 maxLightColor = maxPhotometricLight->GetRGBColor(timeVal)
            * maxPhotometricLight->GetRGBFilter(timeVal);
        pxr::GfVec3f usdLightColor { maxLightColor[0], maxLightColor[1], maxLightColor[2] };
        usdLightPrim.CreateColorAttr().Set(usdLightColor, usdTimeCode);
    }

    // Shadow color
    // note: The shadow color is not exposed in the Photometric light interface (but thru maxscript)
    pxr::UsdLuxShadowAPI usdLightShadowProperties(usdLightPrim);
    Point3               maxLightShadowColor = maxPhotometricLight->GetShadColor(timeVal);
    pxr::GfVec3f         usdLightShadowColor(
        maxLightShadowColor[0], maxLightShadowColor[1], maxLightShadowColor[2]);
    usdLightShadowProperties.CreateShadowColorAttr().Set(usdLightShadowColor, usdTimeCode);

    // Light Falloff values
    // TODO - light filter required - might be a renderer specific thing to expose
    // if (maxPhotometricLight->GetUseAtten())
    //{
    //	usdLightShadowProperties.CreateFalloffLightFilerWhateverAttr().Set(
    //			maxPhotometricLight->GetAtten(timeVal, ATTEN_START, FOREVER), usdTimeCode);
    //	usdLightShadowProperties.CreateFalloffLightFilerWhateverAttr().Set(
    //			maxPhotometricLight->GetAtten(timeVal, ATTEN_END, FOREVER), usdTimeCode);
    //}

    // Light intensity
    {
        // based on the Arnold translator (MAXtoA)
        //
        // the effective intensity in candelas
        float lightIntensity = maxPhotometricLight->GetIntensity(timeVal);
        if (maxPhotometricLight->GetDistribution() == LightscapeLight::WEB_DIST) {
            lightIntensity = lightIntensity / maxPhotometricLight->GetOriginalIntensity() * 1000.f;
        }

        // take care of a dimmed intensity
        if (maxPhotometricLight->GetUseMultiplier()) {
            lightIntensity *= maxPhotometricLight->GetDimmerValue(timeVal) * 0.01f;
        }

        lightIntensity
            /= 1500; // TODO - need
                     // GetRenderSessionContext().GetRenderSettings().GetPhysicalScale(translationTime,
                     // newValidity)

        // ZAP's magic adjustment
        lightIntensity *= static_cast<float>(M_PI);
        // scale to system units.
        lightIntensity /= static_cast<float>(
            GetSystemUnitScale(UNITS_METERS) * GetSystemUnitScale(UNITS_METERS));
        usdLightPrim.CreateIntensityAttr().Set(lightIntensity, usdTimeCode);
    }

    LightscapeLight::DistTypes maxLightDistType = maxPhotometricLight->GetDistribution();
    if (maxLightDistType == LightscapeLight::SPOTLIGHT_DIST) {
        // TODO - the falloff of the spot is not directly the angle value from 3ds Max
        float                 beamAngle = maxPhotometricLight->GetHotspot(timeVal);
        pxr::UsdLuxShapingAPI usdLightShape(usdLightPrim);
        usdLightShape.CreateShapingConeAngleAttr().Set(beamAngle, usdTimeCode);
    }

    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE