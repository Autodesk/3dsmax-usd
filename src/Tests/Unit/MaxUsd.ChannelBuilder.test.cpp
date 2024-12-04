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

#include <MaxUsd/ChannelBuilder.h>
#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/Utilities/TypeUtils.h>

#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

#include <gtest/gtest.h>
#include <max.h>

using namespace MaxUsd;

TEST(ChannelBuilderTests, ConstantPrimvar)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/cube");
    auto       usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
    auto       cube = TestUtils::CreateCube(false);

    // Build a USD cube mesh.
    MeshConverter            converter;
    MaxMeshConversionOptions options;
    options.SetNormalsMode(MaxMeshConversionOptions::NormalsMode::None);
    std::map<MtlID, pxr::VtIntArray>      materialIdToFacesMap;
    MeshConverter::ObjectChannelIntervals intervals;
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

    // Create a constant primvar
    pxr::UsdGeomPrimvarsAPI primVarApi(usdMesh.GetPrim());
    auto                    primvar = primVarApi.CreatePrimvar(
        pxr::TfToken("testPrimvar"), pxr::SdfValueTypeNames->Float3Array);
    primvar.SetInterpolation(pxr::UsdGeomTokens->constant);
    pxr::VtVec3fArray values { pxr::GfVec3f(1.f, 2.f, 3.f) };
    primvar.Set(values);

    auto builder = MapBuilder(&cube, 0);
    builder.Build(
        primvar.GetAttr(),
        primvar.GetInterpolation(),
        &primvar,
        usdMesh,
        pxr::UsdTimeCode::Default());

    MNMap*     map = cube.M(0);
    const auto fNum = map->FNum();

    ASSERT_EQ(fNum, 6);
    ASSERT_EQ(map->VNum(), 1); // Only need a single value.
    EXPECT_EQ(map->V(0), Point3(values[0][0], values[0][1], values[0][2]));
    for (int i = 0; i < fNum; ++i) {
        const auto& face = map->F(i);
        for (int j = 0; j < face->deg; ++j) {
            ASSERT_EQ(face->tv[j], 0); // All values are the same.
        }
    }

#ifdef TEST_OUTPUT_USD_FILES
    std::string exportPath = TestUtils::GetOutputDirectory();
    exportPath.append("/ChannelBuilder/ConstantPrimvar.usda");
    stage->Export(exportPath);
#endif
}

void TestPointDataPrimvar(bool indexed, const pxr::TfToken& interpolation)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/cube");
    auto       usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
    auto       cube = TestUtils::CreateCube(false);

    // Build a USD cube mesh.
    MeshConverter            converter;
    MaxMeshConversionOptions options;
    options.SetNormalsMode(MaxMeshConversionOptions::NormalsMode::None);
    std::map<MtlID, pxr::VtIntArray>      materialIdToFacesMap;
    MeshConverter::ObjectChannelIntervals intervals;
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

    // Create a vertex interpolated primvar
    pxr::UsdGeomPrimvarsAPI primVarApi(usdMesh.GetPrim());
    auto                    primvar = primVarApi.CreatePrimvar(
        pxr::TfToken("testPrimvar"), pxr::SdfValueTypeNames->Float3Array);
    primvar.SetInterpolation(interpolation);
    pxr::VtVec3fArray values;

    const auto value1 = pxr::GfVec3f(1.f, 1.f, 1.f);
    const auto value2 = pxr::GfVec3f(2.f, 2.f, 2.f);

    if (indexed) {
        values.push_back(value1);
        values.push_back(value2);
        pxr::VtIntArray indices;
        indices.push_back(0);
        for (int i = 1; i < 8; ++i) {
            indices.push_back(1);
        }
        primvar.SetIndices(indices);
    } else {
        values.push_back(value1);
        for (int i = 1; i < 8; ++i) {
            values.push_back(value2);
        }
    }
    primvar.Set(values);

    auto builder = MapBuilder(&cube, 0);
    builder.Build(
        primvar.GetAttr(),
        primvar.GetInterpolation(),
        &primvar,
        usdMesh,
        pxr::UsdTimeCode::Default());

    MNMap*     map = cube.M(0);
    const auto fNum = map->FNum();
    ASSERT_EQ(fNum, 6);
    if (indexed) {
        ASSERT_EQ(map->VNum(), 2);
        EXPECT_EQ(map->V(0), Point3(value1[0], value1[1], value1[2]));
        EXPECT_EQ(map->V(1), Point3(value2[0], value2[1], value2[2]));

        // Value0 ends up on 3 face-vertices from 3 faces.
        EXPECT_EQ(map->F(0)->tv[0], 0); // Value1
        EXPECT_EQ(map->F(0)->tv[1], 1);
        EXPECT_EQ(map->F(0)->tv[2], 1);
        EXPECT_EQ(map->F(0)->tv[3], 1);

        EXPECT_EQ(map->F(1)->tv[0], 1);
        EXPECT_EQ(map->F(1)->tv[1], 1);
        EXPECT_EQ(map->F(1)->tv[2], 1);
        EXPECT_EQ(map->F(1)->tv[3], 1);

        EXPECT_EQ(map->F(2)->tv[0], 0); // Value1
        EXPECT_EQ(map->F(2)->tv[1], 1);
        EXPECT_EQ(map->F(2)->tv[2], 1);
        EXPECT_EQ(map->F(2)->tv[3], 1);

        EXPECT_EQ(map->F(3)->tv[0], 1);
        EXPECT_EQ(map->F(3)->tv[1], 1);
        EXPECT_EQ(map->F(3)->tv[2], 1);
        EXPECT_EQ(map->F(3)->tv[3], 1);

        EXPECT_EQ(map->F(4)->tv[0], 1);
        EXPECT_EQ(map->F(4)->tv[1], 1);
        EXPECT_EQ(map->F(4)->tv[2], 1);
        EXPECT_EQ(map->F(4)->tv[3], 1);

        EXPECT_EQ(map->F(5)->tv[0], 1);
        EXPECT_EQ(map->F(5)->tv[1], 0); // Value1
        EXPECT_EQ(map->F(5)->tv[2], 1);
        EXPECT_EQ(map->F(5)->tv[3], 1);
    } else {
        ASSERT_EQ(map->VNum(), 8);
        EXPECT_EQ(map->V(0), Point3(value1[0], value1[1], value1[2]));
        for (int i = 1; i < 8; i++) {
            EXPECT_EQ(map->V(i), Point3(value2[0], value2[1], value2[2]));
        }

        EXPECT_EQ(map->F(0)->tv[0], 0); // Value1
        EXPECT_EQ(map->F(0)->tv[1], 2);
        EXPECT_EQ(map->F(0)->tv[2], 3);
        EXPECT_EQ(map->F(0)->tv[3], 1);

        EXPECT_EQ(map->F(1)->tv[0], 4);
        EXPECT_EQ(map->F(1)->tv[1], 5);
        EXPECT_EQ(map->F(1)->tv[2], 7);
        EXPECT_EQ(map->F(1)->tv[3], 6);

        EXPECT_EQ(map->F(2)->tv[0], 0); // Value1
        EXPECT_EQ(map->F(2)->tv[1], 1);
        EXPECT_EQ(map->F(2)->tv[2], 5);
        EXPECT_EQ(map->F(2)->tv[3], 4);

        EXPECT_EQ(map->F(3)->tv[0], 1);
        EXPECT_EQ(map->F(3)->tv[1], 3);
        EXPECT_EQ(map->F(3)->tv[2], 7);
        EXPECT_EQ(map->F(3)->tv[3], 5);

        EXPECT_EQ(map->F(4)->tv[0], 3);
        EXPECT_EQ(map->F(4)->tv[1], 2);
        EXPECT_EQ(map->F(4)->tv[2], 6);
        EXPECT_EQ(map->F(4)->tv[3], 7);

        EXPECT_EQ(map->F(5)->tv[0], 2);
        EXPECT_EQ(map->F(5)->tv[1], 0); // Value1
        EXPECT_EQ(map->F(5)->tv[2], 4);
        EXPECT_EQ(map->F(5)->tv[3], 6);
    }

