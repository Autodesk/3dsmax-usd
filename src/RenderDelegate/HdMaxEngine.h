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

#include "HdMaxConsolidator.h"
#include "Imaging/HdMaxRenderDelegate.h"
#include "Imaging/HdMaxTaskController.h"
#include "RenderDelegateAPI.h"

#include <MaxUsd/Utilities/MaterialRef.h>
#include <MaxUsd/Utilities/ProgressReporter.h>

#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/usdImaging/usdImaging/delegate.h>

#include <stdmat.h>

class PrimvarMappingOptions;

class RenderDelegateAPI HdMaxEngine final
{
public:
    HdMaxEngine();
    ~HdMaxEngine();

    /**
     * \brief "Renders" from the given prim root, as Nitrous RenderItems.
     * \param rootPrim The prim to render from.
     * \param rootTransform The transform to apply at the root when rendering.
     * \param targetRenderItemContainer A container for the produced render items.
     * \param timeCode The time code at which to render the USD content.
     * \param updateDisplayContext Update display context (required to generate instance render items).
     * \param nodeContext Node context (required to generate instance render items).
     * \param reprs Required representations, currently support wireframe ("wire") and shaded ("smootHull")
     * \param renderTags Render tags to be used in the hydra render.
     * \param multiMat A multiMaterial meant to hold all of the stage's material, will be filled by the render call.
     * \param consolidationConfig Consolidation configuration.
     * \param view View information, if specified, it is used for frustum culling and configuring Zbiases, for example.
     * \param buildOfflineRenderMaterial If true, build the 3dsMax materials from the Stage's UsdPreviewSurface materials,
     * and the multimaterial carrying them, which can be used for offline rendering.
     * \param progressReporter Provides means to report progress on lengthy operations triggered from the render,
     * typically hooked up to UI.
     */
    void Render(
        const pxr::UsdPrim&                           rootPrim,
        const pxr::GfMatrix4d&                        rootTransform,
        MaxSDK::Graphics::IRenderItemContainer&       targetRenderItemContainer,
        const pxr::UsdTimeCode&                       timeCode,
        const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
        MaxSDK::Graphics::UpdateNodeContext&          nodeContext,
        const pxr::TfTokenVector&                     reprs,
        const pxr::TfTokenVector&                     renderTag,
        MultiMtl*                                     multiMat,
        const HdMaxConsolidator::Config& consolidationConfig = HdMaxConsolidator::Config {},
        ViewExp*                         view = nullptr,
        bool                             buildOfflineRenderMaterial = false,
        const MaxUsd::ProgressReporter&  progressReporter = {});

    /**
     * \brief Sets the current USD selection (so that it can be properly drawn). This will populate the
     * hydra selection held by the render delegate with these paths, and all their children.
     * \param paths The new selection. In case of point instances, the prim path is the instancer's path, and
     * the int array represent the instance indices.
     */
    void SetSelection(const std::unordered_map<pxr::SdfPath, pxr::VtIntArray, pxr::SdfPath::Hash>&
                          newSelection) const;

    /**
     * \brief Renders the USD stage to 3dsMax TriMeshes (suitable for rendering by any renderer).
     * \param node The node from which the UsdStage is being rendered.
     * \param rootPrim The prim to render from.
     * \param rootTransform The transform to apply at the root when rendering.
     * \param outputMeshes Filled with the generated meshes.
     * \param meshTransforms The mesh transforms.
     * \param timeCode The UsdTimeCode at which to render.
     * \param renderTags The render tags (purposes) to use.
     */
    void RenderToMeshes(
        INode*                              node,
        const pxr::UsdPrim&                 rootPrim,
        const pxr::GfMatrix4d&              rootTransform,
        std::vector<std::shared_ptr<Mesh>>& outputMeshes,
        std::vector<Matrix3>&               meshTransforms,
        const pxr::UsdTimeCode&             timeCode,
        const pxr::TfTokenVector&           renderTags);

    /**
     * \brief Updates the root primitive to render from and initialize materials. If the given root prim is different from the
     * currently set root prim, a new scene delegate is created for the new root.
     * \param rootPrim The prim to use as root for the render.
     * \param nodeMaterial If the root prim changes, we try to connect any existing 3dsMax material
     * bound to the node, to usd materials in the source data.
     * \return True if the root prim was changed, false otherwise.
     */
    bool UpdateRootPrim(const pxr::UsdPrim& rootPrim, Mtl* nodeMaterial = nullptr);

    /**
     * \brief Returns the number of prims that actually get rendered.
     * \param renderTags Render tags to consider.
     * \return The number of prims that will be rendered.
     */
    size_t GetNumRenderPrim(const pxr::TfTokenVector& renderTags) const;

    /**
     * \brief Returns the engine's render delegate.
     * \return The render delegate used by the engine.
     */
    const std::shared_ptr<pxr::HdMaxRenderDelegate> GetRenderDelegate() const
    {
        return renderDelegate;
    }

    /**
     * \brief Initializes the material collection from a 3dsMax material. Typically, this is called
     * with the material currently on the node hosting the USD Stage Object. The method will check
     * if the material passed is a MultiMtl and if it carries any UsdPreviewSurface materials  that
     * can be linked to the Stage materials (looking at the material type and name). When a 3dsMax
     * scene with a UsdStage is loaded from disk, this is the function reconnecting the saved 3dsMax
     * materials to the stage materials - so that if materials change again in the stage, the 3dsMax
     * materials are updated.
     * \param stage The USD Stage we are working with.
     * \param material The 3dsMax material to initialize the collection from.
     */
    void InitializeMaterialCollection(pxr::UsdStageWeakPtr stage, Mtl* material);

