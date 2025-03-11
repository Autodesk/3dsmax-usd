//
// Copyright 2024 Autodesk
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

#include "RenderDelegate/ConsolidatedGizmoRenderItem.h"
#include "pxr/pxr.h" // PXR_VERSION

#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdLux/shapingAPI.h>

// Light gizmos are only supported in version with USD 23.11+
#if PXR_VERSION >= 2311

#include "TestHelpers.h"

#include <RenderDelegate/HdMaxEngine.h>

#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>

#include <pxr/base/gf/rotation.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/sphereLight.h>

#include <Graphics/SimpleRenderGeometry.h>

#include <gtest/gtest.h>
#include <max.h>

namespace {
const Box3 baseGizmoBb
    = { { -3.83829856, -3.83829856, -3.93700695 }, { 3.83829856, 3.83829856, 3.93700790 } };
}

class LightGizmosTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        // Keep track of current units so we can set them back properly. Just making sure we always
        // run the test with the same units.
        GetSystemUnitInfo(&unitType, &unitScale);
        SetSystemUnitInfo(UNITS_INCHES, 1.0f);
    }
    void TearDown() override
    {
        GetCOREInterface()->FileReset(TRUE);
        SetSystemUnitInfo(unitType, unitScale);
    }
    int   unitType;
    float unitScale;
};

// Helper to test light gizmos' geometry/bounding box and visibility group.
void TestLightRenderItem(
    const pxr::UsdStageRefPtr& stage,
    int                        expectedVertCount,
    int                        expectedLineCount,
    const Box3&                expectedBb)
{
    HdMaxEngine                      testEngine;
    MockRenderItemDecoratorContainer renderItems;

    TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull });

    auto& renderDelegate = testEngine.GetRenderDelegate();
    auto  renderData = renderDelegate->GetMeshRenderDataIdMap();

    ASSERT_EQ(1, renderData.size());
    auto it = renderData.find(pxr::SdfPath("/light"));
    ASSERT_TRUE(it != renderData.end());

    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());

    auto& gizmoRenderItem = renderItems.At(0);
    ASSERT_EQ(
        MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Gizmo,
        gizmoRenderItem.GetVisibilityGroup());

    auto shadedGeometry = GetRenderItemGeometry(gizmoRenderItem, true);
    EXPECT_EQ(
        shadedGeometry,
        renderDelegate->GetMeshRenderData(it->second)
            .wireframe.geometry->GetSimpleRenderGeometry());

    ASSERT_NE(nullptr, shadedGeometry);
    EXPECT_EQ(MaxSDK::Graphics::PrimitiveLineList, shadedGeometry->GetPrimitiveType());
    ASSERT_TRUE(shadedGeometry->GetVertexBuffer(0).IsValid());
    EXPECT_EQ(expectedVertCount, shadedGeometry->GetVertexCount());
    EXPECT_TRUE(shadedGeometry->GetIndexBuffer().IsValid());
    EXPECT_EQ(expectedLineCount, shadedGeometry->GetPrimitiveCount());

    auto bb = GetBoundingBox(gizmoRenderItem, true);

    EXPECT_TRUE(BoundingBoxesAreEquivalent(bb, expectedBb));
}

TEST_F(LightGizmosTest, RectLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.0254);
    auto rectLight = pxr::UsdLuxRectLight::Define(stage, pxr::SdfPath("/light"));

    TestLightRenderItem(stage, 54, 212, baseGizmoBb);

    rectLight.CreateWidthAttr().Set(10.0f);
    rectLight.CreateHeightAttr().Set(20.0f);

    Box3 newBB = { { -5.0, -10.0, -3.93700695 }, { 5.0, 10.0, 3.93700695 } };

    TestLightRenderItem(stage, 54, 212, newBB);
}

TEST_F(LightGizmosTest, DiskLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       diskLight = pxr::UsdLuxDiskLight::Define(stage, pxr::SdfPath("/light"));
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.0254);
    TestLightRenderItem(stage, 78, 236, baseGizmoBb);

    diskLight.CreateRadiusAttr().Set(10.f);

    Box3 newBB = { { -10.0, -10.0, -3.93700695 }, { 10.0, 10.0, 3.93700695 } };

    TestLightRenderItem(stage, 78, 236, newBB);
}