#ifdef TEST_OUTPUT_USD_FILES
    std::string exportPath = TestUtils::GetOutputDirectory();
    exportPath.append("/ChannelBuilder/VertexPrimvar_indexed_");
    exportPath.append(std::to_string(indexed));
    exportPath.append(".usda");
    stage->Export(exportPath);
#endif
}

TEST(ChannelBuilderTests, VertexPrimvar)
{
    TestPointDataPrimvar(false, pxr::UsdGeomTokens->vertex);
}

TEST(ChannelBuilderTests, VertexPrimvar_Indexed)
{
    TestPointDataPrimvar(true, pxr::UsdGeomTokens->vertex);
}

TEST(ChannelBuilderTests, VaryingPrimvar)
{
    TestPointDataPrimvar(false, pxr::UsdGeomTokens->varying);
}

TEST(ChannelBuilderTests, VaryingPrimvar_Indexed)
{
    TestPointDataPrimvar(true, pxr::UsdGeomTokens->varying);
}

void TestFaceVaryingPrimvar(bool indexed)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/cube");
    auto       usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
    auto       cube = TestUtils::CreateCube(false);

    // Build a USD cube.
    MeshConverter            converter;
    MaxMeshConversionOptions options;
    options.SetNormalsMode(MaxMeshConversionOptions::NormalsMode::None);
    std::map<MtlID, pxr::VtIntArray>      materialIdToFacesMap;
    MeshConverter::ObjectChannelIntervals intervals;
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

    // Create a faceVarying interpolated primvar.
    pxr::UsdGeomPrimvarsAPI primVarApi(usdMesh.GetPrim());
    auto                    primvar = primVarApi.CreatePrimvar(
        pxr::TfToken("testPrimvar"), pxr::SdfValueTypeNames->Float3Array);
    primvar.SetInterpolation(pxr::UsdGeomTokens->faceVarying);
    pxr::VtVec3fArray values;

    const auto value1 = pxr::GfVec3f(1.f, 1.f, 1.f);
    const auto value2 = pxr::GfVec3f(2.f, 2.f, 2.f);

    if (indexed) {
        values.push_back(value1);
        values.push_back(value2);

        pxr::VtIntArray indices;
        for (int i = 0; i < 24; ++i) {
            indices.push_back(i % 2); // Alternate values odd/even
        }
        primvar.SetIndices(indices);
    } else {
        for (int i = 0; i < 24; ++i) {
            values.push_back(i % 2 == 0 ? value1 : value2); // Alternate values odd/even
        }
    }
    primvar.Set(values);

    auto builder = MapBuilder(&cube, 0);
    builder.Build(
        primvar.GetAttr(),
        primvar.GetInterpolation(),
        &primvar,
        usdMesh,
        pxr::UsdTimeCode::Default());

    MNMap*     map = cube.M(0);
    const auto fNum = map->FNum();
    ASSERT_EQ(fNum, 6);

    if (indexed) {
        ASSERT_EQ(map->VNum(), 2);
        EXPECT_EQ(map->V(0), Point3(value1[0], value1[1], value1[2]));
        EXPECT_EQ(map->V(1), Point3(value2[0], value2[1], value2[2]));

        int idx = 0;
        for (int i = 0; i < fNum; ++i) {
            const auto& face = map->F(i);
            for (int j = 0; j < face->deg; ++j) {
                EXPECT_EQ(map->F(i)->tv[j], idx++ % 2);
            }
        }
    } else {
        ASSERT_EQ(map->VNum(), 24);
        for (int i = 1; i < 24; i++) {
            EXPECT_EQ(map->V(i), Point3(values[i][0], values[i][1], values[i][2]));
        }
        int idx = 0;
        for (int i = 0; i < fNum; ++i) {
            const auto& face = map->F(i);
            for (int j = 0; j < face->deg; ++j) {
                EXPECT_EQ(map->F(i)->tv[j], idx++); // One index per face-vertex.
            }
        }
    }

#ifdef TEST_OUTPUT_USD_FILES
    std::string exportPath = TestUtils::GetOutputDirectory();
    exportPath.append("/ChannelBuilder/FaceVaryingPrimvar_indexed_");
    exportPath.append(std::to_string(indexed));
    exportPath.append(".usda");
    stage->Export(exportPath);
#endif
}

