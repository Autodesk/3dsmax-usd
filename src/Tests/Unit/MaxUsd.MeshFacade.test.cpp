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
#include "TestUtils.h"

#include <gtest/gtest.h>

// Helper class to test MeshFacade, giving access to its protected members.
class MeshFacadeTester : public MaxUsd::MeshFacade
{
public:
    using MeshFacade::MeshFacade;
    std::shared_ptr<std::vector<int>> GetFaceIndicesCache() { return faceIndices; };
    int GetFaceVertexIndicesCountCache() { return faceVertexIndicesCountCache; }
};

TEST(MeshFacadeTest, VertexCount)
{
    // Test for MNMesh
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };
    ASSERT_EQ(8, polyFacade.VertexCount());

    // Test for Mesh
    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };
    ASSERT_EQ(8, triFacade.VertexCount());
}

TEST(MeshFacadeTest, Vertex)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };

    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(polyCube.V(i)->p, polyFacade.Vertex(i));
        ASSERT_EQ(triCube.verts[i], triFacade.Vertex(i));
    }
}

TEST(MeshFacadeTest, FaceCount)
{
    // Test for MNMesh
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };
    ASSERT_EQ(6, polyFacade.FaceCount());

    // Test for Mesh
    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };
    ASSERT_EQ(12, triFacade.FaceCount());
}

TEST(MeshFacadeTest, FaceDegree)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };

    for (int i = 0; i < 6; i++) {
        ASSERT_EQ(4, polyFacade.FaceDegree(i));
    }
    for (int i = 0; i < 12; i++) {
        ASSERT_EQ(3, triFacade.FaceDegree(i));
    }
}

TEST(MeshFacadeTest, FaceVertex)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 4; j++) {
            ASSERT_EQ(polyCube.F(i)->vtx[j], polyFacade.FaceVertex(i, j));
        }
    }

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 3; j++) {
            ASSERT_EQ(triCube.faces[i].v[j], triFacade.FaceVertex(i, j));
        }
    }
}

TEST(MeshFacadeTest, FaceIsDead)
{
    MNMesh polyCube = TestUtils::CreateCube(false);
    for (int i = 0; i < 6; i += 2) // Set even faces as DEAD.
    {
        polyCube.F(i)->SetFlag(MN_DEAD);
    }
    MaxUsd::MeshFacade polyFacade { &polyCube };

    for (int i = 0; i < 6; i++) {
        ASSERT_EQ(i % 2 == 0, polyFacade.FaceIsDead(i));
    }

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };
    ASSERT_EQ(6, triFacade.FaceCount()); // Half the faces were not converted, as they are dead.
    for (int i = 0; i < 6; i++) {
        ASSERT_EQ(false, triFacade.FaceIsDead(i));
    }
}

TEST(MeshFacadeTest, FaceIndices)
{
    MNMesh           polyCube = TestUtils::CreateCube(false);
    MeshFacadeTester polyFacade { &polyCube };

    ASSERT_EQ(nullptr, polyFacade.GetFaceIndicesCache().get());

    std::vector<int> expectedPolyIndices
        = { 0, 2, 3, 1, 4, 5, 7, 6, 0, 1, 5, 4, 1, 3, 7, 5, 3, 2, 6, 7, 2, 0, 4, 6 };
    auto polyIndices = polyFacade.FaceIndices();
    ASSERT_TRUE(std::equal(
        expectedPolyIndices.begin(),
        expectedPolyIndices.end(),
        polyIndices->begin(),
        polyIndices->end()));

    // 2nd call, fetched from a cache.
    polyIndices = polyFacade.FaceIndices();
    ASSERT_EQ(polyFacade.GetFaceIndicesCache().get(), polyIndices.get());

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MeshFacadeTester triFacade { &triCube };

    ASSERT_EQ(nullptr, triFacade.GetFaceIndicesCache().get());

    std::vector<int> expectedTriIndices = { 0, 2, 3, 3, 1, 0, 4, 5, 7, 7, 6, 4, 0, 1, 5, 5, 4, 0,
                                            1, 3, 7, 7, 5, 1, 3, 2, 6, 6, 7, 3, 2, 0, 4, 4, 6, 2 };
    auto             triIndices = triFacade.FaceIndices();
    ASSERT_TRUE(std::equal(
        expectedTriIndices.begin(),
        expectedTriIndices.end(),
        triIndices->begin(),
        triIndices->end()));

    // 2nd call, fetched from a cache.
    triIndices = triFacade.FaceIndices();
    ASSERT_EQ(triFacade.GetFaceIndicesCache().get(), triIndices.get());
}

