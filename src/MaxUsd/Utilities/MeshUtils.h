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
#include <MaxUsd/MaxUSDAPI.h>

#include <MaxUsd.h>
#include <Mesh.h>

namespace MAXUSD_NS_DEF {

class PrimvarMappingOptions;

namespace MeshUtils {

/**
 * \brief This function sets up the edge visibility to ensure safe conversion to polygonal meshes.
 * \param mesh The mesh for which to adjust edge visibility.
 * \param fromTriNormals If true, makes sure that all edges which are between triangles which are not coplanar
 * are visible. Edges at material boundaries are also made visible. If false, all edges are made
 * visible.
 */
MaxUSDAPI void SetupEdgeVisibility(Mesh& mesh, bool fromTriNormals);

struct UvChannel
{
    pxr::TfToken      varname;
    pxr::VtVec3fArray data;
};

// A struct holding a triangulated USD geometry. It is assumed that all mapped data (primvars)
// shares the same indices.
struct UsdRenderGeometry
{
    // The mesh's triangle indices for each subset (need one subset per bound material).
    std::vector<pxr::VtVec3iArray> subsetTopoIndices;
    // The primvar data indices for each subset : normals, uv and vertex color.
    std::vector<pxr::VtVec3iArray> subsetPrimvarIndices;
    pxr::VtVec3fArray              points;
    pxr::VtVec3fArray              normals;
    std::vector<UvChannel>         uvs;
    pxr::VtVec3fArray              colors;
    // Material ids associated with each subset.
    std::vector<int> materialIds;
};

/**
 * \brief Converts USD render geometry, to a 3dsMax Mesh, meant for rendering.
 * \param usdGeom The USD geometry to convert.
 * \param mesh The output mesh.
 * \param primvarMappingOpts Primvar to channel mapping options.
 * \param unmappedPrimvar Output set of primvars which are not mapped to a channel, and so missing from the resulting mesh.
 * \return True if the conversion to a render mesh was sucessful, false otherwise.
 */
MaxUSDAPI bool ToRenderMesh(
    const UsdRenderGeometry&                                 usdGeom,
    Mesh&                                                    mesh,
    const MaxUsd::PrimvarMappingOptions&                     primvarMappingOpts,
    pxr::TfHashSet<pxr::TfToken, pxr::TfToken::HashFunctor>& unmappedPrimvars);

/**
 * \brief Efficiently attaches all the passes meshes, at the given offsets.
 * \param meshes The meshes to attach. NOTE : this is not a generalized function, it is specifically meant to work
 * on meshes built via MeshUtils::ToRenderMesh.
 * \param transforms The offset transform for each meshes.
 * \param attachedMesh The output, attached mesh.
 */
MaxUSDAPI void AttachAll(
    std::vector<std::shared_ptr<Mesh>> meshes,
    std::vector<Matrix3>               transforms,
    Mesh&                              attachedMesh);

} // namespace MeshUtils
} // namespace MAXUSD_NS_DEF
