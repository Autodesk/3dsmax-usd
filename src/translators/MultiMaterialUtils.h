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
#pragma once

#include <MaxUsd/MaxUSDAPI.h>
#include <MaxUsd/Translators/WriteJobContext.h>

#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>

#include <set>
#include <vector>
#include <list>
#include <functional>

class Mtl;

PXR_NAMESPACE_OPEN_SCOPE

namespace MaxUsdMultiMaterialUtils {

/**
 * \brief Base structure for material bundles that group geometry by material ID sets
 */
struct MaterialBundle
{
    std::set<int>      matSetIdx;
    std::list<SdfPath> geomBindPaths;
};

/**
 * \brief Add material dependencies (handles both simple and Multi/Sub-Object materials)
 * \param material The material to process
 * \param subMtl The vector to add dependencies to
 */
void AddMaterialDependencies(Mtl* material, std::vector<Mtl*>& subMtl);

/**
 * \brief Get material IDs from a Multi/Sub-Object material
 * \param mat The Multi/Sub-Object material
 * \param matIdSet The set to report the material IDs
 */
void GetMatIDsFromMultiMat(Mtl* mat, std::set<int>& matIdSet);

/**
 * \brief Check if a material has Multi/Sub-Object dependencies
 * \param material The material to check
 * \return True if the material is a Multi/Sub-Object material
 */
bool HasMultiSubDependency(Mtl* material);

/**
 * \brief Find material bindings for a given material
 * \param material The material to find bindings for
 * \param writeJobCtx The write job context
 * \return Iterator to the material binding, or end() if not found
 */
MaterialBindings::const_iterator FindMaterialBindings(
    Mtl*                            material,
    const MaxUsdWriteJobContext&    writeJobCtx);

/**
 * \brief Callback function type for creating material bundles
 * \param geomBindPath The geometry binding path
 * \param materialIdsSet The material IDs set for this geometry
 * \param matIDsSets Vector tracking all material ID sets
 */
using CreateBundleCallback = std::function<void(
    const SdfPath&              geomBindPath,
    const std::set<int>&        materialIdsSet,
    std::vector<std::set<int>>& matIDsSets)>;

/**
 * \brief Discover material IDs from geometry prims and create material bundles
 * \param geomBindPaths The geometry binding paths to process
 * \param stage The USD stage
 * \param writeJobCtx The write job context
 * \param matIDsSets Vector to track material ID sets
 * \param createBundleCallback Callback to create bundles for each material ID set
 */
void DiscoverMaterialIDsAndCreateBundles(
    const std::list<SdfPath>&       geomBindPaths,
    UsdStagePtr                     stage,
    const MaxUsdWriteJobContext&    writeJobCtx,
    std::vector<std::set<int>>&     matIDsSets,
    const CreateBundleCallback&     createBundleCallback);

/**
 * \brief Get the correct sub-material from a Multi/Sub-Object material based on material ID
 * \param material The Multi/Sub-Object material
 * \param matID The material ID to look up
 * \param matIdSet The set of material IDs from the Multi/Sub-Object material
 * \return The sub-material corresponding to the material ID, or the material itself if not Multi/Sub-Object
 */
Mtl* GetSubMaterialByID(Mtl* material, int matID, const std::set<int>& matIdSet);

} // namespace MaxUsdMultiMaterialUtils

PXR_NAMESPACE_CLOSE_SCOPE