TEST(MeshFacadeTest, FaceVertexIndicesCount)
{
    MNMesh           polyCube = TestUtils::CreateCube(false);
    MeshFacadeTester polyFacade { &polyCube };

    ASSERT_EQ(-1, polyFacade.GetFaceVertexIndicesCountCache());

    ASSERT_EQ(24, polyFacade.FaceVertexIndicesCount());
    ASSERT_EQ(24, polyFacade.GetFaceVertexIndicesCountCache());
    ASSERT_EQ(24, polyFacade.FaceVertexIndicesCount()); // from cache.

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MeshFacadeTester triFacade { &triCube };

    ASSERT_EQ(-1, triFacade.GetFaceVertexIndicesCountCache());

    ASSERT_EQ(36, triFacade.FaceVertexIndicesCount());
    ASSERT_EQ(36, triFacade.GetFaceVertexIndicesCountCache());
    ASSERT_EQ(36, triFacade.FaceVertexIndicesCount()); // from cache.
}

TEST(MeshFacadeTest, AllSmGroups)
{
    MNMesh polyCube = TestUtils::CreateCube(false);
    Mesh   triCube;
    polyCube.OutToTri(triCube);

    MaxUsd::MeshFacade polyFacade { &polyCube };
    ASSERT_EQ(0, polyFacade.GetAllSmGroups());
    // Generate smoothing groups (one per face with this angle treshold)
    polyCube.AutoSmooth(0.1f, FALSE, FALSE);
    ASSERT_EQ(7, polyFacade.GetAllSmGroups()); // 7 -> 0111 -> 3 smoothing groups needed for a cube.

    MaxUsd::MeshFacade triFacade { &triCube };
    ASSERT_EQ(0, triFacade.GetAllSmGroups());
    // Generate smoothing groups (one per face with this angle treshold)
    triCube.AutoSmooth(0.1f, FALSE, FALSE);
    ASSERT_EQ(7, triFacade.GetAllSmGroups()); // 7 -> 0111 -> 3 smoothing groups needed for a cube.
}

TEST(MeshFacadeTest, LoadNormals_Specified_Mesh)
{
    MNMesh polyCubeSpec = TestUtils::CreateCube(true /*specifyNormals*/);
    Mesh   triCubeSpec;
    polyCubeSpec.OutToTri(triCubeSpec);

    MeshFacadeTester triFacade { &triCubeSpec };

    // Before call to load normals.
    ASSERT_EQ(nullptr, triFacade.NormalIndices().get());

    triFacade.LoadNormals();

    std::vector<int> expectedIndices = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
                                         3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5 };

    auto normalIndices = triFacade.NormalIndices().get();
    ASSERT_NE(nullptr, normalIndices);

    ASSERT_TRUE(std::equal(
        expectedIndices.begin(),
        expectedIndices.end(),
        normalIndices->begin(),
        normalIndices->end()));
}