TEST_F(LightGizmosTest, CylinderLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       cylinderLight = pxr::UsdLuxCylinderLight::Define(stage, pxr::SdfPath("/light"));
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.0254);
    TestLightRenderItem(stage, 158, 604, baseGizmoBb);

    // X axis
    cylinderLight.CreateLengthAttr().Set(20.f);
    // Y/Z axis
    cylinderLight.CreateRadiusAttr().Set(10.f);

    Box3 newBB = { { -10.0, -9.84807873, -10.0 }, { 10.0, 9.84807777, 10.0 } };

    TestLightRenderItem(stage, 158, 604, newBB);
}

TEST_F(LightGizmosTest, SphereLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       sphereLight = pxr::UsdLuxSphereLight::Define(stage, pxr::SdfPath("/light"));
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.0254);
    TestLightRenderItem(stage, 134, 292, baseGizmoBb);

    sphereLight.CreateRadiusAttr().Set(10.f);

    Box3 newBB = { { -10.0, -10.0, -10.0 }, { 10.0, 10.0, 10.0 } };

    TestLightRenderItem(stage, 134, 292, newBB);
}

TEST_F(LightGizmosTest, DistantLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       distantLight = pxr::UsdLuxDistantLight::Define(stage, pxr::SdfPath("/light"));
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.0254);
    // Distant lights have their own base shape.
    Box3 bb
        = { { -4.30036831, -4.72040319, -5.67147160 }, { 4.84014225, 4.65517282, -0.631725729 } };
    TestLightRenderItem(stage, 172, 532, bb);
}

TEST_F(LightGizmosTest, DomeLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       domeLight = pxr::UsdLuxDomeLight::Define(stage, pxr::SdfPath("/light"));
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.0254);
    // No light shape, just the base gizmo.
    TestLightRenderItem(stage, 50, 208, baseGizmoBb);
}

TEST_F(LightGizmosTest, ShapingApiConeLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       coneLight = pxr::UsdLuxSphereLight::Define(stage, pxr::SdfPath("/light"));
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.0254);
    auto shapingApi = pxr::UsdLuxShapingAPI::Apply(coneLight.GetPrim());
    shapingApi.CreateShapingConeAngleAttr().Set(30);

    // Lights with the shapingApi display as cones.
    Box3 bb
        = { { -3.93699980, -3.93700027, -6.81908417 }, { 3.93699980, 3.93699932, 0.500000000 } };

    TestLightRenderItem(stage, 101, 132, bb);
}

TEST_F(LightGizmosTest, LightGizmoUnits)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       sphereLight = pxr::UsdLuxSphereLight::Define(stage, pxr::SdfPath("/light"));
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.0254);
    TestLightRenderItem(stage, 134, 292, baseGizmoBb);

    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.01);

    const auto factor = 2.54;
    const Box3 cmBB = {
        { baseGizmoBb.pmin.x * factor, baseGizmoBb.pmin.y * factor, baseGizmoBb.pmin.z * factor },
        { baseGizmoBb.pmax.x * factor, baseGizmoBb.pmax.y * factor, baseGizmoBb.pmax.z * factor }
    };
    TestLightRenderItem(stage, 134, 292, cmBB);
}

TEST_F(LightGizmosTest, LightGizmoScaling)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       sphereLight = pxr::UsdLuxSphereLight::Define(stage, pxr::SdfPath("/light"));
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.0254);

    HdMaxEngine testEngine;

    testEngine.GetRenderDelegate()->GetDisplaySettings().SetLightGizmoScale(1.0);

    MockRenderItemDecoratorContainer renderItems;

    TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull });

    auto& gizmoRenderItem = renderItems.At(0);
    auto  bb = GetBoundingBox(gizmoRenderItem, true);
    EXPECT_TRUE(BoundingBoxesAreEquivalent(bb, baseGizmoBb));

    const auto scaling = 2.0;

    testEngine.GetRenderDelegate()->GetDisplaySettings().SetLightGizmoScale(scaling);
    TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull });

    auto gizmoRenderItem2 = renderItems.At(0);
    auto bb2 = GetBoundingBox(gizmoRenderItem, true);

    const Box3 doubledBb = {
        { baseGizmoBb.pmin.x * scaling,
          baseGizmoBb.pmin.y * scaling,
          baseGizmoBb.pmin.z * scaling },
        { baseGizmoBb.pmax.x * scaling, baseGizmoBb.pmax.y * scaling, baseGizmoBb.pmax.z * scaling }
    };
    EXPECT_TRUE(BoundingBoxesAreEquivalent(bb2, doubledBb));
}

