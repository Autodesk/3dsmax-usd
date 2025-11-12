//
// Copyright 2025 Autodesk
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
#include "ShellMtlWriter.h"

#include "MultiMaterialUtils.h"

#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/Translators/ShaderWriterRegistry.h>
#include <MaxUsd/Translators/ShadingUtils.h>

#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <plugapi.h>

PXR_NAMESPACE_USING_DIRECTIVE

static Class_ID bakeShellClassID(BAKE_SHELL_CLASS_ID, 0);
PXR_MAXUSD_REGISTER_SHADER_WRITER(bakeShellClassID, ShellMtlWriter);

ShellMtlWriter::ShellMtlWriter(Mtl* material, const SdfPath& usdPath, MaxUsdWriteJobContext& jobCtx)
    : MaxUsdShaderWriter(material, usdPath, jobCtx)
{
    exportStyle = jobCtx.GetArgs().GetShellMtlExportStyle();
}

MaxUsdShaderWriter::ContextSupport
ShellMtlWriter::CanExport(const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    return ContextSupport::Fallback;
}

bool ShellMtlWriter::IsMaterialTargetAgnostic() { return true; }

void ShellMtlWriter::Write()
{
    Mtl* originalMaterial = GetOriginalMaterial();
    Mtl* bakedMaterial = GetBakedMaterial();

    if (!originalMaterial && !bakedMaterial) {
        MaxUsd::Log::Warn(
            "Shell Material \"{0}\" has neither original nor baked materials. No material will be "
            "exported.",
            MaxUsd::MaxStringToUsdString(GetMaterial()->GetName()));
        return;
    }

    // Check if we have Multi/Sub-Object materials
    hasMultiSubDependency = MaxUsdMultiMaterialUtils::HasMultiSubDependency(originalMaterial)
        || MaxUsdMultiMaterialUtils::HasMultiSubDependency(bakedMaterial);

    if (hasMultiSubDependency) {
        // Handle Multi/Sub-Object materials - need to create placeholder materials
        auto bindings = writeJobCtx.GetMaterialBindings();
        auto myMtl = GetMaterial();
        auto it
            = std::find_if(bindings.begin(), bindings.end(), [myMtl](const MaterialBinding& mb) {
                  return mb.GetMaterial() == myMtl;
              });

        if (it == bindings.end()) {
            MaxUsd::Log::Warn(
                "Shell Material \"{0}\" cannot be exported, the export of a Shell material "
                "with a Multi/Sub-Object material dependency is supported only when directly "
                "connected to an object.",
                MaxUsd::MaxStringToUsdString(myMtl->GetName()));
            return;
        }

        auto geomBindPaths = it->GetBindings();

        // Used to keep track of the material IDs set discovered.
        std::vector<std::set<int>> matIDsSets;

        // Use shared utility for material ID discovery and bundle creation
        auto createShellBundleCallback = [this](
                                             const SdfPath&              geomBindPath,
                                             const std::set<int>&        materialIdsSet,
                                             std::vector<std::set<int>>& matIDsSets) {
            const auto findIt = std::find(matIDsSets.begin(), matIDsSets.end(), materialIdsSet);
            if (findIt != matIDsSets.end()) {
                // The bundle for this matID set already exists, just add the binding path to it.
                shellBundles[findIt - matIDsSets.begin()].geomBindPaths.push_back(geomBindPath);
                return;
            }
            matIDsSets.push_back(materialIdsSet);
            ShellBundle bundle;
            bundle.geomBindPaths.push_back(geomBindPath);
            bundle.matSetIdx = materialIdsSet;
            shellBundles.emplace_back(bundle);
        };

        MaxUsdMultiMaterialUtils::DiscoverMaterialIDsAndCreateBundles(
            geomBindPaths, GetUsdStage(), writeJobCtx, matIDsSets, createShellBundleCallback);

        // Create placeholder materials for each material ID set
        int bundleCount = 0;
        for (auto& shellBundle : shellBundles) {
            for (int i : shellBundle.matSetIdx) {
                // Create placeholder materials for both purposes
                if (exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Both
                    || exportStyle
                        == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Original) {
                    TfToken originalSubName { GetUsdPath().GetNameToken().GetString()
                                              + "_Original_Set_" + std::to_string(bundleCount + 1)
                                              + "_MatID_" + std::to_string(i + 1) };
                    shellBundle.originalMatPrims.push_back(pxr::UsdShadeMaterial::Define(
                        GetUsdStage(), GetUsdPath().GetParentPath().AppendChild(originalSubName)));
                }

                if (exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Both
                    || exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Baked) {
                    TfToken bakedSubName { GetUsdPath().GetNameToken().GetString() + "_Baked_Set_"
                                           + std::to_string(bundleCount + 1) + "_MatID_"
                                           + std::to_string(i + 1) };
                    shellBundle.bakedMatPrims.push_back(pxr::UsdShadeMaterial::Define(
                        GetUsdStage(), GetUsdPath().GetParentPath().AppendChild(bakedSubName)));
                }
            }
            bundleCount++;
        }
    }
    // Material binding is handled in PostWrite
}

