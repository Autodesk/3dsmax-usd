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
#include "MaterialConverter.h"

#include <MaxUsd/Translators/ShadingModeExporterContext.h>
#include <MaxUsd/Translators/ShadingModeRegistry.h>
#include <MaxUsd/Translators/WriteJobContext.h>

namespace MAXUSD_NS_DEF {

pxr::UsdShadeMaterial MaterialConverter::ConvertToUSDMaterial(
    Mtl*                           material,
    const pxr::UsdStageRefPtr&     stage,
    const std::string&             fileName,
    bool                           isUSDZ,
    const pxr::SdfPath&            targetPath,
    const USDSceneBuilderOptions&  options,
    const std::list<pxr::SdfPath>& bindings)
{
    PXR_NAMESPACE_USING_DIRECTIVE

    if (material->IsMultiMtl()) {
        return UsdShadeMaterial {};
    }

    // Find & build a material exporter from the shading mode in the options.
    const auto exporterCreator = MaxUsdShadingModeRegistry::GetInstance().GetExporter(
        pxr::TfToken(options.GetShadingMode()));
    if (!TF_VERIFY(exporterCreator != nullptr)) {
        return UsdShadeMaterial {};
    }
    const auto exporter = exporterCreator();

    // Setup the write job context and shading mode context for the export.
    MaxUsdWriteJobContext          writeJobCtx { stage, fileName, options, isUSDZ, true };
    MaxUsdShadingModeExportContext shadingModeCtx(writeJobCtx);
    shadingModeCtx.SetMaterialAndBindings(material, &bindings);

    // Export the material.
    UsdShadeMaterial usdShadeMaterial;
    exporter->Export(shadingModeCtx, &usdShadeMaterial, nullptr, targetPath);
    return usdShadeMaterial;
}

void MaterialConverter::ConvertToUSDMaterials(
    const std::vector<Mtl*>&                    materials,
    const pxr::UsdStageRefPtr&                  stage,
    const std::string&                          fileName,
    bool                                        isUSDZ,
    const std::vector<pxr::SdfPath>&            targetPaths,
    const USDSceneBuilderOptions&               options,
    const std::vector<std::list<pxr::SdfPath>>& bindings)
{
    PXR_NAMESPACE_USING_DIRECTIVE
    if (!TF_VERIFY(materials.size() == targetPaths.size()) || materials.empty()) {
        return;
    }
    if (!bindings.empty()) {
        if (!TF_VERIFY(materials.size() == bindings.size())) {
            return;
        }
    }
    for (size_t i = 0; i < materials.size(); ++i) {
        const auto& material = materials[i];
        if (material->IsMultiMtl()) {
            continue;
        }
        const auto& targetPath = targetPaths[i];
        const auto& matBindings = !bindings.empty() ? bindings[i] : std::list<pxr::SdfPath> {};

        // Convert each material to USD.
        ConvertToUSDMaterial(material, stage, fileName, isUSDZ, targetPath, options, matBindings);
    }
}

} // namespace MAXUSD_NS_DEF
