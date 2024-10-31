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
#include "GlTFMaterialWriter.h"

#include "Materials/Mtl.h"

#include <MaxUsd/Translators/ShaderWriterRegistry.h>

#include <pxr/usd/usdShade/shader.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <iparamb2.h>
#include <notify.h>
#include <units.h>

// Macro for the pixar namespace "pxr::"
// This is putting all of the code, until the close macro, into the pixar namespace.
// This is needed for the Macros to compile and is required for all the Prim/Shader Writers
PXR_NAMESPACE_OPEN_SCOPE

// This macro registers the Shader Writer, it's adding the GlTFMaterialWriter class as a candidate
// when trying to export a glTF material. It also verifies that the class we're registering
// implements the CanExport() method and is a subclass of MaxUsdShaderWriter. The class_ID
// represents the glTF material. It is also very important to set the project option "Remove
// unreferenced code and data" to NO, not doing so could cause the Macro to be optimized out and the
// Writer to never be properly registered.
PXR_MAXUSD_REGISTER_SHADER_WRITER(Class_ID(0x38420192, 0x45fe4e1b), GlTFMaterialWriter);

PXR_NAMESPACE_CLOSE_SCOPE

PXR_NAMESPACE_USING_DIRECTIVE

/*
 * When this function is called we already know we are dealing with a glTF material,
 * because we register this writer specifically against the glTF material Class_ID (see registration
 * macro above). Which means the only thing we have to check is that we're trying to export to the
 * desired format.
 */
MaxUsdShaderWriter::ContextSupport
GlTFMaterialWriter::CanExport(const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    // GetConvertMaterialsTo returns the current target material being processed
    // In this sample our target is UsdPreviewSurface.
    if (exportArgs.GetConvertMaterialsTo() == UsdImagingTokens->UsdPreviewSurface) {
        return ContextSupport::Supported;
    }
    // Only report as fallback if UsdPreviewSurface was not explicitly requested:
    if (exportArgs.GetAllMaterialConversions().count(UsdImagingTokens->UsdPreviewSurface) == 0) {
        return ContextSupport::Fallback;
    }
    return ContextSupport::Unsupported;
}

enum GlTFMaterialWriterCodes
{
    UnableToDefineShadeShader,
    InvalidPrimForShadeShader,
    MissingShadeShader
};
TF_REGISTRY_FUNCTION(TfEnum)
{
    TF_ADD_ENUM_NAME(UnableToDefineShadeShader, "Could not define UsdShadeShader");
    TF_ADD_ENUM_NAME(InvalidPrimForShadeShader, "Could not get UsdPrim for UsdShadeShader");
    TF_ADD_ENUM_NAME(MissingShadeShader, "Could not get UsdShadeShader schema for UsdPrim");
};

/*
 * The Shader Writer constructor is expected to define the Shader prim.
 * The Write method is then responsible for populating it's data.
 */
GlTFMaterialWriter::GlTFMaterialWriter(
    Mtl*                   material,
    const SdfPath&         usdPath,
    MaxUsdWriteJobContext& jobCtx)
    : MaxUsdShaderWriter(material, usdPath, jobCtx)
{
    UsdShadeShader shaderSchema = UsdShadeShader::Define(GetUsdStage(), GetUsdPath());
    if (!shaderSchema) {
        TF_ERROR(UnableToDefineShadeShader, "at path '%s'\n", GetUsdPath().GetString().c_str());
        return;
    }

    UsdAttribute idAttr = shaderSchema.CreateIdAttr(VtValue(UsdImagingTokens->UsdPreviewSurface));

    usdPrim = shaderSchema.GetPrim();
    if (!usdPrim.IsValid()) {
        TF_ERROR(
            InvalidPrimForShadeShader,
            "at path '%s'\n",
            shaderSchema.GetPath().GetString().c_str());
        return;
    }

    // Surface Output
    shaderSchema.CreateOutput(UsdShadeTokens->surface, SdfValueTypeNames->Token);
}

/*
 * For the purpose of this sample, which is demonstrating the necessary parts needed to implement a
 * Shader Writer, we will just export the base color of the glTF material. A similar approach can be
 * taken for the other parameters.
 */
void GlTFMaterialWriter::Write()
{
    UsdShadeShader shaderSchema(usdPrim);
    if (!shaderSchema) {
        TF_ERROR(MissingShadeShader, "at path '%s'\n ", usdPrim.GetPath().GetString().c_str());
        return;
    }
    const auto      timeConfig = GetExportArgs().GetResolvedTimeConfig();
    Mtl*            mat = GetMaterial();
    Interval        valid = FOREVER;
    auto            pb = mat->GetParamBlock(0);
    Point3          col;
    const TimeValue startTime = timeConfig.GetStartTime();
    const TimeValue endTime = timeConfig.GetEndTime();
    // How much time we need to put for each Max's frame.
    const int     timeStep = timeConfig.GetTimeStep();
    UsdShadeInput in
        = shaderSchema.CreateInput(TfToken("diffuseColor"), SdfValueTypeNames->Color3f);

    for (TimeValue timeVal = startTime; timeVal <= endTime;) {
        pb->GetValueByName(_T("baseColor"), timeVal, col, valid);
        UsdTimeCode usdTimeCode(double(timeVal) / double(GetTicksPerFrame()));
        in.Set(GfVec3f(col[0], col[1], col[2]), usdTimeCode);
        timeVal += timeStep;
    }
}
