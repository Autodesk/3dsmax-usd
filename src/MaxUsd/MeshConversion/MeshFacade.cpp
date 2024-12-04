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
#include "MeshFacade.h"

#include <MNNormalSpec.h>
#include <MeshNormalSpec.h>
#include <meshadj.h>
#include <mnmesh.h>

namespace MAXUSD_NS_DEF {

MeshFacade::MeshFacade(MNMesh* polyMesh, bool ownMesh)
{
    this->polyMesh = polyMesh;
    this->ownMesh = ownMesh;
}

MeshFacade::MeshFacade(Mesh* triMesh, bool ownMesh)
{
    this->triMesh = triMesh;
    this->ownMesh = ownMesh;
}

void MeshFacade::MakePlanar(float planarTresh) const
{
    if (triMesh) {
        return; // No op.
    }
    return polyMesh->MakePlanar(planarTresh);
}

void MeshFacade::MakeConvex() const
{
    if (triMesh) {
        return; // No op
    }
    return polyMesh->MakeConvex();
}

void MeshFacade::Cleanup() const
{
    if (triMesh) {
        triMesh->RemoveDegenerateFaces();
        triMesh->RemoveIllegalFaces();
        return;
    }
    return polyMesh->CollapseDeadStructs();
}

Box3 MeshFacade::BoundingBox() const
{
    if (triMesh) {
        return triMesh->getBoundingBox();
    }
    return polyMesh->getBoundingBox();
}

MeshFacade::~MeshFacade()
{
    if (!ownMesh) {
        return;
    }

    delete triMesh;
    delete polyMesh;
}

int MeshFacade::VertexCount() const
{
    if (triMesh) {
        return triMesh->numVerts;
    }
    return polyMesh->VNum();
}

const Point3& MeshFacade::Vertex(int i) const
{
    if (triMesh) {
        return triMesh->verts[i];
    }
    return polyMesh->V(i)->p;
}

int MeshFacade::FaceCount() const
{
    if (triMesh) {
        return triMesh->numFaces;
    }
    return polyMesh->FNum();
}

int MeshFacade::FaceDegree(int faceIdx) const
{
    if (triMesh) {
        return 3;
    }
    return polyMesh->F(faceIdx)->deg;
}

int MeshFacade::FaceVertex(int faceIdx, int cornerIdx) const
{
    if (triMesh) {
        return triMesh->faces[faceIdx].v[cornerIdx];
    }
    return polyMesh->F(faceIdx)->vtx[cornerIdx];
}

bool MeshFacade::FaceIsDead(int faceIdx) const
{
    if (triMesh) {
        return false; // No op.
    }
    return polyMesh->F(faceIdx)->GetFlag(MN_DEAD);
}

std::shared_ptr<std::vector<int>> MeshFacade::FaceIndices()
{
    if (!faceIndices) {
        faceIndices = std::make_shared<std::vector<int>>();
        faceIndices->reserve(FaceVertexIndicesCount());
        for (int i = 0; i < FaceCount(); ++i) {
            for (int j = 0; j < FaceDegree(i); ++j) {
                faceIndices->push_back(FaceVertex(i, j));
            }
        }
    }
    return faceIndices;
}

MtlID MeshFacade::FaceMaterial(int faceIdx) const
{
    if (triMesh) {
        return triMesh->faces[faceIdx].getMatID();
    }
    return polyMesh->F(faceIdx)->material;
}

int MeshFacade::FaceVertexIndicesCount()
{
    if (faceVertexIndicesCountCache != -1) {
        return faceVertexIndicesCountCache;
    }
    if (triMesh) {
        faceVertexIndicesCountCache = triMesh->numFaces * 3; // Triangles : always 3 sides!
    } else {
        faceVertexIndicesCountCache = std::accumulate<MNFace*, int, int(int, MNFace&)>(
            polyMesh->f, polyMesh->f + polyMesh->FNum(), 0, [](int total, MNFace& face) {
                return total + face.deg;
            });
    }
    return faceVertexIndicesCountCache;
}

const float* MeshFacade::VertexCreaseData() const
{
    if (triMesh) {
        return nullptr;
    }
    return polyMesh->vertexFloat(VDATA_CREASE);
}

const float* MeshFacade::EdgeCreaseData() const
{
    if (triMesh) {
        return nullptr;
    }
    return polyMesh->edgeFloat(EDATA_CREASE);
}

DWORD MeshFacade::GetAllSmGroups() const
{
    if (triMesh) {
        DWORD used = 0;
        for (int i = 0; i < FaceCount(); i++) {
            used |= triMesh->faces[i].smGroup;
        }
        return used;
    }
    return polyMesh->GetAllSmGroups();
}

void MeshFacade::LoadNormals()
{
    // Mesh :

    if (triMesh) {
        MeshNormalSpec* normalSpec = triMesh->GetSpecifiedNormals();
        // If the normals are not specified, generate them from the smoothing groups.
        if (!normalSpec) {
            triMesh->SpecifyNormals();
            normalSpec = triMesh->GetSpecifiedNormals();
            normalSpec->SetParent(triMesh);
        } else {
            // It's easy for modifiers for mess up and leave the normals flag in a bad state,
            // which can lead to corrupt data, for safety we have no choice but to force a
            // recompute.
            normalSpec->SetFlag(MESH_NORMAL_NORMALS_BUILT, FALSE);
            normalSpec->SetFlag(MESH_NORMAL_NORMALS_COMPUTED, FALSE);
        }
        normalSpec->CheckNormals();

        // Load the normal indices from the specified normals faces.
        normalsIndices = std::make_shared<std::vector<int>>();
        normalsIndices->reserve(normalSpec->GetNumFaces() * 3);
        normalSpec->CheckNormals();
        for (int i = 0; i < normalSpec->GetNumFaces(); ++i) {
            for (int j = 0; j < 3; ++j) {
                normalsIndices->push_back(normalSpec->Face(i).GetNormalID(j));
            }
        }
        return;
    }

    // MNMesh :

    MNNormalSpec* normalSpec = polyMesh->GetSpecifiedNormals();
    // If the normals are not specified, generate them from the smoothing groups.
    if (!normalSpec) {
        polyMesh->SpecifyNormals();
        normalSpec = polyMesh->GetSpecifiedNormals();
        normalSpec->SetParent(polyMesh);
    } else {
        // It's easy for modifiers for mess up and leave the normals flag in a bad state,
        // which can lead to corrupt data, for safety we have no choice but to force a recompute.
        normalSpec->SetFlag(MNNORMAL_NORMALS_BUILT, FALSE);
        normalSpec->SetFlag(MNNORMAL_NORMALS_COMPUTED, FALSE);
    }
    normalSpec->CheckNormals();

    // Load the normal indices from the specified normals faces.
    normalsIndices = std::make_shared<std::vector<int>>();
    normalsIndices->reserve(FaceVertexIndicesCount());
    normalSpec->CheckNormals();
    for (int i = 0; i < normalSpec->GetNumFaces(); ++i) {
        for (int j = 0; j < normalSpec->Face(i).GetDegree(); ++j) {
            normalsIndices->push_back(normalSpec->Face(i).GetNormalID(j));
        }
    }
}

const Point3* MeshFacade::NormalData() const
{
    if (triMesh) {
        const auto normalSpec = triMesh->GetSpecifiedNormals();
        if (normalSpec) {
            return normalSpec->GetNormalArray();
        }
    }

    const auto mnNormalSpec = polyMesh->GetSpecifiedNormals();
    if (mnNormalSpec) {
        return mnNormalSpec->GetNormalArray();
    }

    return nullptr;
}

const std::shared_ptr<std::vector<int>> MeshFacade::NormalIndices() const { return normalsIndices; }

int MeshFacade::NormalCount() const
{
    if (triMesh) {
        const auto specNormals = triMesh->GetSpecifiedNormals();
        if (specNormals) {
            return specNormals->GetNumNormals();
        }
    }
    const auto specNormals = polyMesh->GetSpecifiedNormals();
    if (specNormals) {
        return specNormals->GetNumNormals();
    }
    return 0;
}

int MeshFacade::MapCount() const
{
    if (triMesh) {
        return triMesh->getNumMaps();
    }
    return polyMesh->MNum();
}

int MeshFacade::MapFaceCount(int channel) const
{
    if (triMesh) {
        const MeshMap& map = triMesh->Map(channel);
        return map.fnum;
    }
    const auto map = polyMesh->M(channel);
    return map->FNum();
}

int MeshFacade::MapFaceDegree(int channel, int faceIdx) const
{
    if (triMesh) {
        return 3;
    }
    const auto map = polyMesh->M(channel);
    return map->F(faceIdx)->deg;
}

int MeshFacade::MapFaceVertex(int channel, int faceIdx, int cornerIdx) const
{
    if (triMesh) {
        MeshMap& map = triMesh->Map(channel);
        return map.tf[faceIdx].getTVert(cornerIdx);
    }
    const auto map = polyMesh->M(channel);
    return map->F(faceIdx)->tv[cornerIdx];
}

const Point3* MeshFacade::MapData(int channel) const
{
    if (triMesh) {
        MeshMap& map = triMesh->Map(channel);
        return map.tv;
    }
    const auto map = polyMesh->M(channel);
    return map->v;
}

int MeshFacade::MapDataCount(int channel) const
{
    if (triMesh) {
        MeshMap& map = triMesh->Map(channel);
        return map.getNumVerts();
    }
    const auto map = polyMesh->M(channel);
    return map->VNum();
}

int MeshFacade::EdgeCount() const
{
    if (triMesh) {
        // Not implemented.
        return 0;
    }
    return polyMesh->nume;
}

int MeshFacade::EdgeVertex(int edgeIdx, bool start) const
{
    if (triMesh) {
        // Not implemented.
        return 0;
    }

    const auto edge = polyMesh->e[edgeIdx];

    if (start) {
        return edge.v1;
    }
    return edge.v2;
}

void MeshFacade::Triangulate()
{
    if (triMesh) {
        return; // no op, already triangles!
    }

    // MNMesh::Triangulate() doesnt handle spec normals correctly.
    // Instead, swap to a TriMesh behind the scenes.
    triMesh = new Mesh();
    polyMesh->OutToTri(*triMesh);
    // If we own current polyMesh, delete it.
    if (ownMesh) {
        delete polyMesh;
    }
    polyMesh = nullptr;
    // We own the newly generated mesh, make sure its deleted later.
    ownMesh = true;

    // Make sure we don't hold own to any cache.
    ClearCachedData();
}

void MeshFacade::ClearCachedData()
{
    faceIndices = nullptr;
    normalsIndices = nullptr;
    faceVertexIndicesCountCache = -1;
}

void MeshFacade::Transform(Matrix3& transform) const
{
    // Inspired from the MNMesh::Transform()
    if (triMesh) {
        for (int i = 0; i < triMesh->getNumVerts(); i++) {
            triMesh->verts[i] = triMesh->verts[i] * transform;
        }
        const auto specNormals = triMesh->GetSpecifiedNormals();
        if (specNormals) {
            specNormals->Transform(transform);
        }
        triMesh->InvalidateGeomCache();
        triMesh->SetFlag(MESH_CACHEINVALID);
        return;
    }
    polyMesh->Transform(transform);
}

bool MeshFacade::HasCreaseSupport() const { return polyMesh != nullptr; }

} // namespace MAXUSD_NS_DEF