TEST(ChannelBuilderTests, FaceVaryingPrimvar) { TestFaceVaryingPrimvar(false); }

TEST(ChannelBuilderTests, FaceVaryingPrimvar_Indexed) { TestFaceVaryingPrimvar(true); }

void TestUniformPrimvar(bool indexed)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/cube");
    auto       usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
    auto       cube = TestUtils::CreateCube(false);

    // Build a USD cube.
    MeshConverter            converter;
    MaxMeshConversionOptions options;
    options.SetNormalsMode(MaxMeshConversionOptions::NormalsMode::None);
    std::map<MtlID, pxr::VtIntArray>      materialIdToFacesMap;
    MeshConverter::ObjectChannelIntervals intervals;
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

    // Create a uniform interpolated primvar.
    pxr::UsdGeomPrimvarsAPI primVarApi(usdMesh.GetPrim());
    auto                    primvar = primVarApi.CreatePrimvar(
        pxr::TfToken("testPrimvar"), pxr::SdfValueTypeNames->Float3Array);
    primvar.SetInterpolation(pxr::UsdGeomTokens->uniform);
    pxr::VtVec3fArray values;

    const auto value1 = pxr::GfVec3f(1.f, 1.f, 1.f);
    const auto value2 = pxr::GfVec3f(2.f, 2.f, 2.f);

    if (indexed) {
        values.push_back(value1);
        values.push_back(value2);

        pxr::VtIntArray indices;
        indices.push_back(0);
        for (int i = 1; i < 6; ++i) {
            indices.push_back(1);
        }
        primvar.SetIndices(indices);
    } else {
        values.push_back(value1);
        for (int i = 1; i < 6; ++i) {
            values.push_back(value2);
        }
    }
    primvar.Set(values);

    auto builder = MapBuilder(&cube, 0);
    builder.Build(
        primvar.GetAttr(),
        primvar.GetInterpolation(),
        &primvar,
        usdMesh,
        pxr::UsdTimeCode::Default());

    MNMap*     map = cube.M(0);
    const auto fNum = map->FNum();
    ASSERT_EQ(fNum, 6);

    if (indexed) {
        ASSERT_EQ(map->VNum(), 2);
        EXPECT_EQ(map->V(0), Point3(value1[0], value1[1], value1[2]));
        EXPECT_EQ(map->V(1), Point3(value2[0], value2[1], value2[2]));

        for (int i = 0; i < fNum; ++i) {
            const auto& face = map->F(i);
            for (int j = 0; j < face->deg; ++j) {
                EXPECT_EQ(
                    map->F(i)->tv[j], i > 0 ? 1 : 0); // First face uses value1, other faces value2.
            }
        }
    } else {
        ASSERT_EQ(map->VNum(), 6);
        EXPECT_EQ(map->V(0), Point3(value1[0], value1[1], value1[2]));
        for (int i = 1; i < 6; i++) {
            EXPECT_EQ(map->V(i), Point3(value2[0], value2[1], value2[2]));
        }

        for (int i = 0; i < fNum; ++i) {
            const auto& face = map->F(i);
            for (int j = 0; j < face->deg; ++j) {
                EXPECT_EQ(map->F(i)->tv[j], i); // Single value per face.
            }
        }
    }

#ifdef TEST_OUTPUT_USD_FILES
    std::string exportPath = TestUtils::GetOutputDirectory();
    exportPath.append("/ChannelBuilder/UniformPrimvar_indexed_");
    exportPath.append(std::to_string(indexed));
    exportPath.append(".usda");
    stage->Export(exportPath);
#endif
}

TEST(ChannelBuilderTests, Uniform) { TestUniformPrimvar(false); }

TEST(ChannelBuilderTests, Uniform_Indexed) { TestUniformPrimvar(true); }

