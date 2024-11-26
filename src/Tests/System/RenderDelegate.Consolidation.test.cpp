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

#include <Windows.h>
#include <pxr/usd/usdGeom/cone.h>

#include <MaxUsd/Utilities/TypeUtils.h>
#include <RenderDelegate/HdMaxEngine.h>

// Test that a single box is not consolidated.
TEST(Consolidation, Consolidate1Box)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_1_box.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 100;
	consolidationConfig.maxCellSize = 1000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull, pxr::HdReprTokens->wire },
			consolidationConfig);

	ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Shaded,
			renderItems.GetRenderItem(0).GetVisibilityGroup());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe,
			renderItems.GetRenderItem(1).GetVisibilityGroup());

	auto& renderDelegate = testEngine.GetRenderDelegate();
	auto renderData = renderDelegate->GetRenderDataIdMap();

	auto it = renderData.find(pxr::SdfPath("/consolidation_1_box/Box001"));

	// There is just one prim, so consolidation should not happen. Here we test that the render item returned by
	// the render call is indeed the one from the USD prim's own render data.
	auto& item = renderDelegate->GetRenderData(it->second);
	EXPECT_EQ(item.shadedSubsets[0].renderItem, renderItems.GetRenderItem(0));
	EXPECT_EQ(item.wireframe.renderItem, renderItems.GetRenderItem(1));
}

