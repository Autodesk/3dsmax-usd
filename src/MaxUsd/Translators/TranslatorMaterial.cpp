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
#include "TranslatorMaterial.h"

#include "ShadingModeImporter.h"
#include "ShadingModeRegistry.h"

#include <MaxUsd/MeshConversion/MeshConverter.h>

#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include <Animatable.h>
#include <stdmat.h>

PXR_NAMESPACE_OPEN_SCOPE

Mtl* MaxUsdTranslatorMaterial::Read(
    const MaxUsd::MaxSceneBuilderOptions& buildOptions,
    const UsdShadeMaterial&               shadeMaterial,
    const UsdGeomGprim&                   boundPrim,
    MaxUsdReadJobContext&                 context)
{
    MaxUsdShadingModeImportContext importContext(shadeMaterial, boundPrim, context);
    Mtl*                           mat = nullptr;

    auto localOptions = buildOptions;
    for (const auto& shadingMode : buildOptions.GetShadingModes()) {
        if (!VtDictionaryIsHolding<TfToken>(shadingMode, MaxUsdShadingModesTokens->mode)) {
            continue;
        }

        if (VtDictionaryGet<TfToken>(shadingMode, MaxUsdShadingModesTokens->mode)
            == MaxUsdShadingModeTokens->none) {
            break;
        }

        if (MaxUsdShadingModeImporter importer = MaxUsdShadingModeRegistry::GetImporter(
                VtDictionaryGet<TfToken>(shadingMode, MaxUsdShadingModesTokens->mode))) {
            localOptions.SetShadingModes(
                MaxUsd::MaxSceneBuilderOptions::ShadingModes(1, shadingMode));
            mat = importer(&importContext, localOptions);
        }
        if (mat) {
            importContext.AddCreatedMaterial(shadeMaterial.GetPrim(), mat);
            return mat;
        }
    }
    return mat;
}

bool MaxUsdTranslatorMaterial::AssignMaterial(
    const MaxUsd::MaxSceneBuilderOptions& buildOptions,
    const UsdGeomGprim&                   primSchema,
    INode*                                node,
    MaxUsdReadJobContext&                 context)
{
    const UsdShadeMaterialBindingAPI bindingAPI(primSchema.GetPrim());
    UsdShadeMaterial                 meshMaterial = bindingAPI.ComputeBoundMaterial();
    if (meshMaterial) {
        Mtl* mat = Read(buildOptions, meshMaterial, primSchema, context);
        if (!mat) {
            return false;
        }
        auto matName = MaxUsd::UsdStringToMaxString(meshMaterial.GetPath().GetName());
        mat->SetName(matName);

        // assign material to mesh
        node->SetMtl(mat);
        node->NotifyDependents(FOREVER, PART_MTL, REFMSG_CHANGE);
        node->InvalidateWS();

        // assign material to faces
        std::vector<UsdGeomSubset> faceSubsets
            = UsdShadeMaterialBindingAPI(primSchema.GetPrim()).GetMaterialBindSubsets();

        if (!faceSubsets.empty()) {
            std::sort(
                faceSubsets.begin(),
                faceSubsets.end(),
                [](const UsdGeomSubset& s1, const UsdGeomSubset& s2) {
                    return s1.GetPrim().GetName() < s2.GetPrim().GetName();
                });

            // Start by populating all the material ID for subset with custom data
            // std::map<int, std::string> matIdToGeomSubsetPrimNameMap;
            std::vector<int> subsetWithNoCustomDataIndexes;
            for (int i = 0; i < faceSubsets.size(); i++) {
                UsdGeomSubset subset = faceSubsets[i];
                int matId = MaxUsd::MeshConverter::GetMaterialIdFromCustomData(subset.GetPrim());
                if (matId >= 0) {
                    auto multiMat = dynamic_cast<MultiMtl*>(node->GetMtl());
                    if (multiMat) {
                        multiMat->SetSubMtlAndName(matId, mat, matName);
                    }
                }
            }
        }
    } else {
        // assign material to faces
        std::vector<UsdGeomSubset> faceSubsets
            = UsdShadeMaterialBindingAPI(primSchema.GetPrim()).GetMaterialBindSubsets();
        if (faceSubsets.empty()) {
            return false;
        }
        for (int i = 0; i < faceSubsets.size(); i++) {
            auto                             subsetPrim = faceSubsets[i];
            const UsdShadeMaterialBindingAPI bindingAPI(subsetPrim.GetPrim());
            UsdShadeMaterial                 meshMaterial = bindingAPI.ComputeBoundMaterial();
            if (!meshMaterial) {
                continue;
            }

            // fetch material
            Mtl* mat = MaxUsdTranslatorMaterial::Read(
                context.GetArgs(), meshMaterial, UsdGeomGprim(subsetPrim), context);
            if (!mat) {
                continue;
            }
            auto matName = MaxUsd::UsdStringToMaxString(meshMaterial.GetPath().GetName());
            mat->SetName(matName);

            // assign material to proper INode submtl
            MultiMtl* multiMat = dynamic_cast<MultiMtl*>(node->GetMtl());
            if (multiMat) {
                auto matName = MaxUsd::UsdStringToMaxString(subsetPrim.GetPath().GetName());
                for (int i = 0; i < multiMat->NumSubMtls(); ++i) {
                    MSTR name;
                    multiMat->GetSubMtlName(i, name);
                    if (name == matName) {
                        multiMat->SetSubMtlAndName(i, mat, matName);
                    }
                }
            }
        }
    }
    return true;
}

/* static */
void MaxUsdTranslatorMaterial::ExportMaterials(
    MaxUsdWriteJobContext&                                  writeJobContext,
    const pxr::TfHashSet<pxr::SdfPath, pxr::SdfPath::Hash>& primsToMaterialBind,
    MaxUsd::MaxProgressBar&                                 progress)
{
    const TfToken& shadingMode = writeJobContext.GetArgs().GetShadingMode();
    if (shadingMode == MaxUsdShadingModeTokens->none) {
        return;
    }

    if (auto exporterCreator = MaxUsdShadingModeRegistry::GetExporter(shadingMode)) {
        if (auto exporter = exporterCreator()) {
            exporter->DoExport(writeJobContext, primsToMaterialBind, progress);
        } else {
            TF_RUNTIME_ERROR(
                "Failed creating exporter for shadingMode '%s'.", shadingMode.GetText());
        }
    } else {
        TF_RUNTIME_ERROR("No shadingMode '%s' found.", shadingMode.GetText());
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
