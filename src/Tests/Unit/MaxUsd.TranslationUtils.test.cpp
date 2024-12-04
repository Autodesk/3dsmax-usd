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

#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/usd/inherits.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include <gtest/gtest.h>
#include <mesh.h>
#include <mnmesh.h>

using namespace MaxUsd;

TEST(TranslationUtilsTest, ValidateMappedData)
{
    const auto cube = TestUtils::CreateCube(false);

    pxr::VtIntArray indices;

    EXPECT_FALSE(
        ValidateMappedDataForMesh(2, indices, cube, pxr::TfToken("bad_interpolation"), false));

    // Constant interpolation.
    EXPECT_TRUE(ValidateMappedDataForMesh(1, indices, cube, pxr::UsdGeomTokens->constant, false));
    EXPECT_TRUE(ValidateMappedDataForMesh(
        2,
        indices,
        cube,
        pxr::UsdGeomTokens->constant,
        false)); // To many values, but still useable.
    EXPECT_FALSE(ValidateMappedDataForMesh(0, indices, cube, pxr::UsdGeomTokens->constant, false));

    auto vertexDataTest = [&indices, &cube](pxr::TfToken interpolation) {
        EXPECT_TRUE(ValidateMappedDataForMesh(8, indices, cube, interpolation, false));
        EXPECT_TRUE(
            ValidateMappedDataForMesh(9, indices, cube, interpolation, false)); // still workable.
        EXPECT_FALSE(
            ValidateMappedDataForMesh(7, indices, cube, interpolation, false)); // Not enough data.

        indices = { 0, 0, 0, 0, 0, 0, 0, 0 }; // 8 indices, what is expected.
        EXPECT_TRUE(ValidateMappedDataForMesh(1, indices, cube, interpolation, true));
        indices = { 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // 9 indices, too much, but still useable.
        EXPECT_TRUE(ValidateMappedDataForMesh(1, indices, cube, interpolation, true));
        indices = { 0, 0, 0, 0, 0, 0, 0 }; // 7 indices, unusable.
        EXPECT_FALSE(ValidateMappedDataForMesh(1, indices, cube, interpolation, true));
        indices = { 0, 0, 0, 0, 0, 0, 0, 2 }; // Out of range index.
        EXPECT_FALSE(ValidateMappedDataForMesh(1, indices, cube, interpolation, true));
        indices = { 0, 0, 0, 0, 0, 0, 0, -1 }; // Out of range index.
        EXPECT_FALSE(ValidateMappedDataForMesh(1, indices, cube, interpolation, true));
    };

    // Vertex
    vertexDataTest(pxr::UsdGeomTokens->vertex);
    // Varying
    vertexDataTest(pxr::UsdGeomTokens->varying);

    // Uniform
    EXPECT_TRUE(ValidateMappedDataForMesh(6, indices, cube, pxr::UsdGeomTokens->uniform, false));
    EXPECT_TRUE(ValidateMappedDataForMesh(
        7, indices, cube, pxr::UsdGeomTokens->uniform, false)); // still workable.
    EXPECT_FALSE(ValidateMappedDataForMesh(
        5, indices, cube, pxr::UsdGeomTokens->uniform, false)); // Not enough data.
    indices = { 0, 0, 0, 0, 0, 0 };                             // 6 indices, what is expected.
    EXPECT_TRUE(ValidateMappedDataForMesh(1, indices, cube, pxr::UsdGeomTokens->uniform, true));
    indices = { 0, 0, 0, 0, 0, 0, 0 }; // 7 indices, too much, but still usable.
    EXPECT_TRUE(ValidateMappedDataForMesh(1, indices, cube, pxr::UsdGeomTokens->uniform, true));
    indices = { 0, 0, 0, 0, 0 }; // 5 indices, unusable.
    EXPECT_FALSE(ValidateMappedDataForMesh(1, indices, cube, pxr::UsdGeomTokens->uniform, true));
    indices = { 2, 0, 0, 0, 0, 0 }; // Out of range index.
    EXPECT_FALSE(ValidateMappedDataForMesh(1, indices, cube, pxr::UsdGeomTokens->uniform, true));
    indices = { -1, 0, 0, 0, 0, 0 }; // Out of range index.
    EXPECT_FALSE(ValidateMappedDataForMesh(1, indices, cube, pxr::UsdGeomTokens->uniform, true));

    // FaceVarying
    EXPECT_TRUE(
        ValidateMappedDataForMesh(24, indices, cube, pxr::UsdGeomTokens->faceVarying, false));
    EXPECT_TRUE(ValidateMappedDataForMesh(
        25, indices, cube, pxr::UsdGeomTokens->faceVarying, false)); // still workable.
    EXPECT_FALSE(ValidateMappedDataForMesh(
        23, indices, cube, pxr::UsdGeomTokens->faceVarying, false)); // Not enough data.

    indices = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }; // 24 indices, what is expected.
    EXPECT_TRUE(ValidateMappedDataForMesh(1, indices, cube, pxr::UsdGeomTokens->faceVarying, true));
    indices = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }; // 25 indices, too much, but still usable.
    EXPECT_TRUE(ValidateMappedDataForMesh(1, indices, cube, pxr::UsdGeomTokens->faceVarying, true));
    indices = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }; // < 24 indices, unusable.
    EXPECT_FALSE(
        ValidateMappedDataForMesh(1, indices, cube, pxr::UsdGeomTokens->faceVarying, true));
    indices = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }; // Out of range index.
    EXPECT_FALSE(
        ValidateMappedDataForMesh(1, indices, cube, pxr::UsdGeomTokens->faceVarying, true));
    indices = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }; // Out of range index.
    EXPECT_FALSE(
        ValidateMappedDataForMesh(1, indices, cube, pxr::UsdGeomTokens->faceVarying, true));
}

