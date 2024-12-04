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
#include "LastResortUSDPreviewSurfaceWriter.h"

#include "ShaderWriter.h"
#include "ShaderWriterRegistry.h"
#include "ShadingModeRegistry.h"
#include "WriteJobContext.h"

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/tokens.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <Materials/mtl.h>
#include <max.h>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdShaderWriter::ContextSupport
LastResortUSDPreviewSurfaceWriter::CanExport(const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    if (exportArgs.GetConvertMaterialsTo() == pxr::UsdImagingTokens->UsdPreviewSurface) {
        return ContextSupport::Fallback;
    }
    return ContextSupport::Unsupported;
}

LastResortUSDPreviewSurfaceWriter::LastResortUSDPreviewSurfaceWriter(
    Mtl*                   material,
    const SdfPath&         usdPath,
    MaxUsdWriteJobContext& jobCtx)
    : MaxUsdShaderWriter(material, usdPath, jobCtx)
{
    MSTR warning = L"No Shader Writer found to convert Material \"";
    warning.append(material->GetName());
    warning.append(L"\" of type \"");
    MSTR className;
    material->GetClassName(className);
    warning.append(className);
    warning.append(L"\"to USDPreviewSurface. Generating a basic USDPreviewSurface with a diffuse "
                   L"color as a fallback.");
    MaxUsd::Log::Warn(warning.data());

    UsdShadeShader shaderSchema = UsdShadeShader::Define(GetUsdStage(), GetUsdPath());
    if (!TF_VERIFY(
            shaderSchema,
            "Could not define UsdShadeShader at path '%s'\n",
            GetUsdPath().GetText())) {
        return;
    }

    shaderSchema.CreateIdAttr(VtValue(UsdImagingTokens->UsdPreviewSurface));

    usdPrim = shaderSchema.GetPrim();
    if (!TF_VERIFY(
            usdPrim,
            "Could not get UsdPrim for UsdShadeShader at path '%s'\n",
            shaderSchema.GetPath().GetText())) {
        return;
    }

    // Surface Output
    shaderSchema.CreateOutput(UsdShadeTokens->surface, SdfValueTypeNames->Token);
}

/* virtual */
void LastResortUSDPreviewSurfaceWriter::Write()
{
    MaxUsdShaderWriter::Write();

    Mtl* material = GetMaterial();

    UsdShadeShader shaderSchema(usdPrim);
    if (!TF_VERIFY(
            shaderSchema,
            "Could not get UsdShadeShader schema for UsdPrim at path '%s'\n",
            usdPrim.GetPath().GetText())) {
        return;
    }

    auto color = material->GetDiffuse();

    shaderSchema.CreateOutput(UsdShadeTokens->surface, SdfValueTypeNames->Token);

    const auto diffuseColorInput
        = shaderSchema.CreateInput(pxr::TfToken("diffuseColor"), pxr::SdfValueTypeNames->Color3f);

    const pxr::GfVec3f usdColor = { color.r, color.g, color.b };
    diffuseColorInput.Set(usdColor);
}

PXR_NAMESPACE_CLOSE_SCOPE
