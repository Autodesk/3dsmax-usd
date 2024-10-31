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
#include <Graphics/GeometryRenderItemHandle.h>

#include "TestHelpers.h"
#include <MaxUsd/Utilities/TypeUtils.h>
#include <RenderDelegate/HdMaxEngine.h>

TEST(PrimvarsToNitrous, NormalsFaceVaryingInterp)
{
	auto testDataPath = GetTestDataPath();

	const auto filePath = testDataPath.append("cube_normals_face_varying.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));
	auto mesh = pxr::UsdGeomMesh(stage->GetPrimAtPath(pxr::SdfPath("/cube_normals_face_varying/Box001")));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;

	// Timecode 0
	TestRender(stage, testEngine, renderItems, 0);
	const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

	// Check that points are not using a shared layout, because normals are face varying.
	const auto pointsBuffer = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(24, pointsBuffer.GetNumberOfVertices());

	auto normalsBuffer0 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(24, normalsBuffer0.GetNumberOfVertices());

	std::array<Point3, 24> expectedNormals0 = { Point3(0, 0, -1), Point3(0, 0, -1), Point3(0, 0, -1), Point3(0, 0, -1),
		Point3(0, 0, 1), Point3(0, 0, 1), Point3(0, 0, 1), Point3(0, 0, 1), Point3(0, -1, 0), Point3(0, -1, 0),
		Point3(0, -1, 0), Point3(0, -1, 0), Point3(1, 0, 0), Point3(1, 0, 0), Point3(1, 0, 0), Point3(1, 0, 0),
		Point3(0, 1, 0), Point3(0, 1, 0), Point3(0, 1, 0), Point3(0, 1, 0), Point3(-1, 0, 0), Point3(-1, 0, 0),
		Point3(-1, 0, 0), Point3(-1, 0, 0) };
	const auto normalData0 = reinterpret_cast<Point3*>(normalsBuffer0.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedNormals0.begin(), expectedNormals0.end(), normalData0));
	normalsBuffer0.Unlock();

	// Time code 1
	TestRender(stage, testEngine, renderItems, 1);

	auto normalsBuffer1 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(24, normalsBuffer1.GetNumberOfVertices());

	std::array<Point3, 24> expectedNormals1 = { Point3(0, 0, 1), Point3(0, 0, 1), Point3(0, 0, 1), Point3(0, 0, 1),
		Point3(0, 0, -1), Point3(0, 0, -1), Point3(0, 0, -1), Point3(0, 0, -1), Point3(0, 1, 0), Point3(0, 1, 0),
		Point3(0, 1, 0), Point3(0, 1, 0), Point3(-1, 0, 0), Point3(-1, 0, 0), Point3(-1, 0, 0), Point3(-1, 0, 0),
		Point3(0, -1, 0), Point3(0, -1, 0), Point3(0, -1, 0), Point3(0, -1, 0), Point3(1, 0, 0), Point3(1, 0, 0),
		Point3(1, 0, 0), Point3(1, 0, 0) };
	const auto normalData1 = reinterpret_cast<Point3*>(normalsBuffer1.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedNormals1.begin(), expectedNormals1.end(), normalData1));
	normalsBuffer1.Unlock();
}