    /**
     * \brief Returns the current change tracker (internally held by the render index). Callers should avoid
     * holding on to references to the change tracker in between render calls, as it not guaranteed
     * to stay valid. For example, if the stage being rendered is changed, a new RenderIndex is
     * created, and a new change tracker along with it.
     * \return The change tracker.
     */
    pxr::HdChangeTracker& GetChangeTracker();

    /**
     * \brief Performs a hydra render.
     * \param rootTransform The root transform for the render.
     * \param timeCode The timeCode at which we are rendering.
     * \param renderTags Enabled render tags.
     * \param loadAllUnmapped primvars If true, all primvars mapped to 3dsMax channels are loaded from hydra,
     * regardless of usage in materials.
     */
    void HydraRender(
        const pxr::GfMatrix4d&    rootTransform,
        const pxr::UsdTimeCode&   timeCode,
        const pxr::TfTokenVector& renderTags,
        bool                      loadAllUnmappedPrimvars = false);

private:
    /**
     * \brief Update the scene delegate to prepare it for rendering.
     * \param timeCode TimeCode at which the render will take place.
     * \param renderTags Render tags to be used by the render.
     */
    void PrepareBatch(const pxr::UsdTimeCode& timeCode, const pxr::TfTokenVector& renderTags);

    /**
     * \brief Effectively do the processing of the scene, updating the associated render delegate.
     * \warning PrepareBatch must be called first.
     */
    void RenderBatch();

    /**
     * \brief Updates the list of UsdPreviewSurface materials, from the given render data.
     * The list is maintained for the purpose of generating the materials Ids which eventually need
     * to be applied to different parts of the mesh that we end up using for offline rendering (for
     * renderers not supporting USD directly)
     * \param renderData The render data to update the material from.
     * \param collection The material collection in use for the object, just used to get the display color material
     * fallback if required.
     */
    void UpdateMaterialIdsList(
        const std::vector<HdMaxRenderData*>&     renderData,
        std::shared_ptr<HdMaxMaterialCollection> collection);

    /**
     * \brief Updates the 3dsMax multi-material holding all of the converted USD materials, from the given render data.
     * This is typically called when rendering. The multi-material is typically applied to the Node
     * owning the USD Stage, so it can be rendered.
     * \param multiMat The multi-material we are updating.
     */
    void UpdateMultiMaterial(MultiMtl* multiMat) const;

    /**
     * \brief Attempts consolidation of the given render data.
     * \param renderData The render data we are hoping to consolidate.
     * \param lastTimeCode The last time code we rendered, used to find a pre-existing consolidation
     * we might want to update.
     * \param timeCode The current time.
     * \param config Configuration parameters for the consolidation.
     * \param wireMaterial The wireframe material we should use for consolidated wireframe geometry.
     * \return The result of the consolidation, or nullptr.
     */
    HdMaxConsolidator::OutputPtr Consolidate(
        const std::vector<HdMaxRenderData*>&        renderData,
        const pxr::UsdTimeCode&                     lastTimeCode,
        const pxr::UsdTimeCode&                     timeCode,
        const HdMaxConsolidator::Config&            config,
        const MaxSDK::Graphics::BaseMaterialHandle& wireMaterial);

    /// Create a sceneDelegate out of a stage.
    std::unique_ptr<pxr::UsdImagingDelegate> CreateSceneDelegate(const pxr::UsdPrim& rootPrim);
    /// The hydra engine used for rendering.
    pxr::HdEngine engine;
    /// The Nitrous render delegate.
    std::shared_ptr<pxr::HdMaxRenderDelegate> renderDelegate;
    /// The USD scene delegate, producing hydra data from USD.
    std::unique_ptr<pxr::UsdImagingDelegate> sceneDelegate;
    /// The render index. This is essentially a flattened representation of the scene graph. It is
    /// tied to our scene delegate and render delegate.
    std::unique_ptr<pxr::HdRenderIndex> renderIndex;
    /// The collection of primitives to be rendered.
    pxr::HdRprimCollection renderCollection;
    /// Task controller, generates the rendering tasks.
    std::unique_ptr<pxr::HdMaxTaskController> taskController;
    /// The root prim we render from.
    pxr::UsdPrim rootPrim;
    /// Max materials that were converted at the last Render(). The empty path maps to the
    /// displayColor material.
    pxr::TfHashMap<
        pxr::SdfPath,
        std::pair<int, std::shared_ptr<MaxUsd::MaterialRef>>,
        pxr::SdfPath::Hash>
        materials;
    /// Provides the capability to consolidate USD prim render data - while keeping track of where
    /// each prim's
    ///	mesh data ends up so it can potentially be updated.
    std::unique_ptr<HdMaxConsolidator> consolidator;
    // Control variables for static/dynamic consolidation.
    bool                                               staticDelayStarted = false;
    std::chrono::time_point<std::chrono::system_clock> staticDelayStartTime;
    pxr::UsdTimeCode                                   lastVpRenderTime;
};
