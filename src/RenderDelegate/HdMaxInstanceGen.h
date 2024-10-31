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
// For instancing. Instancing SDK changed in 2023.
#include "RenderDelegateAPI.h"

#include <MaxUsd/Utilities/MaxRestrictedSupportUtils.h>

#include <Graphics/RenderItemHandleArray.h>

class MaxRenderGeometryFacade;
class HdMaxDisplaySettings;

/// A class responsible for updating and generating the nitrous instance
/// render items for a given geometry. It handles both shaded and wireframe geometry
/// individually. Typically, RequestUpdate(...) is called from a hydra thread, it sets up the
/// instance data - and later, GenerateInstances(...) is called from the main
/// thread, from the Max viewport render loop.
class RenderDelegateAPI HdMaxInstanceGen
{
public:
    /// Depending on changes that happen over time, instances may need
    /// to be recreated completely, or just updated.
    enum class DirtyState
    {
        Clean,
        NeedUpdate,
        NeedRecreate
    };

    /**
     * \brief Constructor
     */
    HdMaxInstanceGen();

    /**
     * \brief Sets the sync state of the generator to "Clean"
     * \param wire True if we are setting the state for the wireframe instances, false otherwise.
     */
    void SetClean(bool wire);

    /**
     * \brief Returns the number of instances.
     * \return The number of instances.
     */
    size_t GetNumInstances() const { return transforms.size(); };

    /**
     * \brief Returns the transforms of the instances.
     * \return The transforms of the instances.
     */
    const std::vector<Matrix3>& GetTransforms() const;

    /**
     * \brief Request that instances be updated. This can be called from a hydra thread.
     * \param recreateInstances True if the instances need to be fully rebuilt, that is the case
     * if for example the topology of the mesh being instanced has changed, or the number of
     * instances.
     * \param transforms The new transforms of the instances.
     */
    void RequestUpdate(bool recreateInstances, const pxr::VtMatrix4dArray& transforms);

    /**
     * \brief Request that the selection display, i.e. the instanced render items that we use to show selected
     * instances, be updated. Instance selection display is basically drawing a subset of the
     * instances, the selected ones, in a different way.
     * \param recreate If true, selection display instances are completely recreated. Typically, we need to
     * regenerate the instances fully if things like the topology, the materials, or the number of
     * instances have changed.
     */
    void RequestSelectionDisplayUpdate(bool recreate);

    /**
     * \brief Creates or updates the instance data as needed, and generate the instance render items.
     * \param instanceGeometry Render geometry facade wrapping the instances' geometry (selection display
     * instances as well).
     * \param material The material to be used for the instances. Can be left empty.
     * \param targetRenderItemContainer The container to which to add the generated render items.
     * \param updateDisplayContext The update display context.
     * \param nodeContext The node context.
     * \param wireframe Whether we are generating wireframe instance render items, or shaded ones.
     * \param gizmo Whether the instancing is for a gizmo (and so requires the gizmo vis group).
     * \param subset The subset we are generating the instances for - a mesh can have multiple, associated
     * to instanced render items (1 material maximum per render item). The instance generator will
     * keep a cache of instance render items internally.
     * \param viewExp View information used to configure Zbiases for selection.
     */
    void GenerateInstances(
        MaxRenderGeometryFacade*                      instanceGeometry,
        MaxSDK::Graphics::BaseMaterialHandle*         material,
        MaxSDK::Graphics::IRenderItemContainer&       targetRenderItemContainer,
        const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
        MaxSDK::Graphics::UpdateNodeContext&          nodeContext,
        bool                                          wireframe,
        bool                                          gizmo,
        int                                           subset,
        ViewExp*                                      viewExp);

    /**
     * \brief Mark an instance as selected so that the render items for selection display are generated.
     * \param instanceIdx The index of the instance that should be marked selected.
     */
    void Select(int instanceIdx);

    /**
     * \brief Clear the selection of instances.
     */
    void ResetSelection();

    /**
     * \brief Returns the selection status of instances. A vector of booleans, true if an instance at the
     * corresponding index is selected, false otherwise.
     * \return Selection status of held instances.
     */
    const std::vector<bool>& GetSelection();

    /**
     * Sets the number of geom subsets that will be instanced.
     * @param subsetCount The new number of subsets.
     */
    void SetSubsetCount(size_t subsetCount);

    /**
     * Gets the number of geom subsets being instanced.
     * @return The number of subsets.
     */
    size_t GetSubsetCount() const;

    /**
     * \brief Returns computed bounding box of selected instances.
     * \param extent The extent of the selected instances.
     * \return Computed bounding box of selected instances.
     */
    const pxr::GfRange3d ComputeSelectionBoundingBox(pxr::GfRange3d& extent);

private:
    // All the instance transforms, empty if the geometry is not instanced. This vector will back
    // the pointer given to the InstanceData.
    std::vector<Matrix3> transforms;
    std::vector<Matrix3> selectedTransforms;
    std::vector<bool>    selection;

    // Instancing data (used to create the instance vertex buffer).
    MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceData shadedData;
    MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceData shadedSelectionData;
    // Instancing data for the wireframe render items.
    MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceData wireData;
    MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceData wireSelectionData;

    // The state of the instance data, i.e. whether it needs recreation, update, etc.
    // Shaded, wireframe and selection display instances need to be managed independently
    // so we need a state for each.
    DirtyState shadedState = DirtyState::Clean;
    DirtyState shadedSelectionState = DirtyState::Clean;
    DirtyState wireState = DirtyState::Clean;
    DirtyState wireSelectionState = DirtyState::Clean;

    // Cached render items. We get one render item per subset. And one for wireframe.
    std::vector<MaxSDK::Graphics::RenderItemHandleArray> cachedShaded;
    MaxSDK::Graphics::RenderItemHandleArray              cachedWire;
    // Also cache the instance render items used for selection display.
    std::vector<MaxSDK::Graphics::RenderItemHandleArray> cachedSelectionShaded;
    MaxSDK::Graphics::RenderItemHandleArray              cachedSelectionWire;
    // For the cached render items, keep track of the matching selection state of the node.
    // When the selection state change the cached will need to be dropped.
    bool shadedCacheNodeSelectionStatus = false;
    bool wireCacheNodeSelectionStatus = false;
};
