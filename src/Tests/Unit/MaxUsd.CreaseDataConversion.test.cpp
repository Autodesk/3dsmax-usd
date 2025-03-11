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
#include "TestUtils.h"
#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/MeshConversion/MaxMeshConversionOptions.h>
TEST(CreaseDataConversionTests, SimpleCreaseDataConversion)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/cube");
	auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	auto cube = TestUtils::CreateCube(false);

	// Prepare 3ds max edge creasing support
	float* eCreaseData = cube.edgeFloat(EDATA_CREASE);
	if (!eCreaseData)
	{
		cube.setEDataSupport(EDATA_CREASE);
		eCreaseData = cube.edgeFloat(EDATA_CREASE);
		ASSERT_TRUE(eCreaseData);
	}

	// Prepare 3ds max vertex creasing support
	float* vCreaseData = cube.vertexFloat(VDATA_CREASE);
	if (!vCreaseData)
	{
		cube.setVDataSupport(VDATA_CREASE);
		vCreaseData = cube.vertexFloat(VDATA_CREASE);
		ASSERT_TRUE(vCreaseData);
	}

	// Assign some crease data to a few edges and vertices
	pxr::VtIntArray eIndices;
	for (int i = 0; i < 4; ++i)
	{
		vCreaseData[i] = (i + 1) * 0.25f;
		eCreaseData[i] = (i + 1) * 0.25f;
		eIndices.push_back(cube.e[i].v1);
		eIndices.push_back(cube.e[i].v2);
	}


	// Perform the conversion from 3ds max to USD
	MaxUsd::MeshConverter converter;
	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{ &cube }, stage, path, MaxUsd::MaxMeshConversionOptions{}, usdMesh,
			pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);

	pxr::VtIntArray cornerIndices, creaseIndices, creaseLengths;
	pxr::VtFloatArray cornerSharpnesses, creaseSharpnesses;

	// Validate vertex creasing export
	usdMesh.GetCornerIndicesAttr().Get(&cornerIndices);
	usdMesh.GetCornerSharpnessesAttr().Get(&cornerSharpnesses);
	ASSERT_TRUE(cornerIndices.size() != 0 && cornerIndices.size() == cornerSharpnesses.size());
	EXPECT_EQ(cornerIndices, pxr::VtIntArray({ 0, 1, 2, 3 }));
	EXPECT_EQ(cornerSharpnesses, pxr::VtFloatArray({ 2.5f, 5.f, 7.5f, 10.f }));

	// Validate edge creasing export
	usdMesh.GetCreaseIndicesAttr().Get(&creaseIndices);
	usdMesh.GetCreaseLengthsAttr().Get(&creaseLengths);
	usdMesh.GetCreaseSharpnessesAttr().Get(&creaseSharpnesses);
	ASSERT_TRUE(creaseIndices.size() != 0 && creaseIndices.size() == 2 * creaseLengths.size() && creaseLengths.size() == creaseSharpnesses.size());
	EXPECT_EQ(creaseIndices, eIndices);
	EXPECT_EQ(creaseLengths, pxr::VtIntArray({ 2, 2, 2, 2 }));
	EXPECT_EQ(creaseSharpnesses, pxr::VtFloatArray({ 2.5f, 5.f, 7.5f, 10.f }));

	// Tweak USD values to test clamping on re-import
	usdMesh.GetCornerSharpnessesAttr().Set(pxr::VtFloatArray({ -1.f, 5.f, 7.5f, 12.f }));
	usdMesh.GetCreaseSharpnessesAttr().Set(pxr::VtFloatArray({ -1.f, 5.f, 7.5f, 12.f }));

	// Test that the re-converted MNMesh mesh equals the USD mesh (USD -> MNMesh)
	MNMesh reimportedMesh;
	std::map<int, std::string> channelNames;
	converter.ConvertToMNMesh(usdMesh, reimportedMesh, MaxUsd::PrimvarMappingOptions{}, channelNames);
	TestUtils::CompareUsdAndMaxMeshes(reimportedMesh, usdMesh);

	// Get re-imported mesh creasing data for comparison
	vCreaseData = reimportedMesh.vertexFloat(VDATA_CREASE);
	ASSERT_TRUE(vCreaseData); 
	eCreaseData = reimportedMesh.edgeFloat(EDATA_CREASE);
	ASSERT_TRUE(eCreaseData);
	
	// Validate vertex creasing round trip
	ASSERT_TRUE(reimportedMesh.numv == cube.numv);
	EXPECT_FLOAT_EQ(vCreaseData[0], 0.f);
	EXPECT_FLOAT_EQ(vCreaseData[1], 0.5f);
	EXPECT_FLOAT_EQ(vCreaseData[2], 0.75f);
	EXPECT_FLOAT_EQ(vCreaseData[3], 1.f);

	// Validate edge creasing round trip
	ASSERT_TRUE(reimportedMesh.nume == cube.nume);
	EXPECT_FLOAT_EQ(eCreaseData[0], 0.f);
	EXPECT_FLOAT_EQ(eCreaseData[1], 0.5f);
	EXPECT_FLOAT_EQ(eCreaseData[2], 0.75f);
	EXPECT_FLOAT_EQ(eCreaseData[3], 1.f);


