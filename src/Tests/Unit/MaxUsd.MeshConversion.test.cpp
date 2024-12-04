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

#include <MaxUsd/MeshConversion/MaxMeshConversionOptions.h>
#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/MeshConversion/PrimvarMappingOptions.h>

#include <gtest/gtest.h>
#include <max.h>

TEST(MeshConversionTest, SimpleCubeRoundTrip)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/cube");
    MNMesh     cube = TestUtils::CreateCube(false);

    // Test that the converted USD mesh equals the original mesh (MNMesh -> USD).
    MaxUsd::MeshConverter            converter;
    pxr::UsdGeomMesh                 usdMesh;
    MaxUsd::MaxMeshConversionOptions options;
    std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
    options.SetNormalsMode(MaxUsd::MaxMeshConversionOptions::NormalsMode::None);
    MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &cube },
        stage,
        path,
        options,
        usdMesh,
        pxr::UsdTimeCode::Default(),
        materialIdToFacesMap,
        false,
        intervals);
    TestUtils::CompareUsdAndMaxMeshes(cube, usdMesh);

    // Test that the re-converted MNMesh mesh equals the USD mesh (USD -> MNMesh)
    MNMesh                     reimportedMesh;
    std::map<int, std::string> channelNames;
    converter.ConvertToMNMesh(
        usdMesh, reimportedMesh, MaxUsd::PrimvarMappingOptions {}, channelNames);
    TestUtils::CompareUsdAndMaxMeshes(reimportedMesh, usdMesh);
}

// Test conversion of non-planar MNFace to USD. In this case, the face will be
// "made planar", i.e. it will be split into multiple faces.
TEST(MeshConversionTest, NonPlanarFaceToUsd)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/non_planar_face");

    MNMesh nonPlanarFaceMesh;
    nonPlanarFaceMesh.setNumFaces(1);
    nonPlanarFaceMesh.setNumVerts(4);
    nonPlanarFaceMesh.V(0)->p = Point3(0, 0, 0);
    nonPlanarFaceMesh.V(1)->p = Point3(0, 1, 0);
    nonPlanarFaceMesh.V(2)->p = Point3(1, 1, 0);
    nonPlanarFaceMesh.V(3)->p = Point3(1, 0, 1); // One of the points off the plane.
    nonPlanarFaceMesh.F(0)->SetDeg(4);
    nonPlanarFaceMesh.F(0)->vtx[0] = 0;
    nonPlanarFaceMesh.F(0)->vtx[1] = 1;
    nonPlanarFaceMesh.F(0)->vtx[2] = 2;
    nonPlanarFaceMesh.F(0)->vtx[3] = 3;
    nonPlanarFaceMesh.FillInMesh();

    MaxUsd::MeshConverter            converter;
    pxr::UsdGeomMesh                 usdMesh;
    MaxUsd::MaxMeshConversionOptions options;
    options.SetPreserveEdgeOrientation(true);
    std::map<MtlID, pxr::VtIntArray>              materialIdToFacesMap;
    MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &nonPlanarFaceMesh },
        stage,
        path,
        options,
        usdMesh,
        pxr::UsdTimeCode::Default(),
        materialIdToFacesMap,
        false,
        intervals);

    pxr::VtIntArray faceVertexCount;
    usdMesh.GetFaceVertexCountsAttr().Get(&faceVertexCount);

    EXPECT_EQ(faceVertexCount.size(), 2);

    // We expect that the non-planar face was split into 2 planar faces.
    EXPECT_EQ(faceVertexCount[0], 3);
    EXPECT_EQ(faceVertexCount[1], 3);

    pxr::VtIntArray faceIndices;
    usdMesh.GetFaceVertexIndicesAttr().Get(&faceIndices);
    EXPECT_EQ(faceIndices[0], 0);
    EXPECT_EQ(faceIndices[1], 2);
    EXPECT_EQ(faceIndices[2], 3);
    EXPECT_EQ(faceIndices[3], 0);
    EXPECT_EQ(faceIndices[4], 1);
    EXPECT_EQ(faceIndices[5], 2);

    // Vertices should be the same even though the faces are different.
    TestUtils::CompareVertices(nonPlanarFaceMesh, usdMesh);
}