TEST(PrimvarsToNitrous, NormalsUniformInterp)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("cube_normals_uniform.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));
	auto mesh = pxr::UsdGeomMesh(stage->GetPrimAtPath(pxr::SdfPath("/cube_normals_uniform/Box001")));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;

	// Timecode 0
	TestRender(stage, testEngine, renderItems, 0);
	const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

	// Check that points are not using a shared layout, because normals are face varying.
	const auto pointsBuffer = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(24, pointsBuffer.GetNumberOfVertices());

	auto normalsBuffer0 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(24, normalsBuffer0.GetNumberOfVertices());

	std::array<Point3, 24> expectedNormals0 = { Point3(0, 0, -1), Point3(0, 0, -1), Point3(0, 0, -1), Point3(0, 0, -1),
		Point3(0, 0, 1), Point3(0, 0, 1), Point3(0, 0, 1), Point3(0, 0, 1), Point3(0, -1, 0), Point3(0, -1, 0),
		Point3(0, -1, 0), Point3(0, -1, 0), Point3(1, 0, 0), Point3(1, 0, 0), Point3(1, 0, 0), Point3(1, 0, 0),
		Point3(0, 1, 0), Point3(0, 1, 0), Point3(0, 1, 0), Point3(0, 1, 0), Point3(-1, 0, 0), Point3(-1, 0, 0),
		Point3(-1, 0, 0), Point3(-1, 0, 0) };
	const auto normalData0 = reinterpret_cast<Point3*>(normalsBuffer0.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedNormals0.begin(), expectedNormals0.end(), normalData0));
	normalsBuffer0.Unlock();

	// Time code 1
	TestRender(stage, testEngine, renderItems, 1);

	auto normalsBuffer1 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(24, normalsBuffer1.GetNumberOfVertices());

	std::array<Point3, 24> expectedNormals1 = { Point3(0, 0, 1), Point3(0, 0, 1), Point3(0, 0, 1), Point3(0, 0, 1),
		Point3(0, 0, -1), Point3(0, 0, -1), Point3(0, 0, -1), Point3(0, 0, -1), Point3(0, 1, 0), Point3(0, 1, 0),
		Point3(0, 1, 0), Point3(0, 1, 0), Point3(-1, 0, 0), Point3(-1, 0, 0), Point3(-1, 0, 0), Point3(-1, 0, 0),
		Point3(0, -1, 0), Point3(0, -1, 0), Point3(0, -1, 0), Point3(0, -1, 0), Point3(1, 0, 0), Point3(1, 0, 0),
		Point3(1, 0, 0), Point3(1, 0, 0) };
	const auto normalData1 = reinterpret_cast<Point3*>(normalsBuffer1.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedNormals1.begin(), expectedNormals1.end(), normalData1));
	normalsBuffer1.Unlock();
}

TEST(PrimvarsToNitrous, NormalsVertexInterp)
{
	auto testDataPath = GetTestDataPath();

	const auto filePath = testDataPath.append("sphere_normals_vertex.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));
	auto mesh = pxr::UsdGeomMesh(stage->GetPrimAtPath(pxr::SdfPath("/sphere_normals_vertex/Sphere001")));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;

	// Timecode 0
	TestRender(stage, testEngine, renderItems, 0);
	const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

	// Points can use shared layout, as normals can also be shared per vertex.
	const auto pointsBuffer = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(6, pointsBuffer.GetNumberOfVertices());

	auto normalsBuffer0 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(6, normalsBuffer0.GetNumberOfVertices());

	std::array<Point3, 6> expectedNormals0 = { Point3(2.4646326e-8f, 0.f, 1.f),
		Point3(-4.9292645e-8f, 1.f, -2.4646322e-8f), Point3(-1, 0, 0), Point3(0.f, -1.f, -2.4646326e-8f),
		Point3(1.f, 9.85853e-8f, -2.4646326e-8f), Point3(2.4646326e-8f, 0.f, -1.f) };
	const auto normalData0 = reinterpret_cast<Point3*>(normalsBuffer0.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedNormals0.begin(), expectedNormals0.end(), normalData0));
	normalsBuffer0.Unlock();

	// Time code 1
	TestRender(stage, testEngine, renderItems, 1);

	// Make sure that the render item was correctly added to the container.
	auto& usdRenderItem1 = renderItems.At(0);
	const MaxSDK::Graphics::GeometryRenderItemHandle& geometryRenderItem1 =
			static_cast<const MaxSDK::Graphics::GeometryRenderItemHandle&>(usdRenderItem1.GetDecoratedRenderItem());
	const auto iRenderGeometry1 = geometryRenderItem1.GetRenderGeometry();

	auto normalsBuffer1 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(6, normalsBuffer1.GetNumberOfVertices());

	std::array<Point3, 6> expectedNormals1 = { Point3(-2.4646326e-8f, 0.f, -1.f),
		Point3(4.9292645e-8f, -1.f, 2.4646322e-8f), Point3(1, 0, 0), Point3(0.f, 1.f, 2.4646326e-8f),
		Point3(-1.f, -9.85853e-8f, 2.4646326e-8f), Point3(-2.4646326e-8f, 0.f, 1.f) };
	const auto normalData1 = reinterpret_cast<Point3*>(normalsBuffer1.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedNormals1.begin(), expectedNormals1.end(), normalData1));
	normalsBuffer1.Unlock();
}