// Test that 2 boxes are consolidated together correctly.
TEST(Consolidation, Consolidate2Boxes)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_2_boxes.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 100;
	consolidationConfig.maxCellSize = 1000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull, pxr::HdReprTokens->wire },
			consolidationConfig);

	ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Shaded,
			renderItems.GetRenderItem(0).GetVisibilityGroup());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe,
			renderItems.GetRenderItem(1).GetVisibilityGroup());

	// Shaded item...
	const auto& shadedRenderItem = renderItems.GetRenderItem(0);
	auto shadedGeometry = GetRenderItemGeometry(shadedRenderItem, false);

	ASSERT_NE(nullptr, shadedGeometry);
	EXPECT_EQ(MaxSDK::Graphics::PrimitiveTriangleList, shadedGeometry->GetPrimitiveType());
	EXPECT_EQ(4, shadedGeometry->GetVertexBufferCount());
	EXPECT_TRUE(shadedGeometry->GetIndexBuffer().IsValid());

	// Check that points are OK.
	auto pointsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(48, pointsBuffer.GetNumberOfVertices());

	std::array<Point3, 48> expectedPoints = { Point3(-14.170527, -4.749669, 0.000000),
		Point3(-14.170527, 7.248690, 0.000000), Point3(-2.054874, 7.248690, 0.000000),
		Point3(-2.054874, -4.749669, 0.000000), Point3(-14.170527, -4.749669, 7.272934),
		Point3(-2.054874, -4.749669, 7.272934), Point3(-2.054874, 7.248690, 7.272934),
		Point3(-14.170527, 7.248690, 7.272934), Point3(-14.170527, -4.749669, 0.000000),
		Point3(-2.054874, -4.749669, 0.000000), Point3(-2.054874, -4.749669, 7.272934),
		Point3(-14.170527, -4.749669, 7.272934), Point3(-2.054874, -4.749669, 0.000000),
		Point3(-2.054874, 7.248690, 0.000000), Point3(-2.054874, 7.248690, 7.272934),
		Point3(-2.054874, -4.749669, 7.272934), Point3(-2.054874, 7.248690, 0.000000),
		Point3(-14.170527, 7.248690, 0.000000), Point3(-14.170527, 7.248690, 7.272934),
		Point3(-2.054874, 7.248690, 7.272934), Point3(-14.170527, 7.248690, 0.000000),
		Point3(-14.170527, -4.749669, 0.000000), Point3(-14.170527, -4.749669, 7.272934),
		Point3(-14.170527, 7.248690, 7.272934), Point3(5.431611, -3.848971, 6.535490),
		Point3(0.924379, 6.347991, 2.100668), Point3(9.421789, 6.347991, -6.535490),
		Point3(13.929021, -3.848971, -2.100668), Point3(9.837482, -0.016105, 10.870581),
		Point3(18.334892, -0.016105, 2.234422), Point3(13.827660, 10.180856, -2.200399),
		Point3(5.330250, 10.180856, 6.435759), Point3(5.431611, -3.848971, 6.535490),
		Point3(13.929021, -3.848971, -2.100668), Point3(18.334892, -0.016105, 2.234422),
		Point3(9.837482, -0.016105, 10.870581), Point3(13.929021, -3.848971, -2.100668),
		Point3(9.421789, 6.347991, -6.535490), Point3(13.827660, 10.180856, -2.200399),
		Point3(18.334892, -0.016105, 2.234422), Point3(9.421789, 6.347991, -6.535490),
		Point3(0.924379, 6.347991, 2.100668), Point3(5.330250, 10.180856, 6.435759),
		Point3(13.827660, 10.180856, -2.200399), Point3(0.924379, 6.347991, 2.100668),
		Point3(5.431611, -3.848971, 6.535490), Point3(9.837482, -0.016105, 10.870581),
		Point3(5.330250, 10.180856, 6.435759) };

	const auto pointsData = reinterpret_cast<Point3*>(pointsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

	EXPECT_TRUE(Point3ArraysAreAlmostEqual(
			expectedPoints.data(), static_cast<int>(expectedPoints.size()), pointsData, static_cast<int>(pointsBuffer.GetNumberOfVertices())));

	// Check that the normals are OK.
	auto normalsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(48, normalsBuffer.GetNumberOfVertices());

	const auto normalsData = reinterpret_cast<Point3*>(normalsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

	std::array<Point3, 48> expectedNormals = { Point3(0.000000, 0.000000, -1.000000),
		Point3(0.000000, 0.000000, -1.000000), Point3(0.000000, 0.000000, -1.000000),
		Point3(0.000000, 0.000000, -1.000000), Point3(0.000000, 0.000000, 1.000000),
		Point3(0.000000, 0.000000, 1.000000), Point3(0.000000, 0.000000, 1.000000),
		Point3(0.000000, 0.000000, 1.000000), Point3(0.000000, -1.000000, 0.000000),
		Point3(0.000000, -1.000000, 0.000000), Point3(0.000000, -1.000000, 0.000000),
		Point3(0.000000, -1.000000, 0.000000), Point3(1.000000, 0.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(1.000000, 0.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 1.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(0.000000, 1.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(-1.000000, 0.000000, 0.000000),
		Point3(-1.000000, 0.000000, 0.000000), Point3(-1.000000, 0.000000, 0.000000),
		Point3(-1.000000, 0.000000, 0.000000), Point3(-0.605790, -0.527004, -0.596058),
		Point3(-0.605790, -0.527004, -0.596058), Point3(-0.605790, -0.527004, -0.596058),
		Point3(-0.605790, -0.527004, -0.596058), Point3(0.605790, 0.527004, 0.596058),
		Point3(0.605790, 0.527004, 0.596058), Point3(0.605790, 0.527004, 0.596058),
		Point3(0.605790, 0.527004, 0.596058), Point3(0.375654, -0.849863, 0.369619),
		Point3(0.375654, -0.849863, 0.369619), Point3(0.375654, -0.849863, 0.369619),
		Point3(0.375654, -0.849863, 0.369619), Point3(0.701358, 0.000000, -0.712810),
		Point3(0.701358, 0.000000, -0.712810), Point3(0.701358, 0.000000, -0.712810),
		Point3(0.701358, 0.000000, -0.712810), Point3(-0.375654, 0.849863, -0.369619),
		Point3(-0.375654, 0.849863, -0.369619), Point3(-0.375654, 0.849863, -0.369619),
		Point3(-0.375654, 0.849863, -0.369619), Point3(-0.701358, 0.000000, 0.712810),
		Point3(-0.701358, 0.000000, 0.712810), Point3(-0.701358, 0.000000, 0.712810),
		Point3(-0.701358, 0.000000, 0.712810) };
	EXPECT_TRUE(Point3ArraysAreAlmostEqual(
			expectedNormals.data(), static_cast<int>(expectedNormals.size()), normalsData, static_cast<int>(normalsBuffer.GetNumberOfVertices())));

	// Check that the UVs are OK.
	auto uvsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	EXPECT_EQ(48, uvsBuffer.GetNumberOfVertices());

	std::array<Point3, 48> expectedUvs = { Point3(1.000000, 1.000000, 0.000000), Point3(1.000000, 0.000000, 0.000000),
		Point3(0.000000, 0.000000, 0.000000), Point3(0.000000, 1.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(1.000000, 1.000000, 0.000000), Point3(1.000000, 0.000000, 0.000000),
		Point3(0.000000, 0.000000, 0.000000), Point3(0.000000, 1.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000) };
	const auto uvsData = reinterpret_cast<Point3*>(uvsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(Point3ArraysAreAlmostEqual(
			expectedUvs.data(), static_cast<int>(expectedUvs.size()), uvsData, static_cast<int>(uvsBuffer.GetNumberOfVertices())));

	// Check that indices are OK.
	auto trianglesIndexBuffer = shadedGeometry->GetIndexBuffer();
	EXPECT_EQ(72, trianglesIndexBuffer.GetNumberOfIndices());

	std::array<int, 72> expectedIndices = { 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14,
		15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23, 24, 25, 26, 24, 26, 27, 28, 29, 30, 28, 30, 31, 32, 33, 34,
		32, 34, 35, 36, 37, 38, 36, 38, 39, 40, 41, 42, 40, 42, 43, 44, 45, 46, 44, 46, 47 };
	const auto indicesData = reinterpret_cast<int*>(trianglesIndexBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedIndices.begin(), expectedIndices.end(), indicesData));


	// Wireframe item...
	const auto& wireframeRenderItem = renderItems.GetRenderItem(1);
	EXPECT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe,
			wireframeRenderItem.GetVisibilityGroup());

	auto wireframeGeometry = GetRenderItemGeometry(wireframeRenderItem, false);
	
	ASSERT_NE(nullptr, wireframeGeometry);
	ASSERT_EQ(MaxSDK::Graphics::PrimitiveLineList, wireframeGeometry->GetPrimitiveType());
	// No uvs for wireframe, only points, normals, and selection info for highlighting (tested 
	// in details elsewhere).
	ASSERT_EQ(3, wireframeGeometry->GetVertexBufferCount());
	ASSERT_TRUE(wireframeGeometry->GetIndexBuffer().IsValid());

	// The wireframe item should be using the same vertex buffers as the shaded geometry.
	EXPECT_EQ(pointsBuffer, wireframeGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer));
	EXPECT_EQ(normalsBuffer, wireframeGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer));
		
	// Check that indices for the wire edges are OK.
	auto edgeIndexBuffer = wireframeGeometry->GetIndexBuffer();
	EXPECT_EQ(96, edgeIndexBuffer.GetNumberOfIndices());

	const auto wireframeIndicesData = reinterpret_cast<int*>(edgeIndexBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

	std::array<int, 96> wireframeExpectedIndices = { 0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 8, 9, 9, 10, 10,
		11, 11, 8, 12, 13, 13, 14, 14, 15, 15, 12, 16, 17, 17, 18, 18, 19, 19, 16, 20, 21, 21, 22, 22, 23, 23, 20, 24,
		25, 25, 26, 26, 27, 27, 24, 28, 29, 29, 30, 30, 31, 31, 28, 32, 33, 33, 34, 34, 35, 35, 32, 36, 37, 37, 38, 38,
		39, 39, 36, 40, 41, 41, 42, 42, 43, 43, 40, 44, 45, 45, 46, 46, 47, 47, 44 };
	EXPECT_TRUE(std::equal(wireframeExpectedIndices.begin(), wireframeExpectedIndices.end(), wireframeIndicesData));
}

// Test that instances are consolidated correctly.
TEST(Consolidation, ConsolidateInstances)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_4_instances.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 100;
	consolidationConfig.maxCellSize = 1000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull, pxr::HdReprTokens->wire },
			consolidationConfig);

	// 4 instances, consolidated... one for shaded, one for wireframe.
	ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Shaded,
			renderItems.GetRenderItem(0).GetVisibilityGroup());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe,
			renderItems.GetRenderItem(1).GetVisibilityGroup());

	// Shaded item...
	const auto& shadedRenderItem = renderItems.GetRenderItem(0);
	auto shadedGeometry = GetRenderItemGeometry(shadedRenderItem, false);

	ASSERT_NE(nullptr, shadedGeometry);
	EXPECT_EQ(MaxSDK::Graphics::PrimitiveTriangleList, shadedGeometry->GetPrimitiveType());
	EXPECT_EQ(4, shadedGeometry->GetVertexBufferCount());
	EXPECT_TRUE(shadedGeometry->GetIndexBuffer().IsValid());

	// Check that points are OK.
	auto pointsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(48, pointsBuffer.GetNumberOfVertices());

	const auto pointsData = reinterpret_cast<Point3*>(pointsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

	std::array<Point3, 48> expectedPoints = { Point3(0.168677, -0.483523, 7.066816),
		Point3(0.168677, 6.583293, -0.000000), Point3(-6.898139, -0.483524, -0.000000),
		Point3(0.168677, -0.483523, 7.066816), Point3(-6.898139, -0.483524, -0.000000),
		Point3(0.168677, -7.550339, -0.000000), Point3(0.168677, -0.483523, 7.066816),
		Point3(0.168677, -7.550339, -0.000000), Point3(7.235493, -0.483522, -0.000000),
		Point3(0.168677, -0.483523, 7.066816), Point3(7.235493, -0.483522, -0.000000),
		Point3(0.168677, 6.583293, -0.000000), Point3(-21.048500, -0.483523, 7.066816),
		Point3(-21.048500, 6.583293, -0.000000), Point3(-28.115316, -0.483524, -0.000000),
		Point3(-21.048500, -0.483523, 7.066816), Point3(-28.115316, -0.483524, -0.000000),
		Point3(-21.048500, -7.550339, -0.000000), Point3(-21.048500, -0.483523, 7.066816),
		Point3(-21.048500, -7.550339, -0.000000), Point3(-13.981684, -0.483522, -0.000000),
		Point3(-21.048500, -0.483523, 7.066816), Point3(-13.981684, -0.483522, -0.000000),
		Point3(-21.048500, 6.583293, -0.000000), Point3(19.769899, -0.483523, 7.066816),
		Point3(19.769899, 6.583293, -0.000000), Point3(12.703083, -0.483524, -0.000000),
		Point3(19.769899, -0.483523, 7.066816), Point3(12.703083, -0.483524, -0.000000),
		Point3(19.769899, -7.550339, -0.000000), Point3(19.769899, -0.483523, 7.066816),
		Point3(19.769899, -7.550339, -0.000000), Point3(26.836716, -0.483522, -0.000000),
		Point3(19.769899, -0.483523, 7.066816), Point3(26.836716, -0.483522, -0.000000),
		Point3(19.769899, 6.583293, -0.000000), Point3(-0.122053, 18.332500, 7.066816),
		Point3(-0.122053, 25.399317, -0.000000), Point3(-7.188869, 18.332500, -0.000000),
		Point3(-0.122053, 18.332500, 7.066816), Point3(-7.188869, 18.332500, -0.000000),
		Point3(-0.122053, 11.265684, -0.000000), Point3(-0.122053, 18.332500, 7.066816),
		Point3(-0.122053, 11.265684, -0.000000), Point3(6.944763, 18.332502, -0.000000),
		Point3(-0.122053, 18.332500, 7.066816), Point3(6.944763, 18.332502, -0.000000),
		Point3(-0.122053, 25.399317, -0.000000) };

	EXPECT_TRUE(Point3ArraysAreAlmostEqual(
			expectedPoints.data(), static_cast<int>(expectedPoints.size()), pointsData, static_cast<int>(pointsBuffer.GetNumberOfVertices())));

	// Check that the normals are OK.
	auto normalsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(48, normalsBuffer.GetNumberOfVertices());

	std::array<Point3, 48> expectedNormals = { Point3(0.000000, -0.000000, 1.000000),
		Point3(-0.000000, 0.707107, 0.707107), Point3(-0.707107, -0.000000, 0.707107),
		Point3(0.000000, -0.000000, 1.000000), Point3(-0.707107, -0.000000, 0.707107),
		Point3(0.000000, -0.707107, 0.707107), Point3(0.000000, -0.000000, 1.000000),
		Point3(0.000000, -0.707107, 0.707107), Point3(0.707107, 0.000000, 0.707107),
		Point3(0.000000, -0.000000, 1.000000), Point3(0.707107, 0.000000, 0.707107),
		Point3(-0.000000, 0.707107, 0.707107), Point3(0.000000, -0.000000, 1.000000),
		Point3(-0.000000, 0.707107, 0.707107), Point3(-0.707107, -0.000000, 0.707107),
		Point3(0.000000, -0.000000, 1.000000), Point3(-0.707107, -0.000000, 0.707107),
		Point3(0.000000, -0.707107, 0.707107), Point3(0.000000, -0.000000, 1.000000),
		Point3(0.000000, -0.707107, 0.707107), Point3(0.707107, 0.000000, 0.707107),
		Point3(0.000000, -0.000000, 1.000000), Point3(0.707107, 0.000000, 0.707107),
		Point3(-0.000000, 0.707107, 0.707107), Point3(0.000000, -0.000000, 1.000000),
		Point3(-0.000000, 0.707107, 0.707107), Point3(-0.707107, -0.000000, 0.707107),
		Point3(0.000000, -0.000000, 1.000000), Point3(-0.707107, -0.000000, 0.707107),
		Point3(0.000000, -0.707107, 0.707107), Point3(0.000000, -0.000000, 1.000000),
		Point3(0.000000, -0.707107, 0.707107), Point3(0.707107, 0.000000, 0.707107),
		Point3(0.000000, -0.000000, 1.000000), Point3(0.707107, 0.000000, 0.707107),
		Point3(-0.000000, 0.707107, 0.707107), Point3(0.000000, -0.000000, 1.000000),
		Point3(-0.000000, 0.707107, 0.707107), Point3(-0.707107, -0.000000, 0.707107),
		Point3(0.000000, -0.000000, 1.000000), Point3(-0.707107, -0.000000, 0.707107),
		Point3(0.000000, -0.707107, 0.707107), Point3(0.000000, -0.000000, 1.000000),
		Point3(0.000000, -0.707107, 0.707107), Point3(0.707107, 0.000000, 0.707107),
		Point3(0.000000, -0.000000, 1.000000), Point3(0.707107, 0.000000, 0.707107),
		Point3(-0.000000, 0.707107, 0.707107) };

	const auto normalsData = reinterpret_cast<Point3*>(normalsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

	EXPECT_TRUE(Point3ArraysAreAlmostEqual(
			expectedNormals.data(), static_cast<int>(expectedNormals.size()), normalsData, static_cast<int>(normalsBuffer.GetNumberOfVertices())));

	// Check that the UVs are OK.
	auto uvsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	EXPECT_EQ(48, uvsBuffer.GetNumberOfVertices());
	std::array<Point3, 48> expectedUvs = { Point3(0.000000, 0.000000, 0.000000), Point3(0.000000, 0.500000, 0.000000),
		Point3(0.250000, 0.500000, 0.000000), Point3(0.250000, 0.000000, 0.000000),
		Point3(0.250000, 0.500000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
		Point3(0.500000, 0.000000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
		Point3(0.750000, 0.500000, 0.000000), Point3(0.750000, 0.000000, 0.000000),
		Point3(0.750000, 0.500000, 0.000000), Point3(1.000000, 0.500000, 0.000000),
		Point3(0.000000, 0.000000, 0.000000), Point3(0.000000, 0.500000, 0.000000),
		Point3(0.250000, 0.500000, 0.000000), Point3(0.250000, 0.000000, 0.000000),
		Point3(0.250000, 0.500000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
		Point3(0.500000, 0.000000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
		Point3(0.750000, 0.500000, 0.000000), Point3(0.750000, 0.000000, 0.000000),
		Point3(0.750000, 0.500000, 0.000000), Point3(1.000000, 0.500000, 0.000000),
		Point3(0.000000, 0.000000, 0.000000), Point3(0.000000, 0.500000, 0.000000),
		Point3(0.250000, 0.500000, 0.000000), Point3(0.250000, 0.000000, 0.000000),
		Point3(0.250000, 0.500000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
		Point3(0.500000, 0.000000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
		Point3(0.750000, 0.500000, 0.000000), Point3(0.750000, 0.000000, 0.000000),
		Point3(0.750000, 0.500000, 0.000000), Point3(1.000000, 0.500000, 0.000000),
		Point3(0.000000, 0.000000, 0.000000), Point3(0.000000, 0.500000, 0.000000),
		Point3(0.250000, 0.500000, 0.000000), Point3(0.250000, 0.000000, 0.000000),
		Point3(0.250000, 0.500000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
		Point3(0.500000, 0.000000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
		Point3(0.750000, 0.500000, 0.000000), Point3(0.750000, 0.000000, 0.000000),
		Point3(0.750000, 0.500000, 0.000000), Point3(1.000000, 0.500000, 0.000000) };


	const auto uvsData = reinterpret_cast<Point3*>(uvsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(Point3ArraysAreAlmostEqual(
			expectedUvs.data(), static_cast<int>(expectedUvs.size()), uvsData, static_cast<int>(uvsBuffer.GetNumberOfVertices())));

	// Check that indices are OK.
	auto trianglesIndexBuffer = shadedGeometry->GetIndexBuffer();
	EXPECT_EQ(48, trianglesIndexBuffer.GetNumberOfIndices());

	std::array<int, 48> expectedIndices = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47 };

	const auto indicesData = reinterpret_cast<int*>(trianglesIndexBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedIndices.begin(), expectedIndices.end(), indicesData));

	// Wireframe item...
	const auto& wireframeRenderItem = renderItems.GetRenderItem(1);
	EXPECT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe,
			wireframeRenderItem.GetVisibilityGroup());

	auto wireframeGeometry = GetRenderItemGeometry(wireframeRenderItem, false);

	ASSERT_NE(nullptr, wireframeGeometry);
	ASSERT_EQ(MaxSDK::Graphics::PrimitiveLineList, wireframeGeometry->GetPrimitiveType());
	// No uvs for wireframe, only points, normals, and selection info for highlighting (tested 
	// in details elsewhere).
	ASSERT_EQ(3, wireframeGeometry->GetVertexBufferCount());
	ASSERT_TRUE(wireframeGeometry->GetIndexBuffer().IsValid());

	// The wireframe item should be using the same vertex buffers as the shaded geometry.
	EXPECT_EQ(pointsBuffer, wireframeGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer));
	EXPECT_EQ(normalsBuffer, wireframeGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer));

	// Check that indices for the wire edges are OK.
	auto edgeIndexBuffer = wireframeGeometry->GetIndexBuffer();
	EXPECT_EQ(96, edgeIndexBuffer.GetNumberOfIndices());

	std::array<int, 96> wireframeExpectedIndices = { 0, 1, 1, 2, 2, 0, 3, 4, 4, 5, 5, 3, 6, 7, 7, 8, 8, 6, 9, 10, 10,
		11, 11, 9, 12, 13, 13, 14, 14, 12, 15, 16, 16, 17, 17, 15, 18, 19, 19, 20, 20, 18, 21, 22, 22, 23, 23, 21, 24,
		25, 25, 26, 26, 24, 27, 28, 28, 29, 29, 27, 30, 31, 31, 32, 32, 30, 33, 34, 34, 35, 35, 33, 36, 37, 37, 38, 38,
		36, 39, 40, 40, 41, 41, 39, 42, 43, 43, 44, 44, 42, 45, 46, 46, 47, 47, 45 };
	
	const auto wireframeIndicesData = reinterpret_cast<int*>(edgeIndexBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

	EXPECT_TRUE(std::equal(wireframeExpectedIndices.begin(), wireframeExpectedIndices.end(), wireframeIndicesData));
}

// That the case where instances need to be consolidated, but split into multiple cells / merged meshes.
TEST(Consolidation, ConsolidateInstancesSplit)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_4_instances.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 100;
	// Scene is 4 instances of pyramids composed of 4 triangles, so 16 total. With a max cell size of 8, the instances
	// should be split over 2 consolidation cells.
	consolidationConfig.maxCellSize = 8;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull, pxr::HdReprTokens->wire },
			consolidationConfig);

	// 4 instances, consolidated into 2 meshes (2 cells)... so 4 render items 2 for shaded, 2 for wireframe.
	ASSERT_EQ(4, renderItems.GetNumberOfRenderItems());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Shaded,
			renderItems.GetRenderItem(0).GetVisibilityGroup());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe,
			renderItems.GetRenderItem(1).GetVisibilityGroup());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Shaded,
			renderItems.GetRenderItem(2).GetVisibilityGroup());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe,
			renderItems.GetRenderItem(3).GetVisibilityGroup());

	for (int i = 0; i < 2; i++)
	{
		const auto& shadedRenderItem = renderItems.GetRenderItem(i * 2); // shaded render items are at 0 and 2.
		auto shadedGeometry = GetRenderItemGeometry(shadedRenderItem, false);
		
		auto pointsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
		EXPECT_EQ(24, pointsBuffer.GetNumberOfVertices());

		// Check that the normals are OK.
		auto normalsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
		EXPECT_EQ(24, normalsBuffer.GetNumberOfVertices());

		// Check that the selection buffer is OK.
		auto selBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::SelectionBuffer);
		EXPECT_EQ(24, selBuffer.GetNumberOfVertices());

		// Check that the UVs are OK.
		auto uvsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
		EXPECT_EQ(24, uvsBuffer.GetNumberOfVertices());

		// Check that indices are OK.
		auto trianglesIndexBuffer = shadedGeometry->GetIndexBuffer();
		EXPECT_EQ(24, trianglesIndexBuffer.GetNumberOfIndices());

		// Wireframe item...
		const auto& wireframeRenderItem = renderItems.GetRenderItem(i * 2 + 1);
		EXPECT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe,
				wireframeRenderItem.GetVisibilityGroup());

		auto wireframeGeometry = GetRenderItemGeometry(wireframeRenderItem, false);

		// Check that indices for the wire edges are OK.
		auto edgeIndexBuffer = wireframeGeometry->GetIndexBuffer();
		EXPECT_EQ(48, edgeIndexBuffer.GetNumberOfIndices());
	}
}