// Test conversion of concave MNFaces. Currently we do not export concave faces
// as doing this is a source a trouble in both Max and USD view. Instead those faces
// are "made convex" by splitting the face into multiple convex faces.
TEST(MeshConversionTest, ConcaveFaceToUsd)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/concave_face");

    MNMesh nonPlanarFaceMesh;
    nonPlanarFaceMesh.setNumFaces(1);
    nonPlanarFaceMesh.setNumVerts(4);

    nonPlanarFaceMesh.V(0)->p = Point3(0, 0, 0);
    nonPlanarFaceMesh.V(1)->p = Point3(0, 1, 0);
    nonPlanarFaceMesh.V(2)->p = Point3(.1f, 0.1f, 0.f); // Makes it concave
    nonPlanarFaceMesh.V(3)->p = Point3(1, 0, 0);

    nonPlanarFaceMesh.F(0)->SetDeg(4);
    nonPlanarFaceMesh.F(0)->vtx[0] = 0;
    nonPlanarFaceMesh.F(0)->vtx[1] = 1;
    nonPlanarFaceMesh.F(0)->vtx[2] = 2;
    nonPlanarFaceMesh.F(0)->vtx[3] = 3;

    nonPlanarFaceMesh.FillInMesh();

    MaxUsd::MeshConverter                         converter;
    pxr::UsdGeomMesh                              usdMesh;
    MaxUsd::MaxMeshConversionOptions              options;
    std::map<MtlID, pxr::VtIntArray>              materialIdToFacesMap;
    MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &nonPlanarFaceMesh },
        stage,
        path,
        options,
        usdMesh,
        pxr::UsdTimeCode::Default(),
        materialIdToFacesMap,
        false,
        intervals);

    pxr::VtIntArray faceVertexCount;
    usdMesh.GetFaceVertexCountsAttr().Get(&faceVertexCount);

    // We expect that the non-planar face was split into 2 planar faces.
    EXPECT_EQ(faceVertexCount.size(), 2);
    EXPECT_EQ(faceVertexCount[0], 3);
    EXPECT_EQ(faceVertexCount[1], 3);

    pxr::VtIntArray faceIndices;
    usdMesh.GetFaceVertexIndicesAttr().Get(&faceIndices);
    EXPECT_EQ(faceIndices[0], 0);
    EXPECT_EQ(faceIndices[1], 1);
    EXPECT_EQ(faceIndices[2], 2);
    EXPECT_EQ(faceIndices[3], 2);
    EXPECT_EQ(faceIndices[4], 3);
    EXPECT_EQ(faceIndices[5], 0);

    // Vertices should be the same even though the faces are different.
    TestUtils::CompareVertices(nonPlanarFaceMesh, usdMesh);
}

// Test importing concave faces from USD.
TEST(MeshConversionTest, ConcaveFaceFromUsd)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/concave_face");

    const auto      usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
    pxr::VtIntArray faceVertexCount;
    faceVertexCount.reserve(1);
    faceVertexCount.push_back(4);
    usdMesh.CreateFaceVertexCountsAttr().Set(faceVertexCount);

    pxr::VtIntArray faceIndices;
    faceIndices.reserve(4);
    faceIndices.push_back(0);
    faceIndices.push_back(1);
    faceIndices.push_back(2);
    faceIndices.push_back(3);
    usdMesh.CreateFaceVertexIndicesAttr().Set(faceIndices);

    pxr::VtVec3fArray vertices;
    vertices.reserve(4);
    vertices.push_back(pxr::GfVec3f(0, 0, 0));
    vertices.push_back(pxr::GfVec3f(0, 1, 0));
    vertices.push_back(pxr::GfVec3f(.1f, 0.1f, 0.f));
    vertices.push_back(pxr::GfVec3f(1, 0, 0));
    usdMesh.CreatePointsAttr().Set(vertices);

    MNMesh                     maxMesh;
    MaxUsd::MeshConverter      converter;
    std::map<int, std::string> channelNames;
    converter.ConvertToMNMesh(usdMesh, maxMesh, MaxUsd::PrimvarMappingOptions {}, channelNames);
    TestUtils::CompareUsdAndMaxMeshes(maxMesh, usdMesh);
}