TEST(PrimvarsToNitrous, NormalsConstantInterp)
{
	auto testDataPath = GetTestDataPath();

	const auto filePath = testDataPath.append("plane_normals_constant.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));
	auto mesh = pxr::UsdGeomMesh(stage->GetPrimAtPath(pxr::SdfPath("/plane_normals_constant/Plane001")));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;

	// Timecode 0
	TestRender(stage, testEngine, renderItems, 0);
	const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

	// Points can use shared layout, as normals can also be shared per vertex.
	const auto pointsBuffer = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(9, pointsBuffer.GetNumberOfVertices());

	auto normalsBuffer0 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(9, normalsBuffer0.GetNumberOfVertices());

	std::array<Point3, 9> expectedNormals0;
	std::fill(expectedNormals0.begin(), expectedNormals0.end(), Point3(0.f, 0.f, 1.f));
	const auto normalData0 = reinterpret_cast<Point3*>(normalsBuffer0.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedNormals0.begin(), expectedNormals0.end(), normalData0));
	normalsBuffer0.Unlock();

	// Time code 1
	TestRender(stage, testEngine, renderItems, 1);

	auto normalsBuffer1 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::NormalsBuffer);
	EXPECT_EQ(9, normalsBuffer1.GetNumberOfVertices());

	std::array<Point3, 9> expectedNormals1;
	std::fill(expectedNormals1.begin(), expectedNormals1.end(), Point3(0.f, 0.f, -1.f));
	const auto normalData1 = reinterpret_cast<Point3*>(normalsBuffer1.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedNormals1.begin(), expectedNormals1.end(), normalData1));
	normalsBuffer1.Unlock();
}

TEST(PrimvarsToNitrous, UvsFaceVaryingInterp)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("cube_uv_face_varying.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;

	// Timecode 0
	TestRender(stage, testEngine, renderItems, 0);
	const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

	// Check that points are not using a shared layout, because uvs are face varying.
	const auto pointsBuffer = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(24, pointsBuffer.GetNumberOfVertices());

	auto uvBuffer0 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	EXPECT_EQ(24, uvBuffer0.GetNumberOfVertices());

	std::array<Point3, 24> expectedUvs0 = { Point3(1, 0, 0), Point3(1, 1, 0), Point3(0, 1, 0), Point3(0, 0, 0),
		Point3(0, 0, 0), Point3(1, 0, 0), Point3(1, 1, 0), Point3(0, 1, 0), Point3(0, 0, 0), Point3(1, 0, 0),
		Point3(1, 1, 0), Point3(0, 1, 0), Point3(0, 0, 0), Point3(1, 0, 0), Point3(1, 1, 0), Point3(0, 1, 0),
		Point3(0, 0, 0), Point3(1, 0, 0), Point3(1, 1, 0), Point3(0, 1, 0), Point3(0, 0, 0), Point3(1, 0, 0),
		Point3(1, 1, 0), Point3(0, 1, 0) };
	std::transform(expectedUvs0.begin(), expectedUvs0.end(), expectedUvs0.begin(),
			[](Point3& p) { return Point3(p.x, 1 - p.y, p.z); });

	const auto uvData0 = reinterpret_cast<Point3*>(uvBuffer0.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedUvs0.begin(), expectedUvs0.end(), uvData0));
	uvBuffer0.Unlock();

	// Time code 1
	TestRender(stage, testEngine, renderItems, 1);

	auto uvsBuffer1 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	EXPECT_EQ(24, uvsBuffer1.GetNumberOfVertices());
	std::array<Point3, 24> expectedUvs1 = { Point3(0, 1, 0), Point3(0, 0, 0), Point3(1, 0, 0), Point3(1, 1, 0),
		Point3(1, 1, 0), Point3(0, 1, 0), Point3(0, 0, 0), Point3(1, 0, 0), Point3(1, 1, 0), Point3(0, 1, 0),
		Point3(0, 0, 0), Point3(1, 0, 0), Point3(1, 1, 0), Point3(0, 1, 0), Point3(0, 0, 0), Point3(1, 0, 0),
		Point3(1, 1, 0), Point3(0, 1, 0), Point3(0, 0, 0), Point3(1, 0, 0), Point3(1, 1, 0), Point3(0, 1, 0),
		Point3(0, 0, 0), Point3(1, 0, 0) };
	std::transform(expectedUvs1.begin(), expectedUvs1.end(), expectedUvs1.begin(),
			[](Point3& p) { return Point3(p.x, 1 - p.y, p.z); });

	const auto uvData1 = reinterpret_cast<Point3*>(uvsBuffer1.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedUvs1.begin(), expectedUvs1.end(), uvData1));
	uvsBuffer1.Unlock();
}