#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/CreaseDataConversionTests/SimpleCreaseDataConversion.usda");
	stage->Export(exportPath);
#endif
}
TEST(CreaseDataConversionTests, DeadStructsCreasingImportTest)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/dead_structs_import");
	const auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));

	// Prepare the degenerate USD mesh to be imported
	usdMesh.CreateFaceVertexCountsAttr().Set(pxr::VtIntArray({ 0, 2, 3 }));
	usdMesh.CreateFaceVertexIndicesAttr().Set(pxr::VtIntArray({ 0, 2, 0, 1, 3 }));
	pxr::VtVec3fArray vertices;
	vertices.reserve(4);
	vertices.push_back(pxr::GfVec3f(0, 0, 0));
	vertices.push_back(pxr::GfVec3f(0, 1, 0));
	vertices.push_back(pxr::GfVec3f(0, 1, 1));
	vertices.push_back(pxr::GfVec3f(1, 1, 1));
	usdMesh.CreatePointsAttr().Set(vertices);

	// Prepare vertex creasing
	usdMesh.CreateCornerIndicesAttr().Set(pxr::VtIntArray({ 0, 2, 3 }));
	usdMesh.CreateCornerSharpnessesAttr().Set(pxr::VtFloatArray({ 5.f, 8.f, 5.f }));
	
	// Prepare edge creasing
	usdMesh.CreateCreaseIndicesAttr().Set(pxr::VtIntArray({ 0, 1, 0, 2, 1, 3 }));
	usdMesh.CreateCreaseLengthsAttr().Set(pxr::VtIntArray({ 2, 2, 2 }));
	usdMesh.CreateCreaseSharpnessesAttr().Set(pxr::VtFloatArray({ 2.5f, 7.5f, 2.5f }));

	// Import to 3ds max
	MNMesh maxMesh;
	MaxUsd::MeshConverter converter;
	std::map<int, std::string> channelNames;
	converter.ConvertToMNMesh(usdMesh, maxMesh, MaxUsd::PrimvarMappingOptions{}, channelNames);

	// Validate imported creasing data
	float* vCreaseData = maxMesh.vertexFloat(VDATA_CREASE);
	float* eCreaseData = maxMesh.edgeFloat(EDATA_CREASE);
	
	ASSERT_TRUE(vCreaseData);
	ASSERT_TRUE(eCreaseData);

	EXPECT_EQ(maxMesh.FNum(), 1);
	EXPECT_EQ(maxMesh.ENum(), 3);
	EXPECT_EQ(maxMesh.VNum(), 3);

	EXPECT_FLOAT_EQ(vCreaseData[0], 0.5f);
	EXPECT_FLOAT_EQ(vCreaseData[1], 0.f);
	EXPECT_FLOAT_EQ(vCreaseData[2], 0.5f);

	EXPECT_FLOAT_EQ(eCreaseData[0], 0.25f);
	EXPECT_FLOAT_EQ(eCreaseData[1], 0.25f);
	EXPECT_FLOAT_EQ(eCreaseData[2], 0.f);

	EXPECT_EQ(maxMesh.v[0].p, Point3(0, 0, 0));
	EXPECT_EQ(maxMesh.v[1].p, Point3(0, 1, 0));
	EXPECT_EQ(maxMesh.v[2].p, Point3(1, 1, 1));

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/CreaseDataConversionTests/DeadStructsCreasingImportTest.usda");
	stage->Export(exportPath);
