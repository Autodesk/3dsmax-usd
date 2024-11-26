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
#include <gtest/gtest.h>

#include <max.h>
#include <Graphics/GeometryRenderItemHandle.h>
#include <Graphics/SimpleRenderGeometry.h>
#include <Graphics/StandardMaterialHandle.h>

#include <pxr/base/gf/rotation.h>

#include "TestHelpers.h"
#include <MaxUsd/Utilities/TypeUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <RenderDelegate/HdMaxEngine.h>
#include <RenderDelegate/HdMaxRenderData.h>
#include <RenderDelegate/HdMaxColorMaterial.h>

// Tests rendering a USD cube primitive to a RenderItem.
TEST(USDRenderDelegateGeneralTest, SimpleCube)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	stage->DefinePrim(pxr::SdfPath("/cube"), pxr::TfToken("Cube"));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;

	// Render with to both wireframe and shaded items.
	TestRender(stage, testEngine, renderItems, 0, nullptr, {pxr::HdReprTokens->smoothHull, pxr::HdReprTokens->wire});

	auto& renderDelegate = testEngine.GetRenderDelegate();
	auto renderData = renderDelegate->GetRenderDataIdMap();

	ASSERT_EQ(1, renderData.size());
	auto it = renderData.find(pxr::SdfPath("/cube"));
	ASSERT_TRUE(it != renderData.end());

	// Two render items : shaded + wireframe
	ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
	
	// Make sure that the render items was correctly added to the container (shaded / wireframe)

	// Shaded item...
	auto& shadedRenderItem = renderItems.At(0);
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Shaded, shadedRenderItem.GetVisibilityGroup());
	
	auto shadedGeometry = GetRenderItemGeometry(shadedRenderItem, true);
	ASSERT_EQ(shadedGeometry, renderDelegate->GetRenderData(it->second).shadedSubsets[0].geometry->GetSimpleRenderGeometry());
	
	ASSERT_NE(nullptr, shadedGeometry);
	ASSERT_EQ(MaxSDK::Graphics::PrimitiveTriangleList, shadedGeometry->GetPrimitiveType());
	ASSERT_EQ(4, shadedGeometry->GetVertexBufferCount());
	ASSERT_TRUE(shadedGeometry->GetIndexBuffer().IsValid());

	// Check that points are OK.	
	auto pointsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(8, pointsBuffer.GetNumberOfVertices());
	std::array<Point3,8> expectedPoints = {
		Point3(1,1,1),
		Point3(-1,1,1),
		Point3(-1,-1,1),
		Point3(1,-1,1),
		Point3(-1,-1,-1),
		Point3(-1,1,-1),
		Point3(1,1,-1),
		Point3(1,-1,-1) };

	const auto pointsData = reinterpret_cast<Point3*>(pointsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedPoints.begin(), expectedPoints.end(), pointsData));
	
	// Check that computed smooth normals are OK.
	auto normalsBuffer = shadedGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(8, normalsBuffer.GetNumberOfVertices());
	std::array<Point3, 8> expectedNormals = {
		Point3(0.577350259 ,0.577350259 ,0.577350259),
		Point3(-0.577350259 ,0.577350259 ,0.577350259),
		Point3(-0.577350259 ,-0.577350259 ,0.577350259),
		Point3(0.577350259 ,-0.577350259 ,0.577350259),
		Point3(-0.577350259 ,-0.577350259 ,-0.577350259),
		Point3(-0.577350259 ,0.577350259 ,-0.577350259),
		Point3(0.577350259 ,0.577350259 ,-0.577350259),
		Point3(0.577350259 ,-0.577350259 ,-0.577350259)
	};
	const auto normalsdata = reinterpret_cast<Point3*>(normalsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedNormals.begin(), expectedNormals.end(), normalsdata));
	   	 	
	// Check that indices are OK.
	auto trianglesIndexBuffer = shadedGeometry->GetIndexBuffer();
	EXPECT_EQ(36, trianglesIndexBuffer.GetNumberOfIndices());
	std::array<int, 36> expectedIndices = { 0,1,2,0,2,3,4,5,6,4,6,7,0,6,5,0,5,1,4,7,3,4,3,2,0,3,7,0,7,6,4,2,1,4,1,5 };
	const auto indicesData = reinterpret_cast<int*>(trianglesIndexBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedIndices.begin(), expectedIndices.end(), indicesData));

	// Wireframe item...
	auto& wireframeRenderItem = renderItems.At(1);
	EXPECT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe, wireframeRenderItem.GetVisibilityGroup());
	
	const auto wireframeGeometry = GetRenderItemGeometry(wireframeRenderItem, true);
	ASSERT_EQ(wireframeGeometry, renderDelegate->GetRenderData(it->second).wireframe.geometry->GetSimpleRenderGeometry());
	
	ASSERT_NE(nullptr, wireframeGeometry);
	ASSERT_EQ(MaxSDK::Graphics::PrimitiveLineList, wireframeGeometry->GetPrimitiveType());
	ASSERT_EQ(3, wireframeGeometry->GetVertexBufferCount());
	ASSERT_TRUE(wireframeGeometry->GetIndexBuffer().IsValid());

	// The wireframe item should be using the same vertex buffers as the shaded geometry.
	EXPECT_EQ(pointsBuffer, wireframeGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer));
	EXPECT_EQ(normalsBuffer, wireframeGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer));
	   	 	
	// Check that indices for the wire edges are OK.
	auto edgeIndexBuffer = wireframeGeometry->GetIndexBuffer();
	EXPECT_EQ(48, edgeIndexBuffer.GetNumberOfIndices());
	std::array<int, 48> wireframeExpectedIndices = { 0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 6, 6, 5, 5, 1,
		1, 0, 4, 7, 7, 3, 3, 2, 2, 4, 0, 3, 3, 7, 7, 6, 6, 0, 4, 2, 2, 1, 1, 5, 5, 4 };
	const auto wireframeIndicesData = reinterpret_cast<int*>(edgeIndexBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(wireframeExpectedIndices.begin(), wireframeExpectedIndices.end(), wireframeIndicesData));
}

