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
#include <MeshNormalSpec.h>
#include <mesh.h>
#include <mnmesh.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief A facade to work with the 3dsMax Mesh and MNMesh classes transparently.
 * Does some caching internally to avoid recomputing the same things. These assume
 * that the mesh will not change from the moment it is passed to the facade.
 */
class MeshFacade
{
public:
    MeshFacade() = delete;
    MaxUSDAPI explicit MeshFacade(MNMesh*, bool ownMesh = false);
    MaxUSDAPI explicit MeshFacade(Mesh*, bool ownMesh = false);

    MaxUSDAPI ~MeshFacade();

    // Mesh data :
    MaxUSDAPI int           VertexCount() const;
    MaxUSDAPI const Point3& Vertex(int i) const;
    MaxUSDAPI int           FaceCount() const;
    MaxUSDAPI int           FaceDegree(int faceIdx) const;
    MaxUSDAPI int           FaceVertex(int faceIdx, int cornerIdx) const;
    MaxUSDAPI bool          FaceIsDead(int faceIdx) const;

    MaxUSDAPI std::shared_ptr<std::vector<int>> FaceIndices();

    // Returns the sum of all face's degrees.
    MaxUSDAPI int FaceVertexIndicesCount();

    MaxUSDAPI DWORD GetAllSmGroups() const;

    MaxUSDAPI void          LoadNormals();
    MaxUSDAPI int           NormalCount() const;
    MaxUSDAPI const Point3* NormalData() const;
    MaxUSDAPI const std::shared_ptr<std::vector<int>> NormalIndices() const;

    // Map channel data :
    MaxUSDAPI int           MapCount() const;
    MaxUSDAPI int           MapFaceCount(int channel) const;
    MaxUSDAPI int           MapFaceDegree(int channel, int faceIdx) const;
    MaxUSDAPI int           MapFaceVertex(int channel, int faceIdx, int cornerIdx) const;
    MaxUSDAPI const Point3* MapData(int channel) const;
    MaxUSDAPI int           MapDataCount(int channel) const;

    // Material assignment.
    MaxUSDAPI MtlID FaceMaterial(int faceIdx) const;

    // Vertex and Edge creasing :
    MaxUSDAPI bool HasCreaseSupport() const;
    // Currently only properly implemented for MNMesh, Mesh doesn't maintain an
    // edge list (although it can be computed separately).
    MaxUSDAPI int          EdgeCount() const;
    MaxUSDAPI int          EdgeVertex(int edgeIdx, bool start) const;
    MaxUSDAPI const float* VertexCreaseData() const;
    MaxUSDAPI const float* EdgeCreaseData() const;

    MaxUSDAPI void Transform(Matrix3& transform) const;
    MaxUSDAPI void MakePlanar(float planarTresh) const;
    MaxUSDAPI void MakeConvex() const;
    MaxUSDAPI void Cleanup() const;
    MaxUSDAPI Box3 BoundingBox() const;
    MaxUSDAPI void Triangulate();

protected:
    void ClearCachedData();

    MNMesh* polyMesh = nullptr;
    Mesh*   triMesh = nullptr;
    bool    ownMesh = false;

    // Cache..
    std::shared_ptr<std::vector<int>> faceIndices;
    std::shared_ptr<std::vector<int>> normalsIndices;
    int                               faceVertexIndicesCountCache = -1;
};

} // namespace MAXUSD_NS_DEF