TEST(PrimvarsToNitrous, UvsUniformInterp)
{
	auto testDataPath = GetTestDataPath();

	const auto filePath = testDataPath.append("cube_uv_uniform.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));
	auto mesh = pxr::UsdGeomMesh(stage->GetPrimAtPath(pxr::SdfPath("/cube_uv_uniform/Box001")));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;

	// Timecode 0
	TestRender(stage, testEngine, renderItems, 0);

	const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

	// Check that points are not using a shared layout, because uvs are face varying.
	const auto pointsBuffer = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(24, pointsBuffer.GetNumberOfVertices());

	auto uvsBuffer0 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	EXPECT_EQ(24, uvsBuffer0.GetNumberOfVertices());

	std::array<Point3, 24> expectedUvs0 = { Point3(1, 0, 0), Point3(1, 0, 0), Point3(1, 0, 0), Point3(1, 0, 0),
		Point3(1, 1, 0), Point3(1, 1, 0), Point3(1, 1, 0), Point3(1, 1, 0), Point3(0, 1, 0), Point3(0, 1, 0),
		Point3(0, 1, 0), Point3(0, 1, 0), Point3(0, 0, 0), Point3(0, 0, 0), Point3(0, 0, 0), Point3(0, 0, 0),
		Point3(0, 0, 0), Point3(0, 0, 0), Point3(0, 0, 0), Point3(0, 0, 0), Point3(1, 0, 0), Point3(1, 0, 0),
		Point3(1, 0, 0), Point3(1, 0, 0) };

	std::transform(expectedUvs0.begin(), expectedUvs0.end(), expectedUvs0.begin(),
			[](Point3& p) { return Point3(p.x, 1 - p.y, p.z); });
	const auto uvData0 = reinterpret_cast<Point3*>(uvsBuffer0.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedUvs0.begin(), expectedUvs0.end(), uvData0));
	uvsBuffer0.Unlock();

	// Time code 1
	TestRender(stage, testEngine, renderItems, 1);

	auto uvsBuffer1 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	EXPECT_EQ(24, uvsBuffer1.GetNumberOfVertices());

	std::array<Point3, 24> expectedUvs1 = { Point3(0, 1, 0), Point3(0, 1, 0), Point3(0, 1, 0), Point3(0, 1, 0),
		Point3(0, 0, 0), Point3(0, 0, 0), Point3(0, 0, 0), Point3(0, 0, 0), Point3(1, 0, 0), Point3(1, 0, 0),
		Point3(1, 0, 0), Point3(1, 0, 0), Point3(1, 1, 0), Point3(1, 1, 0), Point3(1, 1, 0), Point3(1, 1, 0),
		Point3(1, 1, 0), Point3(1, 1, 0), Point3(1, 1, 0), Point3(1, 1, 0), Point3(0, 1, 0), Point3(0, 1, 0),
		Point3(0, 1, 0), Point3(0, 1, 0) };


	std::transform(expectedUvs1.begin(), expectedUvs1.end(), expectedUvs1.begin(),
			[](Point3& p) { return Point3(p.x, 1 - p.y, p.z); });
	const auto uvData1 = reinterpret_cast<Point3*>(uvsBuffer1.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedUvs1.begin(), expectedUvs1.end(), uvData1));
	uvsBuffer1.Unlock();
}

