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
#include "MeshUtils.h"

#include "MaxSupportUtils.h"

#include <mesh.h>

#ifdef IS_MAX2025_OR_GREATER
#include <Geom/point3.h>
#else
#include <point3.h>
#endif

namespace MAXUSD_NS_DEF {
namespace MeshUtils {

class DirectedEdge
{
public:
    unsigned int v0;
    unsigned int v1;
    unsigned int triangleIdx;
    unsigned int edgeIdx;

    DirectedEdge(
        unsigned int _v0,
        unsigned int _v1,
        unsigned int tidx = invalidIndex,
        unsigned int eidx = invalidIndex)
    {
        v0 = _v0;
        v1 = _v1;
        triangleIdx = tidx;
        edgeIdx = eidx;
    }

    static bool CompareWithoutTriangleIndex(const DirectedEdge& l, const DirectedEdge& r)
    {
        if (l.v0 == r.v0) {
            return l.v1 < r.v1;
        }
        return l.v0 < r.v0;
    }

    static bool Compare(const DirectedEdge& l, const DirectedEdge& r)
    {
        if (l.v0 == r.v0) {
            if (l.v1 == r.v1) {
                if (l.triangleIdx == r.triangleIdx) {
                    return l.edgeIdx < r.edgeIdx;
                }
                return l.triangleIdx < r.triangleIdx;
            }
            return l.v1 < r.v1;
        }
        return l.v0 < r.v0;
    }

    bool operator<(const DirectedEdge& r) const { return CompareWithoutTriangleIndex(*this, r); }

    static const unsigned int invalidIndex = 0xffffffff;
};

void SetupEdgeVisibility(Mesh& mesh, bool fromTriNormals)
{
    if (!fromTriNormals) {
        // Set all edges visible.
        for (int i = 0; i < mesh.numFaces; ++i) {
            for (int j = 0; j < 3; ++j) {
                mesh.faces[i].setEdgeVis(j, EDGE_VIS);
            }
        }
        return;
    }

    // Some useful lambdas..
    auto isDegenerated = [](Face& face) {
        auto v0 = face.getVert(0);
        auto v1 = face.getVert(1);
        auto v2 = face.getVert(2);
        return v0 == v1 || v1 == v2 || v2 == v0;
    };

    auto computeNormal = [](Mesh& mesh, Face& face) {
        Point3 e0 = mesh.verts[face.getVert(1)] - mesh.verts[face.getVert(0)];
        Point3 e1 = mesh.verts[face.getVert(2)] - mesh.verts[face.getVert(0)];
        return (e0 ^ e1).Normalize();
    };

    // The basic idea is to look for shared edges between triangles. If the triangles are not
    // coplanar or do not share the same material, show the edge.
    std::vector<DirectedEdge> orderedEdges;
    const auto                numFaces = mesh.getNumFaces();
    orderedEdges.reserve(numFaces * 3);
    for (int i = 0; i < numFaces; ++i) {
        orderedEdges.emplace_back(mesh.faces[i].getVert(0), mesh.faces[i].getVert(1), i, 0);
        orderedEdges.emplace_back(mesh.faces[i].getVert(1), mesh.faces[i].getVert(2), i, 1);
        orderedEdges.emplace_back(mesh.faces[i].getVert(2), mesh.faces[i].getVert(0), i, 2);
    }
    std::sort(orderedEdges.begin(), orderedEdges.end(), DirectedEdge::Compare);

    for (const auto& currentEdge : orderedEdges) {
        // Fetch the related triangle
        Face& currentTriangle = mesh.faces[currentEdge.triangleIdx];

        // Skip the triangle if its degenerated.
        if (isDegenerated(currentTriangle)) {
            continue;
        }

        // Look for edges with the opposite vertex order. These are the edges from triangles
        // adjacent to currentTriangle and facing the same direction Edges sharing the same vertex
        // order belong to triangles facing the other way (opposite winding).
        DirectedEdge invertedEdge(currentEdge.v1, currentEdge.v0);
        auto         range = std::equal_range(
            orderedEdges.begin(),
            orderedEdges.end(),
            invertedEdge,
            DirectedEdge::CompareWithoutTriangleIndex);

        bool markVisible = true;
        bool isMaterialBoundary = true;

        for (auto adjacentEdge = range.first; adjacentEdge != range.second; ++adjacentEdge) {
            Face& adjacentTriangle = mesh.faces[adjacentEdge->triangleIdx];

            // On non-manifold meshes, if at least two of the N triangles adjacent to that
            // edge have a continuous material, do not consider this edge as a boundary.
            if (currentTriangle.getMatID() == adjacentTriangle.getMatID()) {
                isMaterialBoundary = false;
            }

            // Do not show edges between coplanar triangles if they share the same surface Ids.
            const bool coplanar
                = (computeNormal(mesh, currentTriangle) % computeNormal(mesh, adjacentTriangle))
                >= (1 - FLT_EPSILON);
            if (coplanar && !isMaterialBoundary) {
                markVisible = false;
            }
        }
        if (markVisible) {
            const auto edge
                = mesh.faces[currentEdge.triangleIdx].GetEdgeIndex(currentEdge.v0, currentEdge.v1);
            mesh.faces[currentEdge.triangleIdx].setEdgeVis(edge, EDGE_VIS);
        }
    }
}

} // namespace MeshUtils
} // namespace MAXUSD_NS_DEF