// Test leftHanded face orientation (rightHand is the default).
// From the USD docs : "Orientation specifies whether the gprim's surface normal
// should be computed using the right hand rule, or the left hand rule."
TEST(MeshConversionTest, LeftHandedFaceOrientation)
{
    const auto                                    stage = pxr::UsdStage::CreateInMemory();
    const auto                                    path = pxr::SdfPath("/object");
    auto                                          quad = TestUtils::CreateQuad();
    MaxUsd::MeshConverter                         converter;
    pxr::UsdGeomMesh                              usdMesh;
    MaxUsd::MaxMeshConversionOptions              options;
    std::map<MtlID, pxr::VtIntArray>              materialIdToFacesMap;
    MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &quad },
        stage,
        path,
        options,
        usdMesh,
        pxr::UsdTimeCode::Default(),
        materialIdToFacesMap,
        false,
        intervals);
    usdMesh.CreateOrientationAttr().Set(pxr::UsdGeomTokens->leftHanded);

    // Max works with a right handed coordinate system, and so uses the right hand rule
    // to compute surface normals. To support USD geometries which explicitly specify
    // a left handed orientation, we flip the faces on import (through the vert order).
    MNMesh                     reconvertedQuad;
    std::map<int, std::string> channelNames;
    converter.ConvertToMNMesh(
        usdMesh, reconvertedQuad, MaxUsd::PrimvarMappingOptions {}, channelNames);

    // Reconverted face should equal the original face, flipped.
    quad.F(0)->Flip();
    const auto degree = reconvertedQuad.F(0)->deg;
    ASSERT_EQ(reconvertedQuad.F(0)->deg, degree);
    for (int i = 0; i < degree; i++) {
        EXPECT_EQ(quad.F(0)->vtx[i], reconvertedQuad.F(0)->vtx[i]);
    }
}

// Test that "dead"/degenerate faces are not exported to USD.
TEST(MeshConversionTest, DegenerateFacesToUsd)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/degenerate_face");

    MNMesh nonPlanarFaceMesh;
    nonPlanarFaceMesh.setNumFaces(2);
    nonPlanarFaceMesh.setNumVerts(3);
    nonPlanarFaceMesh.V(0)->p = Point3(0, 0, 0);
    nonPlanarFaceMesh.V(1)->p = Point3(0, 1, 0);
    nonPlanarFaceMesh.V(2)->p = Point3(0, 1, 1);

    // Dead/Degenerate face :
    nonPlanarFaceMesh.F(0)->SetDeg(0);
    nonPlanarFaceMesh.F(0)->SetFlag(MN_DEAD);

    // Valid face  :
    nonPlanarFaceMesh.F(1)->SetDeg(3);
    nonPlanarFaceMesh.F(1)->vtx[0] = 0;
    nonPlanarFaceMesh.F(1)->vtx[1] = 1;
    nonPlanarFaceMesh.F(1)->vtx[3] = 2;

    nonPlanarFaceMesh.FillInMesh();
    MaxUsd::MeshConverter                         converter;
    pxr::UsdGeomMesh                              usdMesh;
    MaxUsd::MaxMeshConversionOptions              options;
    std::map<MtlID, pxr::VtIntArray>              materialIdToFacesMap;
    MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &nonPlanarFaceMesh },
        stage,
        path,
        options,
        usdMesh,
        pxr::UsdTimeCode::Default(),
        materialIdToFacesMap,
        false,
        intervals);
    pxr::VtIntArray faceVertexCount;
    usdMesh.GetFaceVertexCountsAttr().Get(&faceVertexCount);

    EXPECT_EQ(faceVertexCount.size(), 1);
}

