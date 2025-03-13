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
#include "HdMaxPrimRenderData.h"
#include "MaxRenderGeometryFacade.h"

struct HdMaxPrimRenderData;

/**
 * \brief Nitrous data used to render a USD BasisCurves Prim in the viewport.
 */
struct RenderDelegateAPI HdMaxBasisCurvesRenderData : HdMaxPrimRenderData
{
    /**
     * \brief Constructor.
     * \param rPrimId The id of the USD BasisCurves render Prim tied to this render data.
     */
    explicit HdMaxBasisCurvesRenderData(const pxr::SdfPath& rPrimPath)
        : HdMaxPrimRenderData(rPrimPath)
    {
    }

    enum VertexBuffers
    {
        PointsBuffer = 0,
        SelectionBuffer = 1
    };

    HdMaxBasisCurvesRenderData(const HdMaxBasisCurvesRenderData& data) = default;
    HdMaxBasisCurvesRenderData(HdMaxBasisCurvesRenderData&& data) noexcept = default;
    HdMaxBasisCurvesRenderData& operator=(const HdMaxBasisCurvesRenderData& data) = default;
    HdMaxBasisCurvesRenderData& operator=(HdMaxBasisCurvesRenderData&& data) noexcept = default;
    ~HdMaxBasisCurvesRenderData() = default;

    // Shaded Curve Render Data
    SubsetRenderData shadedCurve;
    // Wireframe Curve Render Data
    SubsetRenderData wireframeCurve;

    // The source basiscurves's topology.
    pxr::HdBasisCurvesTopology sourceTopology;

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
     * \brief Returns the required streams to render USD BasisCurves content in Nitrous.
     * \return The material required streams.
     */
    static MaxSDK::Graphics::MaterialRequiredStreams GetRequiredStreams();

    /**
     * \brief Returns true if the first shaded subset is instanced - in the case of BasisCurves, there's only
     * one shaded subset anyway.
     * \return True if instanced.
     */
    bool IsInstanced() const override;

    /**
     * \brief Dirty all shaded subsets with the given dirty flag. In the case of BasisCurves, there's only
     * one shaded subset anyway.
     * \param dirtyFlag The dirty bits to set onto all shaded subsets.
     */
    void SetAllSubsetRenderDataDirty(const pxr::HdDirtyBits& dirtyFlag) override;
};

static_assert(
    std::is_move_constructible<HdMaxBasisCurvesRenderData>::value,
    "HdMaxBasisCurvesRenderData should be move constructible.");
static_assert(
    std::is_move_constructible<HdMaxBasisCurvesRenderData::SubsetRenderData>::value,
    "HdMaxBasisCurvesRenderData should be move constructible.");