#endif
}

TEST(CreaseDataConversionTests, DeadStructsCreasingExportTest)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/dead_structs_export");

	// Setup the the MNMesh
	MNMesh maxMesh;
	maxMesh.setNumFaces(1);
	maxMesh.setNumVerts(4);
	maxMesh.V(0)->p = Point3(0, 0, 0);
	maxMesh.V(1)->p = Point3(0, 1, 0);
	maxMesh.V(2)->p = Point3(0, 1, 1);
	maxMesh.V(3)->p = Point3(0, 0, 1);
	maxMesh.F(0)->SetDeg(4);
	maxMesh.F(0)->vtx[0] = 0;
	maxMesh.F(0)->vtx[1] = 1;
	maxMesh.F(0)->vtx[2] = 2;
	maxMesh.F(0)->vtx[3] = 3;
	maxMesh.FillInMesh();

	EXPECT_EQ(maxMesh.FNum(), 1);
	EXPECT_EQ(maxMesh.ENum(), 4);
	EXPECT_EQ(maxMesh.VNum(), 4);

	// Add vertex creasing data
	float* vCreaseData = maxMesh.vertexFloat(VDATA_CREASE);
	if (!vCreaseData)
	{
		maxMesh.setVDataSupport(VDATA_CREASE);
		vCreaseData = maxMesh.vertexFloat(VDATA_CREASE);
		ASSERT_TRUE(vCreaseData);
	}

	vCreaseData[0] = 0.25f;
	vCreaseData[1] = 0.75;

	// Add edge creasing data
	float* eCreaseData = maxMesh.edgeFloat(EDATA_CREASE);
	if (!eCreaseData)
	{
		maxMesh.setEDataSupport(EDATA_CREASE);
		eCreaseData = maxMesh.edgeFloat(EDATA_CREASE);
		ASSERT_TRUE(eCreaseData);
	}

	eCreaseData[0] = 0.5f;
	eCreaseData[1] = 1.f;
	eCreaseData[3] = 0.5f;

	// Dead structs
	maxMesh.v[1].SetFlag(MN_DEAD);
	maxMesh.e[1].SetFlag(MN_DEAD);

	// Export to USD
	MaxUsd::MeshConverter converter;
	pxr::UsdGeomMesh usdMesh;
	MaxUsd::MaxMeshConversionOptions options;
	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{ &maxMesh }, stage, path, options, usdMesh, pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);

	// Validate exported creasing data
	pxr::VtIntArray cornerIndices, creaseIndices, creaseLengths;
	pxr::VtFloatArray cornerSharpnesses, creaseSharpnesses;

	usdMesh.GetCornerIndicesAttr().Get(&cornerIndices);
	usdMesh.GetCornerSharpnessesAttr().Get(&cornerSharpnesses);
	EXPECT_EQ(cornerIndices, pxr::VtIntArray({ 0 }));
	EXPECT_EQ(cornerSharpnesses, pxr::VtFloatArray({ 2.5f }));

	usdMesh.GetCreaseIndicesAttr().Get(&creaseIndices);
	usdMesh.GetCreaseLengthsAttr().Get(&creaseLengths);
	usdMesh.GetCreaseSharpnessesAttr().Get(&creaseSharpnesses);
	EXPECT_EQ(creaseIndices, pxr::VtIntArray({ 0, 1, 2, 0 }));
	EXPECT_EQ(creaseLengths, pxr::VtIntArray({ 2, 2 }));
	EXPECT_EQ(creaseSharpnesses, pxr::VtFloatArray({ 5.f, 5.f }));

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/CreaseDataConversionTests/DeadStructsCreasingExportTest.usda");
	stage->Export(exportPath);
