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
#include "HdMaxChangeTracker.h"
#include "HdMaxInstanceGen.h"
#include "HdMaxMaterialCollection.h"
#include "MaxRenderGeometryFacade.h"

#include <maxUsd/Utilities/MeshUtils.h>

#include <Graphics/RenderItemHandleDecorator.h>
#include <Graphics/StandardMaterialHandle.h>

/**
 * \brief Nitrous data used to render a USD Prim in the viewport.
 */
struct RenderDelegateAPI HdMaxRenderData
{
    /**
     * \brief Constructor.
     * \param rPrimId The id of the USD render Prim tied to this render data.
     */
    explicit HdMaxRenderData(const pxr::SdfPath& rPrimPath)
        : rPrimPath(rPrimPath)
    {
    }

    enum VertexBuffers
    {
        PointsBuffer = 0,
        NormalsBuffer = 1,
        SelectionBuffer = 2,
        UvsBuffer = 3
    };

    HdMaxRenderData(const HdMaxRenderData& data) = default;
    HdMaxRenderData(HdMaxRenderData&& data) noexcept = default;
    HdMaxRenderData& operator=(const HdMaxRenderData& data) = default;
    HdMaxRenderData& operator=(HdMaxRenderData&& data) noexcept = default;
    ~HdMaxRenderData() = default;

    pxr::SdfPath rPrimPath;
    bool         visible = true;
    bool         renderTagActive = true;

    // Considering UsdGeomSubsets, a prim may have multiple different materials bound
    // to parts of the mesh. Nitrous allows a single material per mesh. Therefor,
    // in those situations we split the mesh. We end up with multiple render items, one for each
    // material bound (could be less than the number of UsdGeomSubsets). The vertex buffers are
    // shared across the different render items).
    struct SubsetRenderData
    {
        /// The material id, used as identifier for the subset.
        pxr::SdfPath materiaId;
        /// The nitrous render item for the subset. In the case of instanced geometry, this remains
        /// null as the render item is generated later, from USDStageObject::UpdatePerNodeItems().
        MaxSDK::Graphics::RenderItemHandleDecorator renderItem;
        /// Render item when selection highlighting. Important : both render items are not meant to
        /// be used at the same time. The selectionRenderItem is able to render everything : the
        /// geometry AND the highlighting. We use a custom render item, and there is some
        /// performance overhead vs GeometryRenderItemHandle so we only use it when needed.
        MaxSDK::Graphics::RenderItemHandleDecorator selectionRenderItem;
        /// The render geometry. MaxRenderGeometryFacade wraps either a SimpleRenderGeometry or
        /// InstanceRenderGeometry.
        std::shared_ptr<MaxRenderGeometryFacade> geometry;
        /// UsdPreviewSurface material data. Null if no material is bound.
        HdMaxMaterialCollection::MaterialDataPtr materialData;
        // Indices belonging to this subset.
        pxr::VtVec3iArray indices;
        // Wireframe indices belonging to this subset.
        pxr::VtIntArray wireIndices;
        // Dirty state. Allows us to know what needs to be update in the nitrous representation of
        // the prim (either in it's own render item, or within a consolidated mesh)
        pxr::HdDirtyBits dirtyBits = HdMaxChangeTracker::Clean;
        /// Flag to keep track of whether the render data is currently part of a consolidated mesh.
        /// Kept here for performance reasons, it allows us to know faster, without map lookups.
        bool inConsolidation = false;

        /**
         * \brief Helper to retrieve the right render item, depending on if we need to show selection or not.
         * Either returning a geometry render item, or a custom render item for selection.
         * \param selected True if we need to show selection.
         * \return The render item.
         */
        MaxSDK::Graphics::RenderItemHandleDecorator& GetRenderItemDecorator(bool selected);

        /**
         * \brief Returns true if the render subset is instanced.
         * \return True if instanced.
         */
        bool IsInstanced() const;
    };