TEST(PrimvarsToNitrous, UvsVertexInterp)
{
	auto testDataPath = GetTestDataPath();

	const auto filePath = testDataPath.append("sphere_uv_vertex.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));
	auto mesh = pxr::UsdGeomMesh(stage->GetPrimAtPath(pxr::SdfPath("/sphere_uv_vertex/Sphere001")));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;

	// Timecode 0
	TestRender(stage, testEngine, renderItems, 0);
	const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

	// Points can use shared layout, as uvs can also be shared per vertex.
	const auto pointsBuffer = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(6, pointsBuffer.GetNumberOfVertices());

	auto uvsBuffer0 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	EXPECT_EQ(6, uvsBuffer0.GetNumberOfVertices());

	std::array<Point3, 6> expectedUvs0 = { Point3(0, 1, 0), Point3(0.25f, 1.f, 0.f), Point3(0.5f, 1.f, 0.f),
		Point3(0.75f, 1.f, 0.f), Point3(0.f, 0.5f, 0.f), Point3(0.25f, 0.5f, 0.f) };
	std::transform(expectedUvs0.begin(), expectedUvs0.end(), expectedUvs0.begin(),
			[](Point3& p) { return Point3(p.x, 1 - p.y, p.z); });
	const auto uvData0 = reinterpret_cast<Point3*>(uvsBuffer0.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedUvs0.begin(), expectedUvs0.end(), uvData0));
	uvsBuffer0.Unlock();

	// Time code 1
	TestRender(stage, testEngine, renderItems, 1);

	auto uvsBuffer1 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	EXPECT_EQ(6, uvsBuffer1.GetNumberOfVertices());

	std::array<Point3, 6> expectedUvs1 = { Point3(1, 0, 0), Point3(0.75f, 0.f, 0.f), Point3(0.5f, 0.f, 0.f),
		Point3(0.25f, 0.f, 0.f), Point3(1.f, 0.5f, 0.f), Point3(0.75f, 0.5f, 0.f) };
	std::transform(expectedUvs1.begin(), expectedUvs1.end(), expectedUvs1.begin(),
			[](Point3& p) { return Point3(p.x, 1 - p.y, p.z); });
	const auto uvData1 = reinterpret_cast<Point3*>(uvsBuffer1.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedUvs1.begin(), expectedUvs1.end(), uvData1));
	uvsBuffer1.Unlock();
}