TEST(TranslationUtilsTest, UniqueNameGenerator)
{
    UniqueNameGenerator nameGenerator;
    EXPECT_EQ(nameGenerator.GetName("test"), "test");
    EXPECT_EQ(nameGenerator.GetName("test"), "test1");
    EXPECT_EQ(nameGenerator.GetName("test1"), "test2");
    EXPECT_EQ(nameGenerator.GetName("test"), "test3");

    EXPECT_EQ(nameGenerator.GetName("test9"), "test9");
    EXPECT_EQ(nameGenerator.GetName("test9"), "test10");

    EXPECT_EQ(nameGenerator.GetName("test01"), "test01");
    EXPECT_EQ(nameGenerator.GetName("test01"), "test02");

    EXPECT_EQ(nameGenerator.GetName("test09"), "test09");
    EXPECT_EQ(nameGenerator.GetName("test09"), "test11"); // test10 already exist

    EXPECT_EQ(nameGenerator.GetName("%&#000999"), "%&#000999");
    EXPECT_EQ(nameGenerator.GetName("%&#000999"), "%&#001000");

    EXPECT_EQ(nameGenerator.GetName("9999"), "9999");
    EXPECT_EQ(nameGenerator.GetName("9999"), "10000");

    EXPECT_EQ(nameGenerator.GetName(""), "");
    EXPECT_EQ(nameGenerator.GetName(""), "1");

    EXPECT_EQ(nameGenerator.GetName("18446744073709551615"), "18446744073709551615");
    EXPECT_EQ(nameGenerator.GetName("18446744073709551615"), "0");
}

TEST(TranslationUtilsTest, HasUnicodeTest)
{
    std::string utf8Str
        = u8"איך הקליטה Ξεσκεπάζω τὴν ψυχοφθόρα βδελυγμία Zwölf Boxkämpfer Sævör grét áðan því "
          u8"úlpan var ónýt いろはにほへ イロハニホヘト พูดจาให้จ๊ะๆ จ๋าๆ น่าฟังเอย";
    const char* utf8EncodedStr = "\xff"
                                 "f";
    std::string asciiStr = "h0h0h0_*+1-~.str";
    EXPECT_TRUE(MaxUsd::HasUnicodeCharacter(utf8Str)) << "failed to detect unicode utf8 characters";
    EXPECT_TRUE(MaxUsd::HasUnicodeCharacter(std::string(utf8EncodedStr)))
        << "failed to detect unicode encoded characters";
    EXPECT_FALSE(MaxUsd::HasUnicodeCharacter(asciiStr))
        << "detected unicode character in ascii only string";
}