// Test faces with less than 3 vertices are not imported from USD.
TEST(MeshConversionTest, DegenerateFacesFromUsd)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/degenerate_faces");
    const auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));

    pxr::VtIntArray faceVertexCount;
    faceVertexCount.reserve(3);
    // Invalid faces :
    faceVertexCount.push_back(0);
    faceVertexCount.push_back(2);
    // Valid face :
    faceVertexCount.push_back(3);
    usdMesh.CreateFaceVertexCountsAttr().Set(faceVertexCount);

    pxr::VtIntArray faceIndices;
    faceIndices.reserve(5);
    // Invalid face
    faceIndices.push_back(0);
    faceIndices.push_back(1);
    // Valid face
    faceIndices.push_back(0);
    faceIndices.push_back(1);
    faceIndices.push_back(2);
    usdMesh.CreateFaceVertexIndicesAttr().Set(faceIndices);

    pxr::VtVec3fArray vertices;
    vertices.reserve(3);
    vertices.push_back(pxr::GfVec3f(0, 0, 0));
    vertices.push_back(pxr::GfVec3f(0, 1, 0));
    vertices.push_back(pxr::GfVec3f(0, 1, 1));
    usdMesh.CreatePointsAttr().Set(vertices);

    MNMesh                     maxMesh;
    MaxUsd::MeshConverter      converter;
    std::map<int, std::string> channelNames;
    converter.ConvertToMNMesh(usdMesh, maxMesh, MaxUsd::PrimvarMappingOptions {}, channelNames);

    EXPECT_EQ(maxMesh.FNum(), 1);
    EXPECT_EQ(maxMesh.VNum(), 3);
    EXPECT_EQ(maxMesh.v[0].p, Point3(0, 0, 0));
    EXPECT_EQ(maxMesh.v[1].p, Point3(0, 1, 0));
    EXPECT_EQ(maxMesh.v[2].p, Point3(0, 1, 1));
}

TEST(MeshConversionTest, UnconnectedVertices)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/unconnected_vertices");
    const auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));

    pxr::VtIntArray faceVertexCount;
    faceVertexCount.reserve(3);
    faceVertexCount.push_back(3);
    usdMesh.CreateFaceVertexCountsAttr().Set(faceVertexCount);

    pxr::VtIntArray faceIndices;
    faceIndices.push_back(0);
    faceIndices.push_back(2);
    faceIndices.push_back(3);
    usdMesh.CreateFaceVertexIndicesAttr().Set(faceIndices);

    pxr::VtVec3fArray vertices;
    vertices.reserve(4);
    vertices.push_back(pxr::GfVec3f(1, 0, 0));
    vertices.push_back(pxr::GfVec3f(2, 2, 2)); // This vertex is not connected to any face.
    vertices.push_back(pxr::GfVec3f(0, 1, 0));
    vertices.push_back(pxr::GfVec3f(0, 0, 1));
    usdMesh.CreatePointsAttr().Set(vertices);

    MNMesh                     maxMesh;
    MaxUsd::MeshConverter      converter;
    std::map<int, std::string> channelNames;
    converter.ConvertToMNMesh(usdMesh, maxMesh, MaxUsd::PrimvarMappingOptions {}, channelNames);

    EXPECT_EQ(maxMesh.VNum(), 3);
    EXPECT_EQ(maxMesh.v[0].p[0], 1);
    EXPECT_EQ(maxMesh.v[1].p[1], 1);
    EXPECT_EQ(maxMesh.v[2].p[2], 1);
}