// Tests rendering animated geometry to a nitrous render items. Points and topology change over time. 
TEST(USDRenderDelegateGeneralTest, AnimatedGeometry)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	auto mesh = pxr::UsdGeomMesh(stage->DefinePrim(pxr::SdfPath("/quad"), pxr::TfToken("Mesh")));
	
	// Setup a quad - topology and vertices will both change over time.
	pxr::VtVec3fArray points0;
	points0.reserve(4);
	points0.push_back({0,2,2});
	points0.push_back({0,2,-2});
	points0.push_back({0,-2,-2});
	points0.push_back({0,-2,2});

	pxr::VtVec3fArray points1;
	points1.reserve(4);
	points1.push_back({ 2,2,0 });
	points1.push_back({ -2,2,0 });
	points1.push_back({ -2,-2,0 });
	points1.push_back({ 2,-2,0 });

	auto pointsAttr = mesh.CreatePointsAttr();
	pointsAttr.Set(points0, 0);
	pointsAttr.Set(points1, 1);

	// At timecode 0, a single quad.
	pxr::VtIntArray faceVertexCount0{ 4 };
	// At timecode 1, two triangles.
	pxr::VtIntArray faceVertexCount1{ 3,3 };

	auto vertexCountAttr = mesh.CreateFaceVertexCountsAttr();
	vertexCountAttr.Set(faceVertexCount0, 0);
	vertexCountAttr.Set(faceVertexCount1, 1);

	pxr::VtIntArray faceVertexIndices0{ 0,1,2,3 };
	pxr::VtIntArray faceVertexIndices1{ 0,2,3,0,1,2 };
	
	auto indicesAttr = mesh.CreateFaceVertexIndicesAttr();
	indicesAttr.Set(faceVertexIndices0, 0);
	indicesAttr.Set(faceVertexIndices1, 1);
	
	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;
	TestRender(stage, testEngine, renderItems, 0);

	auto& renderDelegate = testEngine.GetRenderDelegate();
	auto renderData = renderDelegate->GetRenderDataIdMap();

	ASSERT_EQ(1, renderData.size());
	auto it0 = renderData.find(pxr::SdfPath("/quad"));
	ASSERT_TRUE(it0 != renderData.end());

	// Make sure that the render item was correctly added to the container.
	auto& usdRenderItem = renderItems.At(0);		
	auto simpleRenderGeometry0 = GetRenderItemGeometry(usdRenderItem, true);
	
	ASSERT_EQ(simpleRenderGeometry0, renderDelegate->GetRenderData(it0->second).shadedSubsets[0].geometry->GetSimpleRenderGeometry());

	// Check that points are ok at timecode 0.
	auto pointsBuffer = simpleRenderGeometry0->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(4, pointsBuffer.GetNumberOfVertices());
	std::array<Point3, 4> expectedPoints = {
		MaxUsd::ToMax(points0[0]),
		MaxUsd::ToMax(points0[1]),
		MaxUsd::ToMax(points0[2]),
		MaxUsd::ToMax(points0[3])
	};
	auto pointsData = reinterpret_cast<Point3*>(pointsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedPoints.begin(), expectedPoints.end(), pointsData));
	pointsBuffer.Unlock();
	
	// Check that computed normals are ok at timecode 0.
	auto normalsBuffer = simpleRenderGeometry0->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(4, normalsBuffer.GetNumberOfVertices());
	std::array<Point3, 4> expectedNormals = {
		Point3(-1,0,0),
		Point3(-1,0,0),
		Point3(-1,0,0),
		Point3(-1,0,0)
	};
	auto normalsdata = reinterpret_cast<Point3*>(normalsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedNormals.begin(), expectedNormals.end(), normalsdata));
	normalsBuffer.Unlock();
	
	// Check that indices are OK at timecode 0.
	auto indicesBuffer = simpleRenderGeometry0->GetIndexBuffer();
	EXPECT_EQ(6, indicesBuffer.GetNumberOfIndices());
	std::array<int, 6> expectedIndices = { 0,1,2,0,2,3 };
	indicesBuffer.Unlock();
		
	auto indicesData = reinterpret_cast<int*>(indicesBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedIndices.begin(), expectedIndices.end(), indicesData));

	// Render again, this time at timecode 1.
	renderItems.ClearAllRenderItems();
	TestRender(stage, testEngine, renderItems, 1);
	
	auto renderData1 = renderDelegate->GetRenderDataIdMap();

	ASSERT_EQ(1, renderData.size());
	auto it1 = renderData.find(pxr::SdfPath("/quad"));
	ASSERT_TRUE(it1 != renderData.end());
	
	// Should have updated the same graphic object.
	ASSERT_EQ(renderDelegate->GetRenderData(it0->second).shadedSubsets[0].renderItem, renderDelegate->GetRenderData(it1->second).shadedSubsets[0].renderItem);

	// Make sure that the render item was correctly added to the container.
	auto& usdRenderItem1 = renderItems.At(0);
	auto simpleRenderGeometry1 = GetRenderItemGeometry(usdRenderItem1, true);
	ASSERT_EQ(simpleRenderGeometry1, renderDelegate->GetRenderData(it1->second).shadedSubsets[0].geometry->GetSimpleRenderGeometry());

	// Check that points are ok at timecode 1.
	pointsBuffer = simpleRenderGeometry1->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(4, pointsBuffer.GetNumberOfVertices());
	expectedPoints = {
		MaxUsd::ToMax(points1[0]),
		MaxUsd::ToMax(points1[1]),
		MaxUsd::ToMax(points1[2]),
		MaxUsd::ToMax(points1[3])
	};
	pointsData = reinterpret_cast<Point3*>(pointsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedPoints.begin(), expectedPoints.end(), pointsData));
	pointsBuffer.Unlock();
	
	// Check that computed normals are ok at timecode 1.
	normalsBuffer = simpleRenderGeometry1->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(4, normalsBuffer.GetNumberOfVertices());
	expectedNormals = {
		Point3(0,0,1),
		Point3(0,0,1),
		Point3(0,0,1),
		Point3(0,0,1)
	};
	normalsdata = reinterpret_cast<Point3*>(normalsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedNormals.begin(), expectedNormals.end(), normalsdata));
	normalsBuffer.Unlock();
	
	indicesBuffer = simpleRenderGeometry1->GetIndexBuffer();
	EXPECT_EQ(6, indicesBuffer.GetNumberOfIndices());
	expectedIndices = {0,2,3,0,1,2};
	indicesBuffer.Unlock();
	
	indicesData = reinterpret_cast<int*>(indicesBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedIndices.begin(), expectedIndices.end(), indicesData));
}