TEST(ChannelBuilderTests, TypeCasts)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/cube");
    auto       usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
    auto       cube = TestUtils::CreateCube(false);

    // Build a USD cube.
    MeshConverter            converter;
    MaxMeshConversionOptions options;
    options.SetNormalsMode(MaxMeshConversionOptions::NormalsMode::None);
    std::map<MtlID, pxr::VtIntArray>      materialIdToFacesMap;
    MeshConverter::ObjectChannelIntervals intervals;
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
    Point3 value(1.f, 2.f, 3.f);

    pxr::UsdGeomPrimvarsAPI primVarApi(usdMesh.GetPrim());

    // We should be able to cast a Double3Array to make it fit into a max channel (which carries
    // floats).
    {
        auto primvar = primVarApi.CreatePrimvar(
            pxr::TfToken("Double3ArrayPrimvar"), pxr::SdfValueTypeNames->Double3Array);
        primvar.SetInterpolation(pxr::UsdGeomTokens->constant);
        pxr::VtVec3dArray values;
        values.push_back(pxr::GfVec3d(value.x, value.y, value.z));
        primvar.Set(values);
        auto builder = MapBuilder(&cube, 0);
        builder.Build(
            primvar.GetAttr(),
            primvar.GetInterpolation(),
            &primvar,
            usdMesh,
            pxr::UsdTimeCode::Default());
        MNMap* map = cube.M(0);
        EXPECT_EQ(map->VNum(), 1);
        EXPECT_EQ(map->V(0), value);
        map->ClearAndFree();
    }

    // Double2Array should also work, with Z components set to 0.
    {
        auto primvar = primVarApi.CreatePrimvar(
            pxr::TfToken("Double2ArrayPrimvar"), pxr::SdfValueTypeNames->Double2Array);
        primvar.SetInterpolation(pxr::UsdGeomTokens->constant);
        pxr::VtVec2dArray values;
        values.push_back(pxr::GfVec2d(value.x, value.y));
        primvar.Set(values);
        auto builder = MapBuilder(&cube, 0);
        builder.Build(
            primvar.GetAttr(),
            primvar.GetInterpolation(),
            &primvar,
            usdMesh,
            pxr::UsdTimeCode::Default());
        MNMap* map = cube.M(0);
        EXPECT_EQ(map->VNum(), 1);
        EXPECT_EQ(map->V(0), Point3(value.x, value.y, 0.f));
        map->ClearAndFree();
    }

    // Same for DoubleArray -  Y and Z components set to 0.
    {
        auto primvar = primVarApi.CreatePrimvar(
            pxr::TfToken("DoublePrimvar"), pxr::SdfValueTypeNames->DoubleArray);
        primvar.SetInterpolation(pxr::UsdGeomTokens->constant);
        pxr::VtDoubleArray values;
        values.push_back(value.x);
        primvar.Set(values);
        auto builder = MapBuilder(&cube, 0);
        builder.Build(
            primvar.GetAttr(),
            primvar.GetInterpolation(),
            &primvar,
            usdMesh,
            pxr::UsdTimeCode::Default());
        MNMap* map = cube.M(0);
        EXPECT_EQ(map->VNum(), 1);
        EXPECT_EQ(map->V(0), Point3(value.x, 0.f, 0.f));
        map->ClearAndFree();
    }

    // Dimension 4 type will also work, but will be croped to 3 dimensions.
    {
        auto primvar = primVarApi.CreatePrimvar(
            pxr::TfToken("Double4ArrayPrimvar"), pxr::SdfValueTypeNames->Double4Array);
        primvar.SetInterpolation(pxr::UsdGeomTokens->constant);
        pxr::VtVec4dArray values;
        values.push_back(pxr::GfVec4f(value.x, value.y, value.z, 1.0f));
        primvar.Set(values);
        auto builder = MapBuilder(&cube, 0);
        builder.Build(
            primvar.GetAttr(),
            primvar.GetInterpolation(),
            &primvar,
            usdMesh,
            pxr::UsdTimeCode::Default());
        MNMap* map = cube.M(0);
        EXPECT_EQ(map->VNum(), 1);
        EXPECT_EQ(map->V(0), value);
        map->ClearAndFree();
    }

    // Double (non array type) should also work, casts to an array of doubles of size 1, which is
    // accepted. And could be valid for a constant primvar.
    {
        auto primvar = primVarApi.CreatePrimvar(
            pxr::TfToken("DoublePrimvar"), pxr::SdfValueTypeNames->Double);
        primvar.SetInterpolation(pxr::UsdGeomTokens->constant);
        primvar.Set(pxr::VtDoubleArray { value.x });
        auto builder = MapBuilder(&cube, 0);
        builder.Build(
            primvar.GetAttr(),
            primvar.GetInterpolation(),
            &primvar,
            usdMesh,
            pxr::UsdTimeCode::Default());
        MNMap* map = cube.M(0);
        EXPECT_EQ(map->VNum(), 1);
        EXPECT_EQ(map->V(0), Point3(value.x, 0.f, 0.f));
        map->ClearAndFree();
    }

    // String primvar, wont cast!
    {
        auto primvar = primVarApi.CreatePrimvar(
            pxr::TfToken("StringPrimvar"), pxr::SdfValueTypeNames->String);
        primvar.SetInterpolation(pxr::UsdGeomTokens->constant);
        primvar.Set("foo");
        auto builder = MapBuilder(&cube, 0);
        builder.Build(
            primvar.GetAttr(),
            primvar.GetInterpolation(),
            &primvar,
            usdMesh,
            pxr::UsdTimeCode::Default());
        MNMap* map = cube.M(0);
        EXPECT_EQ(map->VNum(), 0);
    }
}