TEST(MeshFacadeTest, LoadNormals_Mesh_Compute_No_SG)
{
    MNMesh polyCubeSpec = TestUtils::CreateCube(false /*specifyNormals*/);
    Mesh   triCubeSpec;
    polyCubeSpec.OutToTri(triCubeSpec);

    MeshFacadeTester triFacade { &triCubeSpec };

    // Before call to load normals.
    ASSERT_EQ(nullptr, triFacade.NormalIndices().get());

    triFacade.LoadNormals();

    auto normalIndices = triFacade.NormalIndices().get();
    ASSERT_NE(nullptr, normalIndices);

    std::vector<int> expectedIndices
        = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
            18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35 };
    ASSERT_TRUE(std::equal(
        expectedIndices.begin(),
        expectedIndices.end(),
        normalIndices->begin(),
        normalIndices->end()));
}

TEST(MeshFacadeTest, LoadNormals_Specified_MNMesh)
{
    MNMesh polyCubeSpec = TestUtils::CreateCube(true /*specifyNormals*/);

    MeshFacadeTester polyFacade { &polyCubeSpec };

    // Before call to load normals.
    ASSERT_EQ(nullptr, polyFacade.NormalIndices().get());

    polyFacade.LoadNormals();

    std::vector<int> expectedIndices
        = { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5 };

    auto normalIndices = polyFacade.NormalIndices().get();
    ASSERT_NE(nullptr, normalIndices);

    ASSERT_TRUE(std::equal(
        expectedIndices.begin(),
        expectedIndices.end(),
        normalIndices->begin(),
        normalIndices->end()));
}

TEST(MeshFacadeTest, LoadNormals_MNMesh_Compute_No_SG)
{
    MNMesh polyCubeSpec = TestUtils::CreateCube(false /*specifyNormals*/);

    MeshFacadeTester polyFacade { &polyCubeSpec };

    // Before call to load normals.
    ASSERT_EQ(nullptr, polyFacade.NormalIndices().get());

    polyFacade.LoadNormals();

    auto normalIndices = polyFacade.NormalIndices().get();
    ASSERT_NE(nullptr, normalIndices);

    std::vector<int> expectedIndices
        = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
    ASSERT_TRUE(std::equal(
        expectedIndices.begin(),
        expectedIndices.end(),
        normalIndices->begin(),
        normalIndices->end()));
}

TEST(MeshFacadeTest, LoadNormals_MNMesh_Compute_From_SG)
{
    MNMesh polyQuad = TestUtils::CreateCube(false);

    // Generate smoothing groups.
    polyQuad.AutoSmooth(0.1f, FALSE, FALSE);

    MeshFacadeTester meshFacade { &polyQuad };
    meshFacade.LoadNormals();

    const auto normalIndices = meshFacade.NormalIndices().get();
    ASSERT_NE(nullptr, normalIndices);

    std::vector<int> expectedIndices
        = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
    ASSERT_EQ(24, normalIndices->size());
    ASSERT_TRUE(std::equal(
        expectedIndices.begin(),
        expectedIndices.end(),
        normalIndices->begin(),
        normalIndices->end()));
}

TEST(MeshFacadeTest, LoadNormals_Mesh_Compute_From_SG)
{
    MNMesh polyQuad = TestUtils::CreateCube(false);

    // Generate smoothing groups.
    polyQuad.AutoSmooth(0.1f, FALSE, FALSE);

    Mesh meshQuad;
    polyQuad.OutToTri(meshQuad);

    MeshFacadeTester meshFacade { &meshQuad };
    meshFacade.LoadNormals();

    const auto normalIndices = meshFacade.NormalIndices().get();
    ASSERT_NE(nullptr, normalIndices);

    std::vector<int> expectedIndices
        = { 0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
            12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20 };
    ASSERT_EQ(36, normalIndices->size());
    ASSERT_TRUE(std::equal(
        expectedIndices.begin(),
        expectedIndices.end(),
        normalIndices->begin(),
        normalIndices->end()));
}

