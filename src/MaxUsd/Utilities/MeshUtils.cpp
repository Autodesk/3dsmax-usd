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

#include <MaxUsd/MeshConversion/PrimvarMappingOptions.h>

#include <MeshNormalSpec.h>

namespace MAXUSD_NS_DEF {
namespace MeshUtils {

enum
{
    VertexColorChannel = 0,
    UvChannel1 = 1,
};

const Point3 DEFAULT_COLOR { 0.8f, 0.8f, 0.8f };

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

bool ToRenderMesh(
    const UsdRenderGeometry&                                 usdGeometry,
    Mesh&                                                    mesh,
    const MaxUsd::PrimvarMappingOptions&                     primvarMappingOpts,
    pxr::TfHashSet<pxr::TfToken, pxr::TfToken::HashFunctor>& unmappedPrimvars)
{
    // Make sure the subsets are well defined. Each needs topology, mapped data and a material ids,
    // so all those arrays should have matching sizes.
    if (usdGeometry.subsetTopoIndices.size() != usdGeometry.subsetPrimvarIndices.size()
        || usdGeometry.subsetTopoIndices.size() != usdGeometry.materialIds.size()) {
        return false;
    }

    // Figure out the number of faces that we will need in the output mesh. It is an
    // aggregate of the faces of all the subset geometries.
    const auto numFaces = std::accumulate(
        usdGeometry.subsetTopoIndices.begin(),
        usdGeometry.subsetTopoIndices.end(),
        0,
        [](int total, const pxr::VtVec3iArray& subset) {
            return total + static_cast<int>(subset.size());
        });
    mesh.setNumFaces(numFaces);

    const bool hasVertexColors = !usdGeometry.colors.empty();

    // Always setup vertex colors. Will be used as fallback for render if no material is bound.
    mesh.setMapSupport(VertexColorChannel);
    mesh.Map(VertexColorChannel).setNumFaces(numFaces);

    // Initialize the required channels : UVs, Vertex Colors and Normals.

    // Figure out what UV primvars we will actually load into the render mesh.
    // Build a vector of pairs..
    //   first = Index in usdGeometry.uvs
    //   second = The target 3dsMax channel for that primvar
    std::vector<std::pair<int, int>> uvPrimvarsToLoad;
    for (int i = 0; i < usdGeometry.uvs.size(); ++i) {
        const auto& uvChannel = usdGeometry.uvs[i];
        // No data... skip.
        if (uvChannel.data.empty()) {
            continue;
        }
        // Skip any auto-generated/fallback UVs that do not originate from a primvar.
        const auto primvarName = uvChannel.varname;
        if (primvarName.IsEmpty()) {
            continue;
        }

        // Is that primvar mapped to a channel? If not, skip, but keep track of this information
        // as it may be important for the caller to know.
        const auto channel = primvarMappingOpts.GetPrimvarChannelMapping(primvarName.GetString());
        if (channel == PrimvarMappingOptions::invalidChannel) {
            unmappedPrimvars.insert(uvChannel.varname);
            continue;
        }

        if (channel < 1) {
            continue;
        }

        // This primvar will be loaded into the render mesh, keep the channel mapping info for
        // later, and setup the channel.
        uvPrimvarsToLoad.push_back({ i, channel });
        mesh.setMapSupport(channel);
        mesh.Map(channel).setNumFaces(numFaces);
    }

    MeshNormalSpec* specNormals = nullptr;
    if (!usdGeometry.normals.empty()) {
        mesh.SpecifyNormals();
        specNormals = mesh.GetSpecifiedNormals();
        specNormals->SetNumFaces(numFaces);
    }

    int currentFace = 0;

    // Join all the faces from each subsets.
    for (int subsetIndex = 0; subsetIndex < usdGeometry.subsetTopoIndices.size(); ++subsetIndex) {
        const auto& pointIndices = usdGeometry.subsetTopoIndices[subsetIndex];
        const auto& primvarIndices = usdGeometry.subsetPrimvarIndices[subsetIndex];

        // Index/tri counts should always match.
        if (pointIndices.size() != primvarIndices.size()) {
            return false;
        }

        for (int i = 0; i < pointIndices.size(); ++i) {
            const auto v1 = pointIndices[i][0];
            const auto v2 = pointIndices[i][1];
            const auto v3 = pointIndices[i][2];

            mesh.faces[currentFace].setVerts(v1, v2, v3);
            mesh.faces[currentFace].setMatID(usdGeometry.materialIds[subsetIndex]);

            const auto pv1 = primvarIndices[i][0];
            const auto pv2 = primvarIndices[i][1];
            const auto pv3 = primvarIndices[i][2];

            // With the data coming from Nitrous, we know indices are the same
            // for all the channels.
            for (const auto& pvChannel : uvPrimvarsToLoad) {
                mesh.Map(pvChannel.second).tf[currentFace].setTVerts(pv1, pv2, pv3);
            }

            if (hasVertexColors) {
                mesh.Map(VertexColorChannel).tf[currentFace].setTVerts(pv1, pv2, pv3);
            } else {
                // If no displayColor, we will setup a vertex color array with a single entry,
                // filled with gray.
                mesh.Map(VertexColorChannel).tf[currentFace].setTVerts(0, 0, 0);
            }

            if (specNormals) {
                MeshNormalFace& face = specNormals->Face(currentFace);
                face.SetNormalID(0, pv1);
                face.SetNormalID(1, pv2);
                face.SetNormalID(2, pv3);
                face.SpecifyAll();
            }
            currentFace++;
        }
    }

    // Now copy the vertex buffers...
    const auto& points = usdGeometry.points;
    const auto  numVerts = points.size();
    mesh.setNumVerts(static_cast<int>(numVerts));
    std::copy_n(
        reinterpret_cast<const Point3*>(points.cdata()),
        numVerts,
        static_cast<Point3*>(mesh.verts));

    if (specNormals) {
        const auto& normals = usdGeometry.normals;
        const auto  numNormals = static_cast<int>(normals.size());
        specNormals->SetNumNormals(numNormals);
        std::copy_n(
            reinterpret_cast<const Point3*>(normals.cdata()),
            numNormals,
            specNormals->GetNormalArray());
        specNormals->SetAllExplicit();
    }

    for (const auto& entry : uvPrimvarsToLoad) {
        const auto  idx = entry.first;
        const auto& uvs = usdGeometry.uvs[idx].data;
        const auto  numUvs = static_cast<int>(uvs.size());
        const auto  channel = entry.second;
        mesh.Map(channel).setNumVerts(numUvs);
        std::copy_n(reinterpret_cast<const Point3*>(uvs.cdata()), numUvs, mesh.Map(channel).tv);
        for (int j = 0; j < numUvs; ++j) {
            // Adjust UV coordinate convention.
            mesh.Map(channel).tv[j].y = 1 - mesh.Map(channel).tv[j].y;
        }
    }

    if (hasVertexColors) {
        const auto& vcs = usdGeometry.colors;
        const auto  numVcs = static_cast<int>(vcs.size());
        mesh.Map(VertexColorChannel).setNumVerts(numVcs);
        std::copy_n(
            reinterpret_cast<const Point3*>(vcs.cdata()), numVcs, mesh.Map(VertexColorChannel).tv);
    } else {
        // Default to gray. Only need a single entry.
        mesh.Map(VertexColorChannel).setNumVerts(1);
        mesh.Map(VertexColorChannel).tv[0] = DEFAULT_COLOR;
    }

    // Delete any unsused vertices.
    mesh.DeleteIsoVerts();

    // Set all edges to visible. Some renderers don't always handle well hidden edges (for example,
    // Arnold does not like hidden edge between non-coplanar triangles.
    SetupEdgeVisibility(mesh, false);

    return true;
}

void AttachAll(
    std::vector<std::shared_ptr<Mesh>> meshes,
    std::vector<Matrix3>               transforms,
    Mesh&                              attachedMesh)
{
    if (meshes.empty()) {
        return;
    }

    // Figure the total faces/vertices count we will need.
    int totalFaceCount = 0;
    int totalVertCount = 0;
    // For uvs, we can have mulitple channels.
    // Store the uv counts for each channel in this map (map channel -> count)
    std::unordered_map<unsigned short, int> uvCounts;
    int                                     totalColorCount = 0;
    int                                     totalNormalCount = 0;

    for (int i = 0; i < meshes.size(); ++i) {
        totalFaceCount += meshes[i]->getNumFaces();
        totalVertCount += meshes[i]->getNumVerts();

        const auto meshSpecNormal = meshes[i]->GetSpecifiedNormals();
        if (meshSpecNormal) {
            totalNormalCount += meshSpecNormal->GetNumNormals();
        }

        // Figure out the number of UV verts, from each supported map channel.
        for (unsigned short channelIdx = 1; channelIdx < MAX_MESHMAPS; ++channelIdx) {
            if (meshes[i]->mapSupport(channelIdx)) {
                uvCounts[channelIdx] += meshes[i]->Map(channelIdx).getNumVerts();
            }
        }

        if (meshes[i]->mapSupport(VertexColorChannel)) {
            totalColorCount += meshes[i]->Map(VertexColorChannel).getNumVerts();
        } else {
            // Falls back to a plain color for every vertex.
            totalColorCount += meshes[i]->numVerts;
        }
    }

    // Allocate face and vertex buffers for the topology, normals, UVs and vertex colors as needed.
    attachedMesh.setNumFaces(totalFaceCount);
    attachedMesh.setNumVerts(totalVertCount);

    // Setup the normals for the attached mesh.
    attachedMesh.SpecifyNormals();
    const auto fullSpecNormals = attachedMesh.GetSpecifiedNormals();
    fullSpecNormals->SetNumFaces(totalFaceCount);
    fullSpecNormals->SetNumNormals(totalNormalCount);

    // Setup vertex colors for the attached mesh. Typically, vertex colors (from USD displayColors)
    // are used as fallback for rendering, if no material is bound.
    attachedMesh.setMapSupport(VertexColorChannel);
    auto& vcs = attachedMesh.Map(VertexColorChannel);
    vcs.setNumFaces(totalFaceCount);
    vcs.setNumVerts(totalColorCount);

    // If we have some UVs, setup UVs in the attached mesh.
    for (auto uv : uvCounts) {
        const auto channel = uv.first;
        const auto numUvs = uv.second;
        attachedMesh.setMapSupport(channel);
        // Default to planar mapping in case of missing data.
        attachedMesh.MakeMapPlanar(channel);
        auto& uvs = attachedMesh.Map(channel);
        uvs.setNumFaces(totalFaceCount);
        uvs.setNumVerts(numUvs);
    }

    // Now copy the meshes' buffers.

    // The index of the first face for the next mesh to copy.
    int firstFaceIndex = 0;
    // The vertex index offset to apply for this mesh. We need to offset by the total number
    // of vertices of the meshes previously attached.
    int vertsOffset = 0;
    int normalsOffset = 0;
    // We can have multiple uv channels, store offsets per-channel in this map (channel->offset)
    std::unordered_map<unsigned short, int> uvsOffsets;
    int                                     colorsOffset = 0;

    // Simple lambda to copy a map channel's data to a destination channel with the offset described
    // above.
    auto copyMap = [](MeshMap& map, MeshMap& destination, int vertsOffset, int firstFaceIndex) {
        std::transform(
            map.tf,
            map.tf + map.getNumFaces(),
            destination.tf + firstFaceIndex,
            [&vertsOffset](TVFace& uvFace) {
                TVFace f;
                f.t[0] = uvFace.t[0] + vertsOffset;
                f.t[1] = uvFace.t[1] + vertsOffset;
                f.t[2] = uvFace.t[2] + vertsOffset;
                return f;
            });
        std::copy(map.tv, map.tv + map.getNumVerts(), destination.tv + vertsOffset);
    };

    for (int i = 0; i < meshes.size(); ++i) {
        // Faces.
        std::transform(
            static_cast<Face*>(meshes[i]->faces),
            static_cast<Face*>(meshes[i]->faces) + meshes[i]->numFaces,
            static_cast<Face*>(attachedMesh.faces) + firstFaceIndex,
            [&vertsOffset](Face& face) {
                Face f;
                f.v[0] = face.v[0] + vertsOffset;
                f.v[1] = face.v[1] + vertsOffset;
                f.v[2] = face.v[2] + vertsOffset;

                f.setEdgeVis(0, face.getEdgeVis(0));
                f.setEdgeVis(1, face.getEdgeVis(1));
                f.setEdgeVis(2, face.getEdgeVis(2));

                f.setMatID(face.getMatID());
                return f;
            });

        // Verts.
        std::copy(
            static_cast<Point3*>(meshes[i]->verts),
            static_cast<Point3*>(meshes[i]->verts) + meshes[i]->numVerts,
            static_cast<Point3*>(attachedMesh.verts) + vertsOffset);
        transforms[i].TransformPoints(attachedMesh.verts + vertsOffset, meshes[i]->numVerts);
        vertsOffset += meshes[i]->numVerts;

        for (const auto& uv : uvCounts) {
            const auto channel = uv.first;
            auto&      uvs = attachedMesh.Map(channel);
            if (meshes[i]->mapSupport(channel)) {
                auto& map = meshes[i]->Map(channel);
                copyMap(map, uvs, uvsOffsets[channel], firstFaceIndex);
                uvsOffsets[channel] += map.getNumVerts();
            }
        }

        auto& colors = attachedMesh.Map(VertexColorChannel);
        if (meshes[i]->mapSupport(VertexColorChannel)) {
            auto& map0 = meshes[i]->Map(VertexColorChannel);
            copyMap(map0, colors, colorsOffset, firstFaceIndex);
            colorsOffset += map0.getNumVerts();
        } else {
            // This mesh has no vertex colors but the aggregate mesh overall does, so just fill up
            // with a default gray color.
            std::transform(
                static_cast<Face*>(meshes[i]->faces),
                static_cast<Face*>(meshes[i]->faces) + meshes[i]->numFaces,
                colors.tf + firstFaceIndex,
                [&colorsOffset](Face& face) {
                    TVFace f;
                    f.t[0] = face.v[0] + colorsOffset;
                    f.t[1] = face.v[1] + colorsOffset;
                    f.t[2] = face.v[2] + colorsOffset;
                    return f;
                });
            std::fill(
                colors.tv + colorsOffset,
                colors.tv + colorsOffset + meshes[i]->numVerts,
                DEFAULT_COLOR);
            colorsOffset += meshes[i]->numVerts;
        }

        const auto meshSpecNormal = meshes[i]->GetSpecifiedNormals();
        if (meshSpecNormal) {
            // Normal faces.
            std::transform(
                meshSpecNormal->GetFaceArray(),
                meshSpecNormal->GetFaceArray() + meshSpecNormal->GetNumFaces(),
                fullSpecNormals->GetFaceArray() + firstFaceIndex,
                [&normalsOffset](MeshNormalFace& normalFace) {
                    MeshNormalFace f = normalFace;
                    f.SetNormalID(0, normalFace.GetNormalID(0) + normalsOffset);
                    f.SetNormalID(1, normalFace.GetNormalID(1) + normalsOffset);
                    f.SetNormalID(2, normalFace.GetNormalID(2) + normalsOffset);
                    return f;
                });
            // Normals.
            std::copy(
                meshSpecNormal->GetNormalArray(),
                meshSpecNormal->GetNormalArray() + meshSpecNormal->GetNumNormals(),
                fullSpecNormals->GetNormalArray() + normalsOffset);
            for (int idx = normalsOffset; idx < normalsOffset + meshSpecNormal->GetNumNormals();
                 ++idx) {
                const auto allNormals = fullSpecNormals->GetNormalArray();
                allNormals[idx] = VectorTransform(transforms[i], allNormals[idx]);

                const float lenSq = LengthSquared(allNormals[idx]);
                if (lenSq && (lenSq != 1.0f)) {
                    allNormals[idx] /= std::sqrt(lenSq);
                };
            }
            normalsOffset += meshSpecNormal->GetNumNormals();
        }
        firstFaceIndex += meshes[i]->numFaces;
    }
}
} // namespace MeshUtils
} // namespace MAXUSD_NS_DEF