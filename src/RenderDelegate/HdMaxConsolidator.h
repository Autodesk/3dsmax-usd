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

#include "HdMaxDisplaySettings.h"
#include "HdMaxRenderData.h"
#include "MaxRenderGeometryFacade.h"
#include "RenderDelegateAPI.h"

#include <MaxUsd/Utilities/TypeUtils.h>

#include <Graphics/CustomRenderItemHandle.h>
#include <Graphics/GeometryRenderItemHandle.h>
#include <Graphics/IndexBufferHandle.h>

#include <max.h>

PXR_NAMESPACE_OPEN_SCOPE
class HdMaxRenderDelegate;
PXR_NAMESPACE_CLOSE_SCOPE

/**
 * \brief Helper class to consolidate nitrous render data.
 */
class RenderDelegateAPI HdMaxConsolidator
{
public:
    typedef std::pair<pxr::SdfPath, int> PrimSubsetKey;

    struct PrimSubsetHash
    {
        size_t operator()(PrimSubsetKey key) const
        {
            std::size_t hash = pxr::SdfPath::Hash {}(key.first);
            boost::hash_combine(hash, key.second);
            return hash;
        }
    };
    typedef pxr::TfHashSet<PrimSubsetKey, PrimSubsetHash> PrimSubsetSet;

    enum class Strategy
    {
        Static,  // Only consolidate when not animating (currentTime == previousTime).
        Dynamic, // Try to update the consolidation dynamically if possible (vertex animation).
        Off      // Do not consolidate.
    };

    // Consolidation configuration...
    struct Config
    {

        Strategy             strategy = Strategy::Off;
        bool                 visualize = false; // If true, consolidation cells are showed in color.
        size_t               maxTriangles = 0;
        size_t               maxInstanceCount = 0;
        size_t               maxCellSize = 0;
        long long            staticDelay = 500;
        HdMaxDisplaySettings displaySettings;
        bool                 operator==(const Config& config) const;
    };

    // An input Prim geometry (possibly instanced), candidate for consolidation.
    struct Input
    {
        static const int InvalidMultipartIndex = -1;

        pxr::SdfPath      primPath;
        int               subsetIndex = 0;
        pxr::VtVec3iArray indices;
        pxr::VtIntArray   wireIndices;
        pxr::VtVec3fArray points;
        pxr::VtVec3fArray normals;
        pxr::VtVec3fArray uvs;

        std::vector<Matrix3> transforms;
        // Which transforms (instances) are selected. Size will always match the transforms array.
        std::vector<bool> selection;

        int              multipartIndex = InvalidMultipartIndex;
        pxr::HdDirtyBits dirtyBits = HdMaxChangeTracker::Clean;
    };

    // A mapping of a given USD prim in a consolidated mesh.
    struct Mapping
    {
        pxr::SdfPath primPath;
        // Where this prim in in the consolidated buffer. It can be there multiple
        // times if the mesh is instanced.
        pxr::VtIntArray offsets;
        // Size of the mesh associated with the prim in the consolidated buffer.
        size_t vertexCount { 0 };
        int    multipartIndex = Input::InvalidMultipartIndex;
    };

    // A Consolidation Cell is a collection of collection inputs, which might get consolidated
    // into a single mesh. Depending on configuration, cells can be limited in size.
    // For example, if we have 10 prims of 1000 vertices, but a maximum cell size of 5000 vertices,
    // then we would end up with two consolidated meshes (each composed of 5 prims).
    // 10 * 1000 = 10000 = 5000 * 2
    struct Cell
    {
        size_t             numTris = 0;
        std::vector<Input> inputs;
    };

    // Consolidated geometry - built from a ConsolidationCell.
    struct ConsolidatedGeom
    {
        MaxSDK::Graphics::GeometryRenderItemHandle renderItem;
        MaxSDK::Graphics::GeometryRenderItemHandle wireframeRenderItem;
        // When we need to display selection, use a custom render items that can perform
        // an additional pass to display highlighting. We only use it when needed as there is
        // a performance cost to custom render items (even without the additional render pass).
        MaxSDK::Graphics::CustomRenderItemHandle renderItemSelection;
        MaxSDK::Graphics::CustomRenderItemHandle wireframeRenderItemSelection;

        MaxSDK::Graphics::BaseMaterialHandle                      material;
        pxr::TfHashMap<pxr::SdfPath, Mapping, pxr::SdfPath::Hash> dataMapping;
        bool                                                      hasActiveSelection { false };

        MaxSDK::Graphics::RenderItemHandle& GetRenderItem(bool wireframe);
    };
    typedef std::shared_ptr<ConsolidatedGeom>       ConsolidatedGeomPtr;
    typedef std::vector<ConsolidatedGeomPtr>        ConsolidatedGeomVector;
    typedef std::shared_ptr<ConsolidatedGeomVector> ConsolidatedGeomVectorPtr;