TEST(MeshFacadeTest, NormalCount)
{
    // MNMesh - unspec. normals
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    polyFacade.LoadNormals();
    ASSERT_EQ(24, polyFacade.NormalCount());

    // MNMesh - spec. normals
    MNMesh             polyCubeSpec = TestUtils::CreateCube(true);
    MaxUsd::MeshFacade polyFacadeSpec { &polyCubeSpec };
    polyFacadeSpec.LoadNormals();
    ASSERT_EQ(6, polyFacadeSpec.NormalCount());

    // Mesh - unspec. normals
    Mesh triCube;
    auto polyCube2 = TestUtils::CreateCube(false);
    polyCube2.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };
    triFacade.LoadNormals();
    ASSERT_EQ(36, triFacade.NormalCount());

    // Mesh - spec. normals
    Mesh   triCubeSpec;
    MNMesh polyCubeSpec2 = TestUtils::CreateCube(true);
    polyCubeSpec2.OutToTri(triCubeSpec);
    MaxUsd::MeshFacade triFacadeSpec { &triCubeSpec };
    triFacadeSpec.LoadNormals();
    ASSERT_EQ(6, triFacadeSpec.NormalCount());
}

TEST(MeshFacadeTest, NormalData)
{
    // MNMesh - unspec. normals
    MNMesh           polyCube = TestUtils::CreateCube(false);
    MeshFacadeTester polyFacade { &polyCube };
    polyFacade.LoadNormals();
    ASSERT_EQ(polyCube.GetSpecifiedNormals()->GetNormalArray(), polyFacade.NormalData());

    // MNMesh - spec. normals
    MNMesh             polyCubeSpec = TestUtils::CreateCube(true);
    MaxUsd::MeshFacade polyFacadeSpec { &polyCubeSpec };
    polyFacadeSpec.LoadNormals();
    ASSERT_EQ(polyCubeSpec.GetSpecifiedNormals()->GetNormalArray(), polyFacadeSpec.NormalData());

    // Mesh - unspec. normals
    Mesh triCube;
    polyCube.OutToTri(triCube);
    MeshFacadeTester triFacade { &triCube };
    triFacade.LoadNormals();
    ASSERT_EQ(triCube.GetSpecifiedNormals()->GetNormalArray(), triFacade.NormalData());

    // Mesh - spec. normals
    Mesh triCubeSpec;
    polyCubeSpec.OutToTri(triCubeSpec);
    MaxUsd::MeshFacade triFacadeSpec { &triCubeSpec };
    triFacadeSpec.LoadNormals();
    ASSERT_EQ(triCubeSpec.GetSpecifiedNormals()->GetNormalArray(), triFacadeSpec.NormalData());
}

TEST(MeshFacadeTest, MapCount)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    polyCube.SetMapNum(0);
    ASSERT_EQ(0, polyFacade.MapCount());
    polyCube.SetMapNum(10);
    ASSERT_EQ(10, polyFacade.MapCount());
    polyCube.SetMapNum(MAX_MESHMAPS - 1);
    ASSERT_EQ(MAX_MESHMAPS - 1, polyFacade.MapCount());

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };
    triCube.setNumMaps(2);
    ASSERT_EQ(2, triFacade.MapCount());
    triCube.setNumMaps(10);
    ASSERT_EQ(10, triFacade.MapCount());
    triCube.setNumMaps(MAX_MESHMAPS - 1);
    ASSERT_EQ(MAX_MESHMAPS - 1, triFacade.MapCount());
}

TEST(MeshFacadeTest, MapFaceCount)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    polyCube.SetMapNum(10);
    polyCube.InitMap(5);
    ASSERT_EQ(6, polyFacade.MapFaceCount(5));
    ASSERT_EQ(0, polyFacade.MapFaceCount(1));

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };
    ASSERT_EQ(12, triFacade.MapFaceCount(5));
    ASSERT_EQ(0, triFacade.MapFaceCount(1));
}

