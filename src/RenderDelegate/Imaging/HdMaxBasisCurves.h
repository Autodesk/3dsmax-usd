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
#include <RenderDelegate/HdMaxBasisCurvesRenderData.h>
#include <RenderDelegate/PrimvarInfo.h>

#include <pxr/imaging/hd/basisCurves.h>

PXR_NAMESPACE_OPEN_SCOPE
class HdMaxRenderDelegate;

/**
 * \brief Handles hydra basiscurves to Nitrous render data synchronization.
 */
class HdMaxBasisCurves : public HdBasisCurves
{
public:
    enum DirtyBits : HdDirtyBits
    {
        DirtySelectionHighlight = HdChangeTracker::CustomBitsBegin,
    };

    /**
     * \brief Builds a hydra Max basiscurves.
     * \param rPrimId Render prim identifier, its full path within hydra (perhaps not exactly its path within the stage, as
     * multiple scene delegates can be tied to the same render index).
     * \param renderDataIdx The index of the nitrous render data we will be synchronizing the render primitive with.
     */
    HdMaxBasisCurves(HdMaxRenderDelegate* delegate, SdfPath const& rPrimId, size_t renderDataIdx);

    /**
     * \brief Default destructor.
     */
    ~HdMaxBasisCurves() override = default;

    // Inherited via HdBasisCurves
    /**
     * \brief Returns the initial dirty bit mask. This will tell Hydra what, within render primitives, needs to be
     * flagged dirty so it can be refreshed initially.
     * \return The dirty bit mask.
     */
    HdDirtyBits GetInitialDirtyBitsMask() const override;

    /**
     * \brief Check if a primvar is currently required to be loaded.
     * \param primvar The primvar name to check.
     * \return True if the primvar is required, false otherwise.
     */
    bool PrimvarIsRequired(const TfToken& primvar) const;

    /**
     * \brief Performs synchronization of the hydra basiscurves with the nitrous render data.
     * \param delegate A reference to the scene delegate.
     * \param renderParam Render parameter.
     * \param dirtyBits The dirty bit mask.
     * \param reprToken The representation that needs to be updated. We do not use this for now.
     */
    void Sync(
        HdSceneDelegate* delegate,
        HdRenderParam*   renderParam,
        HdDirtyBits*     dirtyBits,
        TfToken const&   reprToken) override;

    /**
     * \brief Finalizes the render basiscurves.
     * \param renderParam Render parameter.
     */
    void Finalize(HdRenderParam* renderParam) override;

protected:
    HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;
    void        _InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits) override;

    /**
     * \brief Updates the primvar data cached in primvarInfoMap.
     * \param sceneDelegate The scene delegate.
     * \param dirtyBits Currently dirty bits.
     * \param requiredPrimvars Primvars that should be considered in the update.
     */
    void _UpdatePrimvarSources(
        HdSceneDelegate*     sceneDelegate,
        HdDirtyBits          dirtyBits,
        const TfTokenVector& requiredPrimvars);

    /**
     * \brief Loads the points that will be used for rendering in Nitrous.
     * \param id Id of the BasisCurves.
     * \param delegate The scene delegate.
     * \param topology The basiscurves's topology.
     */
    void _LoadPoints(
        const pxr::SdfPath&          id,
        HdSceneDelegate*             delegate,
        const HdBasisCurvesTopology& topology);

    /**
     * \brief Initializes a render data object for a "subset" of a BasisCurves. Note here in the case of BasisCurves,
     * we don't use the subset concept as we do with meshes, but we still create a similar object to
     * hold the render data.
     * \param materialId The material Id that is bound to this subset.
     * \param instanced Whether this geometry will be instanced.
     * \param wireframe If we are initializing a wireframe or shaded item.
     * \return A SubsetRenderData initialized for the subset. Note that as this point, no actual geometry
     * is loaded into the render data, but it is ready to accept it.
     */
    HdMaxBasisCurvesRenderData::SubsetRenderData
    _InitializeSubsetRenderData(const SdfPath& materialId, bool instanced, bool wireframe);

    /**
     * \brief Returns a reference to the nitrous render data associated with this hydra basiscurves.
     * \return The render data.
     */
    HdMaxBasisCurvesRenderData& _GetRenderData();

    static PrimvarInfo* _GetPrimvarInfo(const PrimvarInfoMap& infoMap, const TfToken& token);

private:
    /// The current dirty bits mask.
    HdDirtyBits dirtyBits;
    /// The render delegate associated with this basiscurves.
    HdMaxRenderDelegate* renderDelegate;
    /// Primvar data cache.
    PrimvarInfoMap primvarInfoMap;
    /// The topology of the basiscurves.
    HdBasisCurvesTopology sourceTopology;
    /// The topology we use for rendering.
    HdBasisCurvesTopology renderingTopology;
    /// The curent number of instances for this basiscurves.
    size_t instanceCount = 0;
    /// Reference to the basiscureves's instancer. Can remain nullptr.
    HdInstancer* instancer = nullptr;
    /// Primvars that are currently required to be loaded.
    TfTokenVector requiredPrimvars;
};

PXR_NAMESPACE_CLOSE_SCOPE