// Tests USD prim visibility handling vs Nitrous render items.
TEST(USDRenderDelegateGeneralTest, PrimVisibility)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto cube = stage->DefinePrim(pxr::SdfPath("/cube"), pxr::TfToken("Cube"));

	// Setup animated visibility.
	const pxr::UsdGeomImageable imageablePrim(cube);
	imageablePrim.GetVisibilityAttr().Set(pxr::UsdGeomTokens->invisible, 0);
	imageablePrim.GetVisibilityAttr().Set(pxr::UsdGeomTokens->inherited, 1);
	
	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;
	TestRender(stage, testEngine, renderItems, 0);
	
	// Invisible primitives should not be returned as render items.
	EXPECT_EQ(0, renderItems.GetNumberOfRenderItems());

	TestRender(stage, testEngine, renderItems, 1);
	
	// At time code 1, the prim is visible...
	EXPECT_EQ(1, renderItems.GetNumberOfRenderItems());
}

// Tests that USD transforms are propagated correctly to nitrous render items.
TEST(USDRenderDelegateGeneralTest, PrimTransforms)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	auto cube = stage->DefinePrim(pxr::SdfPath("/cube"), pxr::TfToken("Cube"));

	// Configure an animated transform for our cube.
	pxr::UsdGeomXformable xformable(cube);
	xformable.ClearXformOpOrder();

	pxr::GfMatrix4d xform0;
	xform0.SetTranslate({ 5,5,5 });
	pxr::GfMatrix4d xform1;
	xform1.SetTranslate({ 10,10,10 });

	auto xformOp = xformable.AddTransformOp();	
	xformOp.Set(xform0, 0);
	xformOp.Set(xform1, 1);
	
	HdMaxEngine testEngine;
	pxr::GfMatrix4d rootTransform;
	rootTransform.SetIdentity();
	MockRenderItemDecoratorContainer renderItems;

	// Render at timecode 0
	MockUpdateNodeContext nodeContext1{};
	testEngine.Render(stage->GetPseudoRoot(), rootTransform, renderItems, 0, MockUpdateDisplayContext{}, nodeContext1, {pxr::HdReprTokens->smoothHull}, {pxr::HdTokens->geometry},nullptr);
	
	ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());

	auto expectMatricesEqual = [](const MaxSDK::Graphics::Matrix44& m1, const MaxSDK::Graphics::Matrix44& m2) {
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				EXPECT_FLOAT_EQ(m1.m[i][j], m2.m[i][j]);
			}
		}
	};
		
	auto& usdRenderItem0 = renderItems.At(0);
	
	auto expectedMatrix = MaxUsd::ToMax(xform0);
	MaxSDK::Graphics::Matrix44 renderItemOffset; 
	usdRenderItem0.GetOffsetMatrix(renderItemOffset);

	// Check that our render item is transformed properly.
	expectMatricesEqual(renderItemOffset, expectedMatrix);

	// Render again, at timecode 1.
	renderItems.ClearAllRenderItems();
	MockUpdateNodeContext nodeContext2{};
	testEngine.Render(stage->GetPseudoRoot(), rootTransform, renderItems, 1, MockUpdateDisplayContext{}, nodeContext2, {pxr::HdReprTokens->smoothHull},  {pxr::HdTokens->geometry}, nullptr);

	auto& usdRenderItem1 = renderItems.At(0);

	// Make sure it is still the same graphics objects, moved.
	EXPECT_EQ(usdRenderItem0.GetPointer(), usdRenderItem1.GetPointer());
	expectedMatrix = MaxUsd::ToMax(xform1);
	usdRenderItem1.GetOffsetMatrix(renderItemOffset);

	// Check that the new transform was applied.
	expectMatricesEqual(renderItemOffset, expectedMatrix);

	// Now test rendering with a root transform for the render.
	rootTransform.SetTranslate({ 5,5,5 });
	MockUpdateNodeContext nodeContext3{};
	testEngine.Render(stage->GetPseudoRoot(), rootTransform, renderItems, 0, MockUpdateDisplayContext{}, nodeContext3, {pxr::HdReprTokens->smoothHull}, {pxr::HdTokens->geometry}, nullptr);
	auto& usdRenderItemWithRootTransform = renderItems.At(0);
	usdRenderItemWithRootTransform.GetOffsetMatrix(renderItemOffset);
	expectedMatrix = MaxUsd::ToMax(rootTransform  * xform0);
	expectMatricesEqual(renderItemOffset, expectedMatrix);
	
	// Test that the transform applied to the render item is the composed world transform in usd.
	auto subCube = stage->DefinePrim(pxr::SdfPath("/cube/subcube"), pxr::TfToken("Cube"));
	pxr::GfMatrix4d subXform;
	subXform.SetRotate(pxr::GfRotation(pxr::GfVec3d(1,1,1), 45));

	// Configure a transform for the sub geometry.
	pxr::UsdGeomXformable subXformable(subCube);
	subXformable.ClearXformOpOrder();
	subXformable.AddTransformOp().Set(subXform, 0);
	renderItems.ClearAllRenderItems();
	MockUpdateNodeContext nodeContext4{};
	testEngine.Render(stage->GetPseudoRoot(), rootTransform, renderItems, 0, MockUpdateDisplayContext{}, nodeContext4, {pxr::HdReprTokens->smoothHull}, {pxr::HdTokens->geometry}, nullptr);
	ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
	auto& leafRenderItem = renderItems.At(1);
	leafRenderItem.GetOffsetMatrix(renderItemOffset);
	expectedMatrix = MaxUsd::ToMax(rootTransform  * xform0 * subXform);
	expectMatricesEqual(renderItemOffset, expectedMatrix);
}