TEST(TranslationUtilsTest, GetOffsetTimeCodeTest)
{
    auto ticksPerSecond = 160;
    auto maxFPS = 30.0;

    auto usdStartTimeCode = 101.0;
    auto usdEndTimeCode = 129.0;
    auto usdFPS = 24.0;

    auto stage = pxr::UsdStage::CreateInMemory();
    stage->SetStartTimeCode(usdStartTimeCode);
    stage->SetEndTimeCode(usdEndTimeCode);
    stage->SetFramesPerSecond(usdFPS);

    TimeValue startAnimationTimeValue = ticksPerSecond * 0;
    EXPECT_FLOAT_EQ(
        static_cast<float>(MaxUsd::GetOffsetTimeCode(stage, startAnimationTimeValue, 0).GetValue()),
        static_cast<float>(usdStartTimeCode))
        << "At time" + std::to_string(startAnimationTimeValue)
            + "for start from 0.0 the offset time code should be "
            + std::to_string(usdStartTimeCode);
    TimeValue endAnimationTimeValue = static_cast<TimeValue>(
        startAnimationTimeValue + ticksPerSecond * (28.0 * (maxFPS / usdFPS)));
    EXPECT_FLOAT_EQ(
        static_cast<float>(MaxUsd::GetOffsetTimeCode(stage, endAnimationTimeValue, 0).GetValue()),
        static_cast<float>(usdEndTimeCode))
        << "At time " + std::to_string(endAnimationTimeValue)
            + " for end from 0.0 the offset time code should be " + std::to_string(usdEndTimeCode);

    startAnimationTimeValue = ticksPerSecond * -10;
    EXPECT_FLOAT_EQ(
        static_cast<float>(
            MaxUsd::GetOffsetTimeCode(stage, startAnimationTimeValue, -10.0).GetValue()),
        static_cast<float>(usdStartTimeCode))
        << "At time" + std::to_string(startAnimationTimeValue)
            + "for start from -10.0 the offset time code should be "
            + std::to_string(usdStartTimeCode);
    endAnimationTimeValue
        = startAnimationTimeValue + static_cast<int>(ticksPerSecond * (28.0 * (maxFPS / usdFPS)));
    EXPECT_FLOAT_EQ(
        static_cast<float>(
            MaxUsd::GetOffsetTimeCode(stage, endAnimationTimeValue, -10.0).GetValue()),
        static_cast<float>(usdEndTimeCode))
        << "At time " + std::to_string(endAnimationTimeValue)
            + " for end from -10.0 the offset time code should be "
            + std::to_string(usdEndTimeCode);

    startAnimationTimeValue = ticksPerSecond * 0;
    EXPECT_FLOAT_EQ(
        static_cast<float>(
            MaxUsd::GetOffsetTimeCode(stage, startAnimationTimeValue, 0, 14.0).GetValue()),
        static_cast<float>(usdStartTimeCode))
        << "At time" + std::to_string(startAnimationTimeValue)
            + "for start from 0.0 and length 14.0 the offset time code should be "
            + std::to_string(usdStartTimeCode);
    endAnimationTimeValue = startAnimationTimeValue + ticksPerSecond * 14;
    EXPECT_FLOAT_EQ(
        static_cast<float>(
            MaxUsd::GetOffsetTimeCode(stage, endAnimationTimeValue, 0, 14.0).GetValue()),
        static_cast<float>(usdEndTimeCode))
        << "At time " + std::to_string(endAnimationTimeValue)
            + " for end from 0.0 and length 14.0 the offset time code should be "
            + std::to_string(usdEndTimeCode);

    startAnimationTimeValue = ticksPerSecond * 10;
    EXPECT_FLOAT_EQ(
        static_cast<float>(
            MaxUsd::GetOffsetTimeCode(stage, startAnimationTimeValue, 10.0, 14.0).GetValue()),
        static_cast<float>(usdStartTimeCode))
        << "At time" + std::to_string(startAnimationTimeValue)
            + "for start from 10.0 and length 14.0 the offset time code should be "
            + std::to_string(usdStartTimeCode);
    endAnimationTimeValue = startAnimationTimeValue + ticksPerSecond * 14;
    EXPECT_FLOAT_EQ(
        static_cast<float>(
            MaxUsd::GetOffsetTimeCode(stage, endAnimationTimeValue, 10.0, 14.0).GetValue()),
        static_cast<float>(usdEndTimeCode))
        << "At time " + std::to_string(endAnimationTimeValue)
            + " for end from 10.0 and length 14.0 the offset time code should be "
            + std::to_string(usdEndTimeCode);

    startAnimationTimeValue = ticksPerSecond * -10;
    EXPECT_FLOAT_EQ(
        static_cast<float>(
            MaxUsd::GetOffsetTimeCode(stage, startAnimationTimeValue, -10.0, 14.0).GetValue()),
        static_cast<float>(usdStartTimeCode))
        << "At time" + std::to_string(startAnimationTimeValue)
            + "for start from -10.0 and length 14.0 the offset time code should be "
            + std::to_string(usdStartTimeCode);
    endAnimationTimeValue = startAnimationTimeValue + ticksPerSecond * 14;
    EXPECT_FLOAT_EQ(
        static_cast<float>(
            MaxUsd::GetOffsetTimeCode(stage, endAnimationTimeValue, -10.0, 14.0).GetValue()),
        static_cast<float>(usdEndTimeCode))
        << "At time " + std::to_string(endAnimationTimeValue)
            + " for end from -10.0 and length 14.0 the offset time code should be "
            + std::to_string(usdEndTimeCode);

    startAnimationTimeValue = ticksPerSecond * 10;
    EXPECT_FLOAT_EQ(
        static_cast<float>(
            MaxUsd::GetOffsetTimeCode(stage, startAnimationTimeValue, 10.0, -14.0).GetValue()),
        static_cast<float>(usdStartTimeCode))
        << "At time" + std::to_string(startAnimationTimeValue)
            + "for start from 10.0 and length -14.0 the offset time code should be "
            + std::to_string(usdStartTimeCode);
    endAnimationTimeValue = startAnimationTimeValue + ticksPerSecond * -14;
    EXPECT_FLOAT_EQ(
        static_cast<float>(
            MaxUsd::GetOffsetTimeCode(stage, endAnimationTimeValue, 10.0, -14.0).GetValue()),
        static_cast<float>(usdEndTimeCode))
        << "At time " + std::to_string(endAnimationTimeValue)
            + " for end from 10.0 and length -14.0 the offset time code should be "
            + std::to_string(usdEndTimeCode);
}

