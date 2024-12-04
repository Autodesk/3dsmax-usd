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

#include "RenderDelegateAPI.h"

#include <MaxUsd/MeshConversion/PrimvarMappingOptions.h>
#include <MaxUsd/Utilities/MaterialRef.h>
#include <MaxUsd/Utilities/ProgressReporter.h>

#include <pxr/imaging/hd/material.h>

#include <Graphics/BaseMaterialHandle.h>
#include <Graphics/StandardMaterialHandle.h>

#include <MaxUsd.h>

class RenderDelegateAPI HdMaxMaterialCollection
{
public:
    // Bitmap key [file path, channel, is for viewport]
    typedef std::tuple<std::string, int, bool> BitmapKey;
    struct BitmapKeyHash
    {
        size_t operator()(const BitmapKey& key) const
        {
            std::size_t hash = std::hash<std::string> {}(std::get<0>(key));
            boost::hash_combine(hash, std::get<1>(key));
            boost::hash_combine(hash, std::get<2>(key));
            return hash;
        }
    };

    typedef pxr::TfHashMap<BitmapKey, std::shared_ptr<MaxUsd::MaterialRef>, BitmapKeyHash>
        BitmapCache;

    class MaterialData
    {
    public:
        /**
         * \brief Constructor. Used when the material data is first created from a hydra material network.
         * \param materialId The material's path.
         * \param materialNetwork The material network the 3dsMax material will be built from.
         */
        MaterialData(const pxr::SdfPath& materialId, const pxr::HdMaterialNetwork& materialNetwork);

        /**
         * \brief Constructor. Used when the material data is first created from a pre-existing 3dsMax UsdPreviewSurface
         * material.
         * \param materialId The material's path.
         * \param maxMaterial The 3dsMax UsdPreviewSurface material.
         */
        MaterialData(const pxr::SdfPath& materialId, Mtl* maxMaterial);

        /**
         * \brief Updates the material's source network. After calling this, the material will need to be rebuilt.
         * \param network The new material network.
         */
        void UpdateSource(const pxr::HdMaterialNetwork& network);

        /**
         * \brief Builds the Max material from the HdMaterialNetwork. Note that this does not generate the nitrous
         * representations. These are created lazily when requested. Building materials should only
         * be done from the main thread of 3dsMax.
         * \param bitmapCache A bitmap cache to be used when building the material. Bitmaps built from USD texture
         * nodes with no transform applied are added to this cache for reuse.
         * \param primvarMappingOptions Primvar to channel mapping options. Required to correctly set the input channel
         * on the created 3dsMax bitmaps.
         * \param stdVpMaterial Whether or not to build the standard viewport representation of the material.
         * \param maxMaterial Whether or not to build the full MaxUsdPreviewSurface corresponding material.
         */
        void Build(
            BitmapCache&                         bitmapCache,
            const MaxUsd::PrimvarMappingOptions& primvarMappingOptions,
            bool                                 stdVpMaterial,
            bool                                 maxMaterial);

        /**
         * \brief Returns the source hydra material network.
         * \return The network.
         */
        const pxr::HdMaterialNetwork& GetSourceMaterialNetwork() const;

        /**
         * \brief Returns the built 3dsMax material, if not build, this returns null.
         * \return The 3dsMax material.
         */
        std::shared_ptr<MaxUsd::MaterialRef> GetMaxMaterial() const;

        /**
         * \brief Gets the nitrous representation of the material.
         * \param forInstances We need to keep different versions of viewport materials whether they
         * are used for simple geometry or instances (sharing may break the display). If True,
         * the returned material is the one meant for instances.
         * \return The nitrous material. Should only be called from the 3dsMax main thread.
         */
        MaxSDK::Graphics::BaseMaterialHandle& GetNitrousMaterial(bool forInstances);

        /**
         * \brief Returns true if the standard viewport material representation is built.
         * \return True if the viewport material is built, false otherwise.
         */
        bool IsVpMaterialBuilt() const;

        /**
         * \brief Returns the material Id (the USD prim path).
         * \return The id.
         */
        pxr::SdfPath GetId() { return id; };

    private:
        /**
         * \brief Converts a Mtl* pointer to a Nitrous handle. The passed material is first copied.
         * Materials converted to nitrous can react poorly in the material editor. This is a
         * workaround...
         * \param material The material to convert. Should only be called from the 3dsMax main thread.
         * \return The nitrous handle for the given material.
         */
        MaxSDK::Graphics::BaseMaterialHandle ConvertToNitrous(Mtl* material);

        /// The material's unique identifier (its path).
        pxr::SdfPath id;
        /// The source material network this material will be built from.
        pxr::HdMaterialNetwork sourceNetwork;
        /// A token representing the network, for fast comparison.
        pxr::TfToken sourceNetworkToken;
        /// The converted max Material (wrapped in a RefMaker to protect it from deletion).
        std::shared_ptr<MaxUsd::MaterialRef> materialRef = nullptr;