TEST(MeshConversionTest, PreserveEdgeOrientation)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/plane");

    // Create a square plane of 10 segments by 10.
    MNMesh    plane = TestUtils::CreatePlane(10, 10);
    const int NB_FACES = 100, NB_VERTS = 121;
    ASSERT_TRUE(plane.FNum() == NB_FACES && plane.VNum() == NB_VERTS);

    Matrix3 transformMat = ScaleMatrix(Point3(10, 10, 0));
    plane.Transform(transformMat);

    // Move a couple of vertices on the z axis
    plane.V(31)->p += Point3(0.f, 0.f, 2.52511f);
    plane.V(32)->p += Point3(0.f, 0.f, 2.52511f);
    plane.V(41)->p += Point3(0.f, 0.f, 2.52511f);
    plane.V(42)->p += Point3(0.f, 0.f, 2.52511f);
    plane.V(43)->p += Point3(0.f, 0.f, 2.52511f);
    plane.V(44)->p += Point3(0.f, 0.f, 2.52511f);
    plane.V(51)->p += Point3(0.f, 0.f, -4.40147f);
    plane.V(53)->p += Point3(0.f, 0.f, 2.52511f);
    plane.V(54)->p += Point3(0.f, 0.f, 2.52511f);
    plane.V(62)->p += Point3(0.f, 0.f, -4.40147f);
    plane.V(63)->p += Point3(0.f, 0.f, -4.40147f);
    plane.V(64)->p += Point3(0.f, 0.f, 3.95149f);
    plane.V(65)->p += Point3(0.f, 0.f, 3.95149f);
    plane.V(74)->p += Point3(0.f, 0.f, -4.40147f);
    plane.V(75)->p += Point3(0.f, 0.f, -4.40147f);
    plane.V(76)->p += Point3(0.f, 0.f, 3.95149f);
    plane.V(77)->p += Point3(0.f, 0.f, 3.95149f);
    plane.V(84)->p += Point3(0.f, 0.f, 4.54109f);
    plane.V(85)->p += Point3(0.f, 0.f, 4.54109f);
    plane.V(87)->p += Point3(0.f, 0.f, -2.66383f);
    plane.V(88)->p += Point3(0.f, 0.f, 1.28767f);
    plane.V(95)->p += Point3(0.f, 0.f, 4.54109f);
    plane.V(96)->p += Point3(0.f, 0.f, 4.54109f);
    plane.V(97)->p += Point3(0.f, 0.f, -2.66383f);
    plane.V(98)->p += Point3(0.f, 0.f, -2.66383f);
    plane.V(99)->p += Point3(0.f, 0.f, -2.66383f);
    plane.V(106)->p += Point3(0.f, 0.f, 4.54109f);
    plane.V(107)->p += Point3(0.f, 0.f, 4.54109f);
    plane.V(108)->p += Point3(0.f, 0.f, 1.87727f);
    plane.V(109)->p += Point3(0.f, 0.f, -2.66383f);
    plane.V(110)->p += Point3(0.f, 0.f, -2.66383f);
    plane.V(120)->p += Point3(0.f, 0.f, -2.66383f);

    // Test that the converted USD mesh equals the original mesh (MNMesh -> USD).
    MaxUsd::MeshConverter            converter;
    pxr::UsdGeomMesh                 usdMeshPreserve, usdMeshDontPreserve;
    MaxUsd::MaxMeshConversionOptions options;
    std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
    pxr::VtIntArray                  faceVertexCounts;

    // Convert to USD without preserving edge orientation
    options.SetPreserveEdgeOrientation(false);
    MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &plane },
        stage,
        path,
        options,
        usdMeshDontPreserve,
        pxr::UsdTimeCode::Default(),
        materialIdToFacesMap,
        false,
        intervals);

    // Compare result when not preserving
    usdMeshDontPreserve.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
    EXPECT_EQ(faceVertexCounts.size(), NB_FACES);

    // Convert to USD preserving edge orientation
    options.SetPreserveEdgeOrientation(true);
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &plane },
        stage,
        path,
        options,
        usdMeshPreserve,
        pxr::UsdTimeCode::Default(),
        materialIdToFacesMap,
        false,
        intervals);

    // Compare result when preserving (some faces will have triangulated, creating more faces and
    // vertices on exported mesh)
    usdMeshPreserve.GetFaceVertexCountsAttr().Get(&faceVertexCounts);
    EXPECT_EQ(faceVertexCounts.size(), 128);