TEST(MeshFacadeTest, MapFaceDegree)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    polyCube.SetMapNum(10);
    polyCube.InitMap(5);

    for (int i = 0; i < 6; i++) {
        ASSERT_EQ(4, polyFacade.MapFaceDegree(5, i));
    }

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };
    for (int i = 0; i < 12; i++) {
        ASSERT_EQ(3, triFacade.MapFaceDegree(5, i));
    }
}

TEST(MeshFacadeTest, MapFaceVertex)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    polyCube.SetMapNum(10);
    polyCube.InitMap(5);

    EXPECT_EQ(polyFacade.MapFaceVertex(5, 0, 0), 0);
    EXPECT_EQ(polyFacade.MapFaceVertex(5, 0, 1), 2);
    EXPECT_EQ(polyFacade.MapFaceVertex(5, 0, 2), 3);
    EXPECT_EQ(polyFacade.MapFaceVertex(5, 0, 3), 1);

    EXPECT_EQ(polyFacade.MapFaceVertex(5, 4, 0), 3);
    EXPECT_EQ(polyFacade.MapFaceVertex(5, 4, 1), 2);
    EXPECT_EQ(polyFacade.MapFaceVertex(5, 4, 2), 6);
    EXPECT_EQ(polyFacade.MapFaceVertex(5, 4, 3), 7);

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };

    EXPECT_EQ(triFacade.MapFaceVertex(5, 0, 0), 0);
    EXPECT_EQ(triFacade.MapFaceVertex(5, 0, 1), 2);
    EXPECT_EQ(triFacade.MapFaceVertex(5, 0, 2), 3);

    EXPECT_EQ(triFacade.MapFaceVertex(5, 4, 0), 0);
    EXPECT_EQ(triFacade.MapFaceVertex(5, 4, 1), 1);
    EXPECT_EQ(triFacade.MapFaceVertex(5, 4, 2), 5);
}

TEST(MeshFacadeTest, MapData)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    polyCube.SetMapNum(10);
    polyCube.InitMap(5);

    EXPECT_EQ(polyCube.M(5)->v, polyFacade.MapData(5));

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };
    EXPECT_EQ(triCube.Map(5).tv, triFacade.MapData(5));
}

TEST(MeshFacadeTest, MapDataCount)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    polyCube.SetMapNum(10);
    polyCube.InitMap(5);

    EXPECT_EQ(8, polyFacade.MapDataCount(5));

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };
    EXPECT_EQ(8, triFacade.MapDataCount(5));
}

TEST(MeshFacadeTest, FaceMaterial)
{
    MNMesh polyCube = TestUtils::CreateCube(false);

    polyCube.f[0].material = 6;
    polyCube.f[1].material = 5;
    polyCube.f[2].material = 4;
    polyCube.f[3].material = 3;
    polyCube.f[4].material = 2;
    polyCube.f[5].material = 1;

    MaxUsd::MeshFacade polyFacade { &polyCube };
    EXPECT_EQ(6, polyFacade.FaceMaterial(0));
    EXPECT_EQ(5, polyFacade.FaceMaterial(1));
    EXPECT_EQ(4, polyFacade.FaceMaterial(2));
    EXPECT_EQ(3, polyFacade.FaceMaterial(3));
    EXPECT_EQ(2, polyFacade.FaceMaterial(4));
    EXPECT_EQ(1, polyFacade.FaceMaterial(5));

    Mesh triCube;
    polyCube.OutToTri(triCube);

    MaxUsd::MeshFacade triFacade { &triCube };
    EXPECT_EQ(6, triFacade.FaceMaterial(0));
    EXPECT_EQ(6, triFacade.FaceMaterial(1));
    EXPECT_EQ(5, triFacade.FaceMaterial(2));
    EXPECT_EQ(5, triFacade.FaceMaterial(3));
    EXPECT_EQ(4, triFacade.FaceMaterial(4));
    EXPECT_EQ(4, triFacade.FaceMaterial(5));
    EXPECT_EQ(3, triFacade.FaceMaterial(6));
    EXPECT_EQ(3, triFacade.FaceMaterial(7));
    EXPECT_EQ(2, triFacade.FaceMaterial(8));
    EXPECT_EQ(2, triFacade.FaceMaterial(9));
    EXPECT_EQ(1, triFacade.FaceMaterial(10));
    EXPECT_EQ(1, triFacade.FaceMaterial(11));
}