TEST(ChannelBuilderTests, PrimvarChannelResolution)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/object");
    auto       usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));

    // Create a couple primvars to work with.
    const auto floatArrayName = pxr::TfToken("floatArray");
    const auto float2Array1Name = pxr::TfToken("float2Array1");
    const auto float2Array2Name = pxr::TfToken("float2Array2");
    const auto texCoord2fArrayName = pxr::TfToken("texCoord2fArray");
    const auto texCoord3fArrayName = pxr::TfToken("texCoord3fArray");
    const auto color3fArrayName = pxr::TfToken("color3fArray");

    pxr::VtFloatArray floatArray { 1.0f };
    pxr::VtVec2fArray float2Array { { 1.0f, 1.0f } };
    pxr::VtVec3fArray float3Array { { 1.0f, 1.0f, 1.0f } };

    auto primvar = pxr::UsdGeomPrimvarsAPI(usdMesh).CreatePrimvar(
        floatArrayName, pxr::SdfValueTypeNames->FloatArray, pxr::UsdGeomTokens->constant);
    primvar.Set(floatArray);
    primvar = pxr::UsdGeomPrimvarsAPI(usdMesh).CreatePrimvar(
        float2Array1Name, pxr::SdfValueTypeNames->Float2Array, pxr::UsdGeomTokens->constant);
    primvar.Set(float2Array);
    primvar = pxr::UsdGeomPrimvarsAPI(usdMesh).CreatePrimvar(
        float2Array2Name, pxr::SdfValueTypeNames->Float2Array, pxr::UsdGeomTokens->constant);
    primvar.Set(float2Array);
    primvar = pxr::UsdGeomPrimvarsAPI(usdMesh).CreatePrimvar(
        texCoord2fArrayName, pxr::SdfValueTypeNames->TexCoord2fArray, pxr::UsdGeomTokens->constant);
    primvar.Set(float2Array);
    primvar = pxr::UsdGeomPrimvarsAPI(usdMesh).CreatePrimvar(
        texCoord3fArrayName, pxr::SdfValueTypeNames->TexCoord3fArray, pxr::UsdGeomTokens->constant);
    primvar.Set(float3Array);
    primvar = pxr::UsdGeomPrimvarsAPI(usdMesh).CreatePrimvar(
        color3fArrayName, pxr::SdfValueTypeNames->Color3fArray, pxr::UsdGeomTokens->constant);
    primvar.Set(float3Array);

    PrimvarMappingOptions options;
    options.SetImportUnmappedPrimvars(false);

    std::map<int, pxr::UsdGeomPrimvar> channelPrimvars;

    // Test typical case...
    options.ClearMappedPrimvars();
    options.SetPrimvarChannelMapping(floatArrayName, 1);
    options.SetPrimvarChannelMapping(float2Array1Name, 2);
    options.SetPrimvarChannelMapping(float2Array2Name, 3);
    options.SetPrimvarChannelMapping(texCoord2fArrayName, 4);
    options.SetPrimvarChannelMapping(texCoord3fArrayName, 5);
    options.SetPrimvarChannelMapping(color3fArrayName, 6);

    TestUtils::MeshConverterTester::ResolveChannelPrimvars(usdMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars[1].GetPrimvarName(), floatArrayName);
    EXPECT_EQ(channelPrimvars[2].GetPrimvarName(), float2Array1Name);
    EXPECT_EQ(channelPrimvars[3].GetPrimvarName(), float2Array2Name);
    EXPECT_EQ(channelPrimvars[4].GetPrimvarName(), texCoord2fArrayName);
    EXPECT_EQ(channelPrimvars[5].GetPrimvarName(), texCoord3fArrayName);
    EXPECT_EQ(channelPrimvars[6].GetPrimvarName(), color3fArrayName);

    // Test unmapped primvars not imported.
    options.ClearMappedPrimvars();
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(usdMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars.size(), 0);

    // Test unused channels "hole"...
    options.ClearMappedPrimvars();
    options.SetPrimvarChannelMapping(floatArrayName, 1);
    options.SetPrimvarChannelMapping(float2Array1Name, 3);
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(usdMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars[1].GetPrimvarName(), floatArrayName);
    EXPECT_EQ(channelPrimvars.find(2), channelPrimvars.end());
    EXPECT_EQ(channelPrimvars[3].GetPrimvarName(), float2Array1Name);

    // Test primvar collision on the same mesh (two or more primvars targeting the same channel).
    options.ClearMappedPrimvars();
    options.SetPrimvarChannelMapping(pxr::TfToken("aaa"), 1); // Primvar doesn't exist on the mesh.
    options.SetPrimvarChannelMapping(floatArrayName, 1);
    options.SetPrimvarChannelMapping(float2Array1Name, 1);
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(usdMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars.size(), 1);
    // float2Array1 will "win" because of alphabetical ordering & aaa doesn't exist.
    EXPECT_EQ(channelPrimvars[1].GetPrimvarName(), float2Array1Name);

    options.SetImportUnmappedPrimvars(true);

    // Test that disabled channels are respected.
    options.ClearMappedPrimvars();
    options.SetPrimvarChannelMapping(floatArrayName, PrimvarMappingOptions::invalidChannel);
    options.SetPrimvarChannelMapping(float2Array1Name, PrimvarMappingOptions::invalidChannel);
    options.SetPrimvarChannelMapping(float2Array2Name, PrimvarMappingOptions::invalidChannel);
    options.SetPrimvarChannelMapping(texCoord2fArrayName, PrimvarMappingOptions::invalidChannel);
    options.SetPrimvarChannelMapping(texCoord3fArrayName, PrimvarMappingOptions::invalidChannel);
    options.SetPrimvarChannelMapping(color3fArrayName, PrimvarMappingOptions::invalidChannel);
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(usdMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars.size(), 0);

    // Test fallback to texcoord2fArray type for UVS, and Color3 for vertex color.
    options.ClearMappedPrimvars();
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(usdMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars[1].GetPrimvarName(), texCoord2fArrayName);
    EXPECT_EQ(channelPrimvars[0].GetPrimvarName(), color3fArrayName);

    // Test float2 can be inferred as main UV also, if no available texCoord primvar.
    options.ClearMappedPrimvars();
    options.SetPrimvarChannelMapping(texCoord2fArrayName, 2);
    options.SetPrimvarChannelMapping(texCoord3fArrayName, 3);
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(usdMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars[1].GetPrimvarName(), float2Array1Name);

    // Create a new primvar with a name higher by alphabetical order, type texcoord2fArray, it
    // should now be selected.
    auto aTexcoord2fArray = pxr::TfToken("aTexcoord2fArray");
    primvar = pxr::UsdGeomPrimvarsAPI(usdMesh).CreatePrimvar(
        aTexcoord2fArray, pxr::SdfValueTypeNames->TexCoord2fArray, pxr::UsdGeomTokens->constant);
    primvar.Set(float2Array);
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(usdMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars[1].GetPrimvarName(), aTexcoord2fArray);

    // Make sure that primvars that are aleardy mapped cannot be used as fallback.
    options.ClearMappedPrimvars();
    options.SetPrimvarChannelMapping(aTexcoord2fArray, 2);
    options.SetPrimvarChannelMapping(texCoord2fArrayName, 3);
    options.SetPrimvarChannelMapping(texCoord3fArrayName, 4);
    options.SetPrimvarChannelMapping(float2Array1Name, 5);
    options.SetPrimvarChannelMapping(float2Array2Name, 6);
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(usdMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars.find(1), channelPrimvars.end());

    // Test that all primvars are imported, texCoord primvar are imported in priority to
    // lower channels.
    options.ClearMappedPrimvars();
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(usdMesh, options, channelPrimvars);

    EXPECT_EQ(channelPrimvars[0].GetPrimvarName(), color3fArrayName); // Inferred color3fArrayName
    EXPECT_EQ(channelPrimvars[1].GetPrimvarName(), aTexcoord2fArray); // Inferred main UV
    EXPECT_EQ(
        channelPrimvars[2].GetPrimvarName(), texCoord2fArrayName); // Texcoord2 - from type order.
    EXPECT_EQ(
        channelPrimvars[3].GetPrimvarName(), texCoord3fArrayName); // TexCoord3 -from type order.
    EXPECT_EQ(
        channelPrimvars[4].GetPrimvarName(),
        float2Array1Name); // Next found primvars... order doesn't depend on type.
    EXPECT_EQ(channelPrimvars[5].GetPrimvarName(), float2Array2Name);
    EXPECT_EQ(channelPrimvars[6].GetPrimvarName(), floatArrayName);

    // Test that unmapped primvars are imported to "holes", i.e. available channels.
    options.ClearMappedPrimvars();
    options.SetPrimvarChannelMapping(floatArrayName, 1);
    options.SetPrimvarChannelMapping(float2Array1Name, 3);
    options.SetPrimvarChannelMapping(float2Array2Name, 5);

    TestUtils::MeshConverterTester::ResolveChannelPrimvars(usdMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars[0].GetPrimvarName(), color3fArrayName);    // inferred as vertex color
    EXPECT_EQ(channelPrimvars[1].GetPrimvarName(), floatArrayName);      // explicit mapping
    EXPECT_EQ(channelPrimvars[2].GetPrimvarName(), aTexcoord2fArray);    // auto-mapped
    EXPECT_EQ(channelPrimvars[3].GetPrimvarName(), float2Array1Name);    // explicit mapping
    EXPECT_EQ(channelPrimvars[4].GetPrimvarName(), texCoord2fArrayName); // auto-mapped
    EXPECT_EQ(channelPrimvars[5].GetPrimvarName(), float2Array2Name);    // explicit mapping
    EXPECT_EQ(channelPrimvars[6].GetPrimvarName(), texCoord3fArrayName); // auto-mapped

    // Test ordering of primvars into channels.
    const auto primvarOrderPrim = pxr::SdfPath("/primvar_order");
    auto orderMesh = pxr::UsdGeomMesh(stage->DefinePrim(primvarOrderPrim, pxr::TfToken("Mesh")));

    const auto point3hArray = pxr::TfToken("point3hArray");
    const auto color3hArray = pxr::TfToken("color3hArray");
    const auto color3dArray
        = pxr::TfToken("color3dArray"); // Will be inferred as vertex color (0) from type
    const auto normal3dArray = pxr::TfToken("normal3dArray");
    const auto texCoord3hArray = pxr::TfToken("texCoord3hArray"); // -> channel 6
    const auto texCoord3dArray = pxr::TfToken("texCoord3dArray"); // -> channel 5
    const auto texCoord3fArray = pxr::TfToken("texCoord3fArray"); // -> channel 4
    const auto texCoord2hArray = pxr::TfToken("texCoord2hArray"); // -> channel 3
    const auto texCoord2dArray = pxr::TfToken("texCoord2dArray"); // -> channel 2
    const auto texCoord2fArray
        = pxr::TfToken("texCoord2fArray"); // -> Will be inferred as main UV (1) from type

    pxr::VtArray<pxr::GfVec2d> double2Array = { { 1.0, 1.0 } };
    pxr::VtArray<pxr::GfVec3d> double3Array = { { 1.0, 1.0, 1.0 } };

    pxr::VtArray<pxr::GfVec2h> half2Array = { { 1.0f, 1.0f } };
    pxr::VtArray<pxr::GfVec3h> half3Array = { { 1.0f, 1.0f, 1.0f } };

    primvar = pxr::UsdGeomPrimvarsAPI(orderMesh).CreatePrimvar(
        point3hArray, pxr::SdfValueTypeNames->Point3hArray, pxr::UsdGeomTokens->constant);
    primvar.Set(half3Array);
    primvar = pxr::UsdGeomPrimvarsAPI(orderMesh).CreatePrimvar(
        color3hArray, pxr::SdfValueTypeNames->Color3hArray, pxr::UsdGeomTokens->constant);
    primvar.Set(half3Array);
    primvar = pxr::UsdGeomPrimvarsAPI(orderMesh).CreatePrimvar(
        color3dArray, pxr::SdfValueTypeNames->Color3dArray, pxr::UsdGeomTokens->constant);
    primvar.Set(double3Array);
    primvar = pxr::UsdGeomPrimvarsAPI(orderMesh).CreatePrimvar(
        normal3dArray, pxr::SdfValueTypeNames->Normal3dArray, pxr::UsdGeomTokens->constant);
    primvar.Set(double3Array);
    primvar = pxr::UsdGeomPrimvarsAPI(orderMesh).CreatePrimvar(
        texCoord3hArray, pxr::SdfValueTypeNames->TexCoord3hArray, pxr::UsdGeomTokens->constant);
    primvar.Set(half3Array);
    primvar = pxr::UsdGeomPrimvarsAPI(orderMesh).CreatePrimvar(
        texCoord3dArray, pxr::SdfValueTypeNames->TexCoord3dArray, pxr::UsdGeomTokens->constant);
    primvar.Set(double3Array);
    primvar = pxr::UsdGeomPrimvarsAPI(orderMesh).CreatePrimvar(
        texCoord3fArray, pxr::SdfValueTypeNames->TexCoord3fArray, pxr::UsdGeomTokens->constant);
    primvar.Set(float3Array);
    primvar = pxr::UsdGeomPrimvarsAPI(orderMesh).CreatePrimvar(
        texCoord2hArray, pxr::SdfValueTypeNames->TexCoord2hArray, pxr::UsdGeomTokens->constant);
    primvar.Set(half2Array);
    primvar = pxr::UsdGeomPrimvarsAPI(orderMesh).CreatePrimvar(
        texCoord2dArray, pxr::SdfValueTypeNames->TexCoord2dArray, pxr::UsdGeomTokens->constant);
    primvar.Set(double2Array);
    primvar = pxr::UsdGeomPrimvarsAPI(orderMesh).CreatePrimvar(
        texCoord2fArray, pxr::SdfValueTypeNames->TexCoord2fArray, pxr::UsdGeomTokens->constant);
    primvar.Set(float2Array);

    options.ClearMappedPrimvars();
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(orderMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars[0].GetPrimvarName(), color3dArray);    // inferred vertex color
    EXPECT_EQ(channelPrimvars[1].GetPrimvarName(), texCoord2fArray); // inferred UV
    EXPECT_EQ(channelPrimvars[2].GetPrimvarName(), texCoord2dArray); // From type...
    EXPECT_EQ(channelPrimvars[3].GetPrimvarName(), texCoord2hArray);
    EXPECT_EQ(channelPrimvars[4].GetPrimvarName(), texCoord3fArray);
    EXPECT_EQ(channelPrimvars[5].GetPrimvarName(), texCoord3dArray);
    EXPECT_EQ(channelPrimvars[6].GetPrimvarName(), texCoord3hArray);
    EXPECT_EQ(channelPrimvars[7].GetPrimvarName(), color3hArray); // From alphabetical order.
    EXPECT_EQ(channelPrimvars[8].GetPrimvarName(), normal3dArray);
    EXPECT_EQ(channelPrimvars[9].GetPrimvarName(), point3hArray);

    // Test that primvars which cannot be fit into max channels are ignored by the mapping
    // resolution unless the mapping is explicitly specified.
    const auto unusablePrimvars = pxr::SdfPath("/unusable_primvars");
    auto       unusablePrimvarsMesh
        = pxr::UsdGeomMesh(stage->DefinePrim(unusablePrimvars, pxr::TfToken("Mesh")));
    // No values authored.
    primvar
        = pxr::UsdGeomPrimvarsAPI(unusablePrimvarsMesh)
              .CreatePrimvar(
                  point3hArray, pxr::SdfValueTypeNames->Point3hArray, pxr::UsdGeomTokens->constant);
    // String primvar
    primvar = pxr::UsdGeomPrimvarsAPI(unusablePrimvarsMesh)
                  .CreatePrimvar(
                      pxr::TfToken("stringPrimvar"),
                      pxr::SdfValueTypeNames->String,
                      pxr::UsdGeomTokens->constant);
    primvar.Set("foo");
    // Dimension > 3
    const auto float4primvar = pxr::TfToken("float4primvar");
    primvar
        = pxr::UsdGeomPrimvarsAPI(unusablePrimvarsMesh)
              .CreatePrimvar(
                  float4primvar, pxr::SdfValueTypeNames->Float4Array, pxr::UsdGeomTokens->constant);
    primvar.Set(pxr::VtVec4fArray { { 1.0f, 1.0f, 1.0f, 1.0f } });
    options.ClearMappedPrimvars();
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(
        unusablePrimvarsMesh, options, channelPrimvars);
    ASSERT_TRUE(channelPrimvars.empty());
    // If explicitly specified, the primvar is always considered in the resolution.
    options.SetPrimvarChannelMapping(float4primvar, 1);
    TestUtils::MeshConverterTester::ResolveChannelPrimvars(
        unusablePrimvarsMesh, options, channelPrimvars);
    EXPECT_EQ(channelPrimvars[1].GetPrimvarName(), float4primvar);
}

