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
#include "TranslatorLight.h"

#include "TranslatorPrim.h"
#include "TranslatorUtils.h"
#include "TranslatorXformable.h"

#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/usdLux/boundableLightBase.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shadowAPI.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include <AssetManagement/iassetmanager.h>
#include <lslights.h>
#include <units.h>

PXR_NAMESPACE_OPEN_SCOPE

bool MaxUsdTranslatorLight::Read(
    const UsdPrim&                        prim,
    const MaxUsd::MaxSceneBuilderOptions& args,
    MaxUsdReadJobContext&                 context)
{
    // Fingerprint the given USD Light Prim to find the most appropriate type of 3ds Max Photometric
    // Light it should be translated to:
    const auto                 timeConfig = args.GetResolvedTimeConfig(prim.GetStage());
    auto                       timeCode = timeConfig.GetStartTimeCode();
    Class_ID                   photometricLightType = LS_POINT_LIGHT_ID;
    LightscapeLight::DistTypes distributionType = LightscapeLight::DIFFUSE_DIST;
    if (prim.IsA<UsdLuxDiskLight>()) {
        photometricLightType = LS_DISC_LIGHT_ID;
    } else if (prim.IsA<UsdLuxRectLight>()) {
        photometricLightType = LS_AREA_LIGHT_ID;
    } else if (prim.IsA<UsdLuxSphereLight>()) {
        photometricLightType = LS_SPHERE_LIGHT_ID;

        // USD Sphere Lights support a "treatAsPoint" attribute, which can be used to convey
        // information about Point Light characteristics:
        UsdLuxSphereLight sphereLight(prim);
        bool              treatAsPointLight = false;
        if (sphereLight.GetTreatAsPointAttr().Get(&treatAsPointLight, timeCode)
            && treatAsPointLight) {
            photometricLightType = LS_POINT_LIGHT_ID;
        }
        distributionType = LightscapeLight::ISOTROPIC_DIST;
    } else if (prim.IsA<UsdLuxCylinderLight>()) {
        photometricLightType = LS_CYLINDER_LIGHT_ID;

        // USD Cylinder Lights support a "treatAsLine" attribute, which can be used to convey
        // information about Linear Light characteristics:
        UsdLuxCylinderLight cylinderLight(prim);
        bool                treatAsLineLight = false;
        if (cylinderLight.GetTreatAsLineAttr().Get(&treatAsLineLight, timeCode)
            && treatAsLineLight) {
            photometricLightType = LS_LINEAR_LIGHT_ID;
        }
        distributionType = LightscapeLight::ISOTROPIC_DIST;
    }

    UsdLuxBoundableLightBase usdLight(prim);
    TimeValue                time
        = MaxUsd::GetMaxTimeValueFromUsdTimeCode(prim.GetStage(), timeConfig.GetStartTimeCode());
    ;
    LightscapeLight2* photometricLight = static_cast<LightscapeLight2*>(
        GetCOREInterface17()->CreateInstance(LIGHT_CLASS_ID, photometricLightType));
    photometricLight->Enable(TRUE);
    photometricLight->SetUseLight(TRUE);
    photometricLight->SetDistribution(distributionType);

    // These are currently only subsets of both USD and 3ds Max lights, and additional enhancements
    // will be required in the future to support intensity, shadow, shapes, etc.

    // Enable color temperature (mapping to USD's "enableColorTemperature" attribute):
    bool enableColorTemperature = false;
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdLight.GetEnableColorTemperatureAttr(),
        [photometricLight,
         &enableColorTemperature](const VtValue& value, const UsdTimeCode&, const TimeValue&) {
            enableColorTemperature = value.Get<bool>();
            photometricLight->SetUseKelvin(enableColorTemperature);
            return true;
        },
        context);

    // Color temperature (mapping to USD's "colorTemperature" attribute):
    // only set color temperature if the light has specified to use the value
    if (enableColorTemperature) {
        if (!MaxUsdTranslatorUtil::ReadUsdAttribute(
                usdLight.GetColorTemperatureAttr(),
                [photometricLight](
                    const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
                    photometricLight->SetKelvin(timeValue, value.Get<float>());
                    return true;
                },
                context)) {
            MaxUsd::Log::Warn(
                "Light '{0}' is set to use a color temperature but no value specified.",
                prim.GetName().GetString());
        }
    }

    // Color (mapping to USD's "color" attribute):
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdLight.GetColorAttr(),
        [photometricLight, enableColorTemperature](
            const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
            GfVec3f usdLightColor = value.Get<GfVec3f>();
            Point3  maxLightColor(usdLightColor[0], usdLightColor[1], usdLightColor[2]);
            if (!enableColorTemperature) {
                // if Color temperature is not used, the default light color component must
                // be removed from the applied color to get the proper resulting light color
                maxLightColor = maxLightColor / photometricLight->GetRGBColor(timeValue);
            }
            photometricLight->SetRGBFilter(timeValue, maxLightColor);
            return true;
        },
        context);

    // Enable shadow casting (mapping to USD's "shadow:enable" attribute):
    UsdLuxShadowAPI usdlightShadowProperties(prim);
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdlightShadowProperties.GetShadowEnableAttr(),
        [photometricLight](const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
            photometricLight->SetShadow(value.Get<bool>());
            return true;
        },
        context,
        false);

    // Shadow color (mapping to USD's "shadow:color" attribute):
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdlightShadowProperties.GetShadowColorAttr(),
        [photometricLight](const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
            GfVec3f usdLightShadowColor = value.Get<GfVec3f>();
            Point3  maxLightShadowColor(
                usdLightShadowColor[0], usdLightShadowColor[1], usdLightShadowColor[2]);
            photometricLight->SetShadColor(timeValue, maxLightShadowColor);
            return true;
        },
        context,
        false);

    // Multiplier effect of this light on the diffuse response of materials
    // default USD value is 1.0 (same has for 3ds Max)
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdLight.GetDiffuseAttr(),
        [photometricLight,
         prim](const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
            float usdLightDiffuseMultiplier = value.Get<float>();
            photometricLight->SetAffectDiffuse(usdLightDiffuseMultiplier != 0.0f);
            if (usdLightDiffuseMultiplier != 1.0f && usdLightDiffuseMultiplier != 0.0f) {
                MaxUsd::Log::Warn(
                    "Light diffuse multiplier attribute for '{0}' is specified with a value not "
                    "properly "
                    "considered by 3ds Max.",
                    prim.GetName().GetString());
            }
            return true;
        },
        context,
        false);

    // Multiplier effect of this light on the specular response of materials
    // default USD value is 1.0 (same has for 3ds Max)
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdLight.GetSpecularAttr(),
        [photometricLight,
         prim](const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
            float usdLightSpecularMultiplier = value.Get<float>();
            photometricLight->SetAffectSpecular(usdLightSpecularMultiplier != 0.0f);
            if (usdLightSpecularMultiplier != 1.0f && usdLightSpecularMultiplier != 0.0f) {
                MaxUsd::Log::Warn(
                    "Light specular multiplier attribute for '{0}' is specified with a value not "
                    "properly "
                    "considered by 3ds Max.",
                    prim.GetName().GetString());
            }
            return true;
        },
        context,
        false);

    // Light radius (for Disk, Sphere and Cylinder lights, mapping to USD's "radius" attribute):
    auto setLightRadius = [&](auto usdLightWithRadius) {
        MaxUsdTranslatorUtil::ReadUsdAttribute(
            usdLightWithRadius.GetRadiusAttr(),
            [photometricLight](
                const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
                photometricLight->SetRadius(timeValue, value.Get<float>());
                return true;
            },
            context,
            false);
    };
    if (photometricLightType == LS_SPHERE_LIGHT_ID) {
        setLightRadius(UsdLuxSphereLight(prim));
    } else if (photometricLightType == LS_DISC_LIGHT_ID) {
        setLightRadius(UsdLuxDiskLight(prim));
    } else if (photometricLightType == LS_CYLINDER_LIGHT_ID) {
        setLightRadius(UsdLuxCylinderLight(prim));
    }

    // Width & height (for Rectangle lights, mapping to USD's "width" and "height" attributes):
    if (photometricLightType == LS_AREA_LIGHT_ID) {
        UsdLuxRectLight usdRectangleLight(prim);
        MaxUsdTranslatorUtil::ReadUsdAttribute(
            usdRectangleLight.GetWidthAttr(),
            [photometricLight](
                const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
                photometricLight->SetWidth(timeValue, value.Get<float>());
                return true;
            },
            context,
            false);
        MaxUsdTranslatorUtil::ReadUsdAttribute(
            usdRectangleLight.GetHeightAttr(),
            [photometricLight](
                const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
                photometricLight->SetLength(
                    timeValue,
                    value.Get<float>()); // 3ds Max uses "Length" for both "height" and "length".
                return true;
            },
            context,
            false);
    }
    // Length (for Cylinder and Line lights, mapping to USD's "length" attribute):
    else if (
        photometricLightType == LS_CYLINDER_LIGHT_ID
        || photometricLightType == LS_LINEAR_LIGHT_ID) {
        UsdLuxCylinderLight usdCylinderLight(prim);
        MaxUsdTranslatorUtil::ReadUsdAttribute(
            usdCylinderLight.GetLengthAttr(),
            [photometricLight](
                const VtValue& value, const UsdTimeCode&, const TimeValue& timeValue) {
                photometricLight->SetLength(timeValue, value.Get<float>());
                return true;
            },
            context,
            false);
    }

    // IES Light Profile file (mapping to USD's "shaping:ies:file" attribute):
    UsdLuxShapingAPI usdLightShape(prim);
    SdfAssetPath     iesLightProfileAssetPath;
    if (usdLightShape.GetShapingIesFileAttr().Get(&iesLightProfileAssetPath, timeCode)) {
        using namespace MaxSDK::AssetManagement;

        std::wstring assetPath(
            iesLightProfileAssetPath.GetResolvedPath().begin(),
            iesLightProfileAssetPath.GetResolvedPath().end());
        MSTR      lightProfileFile(assetPath.c_str());
        AssetUser lightProfileAsset = IAssetManager::GetInstance()->GetAsset(
            lightProfileFile, AssetType::kPhotometricAsset);
        photometricLight->SetDistribution(LightscapeLight::DistTypes::WEB_DIST);
        photometricLight->SetWebFile(lightProfileAsset);
    }

    // Intensity (mapping to USD's "intensity" attribute):
    // if not specified, keeping default 1500cd
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        usdLight.GetIntensityAttr(),
        [usdLight, photometricLight, prim](
            const VtValue& value, const UsdTimeCode& timeCode, const TimeValue& timeValue) {
            bool usdLightNormalized = false;
            if (usdLight.GetNormalizeAttr().Get(&usdLightNormalized, timeCode)
                && !usdLightNormalized) {
                MaxUsd::Log::Warn(
                    "Light intensity for '{0}' is not normalized and might not give the expected "
                    "output.",
                    prim.GetName().GetString());
            }

            // convert intensity value to 3ds Max candelas
            float lightIntensity = value.Get<float>();
            // scale to system units.
            lightIntensity *= static_cast<float>(
                GetSystemUnitScale(UNITS_METERS) * GetSystemUnitScale(UNITS_METERS));
            // ZAP's magic adjustment
            lightIntensity /= static_cast<float>(M_PI);
            lightIntensity
                *= 1500; // TODO - need
                         // GetRenderSessionContext().GetRenderSettings().GetPhysicalScale(translationTime,
                         // newValidity)

            if (photometricLight->GetDistribution() == LightscapeLight::WEB_DIST) {
                lightIntensity = lightIntensity * photometricLight->GetOriginalIntensity() / 1000.f;
            }

            photometricLight->SetIntensityType(LightscapeLight::IntensityType::CANDELAS);
            photometricLight->SetIntensity(timeValue, lightIntensity);
            return true;
        },
        context);

    auto createdNode = MaxUsdTranslatorPrim::CreateAndRegisterNode(
        prim, photometricLight, prim.GetName(), context);

    // position the node
    Matrix3 correctionMatrix = Matrix3::Identity;
    if (prim.IsA<UsdLuxCylinderLight>()) {
        // Special case with cylinder lights
        // In USD, the expected orientation is on the x-axis, but 3ds Max has it set on the y-axis
        correctionMatrix.SetRotateZ(HALFPI);
    }
    MaxUsdTranslatorXformable::Read(prim, createdNode, context, correctionMatrix);

    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE

// Disable obscure warning only occurring for 2022 (pixar usd version: 21.11):
// no definition for inline function : pxr::DefaultValueHolder Was not able to identify what is
// triggering this.
#pragma warning(disable : 4506)