// A few helpers to help validate the data we generate.
static int GetVertexCount(const MaxSDK::Graphics::RenderItemHandle& renderItem, bool decorated)
{
	const auto pointsBuffer = GetRenderItemGeometry(renderItem, decorated)->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	return static_cast<int>(pointsBuffer.GetNumberOfVertices());
}

static int GetTriCount(const MaxSDK::Graphics::RenderItemHandle& renderItem, bool decorated)
{
	const auto indices = GetRenderItemGeometry(renderItem, decorated)->GetIndexBuffer();
	return static_cast<int>(indices.GetNumberOfIndices() / 3);
}

static Box3 GetBoundingBox(const MaxSDK::Graphics::RenderItemHandle& renderItem, bool decorated, Matrix3* tm = nullptr)
{
	auto points = GetRenderItemGeometry(renderItem, decorated)->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	auto rawPoints = reinterpret_cast<Point3*>(points.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	Box3 bbox;
	bbox.IncludePoints(rawPoints, static_cast<int>(points.GetNumberOfVertices()), tm);
	points.Unlock();
	return bbox;
}

// Test the consolidation of when multiple materials/objects are present.
TEST(Consolidation, Consolidate10Objects4Materials)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_4_materials.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 500;
	consolidationConfig.maxCellSize = 1000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	pxr::HdChangeTracker dummyTracker;
	auto& displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
	displaySettings.SetDisplayMode(HdMaxDisplaySettings::USDPreviewSurface, dummyTracker);
	consolidationConfig.displaySettings = displaySettings;
	
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);

	// There are 10 objects in the scene, but only 4 different materials. One of the materials is used by a single object
	ASSERT_EQ(4, renderItems.GetNumberOfRenderItems());

	// Check that the resulting meshes are of expected sizes (order is non-deterministic, because we key off material
	// handles)
	std::unordered_set<int> expectedPointSizes = { 48, 96, 72, 24 };
	std::unordered_set<int> found = {};

	for (int i = 0; i < 4; i++)
	{
		const auto& shadedRenderItem = renderItems.GetRenderItem(i);
		// The last render item is not consolidated as it is a single mesh, so we expect a decorated render item (with
		// an offset transform).
		found.insert(GetVertexCount(shadedRenderItem, i == 3));
	}
	EXPECT_EQ(expectedPointSizes, found);

	renderItems.ClearAllRenderItems();

	// Setup to use the wire color... now everything can be consolidated together in a single mesh.
	displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);
	consolidationConfig.displaySettings = displaySettings;

	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);

	ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());
	const auto& shadedRenderItem = renderItems.GetRenderItem(0);

	auto shadedGeometry = GetRenderItemGeometry(shadedRenderItem, false);
		
	auto pointsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(240, pointsBuffer.GetNumberOfVertices());
}

