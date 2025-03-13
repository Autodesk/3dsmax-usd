//
// Copyright 2024 Autodesk
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

#include "HdMaxInstanceGen.h"
#include "HdMaxMaterialCollection.h"
#include "HdMaxPrimRenderData.h"
#include "MaxRenderGeometryFacade.h"

/**
 * \brief Nitrous data used to render a USD Prim in the viewport.
 */
struct RenderDelegateAPI HdMaxMeshRenderData : HdMaxPrimRenderData
{
    /**
     * \brief Constructor.
     * \param rPrimId The id of the USD render Prim tied to this render data.
     */
    explicit HdMaxMeshRenderData(const pxr::SdfPath& rPrimPath)
        : HdMaxPrimRenderData(rPrimPath)
    {
    }

    enum VertexBuffers
    {
        PointsBuffer = 0,
        NormalsBuffer = 1,
        SelectionBuffer = 2,
        UvsBuffer = 3
    };

    HdMaxMeshRenderData(const HdMaxMeshRenderData& data) = default;
    HdMaxMeshRenderData(HdMaxMeshRenderData&& data) noexcept = default;
    HdMaxMeshRenderData& operator=(const HdMaxMeshRenderData& data) = default;
    HdMaxMeshRenderData& operator=(HdMaxMeshRenderData&& data) noexcept = default;
    ~HdMaxMeshRenderData() = default;

    // Note : If no UsdGeomSubsets are defined, this vector will contain a single "default" subset,
    // containing the entirety of the mesh.
    std::vector<SubsetRenderData> shadedSubsets;
    // Render data for the wireframe render item. Treat the whole mesh as a single subset containing
    // everything.
    SubsetRenderData wireframe;

    // The source mesh's topology.
    pxr::HdMeshTopology sourceTopology;
    size_t              sourceNumFaces = 0;

    // Geometry data used in the viewport, ready to be loaded in nitrous buffers.
    pxr::VtVec3fArray normals;
    pxr::VtVec3fArray colors;

    std::vector<MaxUsd::MeshUtils::UvChannel> uvs;
    // Map material to associated diffuseColor uv primvar
    pxr::TfHashMap<pxr::SdfPath, std::string, pxr::SdfPath::Hash> materialDiffuseColorUvPrimvars;

    // Subset render data that is no longer in use and should be deleted at the next opportunity on
    // the main thread. Indeed, it is not safe to destroy render items outside of the main thread.
    std::vector<SubsetRenderData> toDelete;
    // Nitrous material for the display color. Keep one for regular meshes and a separate one for
    // instanced meshes, this is a workaround for an issue with the instancing API which can break
    // the material if shared with non-instanced meshes.
    MaxSDK::Graphics::StandardMaterialHandle displayColorNitrousHandle;
    MaxSDK::Graphics::StandardMaterialHandle instanceDisplayColorNitrousHandle;

    /**
     * \brief Loads the geometry data in the render item's geometry. Creating or updating the
     * index and vertex buffers as needed (only "dirty" things are loaded).
     * \param force If true, the geometry is loaded regardless of the dirty state.
     */
    void UpdateRenderGeometry(bool fullReload) override;

    /**
     * \brief Resolves the final material that should be used in the viewport for this prim's subset.
     * \param renderData The render data to resolve the material for.
     * \param subsetRenderData The specific subsetRenderData of the renderData to resolve the material for.
     * \param displaySettings Display settings to use.
     * \param renderNode The 3dsMax render node. Can carry some material information.
     * \param instanced Is this for instanced geometry (we cant share the same material between instanced and non instanced).
     * \return The resolved material.
     */
    MaxSDK::Graphics::BaseMaterialHandle ResolveViewportMaterial(
        const HdMaxPrimRenderData&                renderData,
        const SubsetRenderData&                   subsetRenderData,
        const HdMaxDisplaySettings&               displaySettings,
        const MaxSDK::Graphics::RenderNodeHandle& renderNode,
        bool                                      instanced) const override;

    /**
     * \brief Returns the required streams to render USD content in Nitrous.
     * \param wire If true, return the streams required for the wireframe view, otherwise, the streams for the shaded
     * view are returned
     * \return The material required streams.
     */
    static MaxSDK::Graphics::MaterialRequiredStreams GetRequiredStreams(bool wire);

    /**
     * \brief Returns true if the first shaded subset is instanced - if so, it is expected that all associated render
     * items will be instanced.
     * \return True if instanced.
     */
    bool IsInstanced() const override;

    /**
     * \brief Dirty all shaded subsets with the given dirty flag. Method is for convenience,
     * dirtiness is maintained per-subset, but we often need to set all subsets dirty with
     * the same bits.
     * \param dirtyFlag The dirty bits to set onto all shaded subsets.
     */
    void SetAllSubsetRenderDataDirty(const pxr::HdDirtyBits& dirtyFlag) override;

    /**
     * \brief Returns the USD display color material handle for this prim render data.
     * \param instanced Is this for instanced geometry (we cant share the same material between instanced and non instanced).
     * \return The display color material handle.
     */
    MaxSDK::Graphics::BaseMaterialHandle GetDisplayColorNitrousHandle(bool instanced) const;
};

static_assert(
    std::is_move_constructible<HdMaxMeshRenderData>::value,
    "HdMaxMeshRenderData should be move constructible.");
static_assert(
    std::is_move_constructible<HdMaxMeshRenderData::SubsetRenderData>::value,
    "HdMaxMeshRenderData should be move constructible.");