TEST(ChannelBuilderTests, TimeSampledPrimvar)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/cube");
    auto       usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
    auto       cube = TestUtils::CreateCube(false);

    // Build a USD cube.
    MeshConverter            converter;
    MaxMeshConversionOptions options;
    options.SetNormalsMode(MaxMeshConversionOptions::NormalsMode::None);
    std::map<MtlID, pxr::VtIntArray>      materialIdToFacesMap;
    MeshConverter::ObjectChannelIntervals intervals;
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

    // Create a uniform interpolated primvar.
    pxr::UsdGeomPrimvarsAPI primVarApi(usdMesh.GetPrim());
    auto                    primvar = primVarApi.CreatePrimvar(
        pxr::TfToken("testPrimvar"),
        pxr::SdfValueTypeNames->Float3Array,
        pxr::UsdGeomTokens->uniform);

    auto setupAtTimeCode
        = [&](const pxr::UsdTimeCode& timeCode, const pxr::GfVec3f& value, int index) {
              // Populate the primvar at the specified timecode with the given value / index.
              // Not using a simpler constant primvar so that indexing can be tested.
              pxr::VtVec3fArray values;
              values.push_back(value);
              values.push_back(value);
              values.push_back(value);
              pxr::VtIntArray indices;
              for (int i = 0; i < 6; ++i) {
                  indices.push_back(index);
              }
              primvar.SetIndices(indices, timeCode);
              primvar.Set(values, timeCode);
          };

    setupAtTimeCode(pxr::UsdTimeCode::Default(), pxr::GfVec3f(0.f, 0.f, 0.f), 0);
    setupAtTimeCode(1, pxr::GfVec3f(1.f, 1.f, 1.f), 1);
    setupAtTimeCode(2, pxr::GfVec3f(2.f, 2.f, 2.f), 2);

    auto testAtTimeCode = [&](const pxr::UsdTimeCode& timeCode,
                              const pxr::GfVec3f&     expectedValue,
                              int                     expectedIndex) {
        // Build the channel from the primvar at the specified timeCode.
        auto builder = MapBuilder(&cube, 0);
        builder.Build(primvar.GetAttr(), primvar.GetInterpolation(), &primvar, usdMesh, timeCode);
        MNMap*     map = cube.M(0);
        const auto fNum = map->FNum();
        ASSERT_EQ(fNum, 6);
        ASSERT_EQ(map->VNum(), 3);
        EXPECT_EQ(map->V(0), Point3(expectedValue[0], expectedValue[1], expectedValue[2]));
        EXPECT_EQ(map->V(1), Point3(expectedValue[0], expectedValue[1], expectedValue[2]));
        EXPECT_EQ(map->V(2), Point3(expectedValue[0], expectedValue[1], expectedValue[2]));
        for (int i = 0; i < fNum; ++i) {
            const auto& face = map->F(i);
            for (int j = 0; j < face->deg; ++j) {
                EXPECT_EQ(map->F(i)->tv[j], expectedIndex);
            }
        }
    };

    testAtTimeCode(pxr::UsdTimeCode::Default(), pxr::GfVec3f(0.f, 0.f, 0.f), 0);
    testAtTimeCode(1, pxr::GfVec3f(1.f, 1.f, 1.f), 1);
    testAtTimeCode(2, pxr::GfVec3f(2.f, 2.f, 2.f), 2);