    struct RenderDataInfo
    {
        // Last known index of the render data in the render delegate.
        // using this to retrieve the render data is faster then from the path,
        // we save a map lookup. Always use renderDelegate->SafeGetRenderData()
        // when using this index to fetch render data.
        size_t index;
        // The path of the prim this render data is for.
        pxr::SdfPath primPath;
        // Material subset index.
        int subsetIdx;
    };

    // Consolidation output, a bunch of consolidated mesh and information about what prim
    // went into what consolidated mesh.
    struct Output
    {
        // All resulting consolidated meshes.
        ConsolidatedGeomVectorPtr geoms = std::make_shared<ConsolidatedGeomVector>();
        // Mapping from a prim path and material index to geometry.
        pxr::TfHashMap<PrimSubsetKey, ConsolidatedGeomVector, PrimSubsetHash> primToGeom;
        // Description of the source render data used.
        std::vector<RenderDataInfo> sourceRenderData;
        // Description of the render data that was actually consolidated.
        std::vector<RenderDataInfo> consolidatedRenderData;
    };
    typedef std::shared_ptr<Output> OutputPtr;

    /**
     * \brief Constructor
     * \param renderDelegate Pointer to the render delegate from which the render data we are consolidating
     * originates.
     */
    HdMaxConsolidator(const std::shared_ptr<pxr::HdMaxRenderDelegate>& renderDelegate);

    /**
     * \brief Resets the consolidator, clearing any cached consolidation output.
     */
    void Reset();

    /**
     * \brief Returns the consolidation output for the given time if it exists in the cache.
     * \param time The time to get the consolidation for.
     * \return The consolidation output.
     */
    OutputPtr GetConsolidation(const pxr::UsdTimeCode& time) const;

    /**
     * \brief Returns the current configuration for the consolidator.
     * \return The config.
     */
    const Config& GetConfig() const;

    /**
     * \brief Sets the consolidator config.
     * \param config The new config.
     */
    void SetConfig(const Config& config);

    /**
     * \brief Gets the Prim subsets that are part of the consolidation at the given time.
     * \param time The time at which to get the consolidated Prim subsets.
     * \param consolidatedPrimSubsets Output, the prim subsets that are consolidated at the given time.
     */
    void GetConsolidatedPrimSubsets(
        const pxr::UsdTimeCode& time,
        PrimSubsetSet&          consolidatedPrimSubsets) const;

    /**
     * \brief Builds the consolidation for the given render data, appending to any pre-existing and valid consolidation at that time.
     * \param renderData The USD prim render data to consider for consolidation.
     * \param renderNode The 3dsMax render node being consolidated. Can carry some material information.
     * \param time The time code at which the consolidation takes place.
     * \return The consolidation output result.
     */
    OutputPtr BuildConsolidation(
        const std::vector<HdMaxRenderData*>&      renderData,
        const MaxSDK::Graphics::RenderNodeHandle& renderNode,
        const pxr::UsdTimeCode&                   time);

    /**
     * \brief Attempts to update the consolidation at a given time, given the current consolidation config.
     * There are three possible results to calling this function :
     * 1) The consolidation is still valid, and remains unchanged.
     * 2) The consolidation must be broken as it cannot be updated (because of configuration, or
     * data changes). 3) The consolidation is updated - for example : vertex position animation.
     * \param renderData The render data that we should source the update from.
     * \param renderNode The 3dsMax render node being consolidated. Can carry some material information.
     * \param previousTime The timeCode of the consolidation we want to update.
     * \param newTime The timeCode associated with the new consolidation.
     */
    void UpdateConsolidation(
        const std::vector<HdMaxRenderData*>&      renderData,
        const MaxSDK::Graphics::RenderNodeHandle& renderNode,
        const pxr::UsdTimeCode&                   previousTime,
        const pxr::UsdTimeCode&                   newTime);

private:
    /**
     * \brief Generates consolidation inputs from a USD prim's render data. We may need multiple consolidation
     * inputs for a single USD prim if it is instanced (it may then need to be split across multiple
     * consolidation cells).
     * \param primRenderData The USD prim render data.
     * \param subsetIndex The material subset we are creating the input(s) for.
     * \param numTriWithSameMaterial The total number of triangles in the render data that share the same material as this subset.
     * \param inputs Filled by the function, the generated consolidation inputs.
     */
    void GenerateInputs(
        const HdMaxRenderData* primRenderData,
        int                    subsetIndex,
        size_t                 numTriWithSameMaterial,
        std::vector<Input>&    inputs) const;

