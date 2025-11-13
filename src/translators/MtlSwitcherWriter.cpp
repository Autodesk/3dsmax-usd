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
#include "MtlSwitcherWriter.h"
#ifdef IS_MAX2024_OR_GREATER
#include "MultiMaterialUtils.h"
#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/Translators/ShaderWriterRegistry.h>
#include <MaxUsd/Translators/ShadingUtils.h>

#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <Materials/MaterialSwitcherInterface.h>

PXR_NAMESPACE_USING_DIRECTIVE

PXR_MAXUSD_REGISTER_SHADER_WRITER(MATERIAL_SWITCHER_CLASS_ID, MtlSwitcherWriter);

MtlSwitcherWriter::MtlSwitcherWriter(
    Mtl*                   material,
    const SdfPath&         usdPath,
    MaxUsdWriteJobContext& jobCtx)
    : MaxUsdShaderWriter(material, usdPath, jobCtx)
{
    GetTopLevelMtlDependencies(variantMaterials);
    exportStyle = jobCtx.GetArgs().GetMtlSwitcherExportStyle();
    // if only one material in the switcher, fallback to use only a reference to that material; no
    // need for a variant
    if (variantMaterials.size() == 1
        && exportStyle == MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::AsVariantSets) {
        exportStyle = MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::ActiveMaterialOnly;
    }
}

void MtlSwitcherWriter::Write()
{
    if (variantMaterials.empty()) {
        // no material binding required
        // the material switcher is empty
        MaxUsd::Log::Warn(
            "Material Switcher \"{0}\" is empty. No material binding will be exported.",
            MaxUsd::MaxStringToUsdString(GetMaterial()->GetName()));
        return;
    }

    UsdPrim usdMaterial = GetUsdStage()->GetPrimAtPath(GetUsdPath().GetParentPath());

    if (exportStyle == MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::AsVariantSets) {
        UsdVariantSet variantSet = usdMaterial.GetVariantSets().AddVariantSet("shadingVariant");

        // Discover if one of the material is a Multi material, the export flow will be different.
        for (const auto variant : variantMaterials) {
            if (MaxUsdMultiMaterialUtils::HasMultiSubDependency(variant)) {
                hasMultiSubDependency = true;
                break;
            }
        }
    } else if (
        exportStyle == MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::ActiveMaterialOnly) {
        MaxSDK::MtlSwitcherInterface* msi = static_cast<MaxSDK::MtlSwitcherInterface*>(
            GetMaterial()->GetInterface(MTL_SWITCHER_ACCESS_INTERFACE));
        Mtl* activeMtl = msi->GetActiveMtl();
        if (MaxUsdMultiMaterialUtils::HasMultiSubDependency(activeMtl)) {
            hasMultiSubDependency = true;
        } else {
            auto references = usdMaterial.GetReferences();
            references.AddInternalReference(SdfPath());
        }
    }

    if (hasMultiSubDependency) {
        auto bindings = writeJobCtx.GetMaterialBindings();
        auto myMtl = GetMaterial();
        auto it
            = std::find_if(bindings.begin(), bindings.end(), [myMtl](const MaterialBinding& mb) {
                  return mb.GetMaterial() == myMtl;
              });

        if (it == bindings.end()) {
            // not supported for now if this switcher has a Multi material connected and is nested
            // in the Shader Tree
            MaxUsd::Log::Warn(
                "Material Switcher \"{0}\" cannot be exported, the export of a Material switcher "
                "with a Multi material dependency"
                " is supported only when directly connected to an object.",
                MaxUsd::MaxStringToUsdString(myMtl->GetName()));
            return;
        }

        auto geomBindPaths = it->GetBindings();

        // Used to keep track of the material IDs set discovered.
        std::vector<std::set<int>> matIDsSets;
        
        // Use shared utility for material ID discovery and bundle creation
        auto createVariantBundleCallback = [this](
            const SdfPath& geomBindPath,
            const std::set<int>& materialIdsSet,
            std::vector<std::set<int>>& matIDsSets) {
            const auto findIt = std::find(matIDsSets.begin(), matIDsSets.end(), materialIdsSet);
            if (findIt != matIDsSets.end()) {
                // The bundle for this matID set already exists, just add the binding path to it.
                variantBundles[findIt - matIDsSets.begin()].geomBindPaths.push_back(geomBindPath);
                return;
            }
            matIDsSets.push_back(materialIdsSet);
            VariantBundle bundle;
            bundle.geomBindPaths.push_back(geomBindPath);
            bundle.matSetIdx = materialIdsSet;
            variantBundles.emplace_back(bundle);
        };
        
        MaxUsdMultiMaterialUtils::DiscoverMaterialIDsAndCreateBundles(
            geomBindPaths, GetUsdStage(), writeJobCtx, matIDsSets, createVariantBundleCallback);

        int bundleCount = 0;
        for (auto& variantBundle : variantBundles) {
            // Create a number of materials inside the Material Switcher Prim that represents the
            // bundle material IDs. The variant set will use these materials to add the references
            // to the actual materials without having to alter the bindings.
            for (int i : variantBundle.matSetIdx) {
                TfToken subName { GetUsdPath().GetNameToken().GetString() + "_Set_"
                                  + std::to_string(bundleCount + 1) + "_MatID_"
                                  + std::to_string(i + 1) };
                variantBundle.subObjsMatPrims.push_back(pxr::UsdShadeMaterial::Define(
                    GetUsdStage(), GetUsdPath().GetParentPath().AppendChild(subName)));
            }
            bundleCount++;
        }
    }
}