// Tests to cover consolidation options :

TEST(Consolidation, MaxTrianglesOption)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_3_spheres.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxCellSize = 10000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	// This file contains 3 spheres, with 8,80 and 960 triangles respectively.

	// 1) All spheres eligible for consolidation :
	consolidationConfig.maxTriangles = 5000;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(960 + 80 + 8, GetTriCount(renderItems.GetRenderItem(0), false));

	renderItems.ClearAllRenderItems();

	consolidationConfig.maxTriangles = 960; // At the limit for the 960 tris sphere
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(960 + 80 + 8, GetTriCount(renderItems.GetRenderItem(0), false));
	renderItems.ClearAllRenderItems();

	// 2) 2 spheres eligible for consolidation :
	consolidationConfig.maxTriangles = 959;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(80 + 8, GetTriCount(renderItems.GetRenderItem(0), false));
	EXPECT_EQ(960, GetTriCount(renderItems.GetRenderItem(1), true)); // Not consolidated
	renderItems.ClearAllRenderItems();

	consolidationConfig.maxTriangles = 80; // At the limit for the 80 tris sphere.
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(80 + 8, GetTriCount(renderItems.GetRenderItem(0), false));
	EXPECT_EQ(960, GetTriCount(renderItems.GetRenderItem(1), true)); // Not consolidated
	renderItems.ClearAllRenderItems();

	// 3) 1 sphere eligible for consolidation.. nothing is consolidated:
	consolidationConfig.maxTriangles = 79;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(3, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(8, GetTriCount(renderItems.GetRenderItem(0), true));
	EXPECT_EQ(80, GetTriCount(renderItems.GetRenderItem(1), true));
	EXPECT_EQ(960, GetTriCount(renderItems.GetRenderItem(2), true));
	renderItems.ClearAllRenderItems();
}

TEST(Consolidation, MaxCellSizeOption)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_4_materials.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 5000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	// This file contains 10 boxes, each composed of 12 triangles.

	// We don't care about the materials for this test, we just want to merge the 10 boxes contained
	// in this file with various max cell size values.
	pxr::HdChangeTracker dummyTracker;
	auto& displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
	displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);
	consolidationConfig.displaySettings = displaySettings;

	// 1) All boxes can be merged into a single cell.
	consolidationConfig.maxCellSize = 200;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(120, GetTriCount(renderItems.GetRenderItem(0), false));

	renderItems.ClearAllRenderItems();
	consolidationConfig.maxCellSize = 120; // limit
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(120, GetTriCount(renderItems.GetRenderItem(0), false));

	// 2) A few cases which potentially require multiple cells.

	// 1 consolidated mesh + 1 left over...
	consolidationConfig.maxCellSize = 119;
	renderItems.ClearAllRenderItems();
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(108, GetTriCount(renderItems.GetRenderItem(0), false));
	EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(1), true)); // Unconsolidated
	renderItems.ClearAllRenderItems();

	// 2 unequal cells.
	consolidationConfig.maxCellSize = 96;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(96, GetTriCount(renderItems.GetRenderItem(0), false));
	EXPECT_EQ(24, GetTriCount(renderItems.GetRenderItem(1), false));
	renderItems.ClearAllRenderItems();

	// 2 equal cells.
	consolidationConfig.maxCellSize = 60;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(60, GetTriCount(renderItems.GetRenderItem(0), false));
	EXPECT_EQ(60, GetTriCount(renderItems.GetRenderItem(1), false));
	renderItems.ClearAllRenderItems();

	// 5 equal cells.
	consolidationConfig.maxCellSize = 25;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(5, renderItems.GetNumberOfRenderItems());
	for (int i = 0; i < 5; i++)
	{
		EXPECT_EQ(24, GetTriCount(renderItems.GetRenderItem(i), false));
	}
	renderItems.ClearAllRenderItems();

	// 3) Max cell size < triangle count
	consolidationConfig.maxCellSize = 11;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(10, renderItems.GetNumberOfRenderItems());
	for (int i = 0; i < 10; i++)
	{
		EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(i), true)); // unconsolidated
	}
	renderItems.ClearAllRenderItems();
}

TEST(Consolidation, MaxInstanceCountSizeOption)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_4_instances.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 5000;
	consolidationConfig.maxCellSize = 10000;
	consolidationConfig.staticDelay = 0;

	// 4 Instances in the scene... (pyramids formed of 4 triangles)

	consolidationConfig.maxInstanceCount = 1000; // Over
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(16, GetTriCount(renderItems.GetRenderItem(0), false));
	renderItems.ClearAllRenderItems();

	consolidationConfig.maxInstanceCount = 4; // limit
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(16, GetTriCount(renderItems.GetRenderItem(0), false));
	renderItems.ClearAllRenderItems();

	consolidationConfig.maxInstanceCount = 3; // Under
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	// There is no exposed way to access the instancing data, but we can still know this is a instance render item and
	// not a render item from consolidation by looking at the geometry, which will be null here in the case of
	// instances.
	EXPECT_EQ(nullptr, GetRenderItemGeometry(renderItems.GetRenderItem(0), false));
}

TEST(Consolidation, StaticStrategy)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_2_boxes_animated.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 5000;
	consolidationConfig.maxCellSize = 10000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 100;

	// Time code 0 -> not static -> dont consolidate
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(0), true)); // not consolidated.
	EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(1), true)); // not consolidated.
	renderItems.ClearAllRenderItems();

	Sleep(static_cast<DWORD>(consolidationConfig.staticDelay));

	// Render again time code 0, now will consider we are in the static case... consolidate.
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(24, GetTriCount(renderItems.GetRenderItem(0), false));
	renderItems.ClearAllRenderItems();

	// Render at time code 1 -> meshes have changed -> break consolidation.
	TestRender(stage, testEngine, renderItems, 1, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(0), true));
	EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(1), true));
	renderItems.ClearAllRenderItems();
}

TEST(Consolidation, StaticStrategyOnStaticData)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_2_boxes.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 5000;
	consolidationConfig.maxCellSize = 10000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 100;

	// Time code 0 -> not static -> dont consolidate
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(0), true)); // not consolidated.
	EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(1), true)); // not consolidated.
	renderItems.ClearAllRenderItems();

	Sleep(static_cast<DWORD>(consolidationConfig.staticDelay));

	// Render again time code 0, now will consider we are in the static case... consolidate.
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(24, GetTriCount(renderItems.GetRenderItem(0), false));
	renderItems.ClearAllRenderItems();

	// Render at time code 1 -> nothing has changed in the scene as it is not animated -> the consolidation should still
	// be valid/used even though we are in static mode.
	TestRender(stage, testEngine, renderItems, 1, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	EXPECT_EQ(24, GetTriCount(renderItems.GetRenderItem(0), false));
	renderItems.ClearAllRenderItems();
}

