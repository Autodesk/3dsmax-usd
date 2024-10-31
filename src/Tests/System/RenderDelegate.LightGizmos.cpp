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

#include "pxr/pxr.h" // PXR_VERSION

// Light gizmos are only supported in version with USD 23.11+
#if PXR_VERSION >= 2311

#include "TestHelpers.h"

#include <RenderDelegate/HdMaxEngine.h>
#include <RenderDelegate/HdMaxRenderData.h>

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
    = { { -0.969846368, -0.925416648, -0.998026729 }, { 0.969846368, 0.984807789, 1.00000000 } };
}

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
    auto  renderData = renderDelegate->GetRenderDataIdMap();

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
        renderDelegate->GetRenderData(it->second).wireframe.geometry->GetSimpleRenderGeometry());

    ASSERT_NE(nullptr, shadedGeometry);
    EXPECT_EQ(MaxSDK::Graphics::PrimitiveLineList, shadedGeometry->GetPrimitiveType());
    ASSERT_TRUE(shadedGeometry->GetVertexBuffer(0).IsValid());
    EXPECT_EQ(expectedVertCount, shadedGeometry->GetVertexCount());
    EXPECT_TRUE(shadedGeometry->GetIndexBuffer().IsValid());
    EXPECT_EQ(expectedLineCount, shadedGeometry->GetPrimitiveCount());

    auto bb = GetBoundingBox(gizmoRenderItem, true);

    EXPECT_TRUE(BoundingBoxesAreEquivalent(bb, expectedBb));
}

TEST(USDRenderDelegateLightGizmos, RectLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       rectLight = pxr::UsdLuxRectLight::Define(stage, pxr::SdfPath("/light"));

    TestLightRenderItem(stage, 51, 202, baseGizmoBb);

    rectLight.CreateWidthAttr().Set(10.0f);
    rectLight.CreateHeightAttr().Set(20.0f);

    Box3 newBB = { { -5.0, -10.0, -0.998026729 }, { 5.0, 10.0, 1.00000000 } };

    TestLightRenderItem(stage, 51, 202, newBB);
}

TEST(USDRenderDelegateLightGizmos, DiskLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       diskLight = pxr::UsdLuxDiskLight::Define(stage, pxr::SdfPath("/light"));

    TestLightRenderItem(stage, 75, 226, baseGizmoBb);

    diskLight.CreateRadiusAttr().Set(10.f);

    Box3 newBB = { { -10.0, -10.0, -0.998026729 }, { 10.0, 10.0, 1.0 } };

    TestLightRenderItem(stage, 75, 226, newBB);
}

TEST(USDRenderDelegateLightGizmos, CylinderLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       cylinderLight = pxr::UsdLuxCylinderLight::Define(stage, pxr::SdfPath("/light"));

    TestLightRenderItem(stage, 155, 594, baseGizmoBb);

    // X axis
    cylinderLight.CreateLengthAttr().Set(20.f);
    // Y/Z axis
    cylinderLight.CreateRadiusAttr().Set(10.f);

    Box3 newBB = { { -10.0, -9.84807873, -10.0 }, { 10.0, 9.84807777, 10.0 } };

    TestLightRenderItem(stage, 155, 594, newBB);
}

TEST(USDRenderDelegateLightGizmos, SphereLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       sphereLight = pxr::UsdLuxSphereLight::Define(stage, pxr::SdfPath("/light"));

    TestLightRenderItem(stage, 131, 282, baseGizmoBb);

    sphereLight.CreateRadiusAttr().Set(10.f);

    Box3 newBB = { { -10.0, -10.0, -10.0 }, { 10.0, 10.0, 10.0 } };

    TestLightRenderItem(stage, 131, 282, newBB);
}

TEST(USDRenderDelegateLightGizmos, DistantLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       sphereLight = pxr::UsdLuxDistantLight::Define(stage, pxr::SdfPath("/light"));

    // No light shape, just the base gizmo.
    TestLightRenderItem(stage, 47, 198, baseGizmoBb);
}

TEST(USDRenderDelegateLightGizmos, DomeLight)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    auto       sphereLight = pxr::UsdLuxDomeLight::Define(stage, pxr::SdfPath("/light"));

    // No light shape, just the base gizmo.
    TestLightRenderItem(stage, 47, 198, baseGizmoBb);
}

TEST(USDRenderDelegateLightGizmos, LightGizmoConsolidation)
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
    auto  renderData = renderDelegate->GetRenderDataIdMap();

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

    auto shadedGeometry = GetRenderItemGeometry(gizmoRenderItem, false);

    ASSERT_NE(nullptr, shadedGeometry);
    EXPECT_EQ(MaxSDK::Graphics::PrimitiveLineList, shadedGeometry->GetPrimitiveType());
    ASSERT_TRUE(shadedGeometry->GetVertexBuffer(0).IsValid());
    EXPECT_EQ(94, shadedGeometry->GetVertexCount());
    EXPECT_TRUE(shadedGeometry->GetIndexBuffer().IsValid());
    EXPECT_EQ(396, shadedGeometry->GetPrimitiveCount());

    auto bb = GetBoundingBox(gizmoRenderItem, false);

    EXPECT_TRUE(BoundingBoxesAreEquivalent(bb, baseGizmoBb));
}

TEST(USDRenderDelegateLightGizmos, LightGizmoInstanced)
{
    auto       testDataPath = GetTestDataPath();
    const auto filePath = testDataPath.append("light_gizmo_instances.usda");
    const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

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
    auto renderData = testEngine.GetRenderDelegate()->GetRenderDataIdMap();

#ifdef IS_MAX2026_OR_GREATER
    auto it
        = renderData.find(pxr::SdfPath("/root/PhotometricLight001/proto_PhotometricLight001_id0"));
#else
    auto it
        = renderData.find(pxr::SdfPath("/root/PhotometricLight001.proto_PhotometricLight001_id0"));
#endif

    ASSERT_TRUE(it != renderData.end());
    auto& prototypeRenderData = testEngine.GetRenderDelegate()->GetRenderData(it->second);
    auto  transforms = prototypeRenderData.instancer->GetTransforms();
    EXPECT_EQ(3, transforms.size());
}

#endif