        /// Nitrous representations. We potentially need two versions, one for instanced geometry
        /// and one for regular geometry. This is workaround for an issue where using the material
        /// for instanced geometry breaks it for regular geometry.
        std::shared_ptr<MaxUsd::MaterialRef> stdVpMaterialRef = nullptr;
        MaxSDK::Graphics::BaseMaterialHandle stdVpMaterial;
        MaxSDK::Graphics::BaseMaterialHandle stdVpInstancesMaterial;
        Color                                color; // For "color only" materials.

        /// Flags telling us whether the 3dsmax materials are up to date with the source network.
        /// Standard viewport material (color / basic texture).
        bool stdVpMaterialBuilt = false;
        /// 3dsMax material instance. For fallback rendering, and eventually high quality viewport.
        bool maxUsdPreviewSurfaceMaterialBuilt = false;
    };

    typedef std::shared_ptr<MaterialData> MaterialDataPtr;

    /**
     * \brief Adds a new material to the collection, if not already existing.
     * \param delegate The scene delegate.
     * \param materialId The id of the material we are adding.
     * \return The material data that was added to the collection.
     */
    MaterialDataPtr AddMaterial(pxr::HdSceneDelegate* delegate, const pxr::SdfPath& materialId);

    /**
     * \brief Registers an existing UsdPreviewSurface 3dsMax material in the collection. Useful when
     * loading from a .max file, to "reconnect" the 3dsMax material to the source material at
     * "materialId", if the material ever changes on the USD side, maxMaterial will be updated
     * accordingly. The function assumes that maxMaterial is a UsdPreviewSurface material.
     * \param materialId The material's id/path.
     * \param maxMaterial The 3dsMax material to register for this id.
     * \return The material data pointer for this collection entry.
     */
    MaterialDataPtr RegisterMaxMaterial(const pxr::SdfPath& materialId, Mtl* maxMaterial);

    /**
     * \brief Register's a bitmap texture to the material collection, this simply adds the bitmap to
     * the bitmapCache.
     * \param texture The bitmap to register.
     */
    void RegisterMaxBitmap(BitmapTex* texture);

    /**
     * \brief Update's the source hydra material network of the material. The 3dsMax material will
     * need to be rebuilt after calling this function.
     * \param delegate The scene delegate.
     * \param materialId The material's path.
     * \return The updated material data.
     */
    MaterialDataPtr UpdateMaterial(pxr::HdSceneDelegate* delegate, const pxr::SdfPath& materialId);

    /**
     * \brief Builds all materials present in the collection, i.e. generating the 3dsMax material from
     * the source hydra material networks.
     * \param progressReporter If new materials need to be built from scratch, which can be a lengthy
     * operation, the progress is reported via this progressReporter instance (Typically this would
     * be hooked up to some UI).
     * \param standardViewport True if we want to build the standard viewport representation of the materials.
     * \param maxMaterial True if we want to built the full MaxUsdPreviewSurface representation of the materials.
     */
    void BuildMaterials(
        const MaxUsd::ProgressReporter&      progressReporter,
        const MaxUsd::PrimvarMappingOptions& primvarMappingOptions,
        bool                                 standardViewport,
        bool                                 maxMaterial);

    /**
     * \brief Returns the display color material, a simple 3dsMax material using the vertex color as
     * diffuseColor. Used to properly render the displayColor primvar.
     * \return The display color material.
     */
    std::shared_ptr<MaxUsd::MaterialRef> GetDisplayColorMaterial();

    /**
     * \brief Completely clear the material collection, including any cached bitmaps.
     */
    void Clear();

    /**
     * Removes a material from the collection.
     * IMPORTANT : Removing materials is not thread safe - it should only be called
     * when there is no possibility of concurrent access to the collection.
     * @param path The path of the material to be removed.
     */
    void RemoveMaterial(pxr::SdfPath const& path);

    /**
     * \brief The class ID of the 3dsMax UsdPreviewSurface material. This material class is exposed to users
     * and is essentially a scripted material using a PhysicalMaterial delegate.
     */
    static const Class_ID MaxUsdPreviewSurfaceMaterialClassID;

private:
    /// Material cache.
    tbb::concurrent_unordered_map<pxr::SdfPath, MaterialDataPtr, pxr::SdfPath::Hash> materials;
    /// Display color material.
    std::shared_ptr<MaxUsd::MaterialRef> displayColorMaterial = nullptr;
    /// A cache of bitmap, corresponding to UsdUVTexture prims. We only cache bitmaps that do not
    /// have transforms (UsdTransform2d) applied. Bitmaps meant for the viewport, and for the full
    /// 3dsMax materials are kept seperate.
    BitmapCache bitmapCache;
};
