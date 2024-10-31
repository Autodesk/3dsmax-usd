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
#include "ShadingModeExporterContext.h"

#include "WriteJobContext.h"

#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/envSetting.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/types.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/specializes.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include <regex>
#include <stdmat.h>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(MaterialsRootMaxTokens, MAT_ROOT_TOKENS);

MaxUsdShadingModeExportContext::MaxUsdShadingModeExportContext(
    MaxUsdWriteJobContext& writeJobContext)
    : stage(writeJobContext.GetUsdStage())
    , writeJobContext(writeJobContext)
{
}

UsdPrim MaxUsdShadingModeExportContext::MakeStandardMaterialPrim(const pxr::SdfPath& path) const
{
    UsdShadeMaterial materialShade;
    // No path explicitly supplied. Build it from the export options.
    if (path.IsEmpty()) {
        // Build the material's root path
        const MaxUsd::USDSceneBuilderOptions& buildOptions = GetExportArgs();
        SdfPath                               materialsRoot(buildOptions.GetRootPrimPath());
        materialsRoot = materialsRoot.AppendChild(MaterialsRootMaxTokens->materials);
        // create Standard Material prim
        std::string shaderName
            = TfMakeValidIdentifier(MaxUsd::MaxStringToUsdString(GetMaterial()->GetName()));
        materialShade = MaxUsd::MakeUniquePrimOfType<UsdShadeMaterial>(
            stage, materialsRoot, pxr::TfToken { shaderName });
    } else {
        materialShade = UsdShadeMaterial::Define(stage, path);
    }
    UsdPrim materialPrim = materialShade.GetPrim();
    return materialPrim;
}

void MaxUsdShadingModeExportContext::BindStandardMaterialPrim(const UsdPrim& materialPrim) const
{
    UsdShadeMaterial material(materialPrim);
    if (!material) {
        TF_RUNTIME_ERROR("Invalid material prim.");
        return;
    }

    if (!bindings) {
        // if no bindings was supplied, fall out
        return;
    }

    for (const auto& exportedPrimPath : *bindings) {
        auto exportedPrim = GetUsdStage()->GetPrimAtPath(exportedPrimPath);
        auto bindingAPI = UsdShadeMaterialBindingAPI::Apply(exportedPrim);
        bindingAPI.Bind(material);
    }
}

void MaxUsdShadingModeExportContext::AdditionalMaterials(const std::vector<Mtl*>& additionalMtls)
{
    this->additionalMtls = additionalMtls;
}
const std::vector<Mtl*>& MaxUsdShadingModeExportContext::GetAdditionalMaterials() const
{
    return additionalMtls;
}

void MaxUsdShadingModeExportContext::AddShaderWriter(MaxUsdShaderWriterSharedPtr& writer)
{
    writers.push_back(writer);
}
const std::list<MaxUsdShaderWriterSharedPtr>&
MaxUsdShadingModeExportContext::GetShaderWriters() const
{
    return writers;
}

PXR_NAMESPACE_CLOSE_SCOPE
