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
#include <RenderDelegate/HdMaxRenderData.h>
#include <RenderDelegate/PrimvarInfo.h>

#include <pxr/imaging/hd/mesh.h>

PXR_NAMESPACE_OPEN_SCOPE
class HdMaxRenderDelegate;

/**
 * \brief Handles hydra mesh to Nitrous render data synchronization.
 */
class HdMaxMesh : public HdMesh
{
public:
    enum DirtyBits : HdDirtyBits
    {
        DirtySelectionHighlight = HdChangeTracker::CustomBitsBegin,
    };

    /**
     * \brief Builds a hydra Max mesh.
     * \param rPrimId Render prim identifier, its full path within hydra (perhaps not exactly its path within the stage, as
     * multiple scene delegates can be tied to the same render index).
     * \param renderDataIdx The index of the nitrous render data we will be synchronizing the render primitive with.
     */
    HdMaxMesh(HdMaxRenderDelegate* delegate, SdfPath const& rPrimId, size_t renderDataIdx);

    /**
     * \brief Default destructor.
     */
    ~HdMaxMesh() override = default;

    // Inherited via HdMesh
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
     * \brief Performs synchronization of the hydra mesh with the nitrous render data.
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
     * \brief Finalizes the render mesh.
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
     * \brief Loads the points that will be used for rendering in Nitrous. Considers whether or not
     * vertices can be shared.
     * \param id Id of the Mesh.
     * \param delegate The scene delegate.
     * \param topology The mesh's topology.
     */
    void
    _LoadPoints(const pxr::SdfPath& id, HdSceneDelegate* delegate, const HdMeshTopology& topology);

    /**
     * \brief Loads the normals that will be used for rendering in Nitrous. Considers whether vertices
     * can be shared, and the interpolation scheme used for normals. If normals are not specified,
     * they are computed.
     * \param id The id of the mesh.
     * \param delegate The scene delegate.
     */
    void _LoadNormals(const pxr::SdfPath& id, HdSceneDelegate* delegate);

    /**
     * \brief Loads the UVs that will be used for rendering in Nitrous. Considers whether vertices
     * can be shared, and the interpolation scheme used for UVs.
     * \param id The id of the mesh.
     * \param delegate The scene delegate.
     * \param uvPrimvars A vector of primvar names, to be loaded as UVs.
     */
    void _LoadUvs(
        const pxr::SdfPath&         id,
        HdSceneDelegate*            delegate,
        const std::vector<TfToken>& uvPrimvars);

    /**
     * \brief Loads the display color primvar (will be used as vertex color)
     * \param id  The id of the mesh.
     * \param delegate The scene delegate.
     */
    void _LoadDisplayColor(const pxr::SdfPath& id, HdSceneDelegate* delegate);

    /**
     * \brief Initializes a render data object for a subset of the mesh, faces sharing the same bound material.
     * \param materialId The material Id that is bound to this subset.
     * \param instanced Whether this geometry will be instanced.
     * \param wireframe If we are initializing a wireframe or shaded item.
     * \return A SubsetRenderData initialized for the subset. Note that as this point, no actual geometry
     * is loaded into the render data, but it is ready to accept it.
     */
    HdMaxRenderData::SubsetRenderData
    _InitializeSubsetRenderData(const SdfPath& materialId, bool instanced, bool wireframe);

    /**
     * \brief Initializes the render data for all UsdGeomSubset of the mesh, we need one for every different material bound.
     * If no subsets are defined in the mesh, a single subset is created, containing the entire
     * geometry.
     * \param delegate The scene delegate.
     * \param materialId The material bound to the mesh itself (used if we need to create a subset for geometry that is
     * not already part of any subsets).
     * \param renderData The renderData containing the subsets that may need to be updated.
     * \param instanced True if the geometry is meant to be instanced.
     */
    void _UpdatePerMaterialRenderData(
        HdSceneDelegate* delegate,
        const SdfPath&   materialId,
        HdMaxRenderData& renderData,
        bool             instanced);

    /**
     * \brief Returns the main UV primvar used in a Material. Multiple UV primvars can be used by a material,
     * for not we only support a single one.
     * \param delegate The scene delegate.
     * \param materialId The ID of the material.
     * \return A pair containing the primvar to be used for the diffuseColor uvs, and a vector with all primvars used by
     * the material.
     */
    std::pair<TfToken, std::vector<TfToken>>
    _GetMaterialUvPrimvars(HdSceneDelegate* delegate, const SdfPath& materialId);

    /**
     * \brief Returns a reference to the nitrous render data associated with this hydra mesh.
     * \return The render data.
     */
    HdMaxRenderData& _GetRenderData();

private:
    /// The current dirty bits mask.
    HdDirtyBits dirtyBits;
    /// The render delegate associated with this mesh.
    HdMaxRenderDelegate* renderDelegate;
    /// Whether or not we can share vertices. If any primvar has
    /// faceVarying or uniform interpolation, we can't.
    bool sharedVertexLayout = false;
    /// Primvar data cache.
    PrimvarInfoMap primvarInfoMap;
    /// The topology of the mesh.
    HdMeshTopology sourceTopology;
    /// The topology we use for rendering, if unsharedLayoutRequired = false,
    /// this is the same as the sourceTopology.
    HdMeshTopology renderingTopology;
    /// Triangulated indices for display.
    VtVec3iArray triangulatedIndices;
    /// The curent number of instances for this mesh.
    size_t instanceCount = 0;
    /// Subset its for each face. If not subsets are defined, this remains
    /// empty.
    std::vector<SdfPath> faceIdToMaterialSubset;
    /// Cache so we only need to figure out a material's input UV primvar once.
    ///	The pair is [diffuseColor uv primvar, all material primvars]
    std::map<SdfPath, std::pair<TfToken, std::vector<TfToken>>> materialToUvPrimvars;
    /// Reference to the mesh's instancer. Can remain nullptr.
    HdInstancer* instancer = nullptr;
    /// All used UV primvars for this mesh.
    std::vector<TfToken> allUvPrimvars;
    /// Primvars that are currently required to be loaded.
    TfTokenVector requiredPrimvars;
};

PXR_NAMESPACE_CLOSE_SCOPE