#ifdef TEST_OUTPUT_USD_FILES
    std::string exportPath = TestUtils::GetOutputDirectory();
    exportPath.append("/ChannelBuilder/TimeSampledPrimvar.usda");
    stage->Export(exportPath);
#endif
}
TEST(ChannelBuilderTests, LeftHandedFaceOrientation)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto path = pxr::SdfPath("/object");

    // Export a simple quad to a USD mesh.
    auto                                  maxQuad = TestUtils::CreateQuad();
    MeshConverter                         converter;
    pxr::UsdGeomMesh                      usdMesh;
    MaxMeshConversionOptions              options;
    std::map<MtlID, pxr::VtIntArray>      materialIdToFacesMap;
    MeshConverter::ObjectChannelIntervals intervals;
    converter.ConvertToUSDMesh(
        MaxUsd::MeshFacade { &maxQuad },
        stage,
        path,
        options,
        usdMesh,
        pxr::UsdTimeCode::Default(),
        materialIdToFacesMap,
        false,
        intervals);
    usdMesh.CreateOrientationAttr().Set(pxr::UsdGeomTokens->leftHanded);
    pxr::UsdGeomPrimvarsAPI primVarApi(usdMesh.GetPrim());

    PrimvarMappingOptions conversionOpts;

    // In our test, will use the first channel as target for imports.
    int channel = 1;

    // 1) Test that map faces built from vertex-interpolated primvars are correctly flipped.

    std::string vertexPrimvarName = "vertexPrimvar";
    auto        vertexPrimvar = primVarApi.CreatePrimvar(
        pxr::TfToken(vertexPrimvarName),
        pxr::SdfValueTypeNames->Float3Array,
        pxr::UsdGeomTokens->vertex);
    vertexPrimvar.SetIndices({ 0, 1, 2, 3 });
    vertexPrimvar.Set(pxr::VtVec3fArray { pxr::GfVec3f(0, 1, 2),
                                          pxr::GfVec3f(3, 4, 5),
                                          pxr::GfVec3f(6, 7, 8),
                                          pxr::GfVec3f(9, 10, 11) });
    conversionOpts.SetPrimvarChannelMapping(vertexPrimvarName, channel);

    MNMesh                     importedQuadVertex;
    std::map<int, std::string> channelNames1;
    converter.ConvertToMNMesh(usdMesh, importedQuadVertex, conversionOpts, channelNames1);
    auto quadFaceFromVertex = importedQuadVertex.M(channel)->F(0);

    // Expect the reverse order after import, always starting at 0.
    EXPECT_EQ(0, quadFaceFromVertex->tv[0]);
    EXPECT_EQ(3, quadFaceFromVertex->tv[1]);
    EXPECT_EQ(2, quadFaceFromVertex->tv[2]);
    EXPECT_EQ(1, quadFaceFromVertex->tv[3]);

    // 2) Test that map faces built from faceVarying interpolated primvars are correctly flipped.

    std::string faceVaryingPrimvarName = "faceVaryingPrimvar";

    // Create a face-varying interpolated primvar.
    auto faceVaryingPrimvar = primVarApi.CreatePrimvar(
        pxr::TfToken(faceVaryingPrimvarName),
        pxr::SdfValueTypeNames->Float3Array,
        pxr::UsdGeomTokens->faceVarying);

    faceVaryingPrimvar.SetIndices({ 0, 1, 2, 3 });
    faceVaryingPrimvar.Set(pxr::VtVec3fArray { pxr::GfVec3f(0, 1, 2),
                                               pxr::GfVec3f(3, 4, 5),
                                               pxr::GfVec3f(6, 7, 8),
                                               pxr::GfVec3f(9, 10, 11) });

    conversionOpts.ClearMappedPrimvars();
    conversionOpts.SetPrimvarChannelMapping(faceVaryingPrimvarName, channel);

    MNMesh                     importedQuadFaceVaring;
    std::map<int, std::string> channelNames2;
    converter.ConvertToMNMesh(usdMesh, importedQuadFaceVaring, conversionOpts, channelNames2);
    auto quadFaceFromFaceVarying = importedQuadFaceVaring.M(channel)->F(0);

    // Expect te reverse order after import, always starting at 0.
    EXPECT_EQ(0, quadFaceFromFaceVarying->tv[0]);
    EXPECT_EQ(3, quadFaceFromFaceVarying->tv[1]);
    EXPECT_EQ(2, quadFaceFromFaceVarying->tv[2]);
    EXPECT_EQ(1, quadFaceFromFaceVarying->tv[3]);

    // 3) Test that map faces built from uniform interpolated primvars are not affected (all the
    // points on a face share the same data, nothing to flip).

    std::string uniformPrimvarName = "uniformPrimvar";
    // Create a unifrom interpolated primvar.
    auto uniformPrimvar = primVarApi.CreatePrimvar(
        pxr::TfToken(uniformPrimvarName),
        pxr::SdfValueTypeNames->Float3Array,
        pxr::UsdGeomTokens->uniform);
    uniformPrimvar.SetIndices({ 0 });
    uniformPrimvar.Set(pxr::VtVec3fArray { pxr::GfVec3f(0, 1, 2) });

    conversionOpts.ClearMappedPrimvars();
    conversionOpts.SetPrimvarChannelMapping(uniformPrimvarName, channel);

    MNMesh                     importedQuadUniform;
    std::map<int, std::string> channelNames3;
    converter.ConvertToMNMesh(usdMesh, importedQuadUniform, conversionOpts, channelNames3);
    auto quadFaceFromUniform = importedQuadUniform.M(channel)->F(0);

    EXPECT_EQ(0, quadFaceFromUniform->tv[0]);
    EXPECT_EQ(0, quadFaceFromUniform->tv[1]);
    EXPECT_EQ(0, quadFaceFromUniform->tv[2]);
    EXPECT_EQ(0, quadFaceFromUniform->tv[3]);

    // 4) Test that map faces built from constant interpolated primvars are not affected (all the
    // points on a mesh share the same data, nothing to flip).

    std::string constantPrimvarName = "constantPrimvar";
    // Create a constant interpolated primvar.
    auto constantPrimvar = primVarApi.CreatePrimvar(
        pxr::TfToken(constantPrimvarName),
        pxr::SdfValueTypeNames->Float3Array,
        pxr::UsdGeomTokens->constant);
    constantPrimvar.Set(pxr::VtVec3fArray { pxr::GfVec3f(0, 1, 2) });

    conversionOpts.ClearMappedPrimvars();
    conversionOpts.SetPrimvarChannelMapping(constantPrimvarName, channel);

    MNMesh                     importedQuadConstant;
    std::map<int, std::string> channelNames4;
    converter.ConvertToMNMesh(usdMesh, importedQuadConstant, conversionOpts, channelNames4);
    auto quadFaceFromConstant = importedQuadConstant.M(channel)->F(0);

    EXPECT_EQ(0, quadFaceFromConstant->tv[0]);
    EXPECT_EQ(0, quadFaceFromConstant->tv[1]);
    EXPECT_EQ(0, quadFaceFromConstant->tv[2]);
    EXPECT_EQ(0, quadFaceFromConstant->tv[3]);

#ifdef TEST_OUTPUT_USD_FILES
    std::string exportPath = TestUtils::GetOutputDirectory();
    exportPath.append("/ChannelBuilder/leftHandedOrientationPrimvar.usda");
    stage->Export(exportPath);
#endif
}