MaxUsdShaderWriter::ContextSupport
MtlSwitcherWriter::CanExport(const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    return ContextSupport::Fallback;
}

bool MtlSwitcherWriter::IsMaterialTargetAgnostic() { return true; }

void MtlSwitcherWriter::GetSubMtlDependencies(std::vector<Mtl*>& subMtl) const
{
    if (writeJobCtx.GetArgs().GetMtlSwitcherExportStyle()
        == MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::ActiveMaterialOnly) {
        // only export active material
        MaxSDK::MtlSwitcherInterface* msi = static_cast<MaxSDK::MtlSwitcherInterface*>(
            GetMaterial()->GetInterface(MTL_SWITCHER_ACCESS_INTERFACE));
        const auto activeMtl = msi->GetActiveMtl();

        if (activeMtl == nullptr) {
            return;
        }

        MaxUsdMultiMaterialUtils::AddMaterialDependencies(activeMtl, subMtl);
        return;
    }

    for (int i = 0; i < GetMaterial()->NumSubMtls(); ++i) {
        if (auto mtl = GetMaterial()->GetSubMtl(i)) {
            MaxUsdMultiMaterialUtils::AddMaterialDependencies(mtl, subMtl);
        }
    }
}

void MtlSwitcherWriter::GetTopLevelMtlDependencies(std::vector<Mtl*>& subMtl) const
{
    if (writeJobCtx.GetArgs().GetMtlSwitcherExportStyle()
        == MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::ActiveMaterialOnly) {
        // only export active material
        MaxSDK::MtlSwitcherInterface* msi = static_cast<MaxSDK::MtlSwitcherInterface*>(
            GetMaterial()->GetInterface(MTL_SWITCHER_ACCESS_INTERFACE));
        auto activeMtl = msi->GetActiveMtl();
        if (activeMtl != nullptr) {
            subMtl.push_back(activeMtl);
        }
        return;
    }
    __super::GetSubMtlDependencies(subMtl);
}



void MtlSwitcherWriter::BindPlaceholderMatsToGeom()
{
    // Bind the geomSubSet to the placeholder materials.
    for (const auto& variantBundle : variantBundles) {
        for (const auto& path : variantBundle.geomBindPaths) {
            auto geomPrim = GetUsdStage()->GetPrimAtPath(path);

            int boundMatCount = 0;
            for (auto child : geomPrim.GetAllChildren()) {
                if (child.IsA<UsdGeomSubset>()) {
                    auto bindingAPI = UsdShadeMaterialBindingAPI::Apply(child);
                    bindingAPI.Bind(variantBundle.subObjsMatPrims[boundMatCount]);
                    boundMatCount++;
                }
            }
            UsdShadeMaterialBindingAPI shadeAPI(geomPrim);
            // If we bound some GeomSubSet, we unbind the material switcher from the parent prim.
            if (boundMatCount != 0) {
                shadeAPI.UnbindAllBindings();
            }
            // Otherwise, it means it's a single ID case, so bind it to the placeholder material.
            else {
                shadeAPI.Bind(variantBundle.subObjsMatPrims[0]);
            }
        }
    }
}

void MtlSwitcherWriter::BindVariantBundleToMat(
    const VariantBundle& variantBundle,
    Mtl*                 variant,
    std::set<int>&       matIdSet,
    const UsdVariantSet* variantSet)
{
    int subGeo = 0;
    for (int matID : variantBundle.matSetIdx) {
        Mtl* subMat = MaxUsdMultiMaterialUtils::GetSubMaterialByID(variant, matID, matIdSet);

        const auto matIter = writeJobCtx.GetMaterialsToPrimsMap().find(subMat);
        if (matIter == writeJobCtx.GetMaterialsToPrimsMap().end()) {
            if (subMat != nullptr) {
                MaxUsd::Log::Warn(
                    "Material \"{0}\" from Material Switcher \"{1}\" cannot be referenced as it "
                    "was "
                    "not properly exported.",
                    MaxUsd::MaxStringToUsdString(variant->GetName()),
                    MaxUsd::MaxStringToUsdString(GetMaterial()->GetName()));
            }
            subGeo++;
            continue;
        }
        auto refs = variantBundle.subObjsMatPrims[subGeo].GetPrim().GetReferences();
        if (variantSet != nullptr && variantSet->IsValid()) {
            UsdEditContext context(variantSet->GetVariantEditContext());
            refs.AddInternalReference(matIter->second);
        } else {
            refs.AddInternalReference(matIter->second);
        }
        subGeo++;
    }
}