// Tests the handling of the display color attribute in USD prims when rendering them
// to Nitous render items. A standard material handle is used to represent the display color.
TEST(USDRenderDelegateGeneralTest, PrimDisplayColorMaterial)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	auto cube = stage->DefinePrim(pxr::SdfPath("/cube"), pxr::TfToken("Cube"));

	// Setup animated display colors.
	pxr::UsdGeomCube cubeGeom = pxr::UsdGeomCube(cube);
	pxr::VtVec3fArray color0 = { pxr::GfVec3f(1,0,0) };
	pxr::VtVec3fArray color1 = { pxr::GfVec3f(0,0,1) };
	
	auto colorAttr = cubeGeom.CreateDisplayColorAttr();
	colorAttr.Set(color0, 0);
	colorAttr.Set(color1, 1);

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;
	TestRender(stage, testEngine, renderItems, 0);

	const auto& renderItem = renderItems.At(0);

	// Check that the render item's material is based off the display color at timecode 0.
	const auto& material1  = static_cast<const MaxSDK::Graphics::StandardMaterialHandle&>(renderItem.GetCustomMaterial());

	EXPECT_FLOAT_EQ(material1.GetDiffuse().r, color0[0][0] * HdMaxColorMaterial::DiffuseFactor);
	EXPECT_FLOAT_EQ(material1.GetDiffuse().g, color0[0][1] * HdMaxColorMaterial::DiffuseFactor);
	EXPECT_FLOAT_EQ(material1.GetDiffuse().b, color0[0][2] * HdMaxColorMaterial::DiffuseFactor);

	EXPECT_FLOAT_EQ(material1.GetAmbient().r, color0[0][0] * HdMaxColorMaterial::AmbientFactor);
	EXPECT_FLOAT_EQ(material1.GetAmbient().g, color0[0][1] * HdMaxColorMaterial::AmbientFactor);
	EXPECT_FLOAT_EQ(material1.GetAmbient().b, color0[0][2] * HdMaxColorMaterial::AmbientFactor);

	// Render again, at time code 1.
	renderItems.ClearAllRenderItems();
	TestRender(stage, testEngine, renderItems, 1);
	
	const auto& renderItem2 = renderItems.At(0);
	const auto& material2 = static_cast<const MaxSDK::Graphics::StandardMaterialHandle&>(renderItem2.GetCustomMaterial());
	
	// Now it should be based off the display color at timecode 1.
	EXPECT_FLOAT_EQ(material2.GetDiffuse().r, color1[0][0] * 0.8f);
	EXPECT_FLOAT_EQ(material2.GetDiffuse().g, color1[0][1] * 0.8f);
	EXPECT_FLOAT_EQ(material2.GetDiffuse().b, color1[0][2] * 0.8f);

	EXPECT_FLOAT_EQ(material2.GetAmbient().r, color1[0][0] * 0.2f);
	EXPECT_FLOAT_EQ(material2.GetAmbient().g, color1[0][1] * 0.2f);
	EXPECT_FLOAT_EQ(material2.GetAmbient().b, color1[0][2] * 0.2f);
}