TEST(Consolidation, DynamicStrategy)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_2_planes_anim.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Dynamic;
	consolidationConfig.maxTriangles = 5000;
	consolidationConfig.maxCellSize = 10000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 100;

	// Time code 0 -> dynamic mode -> consolidate!
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	auto geom1 = GetRenderItemGeometry(renderItems.GetRenderItem(0), false);

	{
		auto pointsBuffer = geom1->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
		auto pointsData = reinterpret_cast<Point3*>(pointsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
		std::array<Point3, 40> expectedPoints = { Point3(-22.412651, -2.047300, 0.000000),
			Point3(-22.412651, -13.864553, 0.000000), Point3(-10.205800, -13.864553, 0.000000),
			Point3(-10.205800, -2.047300, 0.000000), Point3(-10.205800, -2.047300, 0.000000),
			Point3(-10.205800, -13.864553, 0.000000), Point3(2.001050, -13.864553, 0.000000),
			Point3(2.001050, -2.047300, 0.000000), Point3(-22.412651, 9.769953, 0.000000),
			Point3(-22.412651, -2.047300, 0.000000), Point3(-10.205800, -2.047300, 0.000000),
			Point3(-10.205800, 9.769953, 0.000000), Point3(-10.205800, 9.769953, 0.000000),
			Point3(-10.205800, -2.047300, 0.000000), Point3(2.001050, -2.047300, 0.000000),
			Point3(2.001050, 9.769953, 0.000000), Point3(15.351128, -10.598882, 15.736800),
			Point3(21.728214, -17.094683, 11.861380), Point3(24.061899, -2.047300, 15.736800),
			Point3(30.438984, -8.543102, 11.861380), Point3(24.061899, -2.047300, 15.736800),
			Point3(21.728214, -17.094683, 11.861380), Point3(24.061899, -2.047300, 15.736800),
			Point3(30.438984, -8.543102, 11.861380), Point3(32.772671, 6.504281, 15.736800),
			Point3(39.149757, 0.008479, 11.861380), Point3(32.772671, 6.504281, 15.736800),
			Point3(30.438984, -8.543102, 11.861380), Point3(8.974042, -4.103080, 19.612221),
			Point3(15.351128, -10.598882, 15.736800), Point3(17.684814, 4.448502, 19.612221),
			Point3(24.061899, -2.047300, 15.736800), Point3(17.684814, 4.448502, 19.612221),
			Point3(15.351128, -10.598882, 15.736800), Point3(17.684814, 4.448502, 19.612221),
			Point3(24.061899, -2.047300, 15.736800), Point3(26.395584, 13.000083, 19.612221),
			Point3(32.772671, 6.504281, 15.736800), Point3(26.395584, 13.000083, 19.612221),
			Point3(24.061899, -2.047300, 15.736800) };
		EXPECT_TRUE(Point3ArraysAreAlmostEqual(
				expectedPoints.data(), static_cast<int>(expectedPoints.size()), pointsData, static_cast<int>(pointsBuffer.GetNumberOfVertices())));
		pointsBuffer.Unlock();
	}

	{
		auto normalsBuffer = geom1->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
		auto normalsData = reinterpret_cast<Point3*>(normalsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
		std::array<Point3, 40> expectedNormals = { Point3(0.00000000, 0.00000000, 1.00000000),
			Point3(0.00000000, 0.00000000, 1.00000000), Point3(0.00000000, 0.00000000, 1.00000000),
			Point3(0.00000000, 0.00000000, 1.00000000), Point3(0.00000000, 0.00000000, 1.00000000),
			Point3(0.00000000, 0.00000000, 1.00000000), Point3(0.00000000, 0.00000000, 1.00000000),
			Point3(0.00000000, 0.00000000, 1.00000000), Point3(0.00000000, 0.00000000, 1.00000000),
			Point3(0.00000000, 0.00000000, 1.00000000), Point3(0.00000000, 0.00000000, 1.00000000),
			Point3(0.00000000, 0.00000000, 1.00000000), Point3(0.00000000, 0.00000000, 1.00000000),
			Point3(0.00000000, 0.00000000, 1.00000000), Point3(0.00000000, 0.00000000, 1.00000000),
			Point3(0.00000000, 0.00000000, 1.00000000), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695), Point3(0.274416447, -0.279525071, 0.920087695),
			Point3(0.274416447, -0.279525071, 0.920087695) };
		;
		EXPECT_TRUE(Point3ArraysAreAlmostEqual(
				expectedNormals.data(), static_cast<int>(expectedNormals.size()), normalsData, static_cast<int>(normalsBuffer.GetNumberOfVertices())));
		normalsBuffer.Unlock();
	}

	{
		auto uvBuffer = geom1->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
		auto uvData = reinterpret_cast<Point3*>(uvBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
		std::array<Point3, 40> expectedUvs = { Point3(0.000000, 0.500000, 0.000000),
			Point3(0.000000, 1.000000, 0.000000), Point3(0.500000, 1.000000, 0.000000),
			Point3(0.500000, 0.500000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
			Point3(0.500000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
			Point3(1.000000, 0.500000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
			Point3(0.000000, 0.500000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
			Point3(0.500000, 0.000000, 0.000000), Point3(0.500000, 0.000000, 0.000000),
			Point3(0.500000, 0.500000, 0.000000), Point3(1.000000, 0.500000, 0.000000),
			Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 1.260000, 0.000000),
			Point3(0.000000, 1.000000, 0.000000), Point3(0.960000, 1.260000, 0.000000),
			Point3(0.960000, 1.000000, 0.000000), Point3(0.960000, 1.260000, 0.000000),
			Point3(0.000000, 1.000000, 0.000000), Point3(0.960000, 1.260000, 0.000000),
			Point3(0.960000, 1.000000, 0.000000), Point3(1.920000, 1.260000, 0.000000),
			Point3(1.919999, 1.000000, 0.000000), Point3(1.920000, 1.260000, 0.000000),
			Point3(0.960000, 1.000000, 0.000000), Point3(0.000001, 1.520000, 0.000000),
			Point3(0.000000, 1.260000, 0.000000), Point3(0.960000, 1.520000, 0.000000),
			Point3(0.960000, 1.260000, 0.000000), Point3(0.960000, 1.520000, 0.000000),
			Point3(0.000000, 1.260000, 0.000000), Point3(0.960000, 1.520000, 0.000000),
			Point3(0.960000, 1.260000, 0.000000), Point3(1.920000, 1.520000, 0.000000),
			Point3(1.920000, 1.260000, 0.000000), Point3(1.920000, 1.520000, 0.000000),
			Point3(0.960000, 1.260000, 0.000000) };
		EXPECT_TRUE(Point3ArraysAreAlmostEqual(
				expectedUvs.data(), static_cast<int>(expectedUvs.size()), uvData, static_cast<int>(uvBuffer.GetNumberOfVertices())));
		uvBuffer.Unlock();
	}

	// Render at time code 1 -> consolidation is updated.
	renderItems.ClearAllRenderItems();

	TestRender(stage, testEngine, renderItems, 1, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());

	auto geom2 = GetRenderItemGeometry(renderItems.GetRenderItem(0), false);

	{
		auto pointsBuffer = geom2->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
		auto pointsData = reinterpret_cast<Point3*>(pointsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
		std::array<Point3, 40> expectedPoints = { Point3(-22.412651, -2.047300, 0.000000),
			Point3(-22.412651, -13.864553, 0.000000), Point3(-10.205800, -13.864553, 0.000000),
			Point3(-10.205800, -2.047300, 0.000000), Point3(-10.205800, -2.047300, 0.000000),
			Point3(-10.205800, -13.864553, 0.000000), Point3(2.001050, -13.864553, 0.000000),
			Point3(2.001050, -2.047300, 0.000000), Point3(-22.412651, 9.769953, 0.000000),
			Point3(-22.412651, -2.047300, 0.000000), Point3(-10.205800, -2.047300, 0.000000),
			Point3(-10.205800, 9.769953, 0.000000), Point3(-10.205800, 9.769953, 0.000000),
			Point3(-10.205800, -2.047300, 0.000000), Point3(2.001050, -2.047300, 0.000000),
			Point3(2.001050, 9.769953, 0.000000), Point3(20.388432, -0.029459, 17.045118),
			Point3(18.441692, -2.428929, 15.279857), Point3(24.061899, -2.047300, 15.736800),
			Point3(22.115160, -4.446770, 13.971539), Point3(24.061899, -2.047300, 15.736800),
			Point3(18.441692, -2.428929, 15.279857), Point3(24.061899, -2.047300, 15.736800),
			Point3(22.115160, -4.446770, 13.971539), Point3(27.735367, -4.065141, 14.428482),
			Point3(25.788630, -6.464611, 12.663221), Point3(27.735367, -4.065141, 14.428482),
			Point3(22.115160, -4.446770, 13.971539), Point3(22.335169, 2.370011, 18.810379),
			Point3(20.388432, -0.029459, 17.045118), Point3(26.008638, 0.352170, 17.502062),
			Point3(24.061899, -2.047300, 15.736800), Point3(26.008638, 0.352170, 17.502062),
			Point3(20.388432, -0.029459, 17.045118), Point3(26.008638, 0.352170, 17.502062),
			Point3(24.061899, -2.047300, 15.736800), Point3(29.682106, -1.665672, 16.193743),
			Point3(27.735367, -4.065141, 14.428482), Point3(29.682106, -1.665672, 16.193743),
			Point3(24.061899, -2.047300, 15.736800) };
		EXPECT_TRUE(Point3ArraysAreAlmostEqual(
				expectedPoints.data(), static_cast<int>(expectedPoints.size()), pointsData, static_cast<int>(pointsBuffer.GetNumberOfVertices())));
		pointsBuffer.Unlock();
	}

	{
		auto normalsBuffer = geom2->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
		auto normalsData = reinterpret_cast<Point3*>(normalsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
		std::array<Point3, 40> expectedNormals = { Point3(-22.412651, -2.047300, 0.000000),
			Point3(-22.412651, -13.864553, 0.000000), Point3(-10.205800, -13.864553, 0.000000),
			Point3(-10.205800, -2.047300, 0.000000), Point3(-10.205800, -2.047300, 0.000000),
			Point3(-10.205800, -13.864553, 0.000000), Point3(2.001050, -13.864553, 0.000000),
			Point3(2.001050, -2.047300, 0.000000), Point3(-22.412651, 9.769953, 0.000000),
			Point3(-22.412651, -2.047300, 0.000000), Point3(-10.205800, -2.047300, 0.000000),
			Point3(-10.205800, 9.769953, 0.000000), Point3(-10.205800, 9.769953, 0.000000),
			Point3(-10.205800, -2.047300, 0.000000), Point3(2.001050, -2.047300, 0.000000),
			Point3(2.001050, 9.769953, 0.000000), Point3(20.388432, -0.029459, 17.045118),
			Point3(18.441692, -2.428929, 15.279857), Point3(24.061899, -2.047300, 15.736800),
			Point3(22.115160, -4.446770, 13.971539), Point3(24.061899, -2.047300, 15.736800),
			Point3(18.441692, -2.428929, 15.279857), Point3(24.061899, -2.047300, 15.736800),
			Point3(22.115160, -4.446770, 13.971539), Point3(27.735367, -4.065141, 14.428482),
			Point3(25.788630, -6.464611, 12.663221), Point3(27.735367, -4.065141, 14.428482),
			Point3(22.115160, -4.446770, 13.971539), Point3(22.335169, 2.370011, 18.810379),
			Point3(20.388432, -0.029459, 17.045118), Point3(26.008638, 0.352170, 17.502062),
			Point3(24.061899, -2.047300, 15.736800), Point3(26.008638, 0.352170, 17.502062),
			Point3(20.388432, -0.029459, 17.045118), Point3(26.008638, 0.352170, 17.502062),
			Point3(24.061899, -2.047300, 15.736800), Point3(29.682106, -1.665672, 16.193743),
			Point3(27.735367, -4.065141, 14.428482), Point3(29.682106, -1.665672, 16.193743),
			Point3(24.061899, -2.047300, 15.736800) };
		EXPECT_TRUE(Point3ArraysAreAlmostEqual(
				expectedNormals.data(), static_cast<int>(expectedNormals.size()), normalsData, static_cast<int>(normalsBuffer.GetNumberOfVertices())));
		normalsBuffer.Unlock();
	}

	{
		auto uvBuffer = geom2->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
		auto uvData = reinterpret_cast<Point3*>(uvBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
		std::array<Point3, 40> expectedUvs = { Point3(0.000000, 0.500000, 0.000000),
			Point3(0.000000, 1.000000, 0.000000), Point3(0.500000, 1.000000, 0.000000),
			Point3(0.500000, 0.500000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
			Point3(0.500000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
			Point3(1.000000, 0.500000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
			Point3(0.000000, 0.500000, 0.000000), Point3(0.500000, 0.500000, 0.000000),
			Point3(0.500000, 0.000000, 0.000000), Point3(0.500000, 0.000000, 0.000000),
			Point3(0.500000, 0.500000, 0.000000), Point3(1.000000, 0.500000, 0.000000),
			Point3(1.000000, 0.000000, 0.000000), Point3(0.624336, 1.260000, 0.000000),
			Point3(0.624336, 1.351873, 0.000000), Point3(0.960000, 1.260000, 0.000000),
			Point3(0.960000, 1.351873, 0.000000), Point3(0.960000, 1.260000, 0.000000),
			Point3(0.624336, 1.351873, 0.000000), Point3(0.960000, 1.260000, 0.000000),
			Point3(0.960000, 1.351873, 0.000000), Point3(1.295664, 1.260000, 0.000000),
			Point3(1.295664, 1.351873, 0.000000), Point3(1.295664, 1.260000, 0.000000),
			Point3(0.960000, 1.351873, 0.000000), Point3(0.624336, 1.168127, 0.000000),
			Point3(0.624336, 1.260000, 0.000000), Point3(0.960000, 1.168127, 0.000000),
			Point3(0.960000, 1.260000, 0.000000), Point3(0.960000, 1.168127, 0.000000),
			Point3(0.624336, 1.260000, 0.000000), Point3(0.960000, 1.168127, 0.000000),
			Point3(0.960000, 1.260000, 0.000000), Point3(1.295664, 1.168127, 0.000000),
			Point3(1.295664, 1.260000, 0.000000), Point3(1.295664, 1.168127, 0.000000),
			Point3(0.960000, 1.260000, 0.000000) };
		EXPECT_TRUE(Point3ArraysAreAlmostEqual(
				expectedUvs.data(), static_cast<int>(expectedUvs.size()), uvData, static_cast<int>(uvBuffer.GetNumberOfVertices())));
		uvBuffer.Unlock();
	}

	renderItems.ClearAllRenderItems();
}

// Test consolidation when adding and removing prims from the render.
TEST(Consolidation, BreakingModificationsPrimChanges)
{
	// Test with static and dynamic modes.
	for (int i = 0; i < int(HdMaxConsolidator::Strategy::Off); ++i)
	{
		HdMaxConsolidator::Config consolidationConfig;
		consolidationConfig.strategy = static_cast<HdMaxConsolidator::Strategy>(i);
		consolidationConfig.maxTriangles = 5000;
		consolidationConfig.maxCellSize = 10000;
		consolidationConfig.maxInstanceCount = 1000;
		consolidationConfig.staticDelay = 0;

		const auto stage = pxr::UsdStage::CreateInMemory();
		auto sphere1 = MaxUsd::FetchOrCreatePrim<pxr::UsdGeomXformable>(stage, pxr::SdfPath("/sphere1"), pxr::TfToken("Sphere"));
		auto cube1 = MaxUsd::FetchOrCreatePrim<pxr::UsdGeomXformable>(stage, pxr::SdfPath("/cube1"), pxr::TfToken("Cube"));

		HdMaxEngine testEngine;
		MockRenderItemContainer renderItems;
		
		// First render, consolidate.
		TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
		ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());
		EXPECT_EQ(192, GetTriCount(renderItems.GetRenderItem(0), false));
		renderItems.ClearAllRenderItems();

		// Add two cones... should not break the existing consolidated mesh just create a new one.
		auto cone1 = MaxUsd::FetchOrCreatePrim<pxr::UsdGeomXformable>(stage, pxr::SdfPath("/cone1"), pxr::TfToken("Cone"));
		auto cone2 = MaxUsd::FetchOrCreatePrim<pxr::UsdGeomXformable>(stage, pxr::SdfPath("/cone2"), pxr::TfToken("Cone"));
		TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
		ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
		EXPECT_EQ(192, GetTriCount(renderItems.GetRenderItem(0), false));
		EXPECT_EQ(60, GetTriCount(renderItems.GetRenderItem(1), false));
		renderItems.ClearAllRenderItems();

		// Hide the first sphere... should start over the consolidation, so we get one consolidated mesh with both cones and the cube.
		const pxr::UsdGeomImageable imageableSphere1Prim(sphere1);
		imageableSphere1Prim.GetVisibilityAttr().Set(pxr::UsdGeomTokens->invisible);

		TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
		ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());
		EXPECT_EQ(72, GetTriCount(renderItems.GetRenderItem(0), false));
		renderItems.ClearAllRenderItems();

		// Hide everything expect one of the cones...only a single prim left, shouldnt be consolidated
		cube1.GetVisibilityAttr().Set(pxr::UsdGeomTokens->invisible);
		cone1.GetVisibilityAttr().Set(pxr::UsdGeomTokens->invisible);
		TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
		ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());
		EXPECT_EQ(30, GetTriCount(renderItems.GetRenderItem(0), true)); // Unconsolidated
		renderItems.ClearAllRenderItems();
	}	
}

// Test consolidation when changing material assignment.
TEST(Consolidation, BreakingModificationsMaterialChanges)
{
	// Test with static and dynamic modes.
	for (int i = 0; i < int(HdMaxConsolidator::Strategy::Off); ++i)
	{
		HdMaxConsolidator::Config consolidationConfig;
		consolidationConfig.strategy = static_cast<HdMaxConsolidator::Strategy>(i);
		consolidationConfig.maxTriangles = 5000;
		consolidationConfig.maxCellSize = 10000;
		consolidationConfig.maxInstanceCount = 1000;
		consolidationConfig.staticDelay = 0;

		const auto stage = pxr::UsdStage::CreateInMemory();
		auto cube1 = MaxUsd::FetchOrCreatePrim<pxr::UsdGeomCube>(stage, pxr::SdfPath("/cube1"), pxr::TfToken("Cube"));
		auto cube2 = MaxUsd::FetchOrCreatePrim<pxr::UsdGeomCube>(stage, pxr::SdfPath("/cube2"), pxr::TfToken("Cube"));

		HdMaxEngine testEngine;
		MockRenderItemContainer renderItems;

		// Use display colors as nitrous materials..
		pxr::HdChangeTracker dummyTracker;
		auto& displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
		displaySettings.SetDisplayMode(HdMaxDisplaySettings::USDDisplayColor, dummyTracker);
		consolidationConfig.displaySettings = displaySettings;

		// Render...consolidate both cubes, they share the same material (color).
		TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
		EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
		EXPECT_EQ(24, GetTriCount(renderItems.GetRenderItem(0), false));
		renderItems.ClearAllRenderItems();

		// Change the color of one of the cubes.
		pxr::VtVec3fArray blueColor = {{ 0.0, 0.0, 1.0 }};
		cube1.CreateDisplayColorAttr().Set(blueColor);
		// Cube no longer should be consolidated together, as they differ in material.
		TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
		EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
		EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(0), true)); // Unconsolidated
		EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(1), true)); // Unconsolidated
		renderItems.ClearAllRenderItems();

		// Add a third cube, sharing the same color/material as cube1...they should get consolidated.
		auto cube3 = MaxUsd::FetchOrCreatePrim<pxr::UsdGeomCube>(stage, pxr::SdfPath("/cube3"), pxr::TfToken("Cube"));
		cube3.CreateDisplayColorAttr().Set(blueColor);

		TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
		EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
		EXPECT_EQ(24, GetTriCount(renderItems.GetRenderItem(0), false));
		EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(1), true)); // Unconsolidated
		renderItems.ClearAllRenderItems();

		// Change the color again and create a cone of the same color.
		pxr::VtVec3fArray greenColor = {{ 0.0, 1.0, 0.0 }};
		cube1.CreateDisplayColorAttr().Set(greenColor);
		auto cone1 = MaxUsd::FetchOrCreatePrim<pxr::UsdGeomCone>(stage, pxr::SdfPath("/cone"), pxr::TfToken("Cone"));
		cone1.CreateDisplayColorAttr().Set(greenColor);
		TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
		EXPECT_EQ(3, renderItems.GetNumberOfRenderItems());
		EXPECT_EQ(42, GetTriCount(renderItems.GetRenderItem(0), false)); // cube1 + cone1 - green - consolidated
		EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(1), true)); // cube2 / default color unconsolidated
		EXPECT_EQ(12, GetTriCount(renderItems.GetRenderItem(2), true)); // cube3 / blue / unconsolidated
		renderItems.ClearAllRenderItems();
	}	
}