TEST(PrimvarsToNitrous, UvsConstantInterp)
{
	auto testDataPath = GetTestDataPath();

	const auto filePath = testDataPath.append("plane_uv_constant.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));
	auto mesh = pxr::UsdGeomMesh(stage->GetPrimAtPath(pxr::SdfPath("/plane_uv_constant/Plane001")));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;

	// Timecode 0
	TestRender(stage, testEngine, renderItems, 0);

	const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

	// Points can use shared layout, as uvs can also be shared per vertex.
	const auto pointsBuffer = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	EXPECT_EQ(9, pointsBuffer.GetNumberOfVertices());

	auto uvsBuffer0 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	EXPECT_EQ(9, uvsBuffer0.GetNumberOfVertices());

	std::array<Point3, 9> expectedUvs0;
	std::fill(expectedUvs0.begin(), expectedUvs0.end(), Point3(0.f, 0.f, 0.f));
	const auto uvData0 = reinterpret_cast<Point3*>(uvsBuffer0.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedUvs0.begin(), expectedUvs0.end(), uvData0));
	uvsBuffer0.Unlock();

	// Time code 1
	TestRender(stage, testEngine, renderItems, 1);

	auto uvsBuffer1 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	EXPECT_EQ(9, uvsBuffer1.GetNumberOfVertices());

	std::array<Point3, 9> expectedUvs1;
	std::fill(expectedUvs1.begin(), expectedUvs1.end(), Point3(1.f, 1.f, 0.f));
	const auto uvData1 = reinterpret_cast<Point3*>(uvsBuffer1.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	EXPECT_TRUE(std::equal(expectedUvs1.begin(), expectedUvs1.end(), uvData1));
	uvsBuffer1.Unlock();
}

// Tests the code figuring out what primvar to use for the UVs. 
TEST(PrimvarsToNitrous, UvsNonStandardPrimvarName)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("non_standard_uv.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;

	TestRender(stage, testEngine, renderItems, 0);

	// First material has the uv varname as value.
	// token inputs:varname = "bar"
	const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));
	auto uvBuffer1 = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	const auto uvData1 = reinterpret_cast<Point3*>(uvBuffer1.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	// Test any meaningful value, to make sure we selected the right primvar as UVs.
	EXPECT_TRUE(uvData1[4] == Point3(-0.26999992f, 1.f, 0.f));
	uvBuffer1.Unlock();

	// token inputs:varname.connect = </non_standard_uv/Materials/Material__26.inputs:frame:foo>
	const auto simpleRenderGeometry2 = GetRenderItemGeometry(renderItems.At(1));
	auto uvBuffer2 = simpleRenderGeometry2->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	const auto uvData2 = reinterpret_cast<Point3*>(uvBuffer2.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	// Test any meaningful value, to make sure we selected the right primvar as UVs.
	EXPECT_TRUE(uvData2[4] == Point3(-0.15f, 1.f, 0.f));
	uvBuffer2.Unlock();
}

TEST(PrimvarsToNitrous, ChangeVertexLayout)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("sphere_edit_interp.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;

	TestRender(stage, testEngine, renderItems, 0);

	const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

	// Check that points are using a shared layout, nothing prevents it.
	const auto pointsBuffer = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	ASSERT_EQ(6, pointsBuffer.GetNumberOfVertices());

	// Now we are going to change the interpolation scheme of the displayColor primvar.
	// We expect the points vertex buffer to be updated, indeed, we can no longer be
	// sharing vertices given the interpolation scheme of displayColor.

	auto mesh = pxr::UsdGeomMesh(stage->GetPrimAtPath(pxr::SdfPath("/Sphere001")));
	auto displayColorPv = mesh.GetDisplayColorPrimvar();
	displayColorPv.SetInterpolation(pxr::TfToken("uniform"));
	const pxr::VtVec3fArray uniformValues = { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 }, { 0, 1, 1 },
		{ 1, 1, 1 } };
	displayColorPv.Set(uniformValues);

	TestRender(stage, testEngine, renderItems, 0);

	const auto simpleRenderGeometryAfterChange = GetRenderItemGeometry(renderItems.At(0));

	// Check that points are no longer using a shared layout.
	const auto pointsBufferAfterChange = simpleRenderGeometryAfterChange->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	ASSERT_EQ(24, pointsBufferAfterChange.GetNumberOfVertices());
}

TEST(PrimvarsToNitrous, UvsFallback)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("box_no_uvs.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemDecoratorContainer renderItems;
	TestRender(stage, testEngine, renderItems, 0);
	const auto simpleRenderGeometry = GetRenderItemGeometry(renderItems.At(0));

	// Check that points are using a shared layout, nothing prevents it.
	auto pointsBuffer = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
	ASSERT_EQ(8, pointsBuffer.GetNumberOfVertices());

	auto uvBuffer = simpleRenderGeometry->GetVertexBuffer(HdMaxRenderData::UvsBuffer);
	ASSERT_EQ(8, uvBuffer.GetNumberOfVertices());

	const auto uvData = reinterpret_cast<Point3*>(uvBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
	const auto pointData = reinterpret_cast<Point3*>(pointsBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

	// Fallback, points directly set as UVs, simple planar mapping.
	EXPECT_TRUE(std::equal(uvData, uvData + uvBuffer.GetNumberOfVertices(), pointData));
	uvBuffer.Unlock();
}