#ifdef TEST_OUTPUT_USD_FILES
    std::string exportPath = TestUtils::GetOutputDirectory();
    exportPath.append("/MeshConversionTest/PreserveEdgeOrientation.usda");
    stage->Export(exportPath);
#endif
}

TEST(MeshConversionTest, TimeSampledUsdMesh)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/animatedMesh");

    // Create and populate a Usd Mesh prim at different timeCodes with different geometry.
    auto animatedMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
    auto setupAtTimeCode = [&](const pxr::VtVec3fArray& points,
                               const pxr::VtIntArray&   faceCounts,
                               const pxr::VtIntArray&   indices,
                               const pxr::UsdTimeCode&  timeCode) {
        animatedMesh.CreatePointsAttr().Set(points, timeCode);
        animatedMesh.CreateFaceVertexCountsAttr().Set(faceCounts, timeCode);
        animatedMesh.CreateFaceVertexIndicesAttr().Set(indices, timeCode);
    };

    // Default timeCode.
    const pxr::VtVec3fArray pointsDefault
        = { { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f } };
    const pxr::VtIntArray faceCountDefault = { 3 };
    const pxr::VtIntArray indicesDefault = { 0, 1, 2 };
    setupAtTimeCode(pointsDefault, faceCountDefault, indicesDefault, pxr::UsdTimeCode::Default());

    // TimeCode 1.
    const pxr::VtVec3fArray points1
        = { { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f, 1.f } };
    const pxr::VtIntArray faceCount1 = { 3, 3 };
    const pxr::VtIntArray indices1 = { 0, 1, 2, 2, 1, 3 };
    setupAtTimeCode(points1, faceCount1, indices1, 1);

    // TimeCode 2.
    const pxr::VtVec3fArray points2 = { { 0.f, 0.f, 0.f },
                                        { 0.f, 1.f, 0.f },
                                        { 0.f, 0.f, 1.f },
                                        { 0.f, 1.f, 1.f },
                                        { 0.f, 1.f, 2.f } };
    const pxr::VtIntArray   faceCount2 = { 3, 3, 3 };
    const pxr::VtIntArray   indices2 = { 0, 1, 2, 2, 1, 3, 2, 3, 4 };
    setupAtTimeCode(points2, faceCount2, indices2, 2);

    // Now test the mesh conversion process to make sure that it respects the specified timeCode.
    auto testAtTimeCode = [&](const pxr::VtVec3fArray& points,
                              const pxr::VtIntArray&   faceCounts,
                              const pxr::VtIntArray&   indices,
                              const pxr::UsdTimeCode&  timeCode) {
        MNMesh                     maxMesh;
        MaxUsd::MeshConverter      converter;
        std::map<int, std::string> channelNames;
        converter.ConvertToMNMesh(
            animatedMesh,
            maxMesh,
            MaxUsd::PrimvarMappingOptions {},
            channelNames,
            nullptr,
            timeCode);

        ASSERT_EQ(maxMesh.VNum(), points.size());
        ASSERT_EQ(maxMesh.FNum(), faceCounts.size());
        for (int i = 0; i < maxMesh.VNum(); ++i) {
            EXPECT_FLOAT_EQ(points[i][0], maxMesh.V(i)->p.x);
            EXPECT_FLOAT_EQ(points[i][1], maxMesh.V(i)->p.y);
            EXPECT_FLOAT_EQ(points[i][2], maxMesh.V(i)->p.z);
        }

        int idx = 0;
        for (int i = 0; i < maxMesh.FNum(); ++i) {
            const auto degree = maxMesh.F(i)->deg;
            EXPECT_EQ(faceCounts[i], degree);
            for (int j = 0; j < degree; ++j) {
                EXPECT_EQ(indices[idx], maxMesh.F(i)->vtx[j]);
                idx++;
            }
        }
    };
    testAtTimeCode(pointsDefault, faceCountDefault, indicesDefault, pxr::UsdTimeCode::Default());
    testAtTimeCode(points1, faceCount1, indices1, 1);
    testAtTimeCode(points2, faceCount2, indices2, 2);