TEST(TranslationUtilsTest, GetPrimOrAncestorWithKind)
{
    const auto assembly = pxr::TfToken("assembly");
    const auto subcomponent = pxr::TfToken("subcomponent");
    const auto component = pxr::TfToken("component");

    // Setup a simple stage with a hierarchy and some kinds.
    auto stage = pxr::UsdStage::CreateInMemory();
    auto foo = stage->DefinePrim(pxr::SdfPath("/foo"), pxr::TfToken("Xform"));
    pxr::UsdModelAPI(foo).SetKind(assembly);
    auto bar = stage->DefinePrim(pxr::SdfPath("/foo/bar"), pxr::TfToken("Xform"));
    pxr::UsdModelAPI(bar).SetKind(subcomponent);
    auto baz = stage->DefinePrim(pxr::SdfPath("/foo/bar/baz"), pxr::TfToken("Xform"));
    // No kind on baz
    auto qux = stage->DefinePrim(pxr::SdfPath("/foo/bar/baz/qux"), pxr::TfToken("Xform"));
    pxr::UsdModelAPI(qux).SetKind(component);

    // Tests going up the hierarchy.
    ASSERT_EQ(foo, MaxUsd::GetPrimOrAncestorWithKind(foo, assembly));
    ASSERT_FALSE(MaxUsd::GetPrimOrAncestorWithKind(foo, subcomponent).IsValid());

    ASSERT_EQ(bar, MaxUsd::GetPrimOrAncestorWithKind(bar, subcomponent));
    ASSERT_EQ(foo, MaxUsd::GetPrimOrAncestorWithKind(bar, assembly));
    ASSERT_FALSE(MaxUsd::GetPrimOrAncestorWithKind(bar, component).IsValid());

    ASSERT_EQ(bar, MaxUsd::GetPrimOrAncestorWithKind(baz, subcomponent));
    ASSERT_EQ(foo, MaxUsd::GetPrimOrAncestorWithKind(baz, assembly));
    ASSERT_FALSE(MaxUsd::GetPrimOrAncestorWithKind(baz, component).IsValid());

    ASSERT_EQ(foo, MaxUsd::GetPrimOrAncestorWithKind(qux, assembly));
    ASSERT_EQ(bar, MaxUsd::GetPrimOrAncestorWithKind(qux, subcomponent));
    ASSERT_EQ(qux, MaxUsd::GetPrimOrAncestorWithKind(qux, component));

    // Test kind inherit
    ASSERT_EQ(qux, MaxUsd::GetPrimOrAncestorWithKind(qux, pxr::TfToken("model")));
    // baz has no kind, bar is a subcomponent, which isnt a model, foo is an assembly, which is a
    // model.
    ASSERT_EQ(foo, MaxUsd::GetPrimOrAncestorWithKind(baz, pxr::TfToken("model")));
}