void MtlSwitcherWriter::PostWrite()
{
    if (variantMaterials.empty()) {
        // no material binding required
        // the material switcher is empty
        return;
    }

    UsdPrim usdMaterial = GetUsdStage()->GetPrimAtPath(GetUsdPath().GetParentPath());
    auto    references = usdMaterial.GetReferences();

    if (exportStyle == MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::AsVariantSets) {
        MaxSDK::MtlSwitcherInterface* msi = static_cast<MaxSDK::MtlSwitcherInterface*>(
            GetMaterial()->GetInterface(MTL_SWITCHER_ACCESS_INTERFACE));
        Mtl*        activeMtl = msi->GetActiveMtl();
        std::string activeMtlName;

        UsdVariantSet variantSet = usdMaterial.GetVariantSets().GetVariantSet("shadingVariant");
        if (!hasMultiSubDependency) {
            for (const auto variant : variantMaterials) {
                const auto matIter = writeJobCtx.GetMaterialsToPrimsMap().find(variant);
                if (matIter == writeJobCtx.GetMaterialsToPrimsMap().end()) {
                    MaxUsd::Log::Warn(
                        "Material \"{0}\" from Material Switcher \"{1}\" cannot be referenced as "
                        "it was "
                        "not properly exported.",
                        MaxUsd::MaxStringToUsdString(variant->GetName()),
                        MaxUsd::MaxStringToUsdString(GetMaterial()->GetName()));
                    continue;
                }

                auto variantName
                    = TfMakeValidIdentifier(MaxUsd::MaxStringToUsdString(variant->GetName()));
                if (variant == activeMtl) {
                    // save the active material variant for later reference
                    activeMtlName = variantName;
                }
                variantSet.AddVariant(variantName);
                variantSet.SetVariantSelection(variantName);
                {
                    UsdEditContext context(variantSet.GetVariantEditContext());
                    references.AddInternalReference(matIter->second);
                }
            }
        } else {
            BindPlaceholderMatsToGeom();

            for (const auto variant : variantMaterials) {
                auto variantName
                    = TfMakeValidIdentifier(MaxUsd::MaxStringToUsdString(variant->GetName()));
                if (variant == activeMtl) {
                    // save the active material variant for later reference
                    activeMtlName = variantName;
                }
                variantSet.AddVariant(variantName);
                variantSet.SetVariantSelection(variantName);

                // Will be used to match the material id with the geom material ID, if the material
                // is a Multi sub material
                std::set<int> matIdSet;
                MaxUsdMultiMaterialUtils::GetMatIDsFromMultiMat(variant, matIdSet);

                for (auto& variantBundle : variantBundles) {
                    BindVariantBundleToMat(variantBundle, variant, matIdSet, &variantSet);
                }
            }
        }
        // set the default selected variant to be the active material from the material switcher
        variantSet.SetVariantSelection(activeMtlName);
    } else if (
        exportStyle == MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::ActiveMaterialOnly) {
        MaxSDK::MtlSwitcherInterface* msi = static_cast<MaxSDK::MtlSwitcherInterface*>(
            GetMaterial()->GetInterface(MTL_SWITCHER_ACCESS_INTERFACE));
        Mtl* activeMtl = msi->GetActiveMtl();
        if (hasMultiSubDependency) {
            BindPlaceholderMatsToGeom();
            // Will be used to match the material id with the geom material ID, if the material is a
            // Multi sub material
            std::set<int> matIdSet;
            MaxUsdMultiMaterialUtils::GetMatIDsFromMultiMat(activeMtl, matIdSet);
            for (auto& variantBundle : variantBundles) {
                BindVariantBundleToMat(variantBundle, activeMtl, matIdSet);
            }
        } else {
            const auto matIter = writeJobCtx.GetMaterialsToPrimsMap().find(activeMtl);
            if (matIter == writeJobCtx.GetMaterialsToPrimsMap().end()) {
                MaxUsd::Log::Warn(
                    "Active Material \"{0}\" for Material Switcher \"{1}\" cannot be referenced as "
                    "it was "
                    "not properly exported.",
                    MaxUsd::MaxStringToUsdString(activeMtl->GetName()),
                    MaxUsd::MaxStringToUsdString(GetMaterial()->GetName()));
                return;
            }
            references.ClearReferences();
            references.AddInternalReference(matIter->second);
        }
    }
}
#endif