void ShellMtlWriter::GetSubMtlDependencies(std::vector<Mtl*>& subMtl) const
{
    switch (exportStyle) {
    case MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Original:
        MaxUsdMultiMaterialUtils::AddMaterialDependencies(GetOriginalMaterial(), subMtl);
        break;

    case MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Baked:
        MaxUsdMultiMaterialUtils::AddMaterialDependencies(GetBakedMaterial(), subMtl);
        break;

    case MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Both:
        MaxUsdMultiMaterialUtils::AddMaterialDependencies(GetOriginalMaterial(), subMtl);
        MaxUsdMultiMaterialUtils::AddMaterialDependencies(GetBakedMaterial(), subMtl);
        break;
    }
}

void ShellMtlWriter::PostWrite()
{
    Mtl* originalMaterial = GetOriginalMaterial();
    Mtl* bakedMaterial = GetBakedMaterial();

    // Validate materials based on export style
    if (exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Original
        && !originalMaterial) {
        MaxUsd::Log::Warn(
            "Shell Material \"{0}\" set to export 'Original' material, but missing original "
            "material.",
            MaxUsd::MaxStringToUsdString(GetMaterial()->GetName()));
        return;
    }

    if (exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Baked
        && !bakedMaterial) {
        MaxUsd::Log::Warn(
            "Shell Material \"{0}\" set to export 'Baked' material, but missing baked material.",
            MaxUsd::MaxStringToUsdString(GetMaterial()->GetName()));
        return;
    }

    if (exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Both
        && (!originalMaterial || !bakedMaterial)) {
        MaxUsd::Log::Warn(
            "Shell Material \"{0}\" set to export 'Both' materials, but missing original or baked "
            "material.",
            MaxUsd::MaxStringToUsdString(GetMaterial()->GetName()));
        return;
    }

    if (hasMultiSubDependency) {
        BindPlaceholderMatsToGeom();

        // Get material IDs from Multi/Sub-Object materials
        std::set<int> originalMatIdSet;
        std::set<int> bakedMatIdSet;

        if (originalMaterial) {
            MaxUsdMultiMaterialUtils::GetMatIDsFromMultiMat(originalMaterial, originalMatIdSet);
        }
        if (bakedMaterial) {
            MaxUsdMultiMaterialUtils::GetMatIDsFromMultiMat(bakedMaterial, bakedMatIdSet);
        }

        for (auto& shellBundle : shellBundles) {
            BindShellBundleToMaterials(
                shellBundle, originalMaterial, bakedMaterial, originalMatIdSet, bakedMatIdSet);
        }
    } else {
        // Simple case - bind materials directly to geometry
        BindMaterialsWithPurposes(originalMaterial, bakedMaterial);
    }
}