#ifdef TEST_OUTPUT_USD_FILES
    std::string exportPath = TestUtils::GetOutputDirectory();
    exportPath.append("/MeshConversionTest/TimeSampledMesh.usda");
    stage->Export(exportPath);
#endif
}

TEST(MeshConversionTest, ConvertMaxMeshToUsdTimeCode)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/OutputTimeSampledMesh");

    // Export different geometries at different timecodes on the same USD mesh.
    // We use geometries with completely different topologies to make sure that the
    // USD data we are validating is not from interpolation from another timecode.
    MNMesh quad = TestUtils::CreateQuad();
    MNMesh roof = TestUtils::CreateRoofShape();
    MNMesh cube = TestUtils::CreateCube(false);

    MaxUsd::MeshConverter            converter;
    pxr::UsdGeomMesh                 usdMesh;
    MaxUsd::MaxMeshConversionOptions options;
    std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
    options.SetNormalsMode(MaxUsd::MaxMeshConversionOptions::NormalsMode::None);
    MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
    // Export the meshes to different timecodes.
    // Default
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &quad },
        stage,
        path,
        options,
        usdMesh,
        pxr::UsdTimeCode::Default(),
        materialIdToFacesMap,
        false,
        intervals);
    // TimeCode 1
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &roof },
        stage,
        path,
        options,
        usdMesh,
        pxr::UsdTimeCode(1),
        materialIdToFacesMap,
        false,
        intervals);
    // TimeCode 2
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &cube },
        stage,
        path,
        options,
        usdMesh,
        pxr::UsdTimeCode(2),
        materialIdToFacesMap,
        false,
        intervals);

    // Validate that we get the expected geometry at each timecode.
    EXPECT_EQ(1, usdMesh.GetFaceCount(pxr::UsdTimeCode::Default()));
    EXPECT_EQ(2, usdMesh.GetFaceCount(pxr::UsdTimeCode(1)));
    EXPECT_EQ(6, usdMesh.GetFaceCount(pxr::UsdTimeCode(2)));

    // Extents
    pxr::VtVec3fArray extent1;
    usdMesh.GetExtentAttr().Get(&extent1, pxr::UsdTimeCode::Default());
    const pxr::VtVec3fArray expectedExtent1 = { { -1.f, -1.f, 0.f }, { 1.f, 1.f, 0.0001f } };
    EXPECT_EQ(expectedExtent1, extent1);

    pxr::VtVec3fArray extent2;
    usdMesh.GetExtentAttr().Get(&extent2, pxr::UsdTimeCode(1));
    const pxr::VtVec3fArray expectedExtent2 = { { -1.f, -1.f, 0.f }, { 1.f, 1.f, 1.f } };
    EXPECT_EQ(expectedExtent2, extent2);

    pxr::VtVec3fArray extent3;
    usdMesh.GetExtentAttr().Get(&extent3, pxr::UsdTimeCode(2));
    const pxr::VtVec3fArray expectedExtent3 = { { -1.f, -1.f, -1.f }, { 1.f, 1.f, 1.f } };
    EXPECT_EQ(expectedExtent3, extent3);

    // Points
    pxr::VtVec3fArray points1;
    usdMesh.GetPointsAttr().Get(&points1, pxr::UsdTimeCode::Default());
    const pxr::VtVec3fArray expectedPoints1
        = { { -1, -1, 0 }, { 1, -1, 0 }, { 1, 1, 0 }, { -1, 1, 0 } };
    EXPECT_EQ(expectedPoints1, points1);

    pxr::VtVec3fArray points2;
    usdMesh.GetPointsAttr().Get(&points2, pxr::UsdTimeCode(1));
    const pxr::VtVec3fArray expectedPoints2
        = { { -1, -1, 0 }, { 0, -1, 1 }, { 0, 1, 1 }, { -1, 1, 0 }, { 1, -1, 0 }, { 1, 1, 0 } };
    EXPECT_EQ(expectedPoints2, points2);

    pxr::VtVec3fArray points3;
    usdMesh.GetPointsAttr().Get(&points3, pxr::UsdTimeCode(2));
    const pxr::VtVec3fArray expectedPoints3
        = { { -1, -1, -1 }, { 1, -1, -1 }, { -1, 1, -1 }, { 1, 1, -1 },
            { -1, -1, 1 },  { 1, -1, 1 },  { -1, 1, 1 },  { 1, 1, 1 } };
    EXPECT_EQ(expectedPoints3, points3);

    // Face vertex counts
    pxr::VtIntArray faceVertexCounts1;
    usdMesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts1, pxr::UsdTimeCode::Default());
    const pxr::VtIntArray expectedFaceVertexCounts1 = { 4 };
    EXPECT_EQ(expectedFaceVertexCounts1, faceVertexCounts1);

    pxr::VtIntArray faceVertexCounts2;
    usdMesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts2, pxr::UsdTimeCode(1));
    const pxr::VtIntArray expectedFaceVertexCounts2 = { 4, 4 };
    EXPECT_EQ(expectedFaceVertexCounts2, faceVertexCounts2);

    pxr::VtIntArray faceVertexCounts3;
    usdMesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts3, pxr::UsdTimeCode(2));
    const pxr::VtIntArray expectedFaceVertexCounts3 = { 4, 4, 4, 4, 4, 4 };
    EXPECT_EQ(expectedFaceVertexCounts3, faceVertexCounts3);

    // Face vertex indices
    pxr::VtIntArray faceIndices1;
    usdMesh.GetFaceVertexIndicesAttr().Get(&faceIndices1, pxr::UsdTimeCode::Default());
    const pxr::VtIntArray expectedfaceIndices1 = { 0, 1, 2, 3 };
    EXPECT_EQ(expectedfaceIndices1, faceIndices1);

    pxr::VtIntArray faceIndices2;
    usdMesh.GetFaceVertexIndicesAttr().Get(&faceIndices2, pxr::UsdTimeCode(1));
    const pxr::VtIntArray expectedfaceIndices2 = { 0, 1, 2, 3, 1, 4, 5, 2 };
    EXPECT_EQ(expectedfaceIndices2, faceIndices2);

    pxr::VtIntArray faceIndices3;
    usdMesh.GetFaceVertexIndicesAttr().Get(&faceIndices3, pxr::UsdTimeCode(2));
    const pxr::VtIntArray expectedfaceIndices3
        = { 0, 2, 3, 1, 4, 5, 7, 6, 0, 1, 5, 4, 1, 3, 7, 5, 3, 2, 6, 7, 2, 0, 4, 6 };
    EXPECT_EQ(expectedfaceIndices3, faceIndices3);

#ifdef TEST_OUTPUT_USD_FILES
    std::string exportPath = TestUtils::GetOutputDirectory();
    exportPath.append("/MeshConversionTest/TimeSampledMeshOutput.usda");
    stage->Export(exportPath);
#endif
}