TEST(MeshFacadeTest, HasCreaseSupport)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    ASSERT_TRUE(polyFacade.HasCreaseSupport());

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };

    ASSERT_FALSE(triFacade.HasCreaseSupport());
}

TEST(MeshFacadeTest, EdgeCount)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    EXPECT_EQ(12, polyFacade.EdgeCount());

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };

    EXPECT_EQ(0, triFacade.EdgeCount());
}

TEST(MeshFacadeTest, EdgeVertex)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };

    EXPECT_EQ(1, polyFacade.EdgeVertex(3, true));
    EXPECT_EQ(0, polyFacade.EdgeVertex(3, false));
    EXPECT_EQ(7, polyFacade.EdgeVertex(6, true));
    EXPECT_EQ(6, polyFacade.EdgeVertex(6, false));

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };

    EXPECT_EQ(0, triFacade.EdgeVertex(3, true));
    EXPECT_EQ(0, triFacade.EdgeVertex(3, false));
    EXPECT_EQ(0, triFacade.EdgeVertex(12, true));
    EXPECT_EQ(0, triFacade.EdgeVertex(12, false));
}

TEST(MeshFacadeTest, VertexCreaseData)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };
    polyCube.setVDataSupport(VDATA_CREASE);

    EXPECT_EQ(polyCube.vertexFloat(VDATA_CREASE), polyFacade.VertexCreaseData());

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };

    EXPECT_EQ(nullptr, triFacade.VertexCreaseData());
}

TEST(MeshFacadeTest, EdgeCreaseData)
{
    MNMesh             polyCube = TestUtils::CreateCube(false);
    MaxUsd::MeshFacade polyFacade { &polyCube };
    polyCube.setEDataSupport(EDATA_CREASE);

    EXPECT_EQ(polyCube.edgeFloat(EDATA_CREASE), polyFacade.EdgeCreaseData());

    Mesh triCube;
    polyCube.OutToTri(triCube);
    MaxUsd::MeshFacade triFacade { &triCube };

    EXPECT_EQ(nullptr, triFacade.EdgeCreaseData());
}

TEST(MeshFacadeTest, Transform)
{
    MNMesh polyQuad = TestUtils::CreateQuad();
    Mesh   triQuad;
    polyQuad.OutToTri(triQuad);

    // Expected before transform :
    auto p1 = Point3 { -1.f, -1.f, 0.f };
    auto p2 = Point3 { 1.f, -1.f, 0.f };
    auto p3 = Point3 { 1.f, 1.f, 0.f };
    auto p4 = Point3 { -1.f, 1.f, 0.f };

    // Expected after transform :
    auto p1t = Point3 { -1.f, -1.f, 10.f };
    auto p2t = Point3 { 1.f, -1.f, 10.f };
    auto p3t = Point3 { 1.f, 1.f, 10.f };
    auto p4t = Point3 { -1.f, 1.f, 10.f };

    Matrix3 transform;
    transform.Translate(Point3 { 0, 0, 10 });

    // MNMesh
    MaxUsd::MeshFacade polyFacade { &polyQuad };

    EXPECT_EQ(p1, polyFacade.Vertex(0));
    EXPECT_EQ(p2, polyFacade.Vertex(1));
    EXPECT_EQ(p3, polyFacade.Vertex(2));
    EXPECT_EQ(p4, polyFacade.Vertex(3));

    polyFacade.Transform(transform);

    EXPECT_EQ(p1t, polyFacade.Vertex(0));
    EXPECT_EQ(p2t, polyFacade.Vertex(1));
    EXPECT_EQ(p3t, polyFacade.Vertex(2));
    EXPECT_EQ(p4t, polyFacade.Vertex(3));

    // Mesh

    MaxUsd::MeshFacade triFacade { &triQuad };

    EXPECT_EQ(p1, triFacade.Vertex(0));
    EXPECT_EQ(p2, triFacade.Vertex(1));
    EXPECT_EQ(p3, triFacade.Vertex(2));
    EXPECT_EQ(p4, triFacade.Vertex(3));

    triFacade.Transform(transform);

    EXPECT_EQ(p1t, triFacade.Vertex(0));
    EXPECT_EQ(p2t, triFacade.Vertex(1));
    EXPECT_EQ(p3t, triFacade.Vertex(2));
    EXPECT_EQ(p4t, triFacade.Vertex(3));
}