// Test consolidation behavior on an animated instancer.
TEST(Consolidation, BreakingModificationsInstancer)
{
	// Test with static and dynamic consolidation.
	for (int i = 0; i < int(HdMaxConsolidator::Strategy::Off); ++i)
	{
		HdMaxConsolidator::Config consolidationConfig;
		consolidationConfig.strategy = static_cast<HdMaxConsolidator::Strategy>(i);
		consolidationConfig.maxTriangles = 5000;
		consolidationConfig.maxCellSize = 10000;
		consolidationConfig.maxInstanceCount = 1000;
		consolidationConfig.staticDelay = 0;

		auto testDataPath = GetTestDataPath();
		const auto filePath = testDataPath.append("consolidation_animated_point_instancer.usda");
		const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));
		
		HdMaxEngine testEngine;
		MockRenderItemContainer renderItems;

		// Use display colors as nitrous materials..
		pxr::HdChangeTracker dummyTracker;
		auto& displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
		displaySettings.SetDisplayMode(HdMaxDisplaySettings::USDDisplayColor, dummyTracker);
		consolidationConfig.displaySettings = displaySettings;

		// File has a point instancer, 2 protos (cone and cube), with animated transforms (and count).

		// Between timecode 0 and 1, only transforms are animated. Use the bounding box of the resulting consolidation
		// To validate the new transform is taken into account. Then, between 1 and 2, more instances are added for each
		// prototype. 

		// Time code 0 - consolidate!
		TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
		EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
		EXPECT_EQ(60, GetTriCount(renderItems.GetRenderItem(0), false)); // 2 cones, same color, consolidated...

		// Custom box compare, math has changed across some usd versions (21.11 -> 22.11) and we cant be
		// too precise in the comparison(epsilon 0.001).
		auto boxesAreEquivalent = [](const Box3& box1, const Box3& box2) {
			const float epsilon = 0.001f;
			return abs(box1.pmax.x - box2.pmax.x) < epsilon &&
				abs(box1.pmax.y - box2.pmax.y) < epsilon &&
				abs(box1.pmax.z - box2.pmax.z) < epsilon &&
				abs(box1.pmin.x - box2.pmin.x) < epsilon &&
				abs(box1.pmin.y - box2.pmin.y) < epsilon &&
				abs(box1.pmin.z - box2.pmin.z) < epsilon;
		};

		auto expectedBbox0 = Box3{ {-1.f, 1.549f, -1.f},{3.5f, 3.45099998f, 3.5f } };
						
		EXPECT_TRUE(boxesAreEquivalent(expectedBbox0, GetBoundingBox(renderItems.GetRenderItem(0), false)));
		// Cube only instanced once, so not consolidated.
		// There is no exposed way to access the instancing data, but we can still know this is a instance render item and
		// not a render item from consolidation by looking at the geometry, which will be null here in the case of
		// instances.
		EXPECT_EQ(nullptr, GetRenderItemGeometry(renderItems.GetRenderItem(1), false));
		renderItems.ClearAllRenderItems();

		// Timecode 1 - only transforms changed
		TestRender(stage, testEngine, renderItems, 1, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
		EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
		EXPECT_EQ(60, GetTriCount(renderItems.GetRenderItem(0), false)); // 2 cones, same color, consolidated...
		auto expectedBbox1 = Box3{ {-1.f, 1.54900002f, 0.f }, {3.5f, 3.45099998f, 11.5f }};
		EXPECT_TRUE(boxesAreEquivalent(expectedBbox1, GetBoundingBox(renderItems.GetRenderItem(0), false)));
		EXPECT_EQ(nullptr, GetRenderItemGeometry(renderItems.GetRenderItem(1), false));
		renderItems.ClearAllRenderItems();
		
		// Time code 2 - more instances added. Test that we get expected consolidated mesh size and bboxes. Order is non deterministic
		// so use a map.
		std::map<int, Box3> expectedSizeAndBbox;
		expectedSizeAndBbox.insert({ 24, Box3{ {-1.f, -1.f, 0.f }, {11.f, 11.f, 11.f } } });
		expectedSizeAndBbox.insert({ 90, Box3{ {-1.f, 1.54900002f, 0.f } ,{6.f, 5.95100021f, 11.5f } }});
		std::map<int, Box3> actualSizeAndBbox;
		TestRender(stage, testEngine, renderItems, 2, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
		EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
		actualSizeAndBbox.insert({ GetTriCount(renderItems.GetRenderItem(0), false),  GetBoundingBox(renderItems.GetRenderItem(0), false) });
		actualSizeAndBbox.insert({ GetTriCount(renderItems.GetRenderItem(1), false),  GetBoundingBox(renderItems.GetRenderItem(1), false) });

		EXPECT_TRUE(boxesAreEquivalent(actualSizeAndBbox[0], expectedSizeAndBbox[0]));
		EXPECT_TRUE(boxesAreEquivalent(actualSizeAndBbox[1], expectedSizeAndBbox[1]));
		renderItems.ClearAllRenderItems();
	}
}

