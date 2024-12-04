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
#include "TestHelpers.h"

#include <RenderDelegate/HdMaxColorMaterial.h>
#include <RenderDelegate/HdMaxEngine.h>

#include <MaxUsd/Utilities/TypeUtils.h>

#include <Graphics/GeometryRenderItemHandle.h>

TEST(ViewportDisplayOptions, MaterialAndPerformance)
{
    // Currently testing the performance mode option and the material option
    // together because both can result in changes on the viewport material.

    auto testDataPath = GetTestDataPath();

    const auto filePath = testDataPath.append("box_sample.usda");
    const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

    auto expectColorsEqual = [](const AColor& a, const AColor& b) {
        EXPECT_FLOAT_EQ(a.r, b.r);
        EXPECT_FLOAT_EQ(a.g, b.g);
        EXPECT_FLOAT_EQ(a.b, b.b);
    };

    HdMaxEngine                      testEngine;
    MockRenderItemDecoratorContainer renderItems;
    MultiMtl*                        multiMat = NewEmptyMultiMtl();

    // Test display mode : USD Preview Surface.
    auto& displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
    displaySettings.SetDisplayMode(
        HdMaxDisplaySettings::USDPreviewSurface, testEngine.GetChangeTracker());
    auto wireColor = Color(1, 0, 0);

    TestRender(stage, testEngine, renderItems, 0, multiMat);

    auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

    // Check points valid.
    EXPECT_TRUE(simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer).IsValid());
    // Check normals valid.
    EXPECT_TRUE(simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer).IsValid());
    // Check uvs valid.
    EXPECT_TRUE(simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer).IsValid());
    // Check multi-material not populated.
    EXPECT_EQ(0, multiMat->NumSubMtls());

    // Check the nitrous material. These are hard to inspect, but there are 2 cases, either
    // a color is displayed, in which case a StandardMaterialHandle would have been used,
    // or the nitrous handle was converted from a Mtl* and it is just a BaseMaterialHandle.
    // GetCustomMaterial() returns a new BaseMaterialHandle and just sets the pointer, so we
    // can't dynamic cast to check the type. Instead, assume a StandardMaterialHandle and look
    // at the diffuse color. If all white, we can assume the pointer wasn't actual from a
    // StandardMaterialHandle. This is what we expect here, as materials are enabled and displayed.
    auto                                     customMaterial = renderItems.At(0).GetCustomMaterial();
    MaxSDK::Graphics::StandardMaterialHandle stdMaterialHandle;
    stdMaterialHandle.SetPointer(customMaterial.GetPointer());
    auto diffuseColor = stdMaterialHandle.GetDiffuse();
    auto expectedDiffuseColor = AColor(1.0, 1.0, 1.0); // Expect color not set.
    expectColorsEqual(diffuseColor, expectedDiffuseColor);
    auto ambiantColor = stdMaterialHandle.GetAmbient();
    auto expectedAmbientColor = AColor(1.0, 1.0, 1.0); // Expect color not set.
    expectColorsEqual(ambiantColor, expectedAmbientColor);

    // Test display mode : USD Display Colors.
    displaySettings.SetDisplayMode(
        HdMaxDisplaySettings::USDDisplayColor, testEngine.GetChangeTracker());
    TestRender(stage, testEngine, renderItems, 0, multiMat);

    simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

    // Check points valid.
    EXPECT_TRUE(simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer).IsValid());
    // Check normals valid.
    EXPECT_TRUE(simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer).IsValid());
    // Check uvs not used.
    EXPECT_FALSE(simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer).IsValid());
    // Check multimaterial not populated.
    EXPECT_EQ(0, multiMat->NumSubMtls());
    // Expecting a standard material, to represent the USD displayColor.
    customMaterial = renderItems.At(0).GetCustomMaterial();
    stdMaterialHandle.SetPointer(customMaterial.GetPointer());
    diffuseColor = stdMaterialHandle.GetDiffuse();
    expectedDiffuseColor = HdMaxColorMaterial::GetDiffuseColor(0.6, 0.89411765, 0.8392157);
    expectColorsEqual(diffuseColor, expectedDiffuseColor);
    ambiantColor = stdMaterialHandle.GetAmbient();
    expectedAmbientColor = HdMaxColorMaterial::GetAmbientColor(0.6, 0.89411765, 0.8392157);
    expectColorsEqual(ambiantColor, expectedAmbientColor);

    // Test display mode : 3dsMax Wire Color.
    displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, testEngine.GetChangeTracker());
    displaySettings.SetWireColor(wireColor, testEngine.GetChangeTracker());
    TestRender(stage, testEngine, renderItems, 0, multiMat);

    simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

    // Check points valid.
    EXPECT_TRUE(simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer).IsValid());
    // Check normals valid.
    EXPECT_TRUE(simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer).IsValid());
    // Check uvs not used.
    EXPECT_FALSE(simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer).IsValid());
    // Check multimaterial not populated.
    EXPECT_EQ(0, multiMat->NumSubMtls());

    customMaterial = renderItems.At(0).GetCustomMaterial();
    stdMaterialHandle.SetPointer(customMaterial.GetPointer());
    diffuseColor = stdMaterialHandle.GetDiffuse();
    expectedDiffuseColor = AColor(1.0 * 0.8, 0.0, 0.0);
    expectColorsEqual(diffuseColor, expectedDiffuseColor);
    ambiantColor = stdMaterialHandle.GetAmbient();
    expectedAmbientColor = AColor(1.0 * 0.2, 0.0, 0.0);
    expectColorsEqual(ambiantColor, expectedAmbientColor);
}