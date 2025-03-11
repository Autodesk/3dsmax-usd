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

// Light gizmos are only supported in version with USD 23.11+
#if PXR_VERSION >= 2311

#include "HdLightGizmoSceneIndexFilter.h"

class HdMaxLightGizmoMeshAccess : public pxr::HdLightGizmoSceneIndexFilter::GizmoMeshAccess
{
public:
    /**
     * Constructor
     */
    RenderDelegateAPI HdMaxLightGizmoMeshAccess();

    /**
     * Sets the scaling offset for the light gizmos. Additional scaling will be
     * applied to consider the current 3dsMax scene units vs the source mesh units on disk.
     * @param scaling The scaling offset.
     * @param changeTracker Change tracker to be used to notify that lights are dirty.
     * @param lights The lights impacted by the change.
     */
    RenderDelegateAPI void SetScalingMatrix(
        const pxr::GfMatrix4d&           scaling,
        pxr::HdChangeTracker&            changeTracker,
        const std::vector<pxr::SdfPath>& lights);

    /**
     * Returns the current light scaling matrix.
     * @return The scaling matrix.
     */
    RenderDelegateAPI pxr::GfMatrix4d GetOffsetMatrix() const { return scalingMatrix; }

    // GizmoMeshAccess overrides.
    RenderDelegateAPI pxr::HdRetainedContainerDataSourceHandle GetGizmoSource(
        const pxr::HdSceneIndexPrim& sourceLight,
        const pxr::SdfPath&          lightPath) const override;

private:
    /**
     * Retrieves the scaling that should be applied on the source gizmo mesh for light types that
     * have specific light shape gizmos. The scaling retuned is relative to the default values.
     * For example, sphere lights have a default radius of 0.5. If a sphere light is found with a
     * radius of 1.0, then the scaling would be 2.0. Supported types are :
     *  - sphereLight
     *  - rectLight
     *  - cylinderLight
     *  - diskLight
     * @param type The light type.
     * @param path The light prim's path.
     * @param source The light prim's data source.
     * @return The scaling.
     */
    static pxr::GfMatrix4d GetLightScalingMatrix(
        const pxr::TfToken&                     type,
        const pxr::SdfPath&                     path,
        const pxr::HdContainerDataSourceHandle& source);

    /**
     * Checks whether a hydra light should be displayed using a cone as base shape.
     * There is no light type for "cone" but we want to display lights using the shapingAPI
     * as cone. The function will look for those attributes, and if found, will return true.
     * @param path The path of the light
     * @param source The light's data source.
     * @return True if a hydra light should be displayed as a cone.
     */
    bool
    IsDisplayAsCone(const pxr::SdfPath& path, const pxr::HdContainerDataSourceHandle& source) const;

    // Mapping of gizmo source file to cached gizmo meshes.
    static std::unordered_map<std::string, pxr::HdLightGizmoSceneIndexFilter::GizmoMesh>
        gizmoMeshes;
    // Mapping of light type to gizmo mesh source file.
    static std::
        unordered_map<pxr::TfToken, std::pair<std::string, std::string>, pxr::TfToken::HashFunctor>
            typeGizmos;
    // Gizmo mesh scaling matrix.
    pxr::GfMatrix4d scalingMatrix;
};

#endif