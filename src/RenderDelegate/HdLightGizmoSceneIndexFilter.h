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

#include "pxr/pxr.h" // PXR_VERSION
#if PXR_VERSION >= 2311

#include "RenderDelegateAPI.h"

#include <pxr/base/tf/declarePtrs.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/retainedDataSource.h>

#include <memory>
#include <stack>

PXR_NAMESPACE_OPEN_SCOPE

class HdLightGizmoSceneIndexFilter;
TF_DECLARE_WEAK_AND_REF_PTRS(HdLightGizmoSceneIndexFilter);

/**
 * A scene index filter to generate gizmos for lights. The light prims are replaced with meshes,
 * and re-added as a child to these meshes. This is to avoid having to remap those gizmo meshes
 * later on in selection (i.e. selecting the light gizmo in the viewport transparently selects the
 * light prim).The gizmo meshes for each light types are obtained from the GizmoMeshAccess derived
 * classes that should be passed to the constructor.
 */
class HdLightGizmoSceneIndexFilter : public HdSingleInputFilteringSceneIndexBase
{
public:
    using ParentClass = HdSingleInputFilteringSceneIndexBase;
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    /**
     * Simple struct representing a gizmo's geometry.
     */
    struct GizmoMesh
    {
        pxr::VtIntArray   vertexCounts;
        pxr::VtIntArray   indices;
        pxr::VtVec3fArray points;
        pxr::VtVec3fArray extent;
    };

    /**
     * Abstract class to fetch a gizmo's geometry data source. Derived class
     * would build data sources for the given source lights.
     */
    class GizmoMeshAccess
    {
    public:
        virtual ~GizmoMeshAccess() = default;
        virtual pxr::HdRetainedContainerDataSourceHandle GetGizmoSource(
            const pxr::HdSceneIndexPrim& sourceLight,
            const pxr::SdfPath&          lightPath) const
            = 0;
    };

    /**
     * Build a new light gizmo index filter.
     * @param inputSceneIndex Input scene index, where USD lights would be found.
     * @param meshAccess Accessor to retrieve gizmo geometry.
     * @return The new light gizmo filter.
     */
    static RenderDelegateAPI HdLightGizmoSceneIndexFilterRefPtr
    New(const HdSceneIndexBaseRefPtr&           inputSceneIndex,
        const std::shared_ptr<GizmoMeshAccess>& meshAccess)
    {
        return TfCreateRefPtr(new HdLightGizmoSceneIndexFilter(inputSceneIndex, meshAccess));
    }

    /**
     * Constructor.
     * @param inputSceneIndex Input scene index, where USD lights would be found.
     * @param meshAccess Accessor to retrieve gizmo geometry.
     */
    RenderDelegateAPI HdLightGizmoSceneIndexFilter(
        const HdSceneIndexBaseRefPtr&           inputSceneIndex,
        const std::shared_ptr<GizmoMeshAccess>& meshAccess);

    /**
     * Destructor.
     */
    RenderDelegateAPI ~HdLightGizmoSceneIndexFilter() override = default;

    /**
     * Notifies added prims from the initial stage traversal upon the filter's initialization. The
     * filter may create some prims (mesh gizmos) if it finds lights in the scene. We need to raise
     * notifications for those, but only once the filter is properly hooked up to in the scene index
     * chain.
     */
    void RenderDelegateAPI NotifyInitialAddedPrims();

    // HdSceneIndexBase overrides
    HdSceneIndexPrim RenderDelegateAPI GetPrim(const SdfPath& primPath) const override;
    SdfPathVector RenderDelegateAPI    GetChildPrimPaths(const SdfPath& primPath) const override;

protected:
    /**
     * Sets up a light gizmo if necessary for the given prim.
     * @param primPath The prim's path.
     * @param newPrims Output variable, any created prims are appended to this.
     * @return True if a light gizmo was generated, false otherwise.
     */
    bool GenerateGizmo(const SdfPath& primPath, HdSceneIndexObserver::AddedPrimEntries& newPrims);

    /**
     *  Sets up a light gizmo if necessary for the given prim. Same as the overload above except
     *  allows to pass in the prim's type to save fetching it from the input scene.
     * @param primPath The prim's path.
     * @param type The prim's type.
     * @param addedPrims Output variable, any created prims are appended to this.
     * @return True if a light gizmo was generated, false otherwise.
     */
    bool GenerateGizmo(
        const SdfPath&                          primPath,
        const pxr::TfToken&                     type,
        HdSceneIndexObserver::AddedPrimEntries& addedPrims);

    // HdFilteringSceneIndexBase overrides.
    void RenderDelegateAPI _PrimsAdded(
        const HdSceneIndexBase&                       sender,
        const HdSceneIndexObserver::AddedPrimEntries& entries) override;
    void RenderDelegateAPI _PrimsRemoved(
        const HdSceneIndexBase&                         sender,
        const HdSceneIndexObserver::RemovedPrimEntries& entries) override;
    void RenderDelegateAPI _PrimsDirtied(
        const HdSceneIndexBase&                         sender,
        const HdSceneIndexObserver::DirtiedPrimEntries& entries) override;

private:
    /**
     * Checks whether a given type token is a light type.
     * @param type The type to check.
     * @return True if the type is a light type.
     */
    static bool IsLight(const pxr::TfToken& type);

    // Name used for the create lights (lights moved under their mesh gizmos).
    pxr::TfToken lightName;
    // Accessor to get gizmo geometries for the lights.
    std::shared_ptr<GizmoMeshAccess> meshAccess;
    // Created prims on the initial stage traversal.
    HdSceneIndexObserver::AddedPrimEntries initialAddedPrims;
    // A map of light prims to gizmo data sources.
    std::unordered_map<pxr::SdfPath, pxr::HdContainerDataSourceHandle, pxr::SdfPath::Hash>
        lightGizmos;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif