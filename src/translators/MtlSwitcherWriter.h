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
#pragma once

#include <MaxUsd/Utilities/MaxSupportUtils.h>
#ifdef IS_MAX2024_OR_GREATER
#include <MaxUsd/Translators/ShaderWriter.h>
#include <MaxUsd/Translators/WriteJobContext.h>

PXR_NAMESPACE_OPEN_SCOPE

class UsdShadeMaterial;

class MtlSwitcherWriter : public MaxUsdShaderWriter
{
public:
    /// A bundle contains the MatID set that it represents, the Bindings that it needs to connect
    /// and the (placeholder) Material Prims that are used to represent it in the stage.
    struct VariantBundle
    {
        std::set<int>                 matSetIdx;
        std::list<SdfPath>            geomBindPaths;
        std::vector<UsdShadeMaterial> subObjsMatPrims;
    };

    MtlSwitcherWriter(Mtl* material, const SdfPath& usdPath, MaxUsdWriteJobContext& jobCtx);

    /// A static function declaring how well this class can support the current context.
    static ContextSupport CanExport(const MaxUsd::USDSceneBuilderOptions& exportArgs);

    /// The switcher is material target agnostic, it does not define any shader of its own.
    static bool IsMaterialTargetAgnostic();

    /// \brief Main export function that runs when the applicable material gets hit.
    void Write() override;

    /// \brief Reports whether the ShaderWriter needs all those dependent materials to
    /// be also exported.
    bool HasMaterialDependencies() const override { return true; };

    /// \brief Retrieve the dependent materials
    /// \param subMtl The array to report the list of dependent materials
    void GetSubMtlDependencies(std::vector<Mtl*>& subMtl) const override;

    /// \brief Method called after all materials are exported
    void PostWrite() override;

private:
    /// \brief Retrieve the materials attached to this switcher
    /// \param subMtl The array to report the list of dependent materials
    void GetTopLevelMtlDependencies(std::vector<Mtl*>& subMtl) const;

    /// \brief Bind the variant bundles placeholder materials to the geometry
    void BindPlaceholderMatsToGeom();

    /// \brief Get the material IDs from a Multi/Sub-Object material
    /// \param mat The Multi/Sub-Object material
    /// \param matIdSet The set to report the material IDs
    void GetMatIDsFromMultiMat(Mtl* mat, std::set<int>& matIdSet);

    /// \brief Adds the references between the placeholder materials and the actual materials
    /// \param variantBundle The variant bundle to connect
    /// \param variant The variant material
    /// \param matIdSet The material IDs of the Multi/Sub-Object material
    /// \param variantSet The variant set being editing
    void BindVariantBundleToMat(
        const VariantBundle& variantBundle,
        Mtl*                 variant,
        std::set<int>&       matIdSet,
        const UsdVariantSet* variantSet = nullptr);

    /// \brief Create a new variant bundle and adds it to the member vector, if a bundle with the
    /// same material IDs set was already created,
    ///  just adds the geomBindPath to the bundle
    /// \param geomBindPath The path to the prim this bundle will be bound to
    /// \param materialIdsSet The set containing the material IDs of the geometry prim
    /// \param matIDsSets The vector of material IDs sets used to keep track of what's already been
    /// found.
    void CreateVariantBundle(
        const SdfPath&              geomBindPath,
        const std::set<int>&        materialIdsSet,
        std::vector<std::set<int>>& matIDsSets);

    // cached list of material variants (sub mtl of the Material Switcher)
    std::vector<Mtl*>                                      variantMaterials;
    MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle exportStyle;
    bool                                                   hasMultiSubDependency = false;
    std::vector<VariantBundle>                             variantBundles;
};

PXR_NAMESPACE_CLOSE_SCOPE
#endif