// Test disabled consolidation.
TEST(Consolidation, Disabled)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_4_materials.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Off;
	consolidationConfig.maxTriangles = 5000;
	consolidationConfig.maxCellSize = 10000;
	consolidationConfig.staticDelay = 0;
	consolidationConfig.maxInstanceCount = 1000;

	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull }, consolidationConfig);
	EXPECT_EQ(10, renderItems.GetNumberOfRenderItems()); // All then objects are expected, as consolidation is disabled.
}

TEST(Consolidation, Consolidate2BoxesWithBadPrimvarIndices)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_2_boxes_bad_primvar_indices.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 100;
	consolidationConfig.maxCellSize = 1000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull, pxr::HdReprTokens->wire },
			consolidationConfig);

	ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Shaded,
			renderItems.GetRenderItem(0).GetVisibilityGroup());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe,
			renderItems.GetRenderItem(1).GetVisibilityGroup());

	// Shaded item...
	const auto& shadedRenderItem = renderItems.GetRenderItem(0);
	auto shadedGeometry = GetRenderItemGeometry(shadedRenderItem, false);

	ASSERT_NE(nullptr, shadedGeometry);
	EXPECT_EQ(MaxSDK::Graphics::PrimitiveTriangleList, shadedGeometry->GetPrimitiveType());
	EXPECT_EQ(4, shadedGeometry->GetVertexBufferCount());
	EXPECT_TRUE(shadedGeometry->GetIndexBuffer().IsValid());

	// Check normals, normals from the first box fall back to 0 on invalid data.
	auto normalsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(48, normalsBuffer.GetNumberOfVertices());

	const auto normalsData = reinterpret_cast<Point3*>(normalsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

	std::array<Point3, 48> expectedNormals = { Point3(0.000000, 0.000000, -1.000000),
		Point3(0.000000, 0.000000, -1.000000), Point3(0.000000, 0.000000, -1.000000),
		Point3(0.000000, 0.000000, -1.000000), Point3(0.000000, 0.000000, 1.000000),
		Point3(0.000000, 0.000000, 1.000000), Point3(0.000000, 0.000000, 1.000000),
		Point3(0.000000, 0.000000, 0.000000), Point3(0.000000, -1.000000, 0.000000),
		Point3(0.000000, -1.000000, 0.000000), Point3(0.000000, -1.000000, 0.000000),
		Point3(0.000000, -1.000000, 0.000000), Point3(1.000000, 0.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(1.000000, 0.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 1.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(0.000000, 1.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(-1.000000, 0.000000, 0.000000),
		Point3(0.000000, 0.000000, 0.000000), Point3(-1.000000, 0.000000, 0.000000),
		Point3(-1.000000, 0.000000, 0.000000), Point3(-0.605790, -0.527004, -0.596058),
		Point3(-0.605790, -0.527004, -0.596058), Point3(-0.605790, -0.527004, -0.596058),
		Point3(-0.605790, -0.527004, -0.596058), Point3(0.605790, 0.527004, 0.596058),
		Point3(0.605790, 0.527004, 0.596058), Point3(0.605790, 0.527004, 0.596058),
		Point3(0.605790, 0.527004, 0.596058), Point3(0.375654, -0.849863, 0.369619),
		Point3(0.375654, -0.849863, 0.369619), Point3(0.375654, -0.849863, 0.369619),
		Point3(0.375654, -0.849863, 0.369619), Point3(0.701358, 0.000000, -0.712810),
		Point3(0.701358, 0.000000, -0.712810), Point3(0.701358, 0.000000, -0.712810),
		Point3(0.701358, 0.000000, -0.712810), Point3(-0.375654, 0.849863, -0.369619),
		Point3(-0.375654, 0.849863, -0.369619), Point3(-0.375654, 0.849863, -0.369619),
		Point3(-0.375654, 0.849863, -0.369619), Point3(-0.701358, 0.000000, 0.712810),
		Point3(-0.701358, 0.000000, 0.712810), Point3(-0.701358, 0.000000, 0.712810),
		Point3(-0.701358, 0.000000, 0.712810) };
	EXPECT_TRUE(Point3ArraysAreAlmostEqual(
			expectedNormals.data(), static_cast<int>(expectedNormals.size()), normalsData, static_cast<int>(normalsBuffer.GetNumberOfVertices())));

	// Check uvs, uvs from the second box falls back to planar mapping (from points) on invalid data.
	auto uvsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	EXPECT_EQ(48, uvsBuffer.GetNumberOfVertices());

	std::array<Point3, 48> expectedUvs = { Point3(1.000000, 1.000000, 0.000000), Point3(1.000000, 0.000000, 0.000000),
		Point3(0.000000, 0.000000, 0.000000), Point3(0.000000, 1.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(0.000000, 1.000000, 0.000000), Point3(1.000000, 1.000000, 0.000000),
		Point3(1.000000, 0.000000, 0.000000), Point3(0.000000, 0.000000, 0.000000),
		Point3(1.00000000, 1.00000000, 0.00000000), Point3(1.00000000, 0.00000000, 0.00000000),
		Point3(0.00000000, 0.00000000, 0.00000000), Point3(0.00000000, 1.00000000, 0.00000000),
		Point3(0.00000000, 1.00000000, 0.00000000), Point3(1.00000000, 1.00000000, 0.00000000),
		Point3(1.00000000, 0.00000000, 0.00000000), Point3(0.00000000, 1.00000000, 0.00000000),
		Point3(0.00000000, 1.00000000, 0.00000000), Point3(1.00000000, 1.00000000, 0.00000000),
		Point3(1.00000000, 0.00000000, 0.00000000), Point3(0.00000000, 0.00000000, 0.00000000),
		Point3(0.00000000, 1.00000000, 0.00000000), Point3(1.00000000, 1.00000000, 0.00000000),
		Point3(1.00000000, 0.00000000, 0.00000000), Point3(0.00000000, 0.00000000, 0.00000000),
		Point3(0.00000000, 1.00000000, 0.00000000), Point3(1.00000000, 1.00000000, 0.00000000),
		Point3(1.00000000, 0.00000000, 0.00000000), Point3(0.00000000, 0.00000000, 0.00000000),
		Point3(0.00000000, 1.00000000, 0.00000000), Point3(0.00000000, 1.00000000, 0.00000000),
		Point3(1.00000000, 0.00000000, 0.00000000), Point3(0.00000000, 0.00000000, 0.00000000)
	};
	const auto uvsData = reinterpret_cast<Point3*>(uvsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(Point3ArraysAreAlmostEqual(
			expectedUvs.data(), static_cast<int>(expectedUvs.size()), uvsData, static_cast<int>(uvsBuffer.GetNumberOfVertices())));

}

// Test consolidation of 2 boxes with only part of the geometry requiring consolidation :
// Only one of the subsets in each of the boxes is bound to a material. So only
// that subset is consolidated, the rest of the boxes' faces are using the
// display color, which is different for each box.
TEST(Consolidation, PartialSubsetsConsolidation)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("partial_subset_consolidation.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 100;
	consolidationConfig.maxCellSize = 1000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);

	// Expect 3 render items.
	// 1 - A consolidated mesh, from the material bound subsets of each box.
	// 2 - The rest of the faces (using the displayColor) from the first box.
	// 2 - The rest of the faces (using the displayColor) from the second box.
	ASSERT_EQ(3, renderItems.GetNumberOfRenderItems());
	
	// Consolidated portion of the boxes...
	const auto consolidatedGeom = GetRenderItemGeometry(renderItems.GetRenderItem(0), false);
	const auto trianglesIndexBuffer = consolidatedGeom->GetIndexBuffer();
	// Both top faces share the same material -> 2 quads -> 4 tris -> 12 indices.
	EXPECT_EQ(12, trianglesIndexBuffer.GetNumberOfIndices());

	// The rest of box 1...
	const auto box1Geom = GetRenderItemGeometry(renderItems.GetRenderItem(1), true);
	const auto box1TrianglesIndexBuffer = box1Geom->GetIndexBuffer();
	EXPECT_EQ(30, box1TrianglesIndexBuffer.GetNumberOfIndices()); // 5 quads -> 10 tris -> 30 indices.

	// The rest of box 2...
	const auto box2Geom = GetRenderItemGeometry(renderItems.GetRenderItem(2), true);
	const auto box2TrianglesIndexBuffer = box2Geom->GetIndexBuffer();
	EXPECT_EQ(30, box2TrianglesIndexBuffer.GetNumberOfIndices()); // 5 quads -> 10 tris -> 30 indices.
}

// We had an issue when only part of a mesh's subsets were being consolidated - because vertex buffers are
// shared across the subsets - we were only looking at the first subset's dirty state when deciding to load
// the geometry into nitrous buffers. However, considering consolidation, it is possible for subsets to have
// different dirty states. If the first subsets was being consolidated, then we would never load the buffers,
// even though they were required by other subsets. Now, we load the buffers if required by any of the subsets.
// The following test makes sure this is done.
TEST(Consolidation, PartialSubsetsConsolidationSecondSubsetDirty)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("partial_subset_consolidation_second_subset.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 100;
	consolidationConfig.maxCellSize = 1000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);

	// Expect 4 render items.
	// 1 - A consolidated mesh, from the material bound subsets of each box.
	// 2,3 - The rest of the first box (one subset with a material, one for the displayColor)
	// 4 - The rest of the second box (using the displayColor).
	ASSERT_EQ(4, renderItems.GetNumberOfRenderItems());
	
	// Consolidated portion of the boxes...
	const auto consolidatedGeom = GetRenderItemGeometry(renderItems.GetRenderItem(0), false);
	const auto consolidatedPointsBuffer = consolidatedGeom->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_TRUE(consolidatedPointsBuffer.IsValid());

	// Make sure the vertex buffers where loaded, even if the first subset of the mesh was consolidated.
	for (int i = 1; i < renderItems.GetNumberOfRenderItems(); ++i)
	{
		const auto geom = GetRenderItemGeometry(renderItems.GetRenderItem(i), true);
		const auto vertexBuffer = geom->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
		EXPECT_TRUE(vertexBuffer.IsValid());
	}
}

TEST(Consolidation, GeomSubsetInstanceSplit)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_6_instanced_boxes.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 20000;

	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	pxr::HdChangeTracker dummyTracker;
	auto& displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
	displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);
	consolidationConfig.displaySettings = displaySettings;

	consolidationConfig.maxCellSize = 72;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
	
	consolidationConfig.maxCellSize = 71;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());

	consolidationConfig.maxCellSize = 50;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
	
	consolidationConfig.maxCellSize = 36;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());

	consolidationConfig.maxCellSize = 35;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);
	EXPECT_EQ(3, renderItems.GetNumberOfRenderItems());

	consolidationConfig.maxCellSize = 24;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);
	EXPECT_EQ(3, renderItems.GetNumberOfRenderItems());

	consolidationConfig.maxCellSize = 23;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);
	EXPECT_EQ(6, renderItems.GetNumberOfRenderItems());

	consolidationConfig.maxCellSize = 11;  // All subsets have the same material, considered together, not enough space in a cell. No consolidation.
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());

	// No consolidation can happen, end up with on render item per subset (instanced render items, containing 6 instances each)
	consolidationConfig.maxCellSize = 3;
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());
}