TEST(MeshFacadeTest, MakePlanar)
{
    MNMesh polyQuad = TestUtils::CreateQuad();
    // Move one of the vertices to make the quad non-planar.
    polyQuad.v[3].p = Point3 { polyQuad.v[3].p.x, polyQuad.v[3].p.y, polyQuad.v[3].p.z + 1.0f };

    MaxUsd::MeshFacade polyFacade { &polyQuad };
    EXPECT_EQ(1, polyFacade.FaceCount());
    polyFacade.MakePlanar(1.0f);
    EXPECT_EQ(2, polyFacade.FaceCount());

    Mesh triQuad;
    polyQuad.OutToTri(triQuad);
    MaxUsd::MeshFacade triFacade { &triQuad };
    EXPECT_EQ(2, triFacade.FaceCount());
    triFacade.MakePlanar(0.1f); // no op, triangles are already planar.
    EXPECT_EQ(2, triFacade.FaceCount());
}

TEST(MeshFacadeTest, MakeConvex)
{
    MNMesh polyQuad = TestUtils::CreateQuad();
    // Move one of the vertices to make the quad concave.
    polyQuad.v[2].p = Point3 { -0.75f, -0.75f, polyQuad.v[3].p.z };

    MaxUsd::MeshFacade polyFacade { &polyQuad };
    EXPECT_EQ(1, polyFacade.FaceCount());
    polyFacade.MakeConvex();
    EXPECT_EQ(2, polyFacade.FaceCount());

    Mesh triQuad;
    polyQuad.OutToTri(triQuad);
    MaxUsd::MeshFacade triFacade { &triQuad };
    EXPECT_EQ(2, triFacade.FaceCount());
    triFacade.MakeConvex();
    EXPECT_EQ(2, triFacade.FaceCount());
}

TEST(MeshFacadeTest, Triangulate)
{
    MNMesh             polyQuad = TestUtils::CreateQuad();
    MaxUsd::MeshFacade polyFacade { &polyQuad };
    EXPECT_EQ(1, polyFacade.FaceCount());
    polyFacade.Triangulate();
    EXPECT_EQ(2, polyFacade.FaceCount());

    Mesh triQuad;
    polyQuad.OutToTri(triQuad);
    MaxUsd::MeshFacade triFacade { &triQuad };
    EXPECT_EQ(2, polyFacade.FaceCount());
    triFacade.Triangulate();
    EXPECT_EQ(2, polyFacade.FaceCount());
}

TEST(MeshFacadeTest, Triangulate_withSpecNormals)
{
    MNMesh             cube = TestUtils::CreateCube(true /*specifyNormals*/);
    MaxUsd::MeshFacade polyFacade { &cube };
    EXPECT_EQ(6, polyFacade.FaceCount());

    polyFacade.LoadNormals();
    ASSERT_NE(nullptr, polyFacade.NormalIndices());
    EXPECT_EQ(24, polyFacade.NormalIndices()->size());
    polyFacade.Triangulate();

    // Triangulate will convert to a triMesh, and clear cached data, normals will need to
    // be reloaded.
    auto normalIndices = polyFacade.NormalIndices();
    EXPECT_EQ(nullptr, normalIndices);

    // Make sure spec normals were properly remapped on the tris.
    polyFacade.LoadNormals();
    normalIndices = polyFacade.NormalIndices();
    ASSERT_EQ(36, normalIndices->size());
    pxr::VtIntArray expectedIndices = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
                                        3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5 };
    for (int j = 0; j < expectedIndices.size(); ++j) {
        EXPECT_EQ(expectedIndices[j], (*normalIndices)[j]);
    }
}