    /**
     * \brief Builds the consolidation cells from the USD prims' render data. A consolidation cell
     * essentially holds a bunch of geometry that will be consolidated (merged) together. We can
     * only merge geometry that shares the same material. For any given material, you will therefor
     * end up with one or more cells, indeed, cells are capped in size, so we may not merge all of
     * the geometry sharing the same material into a single mesh. Any existing / valid consolidation
     * is considered, i.e. the cells returned are only created from USD Prims that are not already
     * part of a valid consolidation.
     * \param renderData The candidate render data.
     * \param renderNode The 3dsMax render node being consolidated. Can carry some material information.
     * \param cells Output, the built cells, organized by material.
     * \param time The timeCode, used to consider any cached consolidation.
     */
    void BuildConsolidationCells(
        const std::vector<HdMaxRenderData*>&                               renderData,
        const MaxSDK::Graphics::RenderNodeHandle&                          renderNode,
        std::map<MaxSDK::Graphics::BaseMaterialHandle, std::vector<Cell>>& cells,
        const pxr::UsdTimeCode&                                            time);

    /**
     * \brief Builds a consolidated Nitrous index buffer for the given cell.
     * \param cell The cell being consolidated.
     * \param output The created nitrous index buffer.
     * \param wireframe True if we are consolidating the wireframe indices, false otherwise.
     */
    void BuildIndexBuffer(
        const Cell&                          cell,
        MaxSDK::Graphics::IndexBufferHandle& output,
        bool                                 wireframe) const;

    /**
     * \brief Builds the consolidated Nitrous vertex buffers for the given cell.
     * \param cell The cell being consolidated.
     * \param outputs The built vertex buffers.
     * \param result The consolidated geometry object - this is where we keep track of where each prim is in the
     * consolidation, so it can be updated later.
     */
    void BuildVertexBuffers(
        const Cell&                                cell,
        MaxSDK::Graphics::VertexBufferHandleArray& outputs,
        ConsolidatedGeom&                          result);

    /**
     * \brief Update's a consolidated geometry vertex buffers, with new vertex data from updated USD
     * prim render data.
     * \param toUpdate Vertex buffers to be updated. Expect up to 4 buffers :
     *      0 -> *Points 1 -> Normals 2 -> Vertex Color (selection) 3 -> UVs
     * \param inputs Consolidation inputs, holding the updated data - the inputs passed may not all
     * be mapped into the buffers.
     * \param mappings Consolidation mappings, informs about where in the consolidation each prim's data is.
     * \param fullUpdate If true, the update is done regardless of the "dirty" status of the prim
     * render data. A prim that was previously unconsolidated may not be dirty, as it is properly
     * loaded in its own render item, but when it does get consolidated, we still want it loaded in
     * the consolidation.
     * \param hasSelectionHighlight Output argument, will be set to true if at
     * least one of the consolidated inputs has an active selection. I.e. the selection buffer has
     * some selection loaded. The passed value is unaffected if selection is not dirty, and a full
     * update is not forced.
     */
    void UpdateVertexBuffers(
        MaxSDK::Graphics::VertexBufferHandleArray&                       toUpdate,
        const std::vector<Input>&                                        inputs,
        const pxr::TfHashMap<pxr::SdfPath, Mapping, pxr::SdfPath::Hash>& mappings,
        bool                                                             fullUpdate,
        bool&                                                            hasSelectionHighlight);

    /**
     * \brief Helper, appends an index buffer, offsetting its value with a base index.
     * \param destIdx Destination index buffer.
     * \param srcIdx Source index buffer.
     * \param baseIndex Base index, we offset the "source" indices by this value.
     * \param count The source buffer size.
     */
    static void AppendIndexBuffer(int* destIdx, UINT* srcIdx, int baseIndex, size_t count);

    struct SubsetInfo
    {
        MaxSDK::Graphics::BaseMaterialHandle material;
        size_t                               numTris = 0;
    };

    /**
     * \brief Computes some information on the subsets of a prim's render data. Notably the material that
     * will be used in the viewport, and the number of triangles.
     * \param renderData The render data to compute the subset infos for.
     * \param renderNode The 3dsMax render node being consolidated. Can carry some material information.
     * \param subsetInfos Output variable,
     * \param materialTris Output variable, for each unique material, the total number of triangles using
     * the material amongst all subsets.
     */
    void ComputeSubsetInfo(
        const HdMaxRenderData&                                                renderData,
        const MaxSDK::Graphics::RenderNodeHandle&                             renderNode,
        std::vector<SubsetInfo>&                                              subsetInfos,
        std::vector<std::pair<MaxSDK::Graphics::BaseMaterialHandle, size_t>>& materialTris) const;

    std::shared_ptr<pxr::HdMaxRenderDelegate>                renderDelegate;
    Config                                                   config;
    pxr::TfHashMap<pxr::UsdTimeCode, OutputPtr, pxr::TfHash> consolidationCache;
};