    // Note : If no UsdGeomSubsets are defined, this vector will contain a single "default" subset,
    // containing the entirety of the mesh.
    std::vector<SubsetRenderData> shadedSubsets;
    // Render data for the wireframe render item. Treat the whole mesh as a single subset containing
    // everything.
    SubsetRenderData wireframe;

    // Subset render data that is no longer in use and should be deleted at the next opportunity on
    // the main thread. Indeed, it is not safe to destroy render items outside of the main thread.
    std::vector<SubsetRenderData> toDelete;
    // Nitrous material for the display color. Keep one for regular meshes and a separate one for
    // instanced meshes, this is a workaround for an issue with the instancing API which can break
    // the material if shared with non-instanced meshes.
    MaxSDK::Graphics::StandardMaterialHandle displayColorNitrousHandle;
    MaxSDK::Graphics::StandardMaterialHandle instanceDisplayColorNitrousHandle;

    // The offset transform for the render items (world space). Not used if instanced, see
    // instanceTransforms.
    pxr::GfMatrix4d transform;
    // Extent of the prim.
    pxr::GfRange3d extent;
    // The total bounding box of the render data. Can be used for culling (transformed and all
    // instances)
    pxr::GfBBox3d boundingBox;

    // Handles instancing data if the prim is instanced.
    // In Max 2023, instancing related objects are all move and copy constructible, but not in 2022.
    // Use pointer to keep the class move-constructible.
    std::shared_ptr<HdMaxInstanceGen> instancer = std::make_shared<HdMaxInstanceGen>();

    // Geometry data used in the viewport, ready to be loaded in nitrous buffers.
    pxr::VtVec3fArray points;
    pxr::VtVec3fArray normals;
    pxr::VtVec3fArray colors;

    std::vector<MaxUsd::MeshUtils::UvChannel> uvs;
    // Map material to associated diffuseColor uv primvar
    pxr::TfHashMap<pxr::SdfPath, std::string, pxr::SdfPath::Hash> materialDiffuseColorUvPrimvars;

    // True if the prim is selected, and should be highlighted in VP.
    bool selected = false;

    // The source mesh's topology.
    pxr::HdMeshTopology sourceTopology;

    // Source data statistics :
    size_t sourceNumPoints = 0;
    size_t sourceNumFaces = 0;

    /**
     * \brief Loads the geometry data in the render item's geometry. Creating or updating the
     * index and vertex buffers as needed (only "dirty" things are loaded).
     * \param force If true, the geometry is loaded regardless of the dirty state.
     */
    void UpdateRenderGeometry(bool fullReload);

    /**
     * \brief Dirty all shaded subsets with the given dirty flag. Method is for convenience,
     * dirtiness is maintained per-subset, but we often need to set all subsets dirty with
     * the same bits.
     * \param dirtyFlag The dirty bits to set onto all shaded subsets.
     */
    void SetAllSubsetRenderDataDirty(const pxr::HdDirtyBits& dirtyFlag);

    /**
     * \brief Resolves the final material that should be used in the viewport for this prim's subset.
     * \param subsetGeometry The subset to resolve the material for.
     * \param displaySettings Display settings to use.
     * \param instanced Is this for instanced geometry (we cant share the same material between instanced and non instanced).
     * \return The resolved material.
     */
    MaxSDK::Graphics::BaseMaterialHandle ResolveViewportMaterial(
        const SubsetRenderData&     subsetGeometry,
        const HdMaxDisplaySettings& displaySettings,
        bool                        instanced) const;

    /**
     * \brief Returns the USD display color material handle for this prim render data.
     * \param instanced Is this for instanced geometry (we cant share the same material between instanced and non instanced).
     * \return The display color material handle.
     */
    MaxSDK::Graphics::BaseMaterialHandle GetDisplayColorNitrousHandle(bool instanced) const;

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
    bool IsInstanced() const;
};

static_assert(
    std::is_move_constructible<HdMaxRenderData>::value,
    "HdMaxRenderData should be move constructible.");
static_assert(
    std::is_move_constructible<HdMaxRenderData::SubsetRenderData>::value,
    "HdMaxRenderData should be move constructible.");