// Tests that changing the root primitives from which the hydra engine renders from
// works as expected. Typically this would happen when the engine is told to render a different
// stage, in which case we need to build a new scene delegate under the hood.
TEST(USDRenderDelegateGeneralTest, RenderRootChange)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	auto cube = stage->DefinePrim(pxr::SdfPath("/cube"), pxr::TfToken("Cube"));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;
	TestRender(stage, testEngine, renderItems, 0);
	
	auto renderData1 = testEngine.GetRenderDelegate()->GetRenderDataIdMap();
	ASSERT_EQ(1, renderData1.size());
	auto it1 = renderData1.find(pxr::SdfPath("/cube"));
	ASSERT_TRUE(it1 != renderData1.end());

	// Create a new stage, and render it from the same engine.
	const auto newStage = pxr::UsdStage::CreateInMemory();
	auto sphere = newStage->DefinePrim(pxr::SdfPath("/sphere"), pxr::TfToken("Sphere"));

	// Now render the sphere, part of a different stage.
	renderItems.ClearAllRenderItems();
	TestRender(newStage, testEngine, renderItems, 0);
	auto renderData2 = testEngine.GetRenderDelegate()->GetRenderDataIdMap();

	ASSERT_EQ(1, renderData2.size());
	auto it2 = renderData2.find(pxr::SdfPath("/sphere"));
	ASSERT_TRUE(it2 != renderData2.end());
}

