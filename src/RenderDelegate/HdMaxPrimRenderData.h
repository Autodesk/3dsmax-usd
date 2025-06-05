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
struct RenderDelegateAPI HdMaxPrimRenderData
{
    /**
     * \brief Constructor.
     * \param rPrimId The id of the USD render Prim tied to this render data.
     */
    explicit HdMaxPrimRenderData(const pxr::SdfPath& rPrimPath)
        : rPrimPath(rPrimPath)
    {
    }

    HdMaxPrimRenderData(const HdMaxPrimRenderData& data) = default;
    HdMaxPrimRenderData(HdMaxPrimRenderData&& data) noexcept = default;
    HdMaxPrimRenderData& operator=(const HdMaxPrimRenderData& data) = default;
    HdMaxPrimRenderData& operator=(HdMaxPrimRenderData&& data) noexcept = default;
    virtual ~HdMaxPrimRenderData() = default;

    pxr::SdfPath rPrimPath;
    bool         visible = true;
    bool         renderTagActive = true;
    pxr::TfToken renderTag;

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
        // Edge visibility - generally speaking, source poly edges are visible (1), and edges added
        // from triangulation are invisible (0).
        pxr::VtVec3iArray edgeVis;
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

    // Source data statistics :
    size_t sourceNumPoints = 0;
    // Geometry data used in the viewport, ready to be loaded in nitrous buffers.
    pxr::VtVec3fArray points;

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

    // True if the prim is selected, and should be highlighted in VP.
    bool selected = false;

    // True if the render data is for a gizmo (which would be drawn in the gizmo vis group)
    bool isGizmo = false;

    /**
     * \brief Loads the geometry data in the render item's geometry. Creating or updating the
     * index and vertex buffers as needed (only "dirty" things are loaded).
     * \param force If true, the geometry is loaded regardless of the dirty state.
     */
    virtual void UpdateRenderGeometry(bool fullReload) = 0;

    /**
     * \brief Dirty all shaded subsets with the given dirty flag. Method is for convenience,
     * dirtiness is maintained per-subset, but we often need to set all subsets dirty with
     * the same bits.
     * \param dirtyFlag The dirty bits to set onto all shaded subsets.
     */
    virtual void SetAllSubsetRenderDataDirty(const pxr::HdDirtyBits& dirtyFlag) = 0;

    /**
     * \brief Resolves the final material that should be used in the viewport for this prim's subset.
     * \param renderData The render data to resolve the material for.
     * \param subsetRenderData The specific subsetRenderData of the renderData to resolve the material for.
     * \param displaySettings Display settings to use.
     * \param renderNode The 3dsMax render node. Can carry some material information.
     * \param instanced Is this for instanced geometry (we cant share the same material between instanced and non instanced).
     * \return The resolved material.
     */
    virtual MaxSDK::Graphics::BaseMaterialHandle ResolveViewportMaterial(
        const HdMaxPrimRenderData&                renderData,
        const SubsetRenderData&                   subsetRenderData,
        const HdMaxDisplaySettings&               displaySettings,
        const MaxSDK::Graphics::RenderNodeHandle& renderNode,
        bool                                      instanced) const
        = 0;

    /**
     * \brief Returns true if the first shaded subset is instanced - if so, it is expected that all associated render
     * items will be instanced.
     * \return True if instanced.
     */
    virtual bool IsInstanced() const = 0;

    static void SetVertexBuffer(
        std::shared_ptr<MaxRenderGeometryFacade> geometry,
        int                                      index,
        MaxSDK::Graphics::VertexBufferHandle     newBuffer);
};