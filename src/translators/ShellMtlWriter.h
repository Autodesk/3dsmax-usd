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

#include "MultiMaterialUtils.h"

#include <MaxUsd/Translators/ShaderWriter.h>
#include <MaxUsd/Translators/WriteJobContext.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>

PXR_NAMESPACE_OPEN_SCOPE

class UsdShadeMaterial;

/**
 * \brief ShaderWriter for 3ds Max Shell Materials (BakeShell).
 *
 * Shell materials allow different materials for viewport and render:
 * - Original Material (index 0): Used for rendering
 * - Baked Material (index 1): Used for viewport display
 *
 * This writer maps these to USD material purposes:
 * - Original → full purpose (render)
 * - Baked → preview purpose (viewport)
 */
class ShellMtlWriter : public MaxUsdShaderWriter
{
public:
    ShellMtlWriter(Mtl* material, const SdfPath& usdPath, MaxUsdWriteJobContext& jobCtx);

    static ContextSupport CanExport(const MaxUsd::USDSceneBuilderOptions& exportArgs);

    /// The shell material is material target agnostic, it does not define any shader of its own.
    static bool IsMaterialTargetAgnostic();

    void Write() override;

    bool HasMaterialDependencies() const override { return true; };

    void GetSubMtlDependencies(std::vector<Mtl*>& subMtl) const override;

    void PostWrite() override;

private:
    /// A shell bundle extends the base MaterialBundle with purpose-specific placeholder materials
    struct ShellBundle : public MaxUsdMultiMaterialUtils::MaterialBundle
    {
        std::vector<UsdShadeMaterial>
            originalMatPrims;                        // Placeholder materials for original (preview)
        std::vector<UsdShadeMaterial> bakedMatPrims; // Placeholder materials for baked (full)
    };

    /// \brief Bind materials to geometry with appropriate purposes
    /// \param originalMaterial The original material to bind
    /// \param bakedMaterial The baked material to bind
    void BindMaterialsWithPurposes(Mtl* originalMaterial, Mtl* bakedMaterial);

    /// \brief Bind placeholder materials to geometry for Multi/Sub-Object case
    void BindPlaceholderMatsToGeom();

    /// \brief Get the original material from the shell material
    /// \return The original material, or nullptr if not found
    Mtl* GetOriginalMaterial() const;

    /// \brief Get the baked material from the shell material
    /// \return The baked material, or nullptr if not found
    Mtl* GetBakedMaterial() const;

    /// \brief Bind a shell bundle to the appropriate materials with purposes
    /// \param shellBundle The shell bundle to bind
    /// \param originalMaterial The original material
    /// \param bakedMaterial The baked material
    /// \param originalMatIdSet Material IDs from original material (if Multi/Sub-Object)
    /// \param bakedMatIdSet Material IDs from baked material (if Multi/Sub-Object)
    void BindShellBundleToMaterials(
        const ShellBundle& shellBundle,
        Mtl*               originalMaterial,
        Mtl*               bakedMaterial,
        std::set<int>&     originalMatIdSet,
        std::set<int>&     bakedMatIdSet);

    MaxUsd::USDSceneBuilderOptions::ShellMtlExportStyle exportStyle;
    bool                                                hasMultiSubDependency = false;
    std::vector<ShellBundle>                            shellBundles;
};

PXR_NAMESPACE_CLOSE_SCOPE