TEST(TranslationUtilsTest, GetFirstNonInstanceProxyPrimAncestor)
{
    const auto assembly = pxr::TfToken("assembly");

    // Setup a simple stage with a hierarchy and some kinds.
    auto stage = pxr::UsdStage::CreateInMemory();
    auto foo = stage->DefinePrim(pxr::SdfPath("/foo"));
    pxr::UsdModelAPI(foo).SetKind(assembly);
    auto fooCube = stage->DefinePrim(pxr::SdfPath("/foo/cube"), pxr::TfToken("Mesh"));
    auto cubeBox = stage->DefinePrim(pxr::SdfPath("/foo/cube/box"), pxr::TfToken("Mesh"));
    auto fooCubeInstance
        = stage->DefinePrim(pxr::SdfPath("/foo/fooCubeInstance"), pxr::TfToken("Xform"));
    fooCubeInstance.GetPrim().GetInherits().AddInherit(fooCube.GetPath());
    fooCubeInstance.SetInstanceable(true);

    auto fooCubeInstanceBox = stage->GetPrimAtPath(pxr::SdfPath("/foo/fooCubeInstance/box"));

    // foo's first non instance proxy prim ancestor is itself.
    ASSERT_EQ(foo, MaxUsd::GetFirstNonInstanceProxyPrimAncestor(foo));

    // fooCubeInstanceBox's first non instance proxy prim ancestor is fooCubeInstance.
    ASSERT_EQ(fooCubeInstance, MaxUsd::GetFirstNonInstanceProxyPrimAncestor(fooCubeInstanceBox));

    // fooCubeInstance's first non instance proxy prim ancestor is itsef.
    ASSERT_EQ(fooCubeInstance, MaxUsd::GetFirstNonInstanceProxyPrimAncestor(fooCubeInstance));

    // Combine with GetPrimOrAncestorWithKind should lead to foo.
    ASSERT_EQ(
        foo,
        MaxUsd::GetPrimOrAncestorWithKind(
            MaxUsd::GetFirstNonInstanceProxyPrimAncestor(fooCubeInstanceBox), assembly));
}