#endif
}


TEST(CreaseDataConversionTests, TimeSampledCreases)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/timeSampledCreases");

	// Create a simple mesh.
	auto animatedMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	const pxr::VtVec3fArray points = { {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f} , {0.f, 0.f, 1.f} };
	const pxr::VtIntArray faceCounts = { 3 };
	const pxr::VtIntArray indices = { 0,1,2 };
	animatedMesh.CreatePointsAttr().Set(points);
	animatedMesh.CreateFaceVertexCountsAttr().Set(faceCounts);
	animatedMesh.CreateFaceVertexIndicesAttr().Set(indices);

	// Setup edge and vertex creasing at different timeCodes.
	auto setAtTimeCode = [&](const pxr::VtIntArray& eIndices, const pxr::VtIntArray& eLengths, const pxr::VtFloatArray& eSharpenesses, const pxr::VtIntArray& vIndices, const pxr::VtFloatArray& vSharpnesses, const pxr::UsdTimeCode& timeCode)
	{
		animatedMesh.CreateCreaseIndicesAttr().Set(eIndices, timeCode);
		animatedMesh.CreateCreaseLengthsAttr().Set(eLengths, timeCode);
		animatedMesh.CreateCreaseSharpnessesAttr().Set(eSharpenesses, timeCode);
		animatedMesh.CreateCornerIndicesAttr().Set(vIndices, timeCode);
		animatedMesh.CreateCornerSharpnessesAttr().Set(vSharpnesses, timeCode);
	};

	// Default timeCode.
	const auto eIndicesDefault = { 0,1 };
	const auto eLengthsDefault = { 2 };
	const auto eSharpnesssesDefault = { 1.f };
	const auto vIndicesDefault = { 0 };
	const auto vSharpnesssesDefault = { 5.f };
	setAtTimeCode(eIndicesDefault, eLengthsDefault, eSharpnesssesDefault, vIndicesDefault, vSharpnesssesDefault, pxr::UsdTimeCode::Default());

	// timeCode 1.
	const auto eIndices1 = { 0,1, 1,2};
	const auto eLengths1 = { 2, 2};
	const auto eSharpnessses1 = { 2.f, 3.f};
	const auto vIndices1 = { 0, 1 };
	const auto vSharpnessses1 = { 5.f, 6.f};
	setAtTimeCode(eIndices1, eLengths1, eSharpnessses1, vIndices1, vSharpnessses1, 1);

	// timeCode 2.
	const auto eIndices2 = { 0,1,2 };
	const auto eLengths2 = { 3 };
	const auto eSharpnessses2 = { 4.f };
	const auto vIndices2 = { 0, 1, 2 };
	const auto vSharpnessses2 = { 7.f, 8.f, 9.f};
	setAtTimeCode(eIndices2, eLengths2, eSharpnessses2, vIndices2, vSharpnessses2, 2);

	// Test that the conversion of creases respects the specified timeCode.
	auto testAtTimeCode = [&](const pxr::VtIntArray& eIndices, const pxr::VtIntArray& eLengths, const pxr::VtFloatArray& eSharpenesses, const pxr::VtIntArray& vIndices, const pxr::VtFloatArray& vSharpnesses, const pxr::UsdTimeCode& timeCode)
	{
		MaxUsd::MeshConverter converter;
		MNMesh reimportedMesh;
		std::map<int, std::string> channelNames;
		converter.ConvertToMNMesh(
				animatedMesh, reimportedMesh, MaxUsd::PrimvarMappingOptions{}, channelNames, nullptr, timeCode);

		// Get re-imported mesh creasing data for comparison
		const auto vCreaseData = reimportedMesh.vertexFloat(VDATA_CREASE);
		ASSERT_TRUE(vCreaseData);
		const auto eCreaseData = reimportedMesh.edgeFloat(EDATA_CREASE);
		ASSERT_TRUE(eCreaseData);

		const auto maxToUsdCreaseFactor = 10.f;
		// Edge creasing
		for (int i = 0; i < eLengths.size(); ++i)
		{
			EXPECT_FLOAT_EQ(eCreaseData[i] * maxToUsdCreaseFactor, eSharpenesses[i]);
		}
		// Vertex creasing.
		for (int i = 0; i < vIndices.size(); ++i)
		{
			EXPECT_FLOAT_EQ(vCreaseData[i] * maxToUsdCreaseFactor, vSharpnesses[i]);
		}
	};
	testAtTimeCode(eIndicesDefault, eLengthsDefault, eSharpnesssesDefault, vIndicesDefault, vSharpnesssesDefault, pxr::UsdTimeCode::Default());
	testAtTimeCode(eIndices1, eLengths1, eSharpnessses1, vIndices1, vSharpnessses1, 1);
	testAtTimeCode(eIndices2, eLengths2, eSharpnessses2, vIndices2, vSharpnessses2, 2);

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/CreaseDataConversionTests/timeSampledCreases.usda");
	stage->Export(exportPath);