void ShellMtlWriter::BindMaterialsWithPurposes(Mtl* originalMaterial, Mtl* bakedMaterial)
{
    // Get the material bindings for this shell material
    auto bindings = writeJobCtx.GetMaterialBindings();
    auto myMtl = GetMaterial();
    auto it = std::find_if(bindings.begin(), bindings.end(), [myMtl](const MaterialBinding& mb) {
        return mb.GetMaterial() == myMtl;
    });

    if (it == bindings.end()) {
        MaxUsd::Log::Warn(
            "Shell Material \"{0}\" has no geometry bindings.",
            MaxUsd::MaxStringToUsdString(myMtl->GetName()));
        return;
    }

    auto geomBindPaths = it->GetBindings();

    // Find the USD materials for our original and baked materials
    const auto originalMatIter = originalMaterial
        ? writeJobCtx.GetMaterialsToPrimsMap().find(originalMaterial)
        : writeJobCtx.GetMaterialsToPrimsMap().end();
    const auto bakedMatIter = bakedMaterial
        ? writeJobCtx.GetMaterialsToPrimsMap().find(bakedMaterial)
        : writeJobCtx.GetMaterialsToPrimsMap().end();

    if (originalMaterial && originalMatIter == writeJobCtx.GetMaterialsToPrimsMap().end()
        && (exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Original
            || exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Both)) {
        MaxUsd::Log::Warn(
            "Original material \"{0}\" from Shell Material \"{1}\" was not properly exported.",
            MaxUsd::MaxStringToUsdString(originalMaterial->GetName()),
            MaxUsd::MaxStringToUsdString(myMtl->GetName()));
        return;
    }

    if (bakedMaterial && bakedMatIter == writeJobCtx.GetMaterialsToPrimsMap().end()
        && (exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Baked
            || exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Both)) {
        MaxUsd::Log::Warn(
            "Baked material \"{0}\" from Shell Material \"{1}\" was not properly exported.",
            MaxUsd::MaxStringToUsdString(bakedMaterial->GetName()),
            MaxUsd::MaxStringToUsdString(myMtl->GetName()));
        return;
    }

    // Bind materials to geometry with appropriate purposes
    for (const auto& geomBindPath : geomBindPaths) {
        UsdPrim geomPrim = GetUsdStage()->GetPrimAtPath(geomBindPath);
        if (!geomPrim.IsValid()) {
            continue;
        }

        UsdShadeMaterialBindingAPI bindingAPI = UsdShadeMaterialBindingAPI::Apply(geomPrim);

        // Bind materials directly based on export style
        switch (exportStyle) {
        case MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Both:
            if (originalMaterial && originalMatIter != writeJobCtx.GetMaterialsToPrimsMap().end()) {
                UsdShadeMaterial originalUsdMaterial
                    = UsdShadeMaterial::Get(GetUsdStage(), originalMatIter->second);
                bindingAPI.Bind(
                    originalUsdMaterial, UsdShadeTokens->fallbackStrength, UsdShadeTokens->full);
            }
            if (bakedMaterial && bakedMatIter != writeJobCtx.GetMaterialsToPrimsMap().end()) {
                UsdShadeMaterial bakedUsdMaterial
                    = UsdShadeMaterial::Get(GetUsdStage(), bakedMatIter->second);
                bindingAPI.Bind(
                    bakedUsdMaterial, UsdShadeTokens->fallbackStrength, UsdShadeTokens->preview);
            }
            break;

        case MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Baked:
            if (bakedMaterial && bakedMatIter != writeJobCtx.GetMaterialsToPrimsMap().end()) {
                UsdShadeMaterial bakedUsdMaterial
                    = UsdShadeMaterial::Get(GetUsdStage(), bakedMatIter->second);
                bindingAPI.Bind(bakedUsdMaterial);
            }
            break;

        case MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Original:
            if (originalMaterial && originalMatIter != writeJobCtx.GetMaterialsToPrimsMap().end()) {
                UsdShadeMaterial originalUsdMaterial
                    = UsdShadeMaterial::Get(GetUsdStage(), originalMatIter->second);
                bindingAPI.Bind(originalUsdMaterial);
            }
            break;
        }
    }
}

