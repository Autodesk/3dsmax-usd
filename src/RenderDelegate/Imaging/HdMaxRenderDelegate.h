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
#include "HdMaxMesh.h"

#include <RenderDelegate/HdMaxDisplaySettings.h>
#include <RenderDelegate/HdMaxMaterialCollection.h>
#include <RenderDelegate/HdMaxRenderData.h>
#include <RenderDelegate/RenderDelegateAPI.h>

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/resourceRegistry.h>

PXR_NAMESPACE_OPEN_SCOPE

/**
 * \brief Implementation of a render delegate mapping hydra primitive to Nitrous.
 */
class RenderDelegateAPI HdMaxRenderDelegate final : public HdRenderDelegate
{
public:
    HdMaxRenderDelegate(HdRenderSettingsMap const& settings = {});
    ~HdMaxRenderDelegate() override;

    /**
     * \brief Returns the map of prim paths to render data ids.
     * \return The map of prim path to render data id.
     */
    const std::unordered_map<pxr::SdfPath, size_t, pxr::SdfPath::Hash>& GetRenderDataIdMap() const;

    /**
     * \brief Gets the render data associated with a given prim.
     * \param primpath The prim to get the render data for.
     * \return The prim's render data.
     */
    HdMaxRenderData& GetRenderData(const pxr::SdfPath& primpath);

    /**
     * \brief Attempt to get the render data from its last known index. Indices of render data can change (rarely) over
     * time (for example if a prim is deactivated). This allows us to avoid a map lookup, in most
     * cases. If the given id doesn't match the path, we fallback to using the path to find the render data.
     * \param index The index of the render data.
     * \param primpath The prim path of render data.
     * \return A reference to the render data.
     */
    HdMaxRenderData& SafeGetRenderData(size_t index, const pxr::SdfPath& primpath);

    /**
     * \brief Return the internal index of the render data associated with a given path.
     * Unsafe to call this method with a prim path not held by the delegate.
     * \param path The path of the render data to get the index for.
     * \return The internal index of the render data. Warning, internal indices can change,
     * unless you know for sure that has not happened, use SafeGetRenderData() to retrieve
     * back a render data from an index.
     */
    size_t GetRenderDataIndex(const pxr::SdfPath& path) const;

    /**
     * \brief Returns the render data associated with a given id (essentially the index
     * in the vector).
     * \param id The render data id.
     * \return The render data.
     */
    HdMaxRenderData& GetRenderData(size_t id);

    /**
     * \brief Returns all the render data maintained by this render delegate.
     * \return All render data.
     */
    std::vector<HdMaxRenderData>& GetAllRenderData();

    /**
     * \brief Returns the render data associated with visible prims only, given their visibility and
     * the specified render tags.
     * \param renderTags The render tags to consider.
     * \param data The visible prims' render data.
     */
    void GetVisibleRenderData(const TfTokenVector& renderTags, std::vector<HdMaxRenderData*>& data);

    /**
     * \brief Returns a reference to the Max viewport display settings used by this render delegate.
     * \return The display settings.
     */
    HdMaxDisplaySettings& GetDisplaySettings();

    /**
     * \brief Sets the current hydra selection.
     * \param selection The new hydra selection to set.
     */
    void SetSelection(const pxr::HdSelectionSharedPtr& selection);

    /**
     * \brief Returns the current hydra selection.
     * \return The current hydra selection.
     */
    const pxr::HdSelectionSharedPtr& GetSelection() const;

    /**
     * \brief Returns the selection status of a particular prim.
     * \param path The path of the prim to get the selection status for.
     * \return The selection status of the prim. If null, the prim can be assumed unselected.
     */
    const pxr::HdSelection::PrimSelectionState* GetSelectionStatus(const pxr::SdfPath& path) const;

    /**
     * \brief Primvar to 3dsMax map channel mapping options.
     * \return The primvar mapping options.
     */
    MaxUsd::PrimvarMappingOptions& GetPrimvarMappingOptions();

    /**
     * \brief Returns the material collection held by the render delegate.
     */
    std::shared_ptr<HdMaxMaterialCollection> GetMaterialCollection();

    /**
     * \brief Clears all cached render data.
     */
    void Clear();

    /**
     * \brief Destroys any obsolete render data (for example, some usdGeomSubset was removed).
     * We keep these around until now to make sure render items get ref counted to 0 while on
     * the main thread, it can cause issues otherwise.
     */
    void GarbageCollect();

    /**
     * \brief Flag the need for garbage collection.
     */
    void RequestGC() { mustGc = true; }

    // HdRenderDelegate overrides.
    const pxr::TfTokenVector&        GetSupportedRprimTypes() const override;
    const pxr::TfTokenVector&        GetSupportedSprimTypes() const override;
    const pxr::TfTokenVector&        GetSupportedBprimTypes() const override;
    pxr::HdRenderParam*              GetRenderParam() const override;
    pxr::HdResourceRegistrySharedPtr GetResourceRegistry() const override;
    pxr::HdRenderPassSharedPtr
    CreateRenderPass(pxr::HdRenderIndex* index, pxr::HdRprimCollection const& collection) override;
    pxr::HdInstancer*
                  CreateInstancer(pxr::HdSceneDelegate* delegate, pxr::SdfPath const& id) override;
    void          DestroyInstancer(pxr::HdInstancer* instancer) override;
    pxr::HdRprim* CreateRprim(pxr::TfToken const& typeId, pxr::SdfPath const& rPrimId) override;
    void          DestroyRprim(pxr::HdRprim* rPrim) override;
    pxr::HdSprim* CreateSprim(pxr::TfToken const& typeId, pxr::SdfPath const& sprimId) override;
    pxr::HdSprim* CreateFallbackSprim(pxr::TfToken const& typeId) override;
    void          DestroySprim(pxr::HdSprim* sprim) override;
    pxr::HdBprim* CreateBprim(pxr::TfToken const& typeId, pxr::SdfPath const& bprimId) override;
    pxr::HdBprim* CreateFallbackBprim(pxr::TfToken const& typeId) override;
    void          DestroyBprim(pxr::HdBprim* bprim) override;
    void          CommitResources(pxr::HdChangeTracker* tracker) override;

private:
    static const TfTokenVector SUPPORTED_RPRIM_TYPES;
    static const TfTokenVector SUPPORTED_SPRIM_TYPES;
    static const TfTokenVector SUPPORTED_BPRIM_TYPES;

    // Keep the nitrous render data in a vector for fast iteration / minimizing cache misses.
    std::vector<HdMaxRenderData>                                 renderDataVector;
    std::unordered_map<pxr::SdfPath, size_t, pxr::SdfPath::Hash> renderDataIndexMap;
    /// The meshes backing the render items.
    std::unordered_map<pxr::SdfPath, std::unique_ptr<HdMaxMesh>, pxr::SdfPath::Hash> meshes;
    /// 3dsMax viewport display settings.
    HdMaxDisplaySettings displaySettings;
    /// A material collection, once render, all the stages material's are held
    /// in this collection.
    std::shared_ptr<HdMaxMaterialCollection> materialCollection;
    // Primvar -> 3dsMax channel mapping options.
    MaxUsd::PrimvarMappingOptions primvarMappingOptions;
    /// Flag indicating we have some render data waiting to be deleted from the main thread.
    /// The destruction itself happens from GarbageCollect() call.
    bool mustGc = false;
    /// The active selection to be displayed when rendering.
    pxr::HdSelectionSharedPtr activeSelection;
};

PXR_NAMESPACE_CLOSE_SCOPE