#endif
}

TEST(CreaseDataConversionTests, NoCreaseDataConversion)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/cube");

	auto cube = TestUtils::CreateCube(false);

	// Perform the conversion from 3ds max to USD.
	MaxUsd::MeshConverter converter;
	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{ &cube }, stage, path, MaxUsd::MaxMeshConversionOptions{}, usdMesh,
			pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);

	// We defined no creases on the max mesh... so USD attributes should not be authored.
	
	// Validate that there are no corner crease attributes.
	ASSERT_FALSE(usdMesh.GetCornerIndicesAttr().IsAuthored());
	ASSERT_FALSE(usdMesh.GetCornerSharpnessesAttr().IsAuthored());
	
	// Validate that there are no vertex crease attributes.
	ASSERT_FALSE(usdMesh.GetCreaseIndicesAttr().IsAuthored());
	ASSERT_FALSE(usdMesh.GetCreaseLengthsAttr().IsAuthored());
	ASSERT_FALSE(usdMesh.GetCreaseSharpnessesAttr().IsAuthored());

	// Even if creases are enabled, if none are actually defined, we should not get the attributes exported in USD.
	cube.setEDataSupport(EDATA_CREASE);
	cube.setVDataSupport(VDATA_CREASE);
	
	auto usdMeshWithCreaseSupport = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("MeshWithCreaseSupport")));
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{ &cube }, stage, path, MaxUsd::MaxMeshConversionOptions{},
			usdMeshWithCreaseSupport,
			pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);

	ASSERT_FALSE(usdMeshWithCreaseSupport.GetCornerIndicesAttr().IsAuthored());
	ASSERT_FALSE(usdMeshWithCreaseSupport.GetCornerSharpnessesAttr().IsAuthored());

	ASSERT_FALSE(usdMeshWithCreaseSupport.GetCreaseIndicesAttr().IsAuthored());
	ASSERT_FALSE(usdMeshWithCreaseSupport.GetCreaseLengthsAttr().IsAuthored());
	ASSERT_FALSE(usdMeshWithCreaseSupport.GetCreaseSharpnessesAttr().IsAuthored());

	// Finally, test that crease support is not enabled on the Max side if there are no USD creases authored.
	MNMesh reimportedMesh;
	std::map<int, std::string> channelNames;
	converter.ConvertToMNMesh(usdMesh, reimportedMesh, MaxUsd::PrimvarMappingOptions{}, channelNames);
	ASSERT_FALSE(reimportedMesh.vertexFloat(VDATA_CREASE));
	ASSERT_FALSE(reimportedMesh.edgeFloat(EDATA_CREASE));
	
#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/CreaseDataConversionTests/NoCreaseDataConversion.usda");
	stage->Export(exportPath);
#endif
}

