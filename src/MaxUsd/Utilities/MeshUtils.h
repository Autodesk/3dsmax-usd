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
#include <MaxUsd.h>
#include <MaxUsd/MaxUSDAPI.h>

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

} // namespace MeshUtils
} // namespace MAXUSD_NS_DEF