TEST_F(LightGizmosTest, LightGizmoConsolidation)
{
    HdMaxConsolidator::Config consolidationConfig;
    consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
    consolidationConfig.maxTriangles = 5000;
    consolidationConfig.maxCellSize = 10000;
    consolidationConfig.maxInstanceCount = 1000;
    consolidationConfig.staticDelay = 0;

    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       sphereLight = pxr::UsdLuxDomeLight::Define(stage, pxr::SdfPath("/light"));
    auto       sphereLight2 = pxr::UsdLuxDomeLight::Define(stage, pxr::SdfPath("/light2"));
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.0254);

    // We are testing that gizmos are also consolidated, not testing consolidation behavior in
    // detail, as this is well tested elsewhere.
    HdMaxEngine             testEngine;
    MockRenderItemContainer renderItems;

    // Render with to both wireframe and shaded items.
    TestRender(
        stage,
        testEngine,
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    auto& renderDelegate = testEngine.GetRenderDelegate();
    auto  renderData = renderDelegate->GetMeshRenderDataIdMap();

    ASSERT_EQ(2, renderData.size());
    auto it = renderData.find(pxr::SdfPath("/light"));
    ASSERT_TRUE(it != renderData.end());
    auto it2 = renderData.find(pxr::SdfPath("/light2"));
    ASSERT_TRUE(it2 != renderData.end());

    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());

    auto gizmoRenderItem = renderItems.GetRenderItem(0);
    ASSERT_EQ(
        MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Gizmo,
        gizmoRenderItem.GetVisibilityGroup());

    const auto& customItem
        = static_cast<const MaxSDK::Graphics::CustomRenderItemHandle&>(gizmoRenderItem);
    const auto usdRenderItem
        = static_cast<ConsolidatedGizmoRenderItem*>(customItem.GetCustomeImplementation());

    auto shadedGeometry = static_cast<MaxSDK::Graphics::SimpleRenderGeometry*>(
        usdRenderItem->GetRenderGeometry().GetPointer());

    ASSERT_NE(nullptr, shadedGeometry);
    EXPECT_EQ(MaxSDK::Graphics::PrimitiveLineList, shadedGeometry->GetPrimitiveType());
    ASSERT_TRUE(shadedGeometry->GetVertexBuffer(0).IsValid());
    EXPECT_EQ(100, shadedGeometry->GetVertexCount());
    EXPECT_TRUE(shadedGeometry->GetIndexBuffer().IsValid());
    EXPECT_EQ(416, shadedGeometry->GetPrimitiveCount());

    auto points = shadedGeometry->GetVertexBuffer(HdMaxMeshRenderData::PointsBuffer);
    auto rawPoints = reinterpret_cast<Point3*>(points.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
    Box3 bb;
    bb.IncludePoints(rawPoints, static_cast<int>(points.GetNumberOfVertices()), nullptr);
    points.Unlock();

    EXPECT_TRUE(BoundingBoxesAreEquivalent(bb, baseGizmoBb));
}

TEST_F(LightGizmosTest, LightGizmoInstanced)
{
    auto       testDataPath = GetTestDataPath();
    const auto filePath = testDataPath.append("light_gizmo_instances.usda");
    const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.0254);

    HdMaxEngine             testEngine;
    MockRenderItemContainer renderItems;

    TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull });

    // The file contains a rect light instanced 3 times.

    // Don't test instancing in detail, it is tested else where, just making sure the gizmo's got
    // instanced and are in the right vis group.

    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());
    auto instancedGizmos = renderItems.GetRenderItem(0);

    ASSERT_EQ(
        MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Gizmo,
        instancedGizmos.GetVisibilityGroup());
    auto renderData = testEngine.GetRenderDelegate()->GetMeshRenderDataIdMap();

#ifdef IS_MAX_BETA
    auto it
        = renderData.find(pxr::SdfPath("/root/PhotometricLight001/proto_PhotometricLight001_id0"));
#else
    auto it
        = renderData.find(pxr::SdfPath("/root/PhotometricLight001.proto_PhotometricLight001_id0"));
#endif

    ASSERT_TRUE(it != renderData.end());
    auto& prototypeRenderData = testEngine.GetRenderDelegate()->GetMeshRenderData(it->second);
    auto  transforms = prototypeRenderData.instancer->GetTransforms();
    EXPECT_EQ(3, transforms.size());
}

#endif