TEST(MeshFacadeTest, Cleanup)
{
    MNMesh polyCube = TestUtils::CreateCube(false);
    Mesh   triCube;
    polyCube.OutToTri(triCube);

    MaxUsd::MeshFacade polyFacade { &polyCube };
    EXPECT_EQ(6, polyFacade.FaceCount());
    // Flag a couple DEAD faces on the MNMesh which should be cleaned up.
    polyCube.F(1)->SetFlag(MN_DEAD);
    polyCube.F(3)->SetFlag(MN_DEAD);
    polyCube.F(5)->SetFlag(MN_DEAD);
    polyFacade.Cleanup();
    EXPECT_EQ(3, polyFacade.FaceCount());

    MaxUsd::MeshFacade triFacade { &triCube };
    EXPECT_EQ(12, triFacade.FaceCount());
    // Make some degenerate faces (2 equal indices), which should be cleaned up.
    triCube.faces[0].v[0] = triCube.faces[0].v[1];
    triCube.faces[6].v[1] = triCube.faces[6].v[2];
    triFacade.Cleanup();
    EXPECT_EQ(10, triFacade.FaceCount());
    // Make some illegal faces (indices out of range), should also be cleaned up.
    triCube.faces[1].v[0] = 999;
    triCube.faces[7].v[1] = -1;
    triFacade.Cleanup();
    EXPECT_EQ(8, triFacade.FaceCount());
}

TEST(MeshFacadeTest, BoundingBox)
{
    MNMesh             polyQuad = TestUtils::CreateQuad();
    MaxUsd::MeshFacade polyFacade { &polyQuad };

    Box3 expectedBoundingBox { Point3 { -1.f, -1.f, 0.f }, Point3 { 1.f, 1.f, 0.0f } };

    auto bbPoly = polyFacade.BoundingBox();
    EXPECT_NEAR(expectedBoundingBox.Min().x, bbPoly.Min().x, 0.0001f);
    EXPECT_NEAR(expectedBoundingBox.Min().y, bbPoly.Min().y, 0.0001f);
    EXPECT_NEAR(expectedBoundingBox.Min().z, bbPoly.Min().z, 0.0001f);
    EXPECT_NEAR(expectedBoundingBox.Max().x, bbPoly.Max().x, 0.0001f);
    EXPECT_NEAR(expectedBoundingBox.Max().y, bbPoly.Max().y, 0.0001f);
    EXPECT_NEAR(expectedBoundingBox.Max().z, bbPoly.Max().z, 0.0001f);

    Mesh triQuad;
    polyQuad.OutToTri(triQuad);
    MaxUsd::MeshFacade triFacade { &triQuad };

    auto bbTri = triFacade.BoundingBox();
    EXPECT_NEAR(expectedBoundingBox.Min().x, bbTri.Min().x, 0.0001f);
    EXPECT_NEAR(expectedBoundingBox.Min().y, bbTri.Min().y, 0.0001f);
    EXPECT_NEAR(expectedBoundingBox.Min().z, bbTri.Min().z, 0.0001f);
    EXPECT_NEAR(expectedBoundingBox.Max().x, bbTri.Max().x, 0.0001f);
    EXPECT_NEAR(expectedBoundingBox.Max().y, bbTri.Max().y, 0.0001f);
    EXPECT_NEAR(expectedBoundingBox.Max().z, bbTri.Max().z, 0.0001f);
}