// Validating the consolidation of a model which was causing a crash MAXX-70256 because
// of instances with subsets.
TEST(Consolidation, InstancedSubsetSplitCrash)
{
	auto testDataPath = GetTestDataPath();
	// This file has a instances with subsets which split unevenly across several merged meshes,
	// this scenario, in this specific cas, was causing a crash.
	const auto filePath = testDataPath.append("consolidation_subset_crash.usdc");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 20000;
	consolidationConfig.maxCellSize = 200000;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	pxr::HdChangeTracker dummyTracker;
	auto& displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
	displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);
	consolidationConfig.displaySettings = displaySettings;
	
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);

	ASSERT_EQ(51, renderItems.GetNumberOfRenderItems());
}

// Testing that instances with subsets, which share materials with non-instanced geometry in the scene,
// get consolidated correctly.
TEST(Consolidation, InstancedSubsetMixed)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("consolidation_instance_subset_mixed.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	HdMaxConsolidator::Config consolidationConfig;
	consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
	consolidationConfig.maxTriangles = 20000;
	consolidationConfig.maxCellSize = 22;
	consolidationConfig.maxInstanceCount = 1000;
	consolidationConfig.staticDelay = 0;

	pxr::HdChangeTracker dummyTracker;
	auto& displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
	displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);
	consolidationConfig.displaySettings = displaySettings;

	// The scene is composed of 3 instanced boxes and 3 planes composed of 8 triangles.
	// (3 * 12 triangles) + (3 * 8 triangles) = 60 total.

	// The boxes have 2 subsets, one of 4 triangles, and one of 8, but have the same material in VP.
	// Expect 3 render items : 3 X (1 box + 1 plane = 20 tris)
	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull },
			consolidationConfig);

	ASSERT_EQ(3, renderItems.GetNumberOfRenderItems());

	// First consolidation cell :
	// 1 box + plane = 20 tris = 60 indices
	const auto geom0 = GetRenderItemGeometry(renderItems.GetRenderItem(0), false);
	const auto indexBuffer0 = geom0->GetIndexBuffer();
	EXPECT_TRUE(indexBuffer0.IsValid());
	EXPECT_EQ(60, indexBuffer0.GetNumberOfIndices());

	// Second cell :
	// 1 box + plane = 20 tris = 60 indices
	const auto geom1 = GetRenderItemGeometry(renderItems.GetRenderItem(1), false);
	const auto indexBuffer1 = geom1->GetIndexBuffer();
	EXPECT_TRUE(indexBuffer1.IsValid());
	EXPECT_EQ(60, indexBuffer1.GetNumberOfIndices());

	// Third cell : 
	// 1 box + plane = 20 tris = 60 indices
	const auto geom2 = GetRenderItemGeometry(renderItems.GetRenderItem(2), false);
	const auto indexBuffer2 = geom2->GetIndexBuffer();
	EXPECT_TRUE(indexBuffer2.IsValid());
	EXPECT_EQ(60, indexBuffer2.GetNumberOfIndices());
}