void ShellMtlWriter::BindPlaceholderMatsToGeom()
{
    // Bind the geomSubSet to the placeholder materials.
    for (const auto& shellBundle : shellBundles) {
        for (const auto& path : shellBundle.geomBindPaths) {
            auto geomPrim = GetUsdStage()->GetPrimAtPath(path);

            int boundMatCount = 0;
            for (auto child : geomPrim.GetAllChildren()) {
                if (child.IsA<UsdGeomSubset>()) {
                    auto bindingAPI = UsdShadeMaterialBindingAPI::Apply(child);

                    // Bind with appropriate purposes (Original->full, Baked->preview)
                    if (exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Both) {
                        if (boundMatCount < shellBundle.originalMatPrims.size()) {
                            bindingAPI.Bind(
                                shellBundle.originalMatPrims[boundMatCount],
                                UsdShadeTokens->fallbackStrength,
                                UsdShadeTokens->full);
                        }
                        if (boundMatCount < shellBundle.bakedMatPrims.size()) {
                            bindingAPI.Bind(
                                shellBundle.bakedMatPrims[boundMatCount],
                                UsdShadeTokens->fallbackStrength,
                                UsdShadeTokens->preview);
                        }
                    } else if (
                        exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Baked) {
                        if (boundMatCount < shellBundle.bakedMatPrims.size()) {
                            bindingAPI.Bind(shellBundle.bakedMatPrims[boundMatCount]);
                        }
                    } else if (
                        exportStyle
                        == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Original) {
                        if (boundMatCount < shellBundle.originalMatPrims.size()) {
                            bindingAPI.Bind(shellBundle.originalMatPrims[boundMatCount]);
                        }
                    }
                    boundMatCount++;
                }
            }

            UsdShadeMaterialBindingAPI shadeAPI(geomPrim);
            // If we bound some GeomSubSet, we unbind the shell material from the parent prim.
            if (boundMatCount != 0) {
                shadeAPI.UnbindAllBindings();
            }
            // Otherwise, it's a single ID case, so bind to the first placeholder material.
            else {
                if (exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Both) {
                    if (!shellBundle.originalMatPrims.empty()) {
                        shadeAPI.Bind(
                            shellBundle.originalMatPrims[0],
                            UsdShadeTokens->fallbackStrength,
                            UsdShadeTokens->full);
                    }
                    if (!shellBundle.bakedMatPrims.empty()) {
                        shadeAPI.Bind(
                            shellBundle.bakedMatPrims[0],
                            UsdShadeTokens->fallbackStrength,
                            UsdShadeTokens->preview);
                    }
                } else if (
                    exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Baked) {
                    if (!shellBundle.bakedMatPrims.empty()) {
                        shadeAPI.Bind(shellBundle.bakedMatPrims[0]);
                    }
                } else if (
                    exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Original) {
                    if (!shellBundle.originalMatPrims.empty()) {
                        shadeAPI.Bind(shellBundle.originalMatPrims[0]);
                    }
                }
            }
        }
    }
}

Mtl* ShellMtlWriter::GetOriginalMaterial() const { return GetMaterial()->GetSubMtl(0); }

Mtl* ShellMtlWriter::GetBakedMaterial() const { return GetMaterial()->GetSubMtl(1); }

void ShellMtlWriter::BindShellBundleToMaterials(
    const ShellBundle& shellBundle,
    Mtl*               originalMaterial,
    Mtl*               bakedMaterial,
    std::set<int>&     originalMatIdSet,
    std::set<int>&     bakedMatIdSet)
{
    int subGeo = 0;
    for (int matID : shellBundle.matSetIdx) {
        // Handle original material binding (preview purpose)
        if ((exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Both
             || exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Original)
            && originalMaterial && subGeo < shellBundle.originalMatPrims.size()) {

            Mtl* originalSubMat = MaxUsdMultiMaterialUtils::GetSubMaterialByID(
                originalMaterial, matID, originalMatIdSet);

            const auto originalMatIter = writeJobCtx.GetMaterialsToPrimsMap().find(originalSubMat);
            if (originalMatIter != writeJobCtx.GetMaterialsToPrimsMap().end()) {
                auto refs = shellBundle.originalMatPrims[subGeo].GetPrim().GetReferences();
                refs.AddInternalReference(originalMatIter->second);
            }
        }

        // Handle baked material binding (full purpose)
        if ((exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Both
             || exportStyle == MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle::Baked)
            && bakedMaterial && subGeo < shellBundle.bakedMatPrims.size()) {

            Mtl* bakedSubMat
                = MaxUsdMultiMaterialUtils::GetSubMaterialByID(bakedMaterial, matID, bakedMatIdSet);

            const auto bakedMatIter = writeJobCtx.GetMaterialsToPrimsMap().find(bakedSubMat);
            if (bakedMatIter != writeJobCtx.GetMaterialsToPrimsMap().end()) {
                auto refs = shellBundle.bakedMatPrims[subGeo].GetPrim().GetReferences();
                refs.AddInternalReference(bakedMatIter->second);
            }
        }

        subGeo++;
    }
}