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
#include "MultiMaterialUtils.h"

#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/Translators/ShadingUtils.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include <iparamb2.h>
#include <stdmat.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace MaxUsdMultiMaterialUtils {

void AddMaterialDependencies(Mtl* material, std::vector<Mtl*>& subMtl)
{
    if (!material) {
        return;
    }

    if (material->IsMultiMtl()) {
        // If the material is a Multi/Sub-Object material, add all its sub-materials
        for (int i = 0; i < material->NumSubMtls(); ++i) {
            if (auto multiSubMtl = material->GetSubMtl(i)) {
                subMtl.push_back(multiSubMtl);
            }
        }
    } else {
        // Simple material, add it directly
        subMtl.push_back(material);
    }
}

void GetMatIDsFromMultiMat(Mtl* mat, std::set<int>& matIdSet)
{
    if (!mat || !mat->IsMultiMtl()) {
        return;
    }

    // Get material IDs from the Multi/Sub-Object material's parameter block
    IParamBlock2* mtlParamBlock2 = mat->GetParamBlockByID(0);
    if (!mtlParamBlock2) {
        return;
    }

    short paramId = MaxUsd::FindParamId(mtlParamBlock2, L"materialIDList");
    if (paramId < 0) {
        return;
    }

    Interval valid = FOREVER;
    for (int subIdx = 0; subIdx < mat->NumSubs(); subIdx++) {
        int matId;
        mtlParamBlock2->GetValue(paramId, 0, matId, valid, subIdx);
        matIdSet.insert(matId);
    }
}

bool HasMultiSubDependency(Mtl* material)
{
    return material && material->IsMultiMtl();
}

MaterialBindings::const_iterator FindMaterialBindings(
    Mtl*                            material,
    const MaxUsdWriteJobContext&    writeJobCtx)
{
    auto bindings = writeJobCtx.GetMaterialBindings();
    return std::find_if(bindings.begin(), bindings.end(), [material](const MaterialBinding& mb) {
        return mb.GetMaterial() == material;
    });
}

Mtl* GetSubMaterialByID(Mtl* material, int matID, const std::set<int>& matIdSet)
{
    if (!material) {
        return nullptr;
    }

    if (material->ClassID() == MULTI_MATERIAL_CLASS_ID) {
        const auto matIdIter = matIdSet.find(matID % material->NumSubMtls());
        if (matIdIter != matIdSet.end()) {
            return material->GetSubMtl(*matIdIter);
        }
    }
    
    return material;
}

void DiscoverMaterialIDsAndCreateBundles(
    const std::list<SdfPath>&       geomBindPaths,
    UsdStagePtr                     stage,
    const MaxUsdWriteJobContext&    writeJobCtx,
    std::vector<std::set<int>>&     matIDsSets,
    const CreateBundleCallback&     createBundleCallback)
{
    for (auto geomBindPath : geomBindPaths) {
        auto geomPrim = stage->GetPrimAtPath(geomBindPath);

        if (geomPrim.IsInstance()) {
            auto protoPrim = geomPrim.GetPrototype().GetChildren().front();
            if (std::find(geomBindPaths.begin(), geomBindPaths.end(), protoPrim.GetPath())
                != geomBindPaths.end()) {
                // Nothing to do for this instance.
                continue;
            }
            // This instance has a different material than its prototype, break it.
            else {
                auto target = writeJobCtx.GetArgs().GetUseSeparateMaterialLayer()
                    ? stage->GetRootLayer()
                    : stage->GetEditTarget();

                UsdEditContext             editContext(stage, target);
                UsdShadeMaterialBindingAPI bindingAPI(protoPrim);
                auto                       subsetToCopy = bindingAPI.GetMaterialBindSubsets();
                if (!subsetToCopy.empty()) {
                    geomPrim = MaxUsdShadingUtils::BreakInstancingAndCopySubset(
                        stage, geomPrim, protoPrim, subsetToCopy);
                    geomBindPath = geomPrim.GetPath();
                }
            }
        }

        std::set<int> materialIdsSet;
        for (auto child : geomPrim.GetAllChildren()) {
            if (child.IsA<UsdGeomSubset>()) {
                materialIdsSet.insert(
                    MaxUsd::MeshConverter::GetMaterialIdFromCustomData(child));
            }
        }

        if (materialIdsSet.empty()) {
            // No geomSubSet, look for the MatID on the prim itself.
            int matId = MaxUsd::MeshConverter::GetMaterialIdFromCustomData(geomPrim);
            if (matId == -1) {
                // Didn't find the custom data, skip this Prim.
                continue;
            }
            materialIdsSet.insert(matId);
        }

        // Use callback to create bundle for this material ID set
        createBundleCallback(geomBindPath, materialIdsSet, matIDsSets);
    }
}



} // namespace MaxUsdMultiMaterialUtils

PXR_NAMESPACE_CLOSE_SCOPE