TEST(USDRenderDelegateGeneralTest, UsdGeomSubsets_6_subsets_6_materials)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("box_6_subsets_materials.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;
	TestRender(stage, testEngine, renderItems, 0);
	
	// Box has 6 faces, 6 usd geom subsets, 6 materials, expect 6 render items.
	EXPECT_EQ(6, renderItems.GetNumberOfRenderItems());
	for (int i = 0; i < 6; i++)
	{
		const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(i));
		const auto indexBuffer = simpleRenderGeometry->GetIndexBuffer();
		EXPECT_EQ(6, indexBuffer.GetNumberOfIndices());
	}
}

TEST(USDRenderDelegateGeneralTest, UsdGeomSubsets_6_subsets_3_materials)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("box_3_subsets_materials.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;
	TestRender(stage, testEngine, renderItems, 0);
	
	// Box has 6 faces, 6 usd geom subsets, but only 3 materials bound, so expect 3 render items.
	EXPECT_EQ(3, renderItems.GetNumberOfRenderItems());
	for (int i = 0; i < 3; i++)
	{
		const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(i));
		const auto indexBuffer = simpleRenderGeometry->GetIndexBuffer();
		EXPECT_EQ(12, indexBuffer.GetNumberOfIndices());
	}
}

TEST(USDRenderDelegateGeneralTest, UsdGeomSubsets_RemainingFaces)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("geomSubsets_remaining_faces_sample.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;
	TestRender(stage, testEngine, renderItems, 0);
	
	// Box has one subset with a bound material. Other faces have no material bound.
	EXPECT_EQ(2, renderItems.GetNumberOfRenderItems());

	auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));
	auto indexBuffer = simpleRenderGeometry->GetIndexBuffer();
	EXPECT_EQ(30, indexBuffer.GetNumberOfIndices());

	simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(1));
	indexBuffer = simpleRenderGeometry->GetIndexBuffer();
	EXPECT_EQ(6, indexBuffer.GetNumberOfIndices());
}

// Test that any referenced alembic files are correctly loaded.
TEST(USDRenderDelegateGeneralTest, AlembicSupport)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("referencing_abc.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;	

	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->smoothHull, pxr::HdReprTokens->wire });

	ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Shaded,
			renderItems.GetRenderItem(0).GetVisibilityGroup());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe,
			renderItems.GetRenderItem(1).GetVisibilityGroup());

	// Validate that we have some geometry loaded, high level is sufficient,
	// alembic geometry is not handled differently than any ot
	const auto shadedGeometry = GetRenderItemGeometry(renderItems.At(0));
	const auto indexBuffer1 = shadedGeometry->GetIndexBuffer();
	EXPECT_EQ(36, indexBuffer1.GetNumberOfIndices());

	const auto wireGeometry = GetRenderItemGeometry(renderItems.At(1));
	const auto indexBuffer2 = wireGeometry->GetIndexBuffer();
	EXPECT_EQ(48, indexBuffer2.GetNumberOfIndices());
}