TEST(CreaseDataConversionTests, OutputCreasesAtTimeCode)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/cube");
	auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	auto cube = TestUtils::CreateCube(false);

	// Prepare 3ds max edge creasing support.
	float* eCreaseData = cube.edgeFloat(EDATA_CREASE);
	if (!eCreaseData)
	{
		cube.setEDataSupport(EDATA_CREASE);
		eCreaseData = cube.edgeFloat(EDATA_CREASE);
		ASSERT_TRUE(eCreaseData);
	}

	// Prepare 3ds max vertex creasing support.
	float* vCreaseData = cube.vertexFloat(VDATA_CREASE);
	if (!vCreaseData)
	{
		cube.setVDataSupport(VDATA_CREASE);
		vCreaseData = cube.vertexFloat(VDATA_CREASE);
		ASSERT_TRUE(vCreaseData);
	}

	// Helper to test the export of vertex and edge creases at a specific USD timecodes.
	// The seed is used to generate different crease sharpness values. 
	// The creaseCount is used to control how many vertex/edge creases are created.
	// It is necessary to test with different values for seed/creaseCount at each timecode, so that
	// we know for sure that the values fetched back from USD, which we are validating against,
	// are not just interpolated from a different timecode.
	const auto testAt = [&](const pxr::UsdTimeCode& timeCode, float seed, int creaseCount) {
		// Assign some crease data to a few edges and vertices.
		pxr::VtIntArray eIndices;
		for (int i = 0; i < creaseCount; ++i)
		{
			vCreaseData[i] = float(i + 1) * seed;
			eCreaseData[i] = float(i + 1) * seed;
			eIndices.push_back(cube.e[i].v1);
			eIndices.push_back(cube.e[i].v2);
		}
		
		MaxUsd::MeshConverter converter;
		std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
		MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
		converter.ConvertToUSDMesh(MaxUsd::MeshFacade{ &cube }, stage, path, MaxUsd::MaxMeshConversionOptions{},
				usdMesh, timeCode, materialIdToFacesMap, false, intervals);

		pxr::VtIntArray cornerIndices, creaseIndices, creaseLengths;
		pxr::VtFloatArray cornerSharpnesses, creaseSharpnesses;

		// Max creases are in [0.0, 1.0], USD creases are in [0.0, 10.0].
		const float maxToUsdCrease = 10.0f;
		
		// Check that vertex creases where correctly exported at the given timecode.
		usdMesh.GetCornerIndicesAttr().Get(&cornerIndices, timeCode);
		usdMesh.GetCornerSharpnessesAttr().Get(&cornerSharpnesses, timeCode);
		ASSERT_TRUE(cornerIndices.size() == cornerSharpnesses.size());
		for (int i = 0; i < creaseCount; ++i)
		{
			EXPECT_EQ(cornerIndices[i], i);
			EXPECT_EQ(cornerSharpnesses[i], vCreaseData[i] * maxToUsdCrease);
		}

		// Check that edge creases where correctly exported at the given timecode.
		usdMesh.GetCreaseIndicesAttr().Get(&creaseIndices, timeCode);
		usdMesh.GetCreaseLengthsAttr().Get(&creaseLengths, timeCode);
		usdMesh.GetCreaseSharpnessesAttr().Get(&creaseSharpnesses, timeCode);
		ASSERT_TRUE(creaseIndices.size() == 2 * creaseLengths.size());
		ASSERT_TRUE(creaseLengths.size() == creaseSharpnesses.size());
		for (int i = 0; i < creaseCount; ++i)
		{	
			EXPECT_EQ(creaseLengths[i], 2);
			EXPECT_EQ(creaseSharpnesses[i], eCreaseData[i] * maxToUsdCrease);
		}
		EXPECT_EQ(creaseIndices, eIndices);
	};

	testAt(pxr::UsdTimeCode::Default(), 0.1f, 2);
	testAt(pxr::UsdTimeCode(1), 0.15f, 3);
	testAt(pxr::UsdTimeCode(2), 0.20f, 4);
}
