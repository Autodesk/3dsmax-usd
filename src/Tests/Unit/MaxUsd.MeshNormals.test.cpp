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
#include <pxr/usdImaging/usdImaging/tokens.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <MaxUsd/MeshConversion/MaxMeshConversionOptions.h>
#include <MaxUsd/MeshConversion/PrimvarMappingOptions.h>

// Test that no normals are converted if using NormalsToConvert::None
TEST(MeshNormalsTests, NormalsToConvert_None)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/object");
	auto cubeWithNormals = TestUtils::CreateCube(true);

	MaxUsd::MeshConverter converter;

	// Test conversion TO usd.	
	MaxUsd::MaxMeshConversionOptions options;
	options.SetNormalsMode(MaxUsd::MaxMeshConversionOptions::NormalsMode::None);
	pxr::UsdGeomMesh usdMesh;
	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{ &cubeWithNormals }, stage, path, options, usdMesh, pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);
	pxr::UsdGeomPrimvarsAPI primVarApi(usdMesh.GetPrim());
	const auto normalPrimvar = primVarApi.GetPrimvar(pxr::UsdImagingTokens->primvarsNormals);
	EXPECT_EQ(normalPrimvar.IsDefined(), false);
	EXPECT_EQ(usdMesh.GetNormalsAttr().HasValue(), false);
}

void UnspecifiedNormals_Test(bool withSmoothingGroups)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/object");
	auto cubeWithUnspecifiedNormals = TestUtils::CreateCube(false);

	if (withSmoothingGroups)
	{
		cubeWithUnspecifiedNormals.AutoSmooth(0.1f, FALSE, FALSE);
	}	
	MaxUsd::MeshConverter converter;

	// Test conversion TO usd.	
	MaxUsd::MaxMeshConversionOptions options;
	pxr::UsdGeomMesh usdMesh;
	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{ &cubeWithUnspecifiedNormals }, stage, path, options, usdMesh, pxr::UsdTimeCode::Default(),
			materialIdToFacesMap, false, intervals);
	pxr::UsdGeomPrimvarsAPI primVarApi(usdMesh.GetPrim());
	const auto normalPrimvar = primVarApi.GetPrimvar(pxr::UsdImagingTokens->primvarsNormals);
	EXPECT_TRUE(normalPrimvar.IsDefined());

	pxr::TfToken subdivScheme;
	usdMesh.GetSubdivisionSchemeAttr().Get(&subdivScheme);
	EXPECT_EQ(subdivScheme, pxr::UsdGeomTokens->none);
		
	// Test reconversion FROM usd
	MNMesh reconvertedMesh;
	std::map<int, std::string> channelNames;
	converter.ConvertToMNMesh(usdMesh, reconvertedMesh, MaxUsd::PrimvarMappingOptions{}, channelNames);
	auto specifiedNormals = reconvertedMesh.GetSpecifiedNormals();
	EXPECT_NE(specifiedNormals, nullptr);

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/Normals/UnspecifiedNormals_SmGroups_");
	exportPath.append(std::to_string(withSmoothingGroups));
	exportPath.append(".usda");
	stage->Export(exportPath);
#endif
}

// Test that no normals are converted if max normals are not specified and there
// are no smoothing groups.
TEST(MeshNormalsTests, UnspecifiedNormals_NoSmoothingGroups)
{
	UnspecifiedNormals_Test(false);
}

// Test that normals are converted if smoothing groups are defined, even if the
// normals are unspecified.
TEST(MeshNormalsTests, UnspecifiedNormals_WithSmoothingGroups)
{
	UnspecifiedNormals_Test(true);
}

void ConstantNormals_Test(bool asPrimvar)
{
	// Test conversion TO usd.	
	pxr::UsdGeomMesh usdMesh;
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/object");
	auto quad = TestUtils::CreateQuad();
	
	quad.SpecifyNormals();
	auto normals = quad.GetSpecifiedNormals();

	normals->SetNumFaces(1);
	normals->SetNumNormals(1);
	normals->Normal(0) = Point3(0, 0, 1);
	normals->Face(0).SetDegree(4);
	normals->Face(0).SpecifyAll();
	for (int i = 0; i < normals->Face(0).GetDegree(); i++)
	{
		normals->SetNormalIndex(0, i, 0);
	}

	normals->SetAllExplicit();
	normals->CheckNormals();
	quad.InvalidateGeomCache();

	MaxUsd::MeshConverter converter;
	MaxUsd::MaxMeshConversionOptions options;
	options.SetNormalsMode(asPrimvar ? MaxUsd::MaxMeshConversionOptions::NormalsMode::AsPrimvar
									 : MaxUsd::MaxMeshConversionOptions::NormalsMode::AsAttribute);
	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{&quad}, stage, path, options, usdMesh, pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);
	pxr::VtVec3fArray usdNormals;

	if (asPrimvar)
	{
		pxr::UsdGeomMesh* ptr = &usdMesh;
		pxr::UsdGeomPrimvarsAPI primVarApi(usdMesh.GetPrim());
		auto normalPrimvar = primVarApi.GetPrimvar(pxr::UsdImagingTokens->primvarsNormals);

		EXPECT_TRUE(normalPrimvar.IsDefined());
		EXPECT_EQ(normalPrimvar.GetInterpolation(), pxr::UsdGeomTokens->constant);
		normalPrimvar.Get(&usdNormals);
	}
	else
	{
		EXPECT_EQ(usdMesh.GetNormalsInterpolation(), pxr::UsdGeomTokens->constant);
		usdMesh.GetNormalsAttr().Get(&usdNormals);
	}
	EXPECT_EQ(usdNormals.size(), 1);
	EXPECT_EQ(usdNormals[0], pxr::GfVec3f(0.f, 0.f, 1.f));

	pxr::TfToken subdivScheme;
	usdMesh.GetSubdivisionSchemeAttr().Get(&subdivScheme);
	EXPECT_EQ(subdivScheme, pxr::UsdGeomTokens->none);
	
	// Test reconversion FROM usd.
	MNMesh reconvertedMesh;
	std::map<int, std::string> channelNames;
	converter.ConvertToMNMesh(usdMesh, reconvertedMesh, MaxUsd::PrimvarMappingOptions{}, channelNames);
	TestUtils::CompareMaxMeshNormals(quad, reconvertedMesh);

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/Normals/Constant_AsPrimvar_");
	exportPath.append(std::to_string(asPrimvar));
	exportPath.append(".usda");
	stage->Export(exportPath);
#endif
}

// Test that the constant normal interpolation scheme is used when possible
// for normal attributes.
TEST(MeshNormalsTests, Constant_AsAttribute)
{
	ConstantNormals_Test(false);
}

// Test that the constant normal interpolation scheme is used when possible
// for normal primvars.
TEST(MeshNormalsTests, Constant_AsPrimvar)
{
	ConstantNormals_Test(true);
}

void Facevarying_Cube_Indexed(bool asPrimvar)
{
	MNMesh cube = TestUtils::CreateCube(true);
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/object");

	MaxUsd::MeshConverter converter;
	MaxUsd::MaxMeshConversionOptions options;
	options.SetNormalsMode(asPrimvar ? MaxUsd::MaxMeshConversionOptions::NormalsMode::AsPrimvar
									 : MaxUsd::MaxMeshConversionOptions::NormalsMode::AsAttribute);
	pxr::UsdGeomMesh usdMesh;
	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{&cube}, stage, path, options, usdMesh, pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);

	std::vector<pxr::GfVec3f> expectedNormals(6);
	expectedNormals[0] = pxr::GfVec3f(0, 0, -1);
	expectedNormals[1] = pxr::GfVec3f(0, 0, 1);
	expectedNormals[2] = pxr::GfVec3f(0, -1, 0);
	expectedNormals[3] = pxr::GfVec3f(1, 0, 0);
	expectedNormals[4] = pxr::GfVec3f(0, 1, 0);
	expectedNormals[5] = pxr::GfVec3f(-1, 0, 0);
	
	if (asPrimvar)
	{
		auto normalsPrimvar = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(pxr::UsdImagingTokens->primvarsNormals);
		EXPECT_EQ(normalsPrimvar.GetInterpolation(), pxr::UsdGeomTokens->faceVarying);

		pxr::VtVec3fArray normals;
		normalsPrimvar.Get(&normals);
		EXPECT_EQ(normals.size(), 6);
		EXPECT_EQ(normals[0], expectedNormals[0]);
		EXPECT_EQ(normals[1], expectedNormals[1]);
		EXPECT_EQ(normals[2], expectedNormals[2]);
		EXPECT_EQ(normals[3], expectedNormals[3]);
		EXPECT_EQ(normals[4], expectedNormals[4]);
		EXPECT_EQ(normals[5], expectedNormals[5]);

		pxr::VtIntArray indices;
		normalsPrimvar.GetIndices(&indices);
		int index = 0;
		for (int i = 0; i < cube.FNum(); i++)
		{
			for (int j = 0; j < cube.F(i)->deg; j++)
			{
				EXPECT_EQ(indices[index++], i);
			}
		}
	}
	else
	{
		EXPECT_EQ(usdMesh.GetNormalsInterpolation(), pxr::UsdGeomTokens->faceVarying);
		pxr::VtVec3fArray normals;
		usdMesh.GetNormalsAttr().Get(&normals);

		EXPECT_EQ(normals.size(), 24);
		for (int i = 0; i < 6; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				EXPECT_EQ(normals[i*4 + j], expectedNormals[i]);
			}
		}		
	}
	
	pxr::TfToken subdivScheme;
	usdMesh.GetSubdivisionSchemeAttr().Get(&subdivScheme);
	EXPECT_EQ(subdivScheme, pxr::UsdGeomTokens->none);

	// If as attribute, we lost information going to USD.
	// No need to test the round trip, covered by the tests without indexing.
	if (asPrimvar)
	{
		// Test reconversion FROM usd
		MNMesh reconvertedMesh;
		std::map<int, std::string> channelNames;
		converter.ConvertToMNMesh(usdMesh, reconvertedMesh, MaxUsd::PrimvarMappingOptions{}, channelNames);
		TestUtils::CompareMaxMeshNormals(cube, reconvertedMesh);
	}
	
#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/Normals/Facevarying_Cube_Indexed_Primvar_");
	exportPath.append(std::to_string(asPrimvar));
	exportPath.append(".usda");
	stage->Export(exportPath);
#endif	
}


// Test normal I/O with indexed max normals, converted as primvar normals.
// The normal indexing is expected to be preserved.
TEST(MeshNormalsTests, Facevarying_Cube_Indexed_AsPrimvar)
{
	Facevarying_Cube_Indexed(true);
}

// Test normal I/O with indexed max normals, converted as normals attributes.
// The normal indexing not expected to be preserved.
TEST(MeshNormalsTests, Facevarying_Cube_Indexed_AsAttibute)
{
	Facevarying_Cube_Indexed(false);
}

void FaceVarying_NotIndexed_Test(bool asPrimvar, bool ordered)
{
	MNMesh roof = TestUtils::CreateRoofShape();
	roof.SpecifyNormals();
	auto normals = roof.GetSpecifiedNormals();

	// 2 quads angled to form a roof.
	normals->SetNumFaces(2);
	// Not indexed so there will be duplications on purpose - we have numNormals = 2 faces * 4 vertices (quads).
	normals->SetNumNormals(8);
	auto nQuad1 = Normalize(Point3(-1, 0, 1));
	auto nQuad2 = Normalize(Point3(1, 0, 1));

	// Ordered means the normals indices come in ascending order.
	// No need to indexing on the USD side. 
	if (ordered)
	{
		normals->Normal(0) = nQuad1;
		normals->Normal(1) = nQuad1;
		normals->Normal(2) = nQuad1;
		normals->Normal(3) = nQuad1;
		normals->Normal(4) = nQuad2;
		normals->Normal(5) = nQuad2;
		normals->Normal(6) = nQuad2;
		normals->Normal(7) = nQuad2;

		normals->SetNormalIndex(0, 0, 0);
		normals->SetNormalIndex(0, 1, 1);
		normals->SetNormalIndex(0, 2, 2);
		normals->SetNormalIndex(0, 3, 3);

		normals->SetNormalIndex(1, 0, 4);
		normals->SetNormalIndex(1, 1, 5);
		normals->SetNormalIndex(1, 2, 6);
		normals->SetNormalIndex(1, 3, 7);
	}
	// Unordered, still no need for indexing because we will just reorder the normals
	// upon export.
	else
	{
		normals->Normal(0) = nQuad2;
		normals->Normal(1) = nQuad2;
		normals->Normal(2) = nQuad2;
		normals->Normal(3) = nQuad2;
		normals->Normal(4) = nQuad1;
		normals->Normal(5) = nQuad1;
		normals->Normal(6) = nQuad1;
		normals->Normal(7) = nQuad1;

		normals->SetNormalIndex(0, 0, 4);
		normals->SetNormalIndex(0, 1, 5);
		normals->SetNormalIndex(0, 2, 6);
		normals->SetNormalIndex(0, 3, 7);

		normals->SetNormalIndex(1, 0, 0);
		normals->SetNormalIndex(1, 1, 1);
		normals->SetNormalIndex(1, 2, 2);
		normals->SetNormalIndex(1, 3, 3);
	}
	
	normals->SetAllExplicit();
	normals->CheckNormals();
	roof.InvalidateGeomCache();

	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/FaceVarying_Unordered_NotIndexed_AsPrimvar");

	MaxUsd::MeshConverter converter;
	MaxUsd::MaxMeshConversionOptions options;
	options.SetNormalsMode(asPrimvar ? MaxUsd::MaxMeshConversionOptions::NormalsMode::AsPrimvar
									 : MaxUsd::MaxMeshConversionOptions::NormalsMode::AsAttribute);
	pxr::UsdGeomMesh usdMesh;
	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{&roof}, stage, path, options, usdMesh, pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);

	pxr::UsdAttribute normalsAttribute;
	if (asPrimvar)
	{
		auto normalPrimvar = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar( pxr::UsdImagingTokens->primvarsNormals);
		normalsAttribute = normalPrimvar.GetAttr();

		// Expect face varying normal interpolation, because some the roof "top" vertices have multiple
		// normals, for each adjacent face.
		EXPECT_EQ(normalPrimvar.GetInterpolation(), pxr::UsdGeomTokens->faceVarying);

		// No need for an index, as the normals there are 8 normals for 8 vertex indices. Although contrary to
		// the ToUsdNormals_FaceVarying_Unordered_Indexed_AsPrimvar test, here some remapping has to happen so that
		// the normals are in order.
		EXPECT_EQ(normalPrimvar.IsIndexed(), false);
	}
	else
	{
		normalsAttribute = usdMesh.GetNormalsAttr();
		EXPECT_EQ(usdMesh.GetNormalsInterpolation(), pxr::UsdGeomTokens->faceVarying);		
	}
	
	// Finally make sure we got the right normals.
	pxr::VtVec3fArray usdNormals;
	normalsAttribute.Get(&usdNormals);
	pxr::GfVec3f nQuad1Usd(nQuad1.x, nQuad1.y, nQuad1.z);
	pxr::GfVec3f nQuad2Usd(nQuad2.x, nQuad2.y, nQuad2.z);

	EXPECT_EQ(usdNormals[0], nQuad1Usd);
	EXPECT_EQ(usdNormals[1], nQuad1Usd);
	EXPECT_EQ(usdNormals[2], nQuad1Usd);
	EXPECT_EQ(usdNormals[3], nQuad1Usd);

	EXPECT_EQ(usdNormals[4], nQuad2Usd);
	EXPECT_EQ(usdNormals[5], nQuad2Usd);
	EXPECT_EQ(usdNormals[6], nQuad2Usd);
	EXPECT_EQ(usdNormals[7], nQuad2Usd);

	pxr::TfToken subdivScheme;
	usdMesh.GetSubdivisionSchemeAttr().Get(&subdivScheme);
	EXPECT_EQ(subdivScheme, pxr::UsdGeomTokens->none);

	// If not ordered, we don't get the exact same mesh back, as we did not use an index
	// and ordered the normals.
	if (ordered)
	{
		MNMesh reconverted;
		std::map<int, std::string> channelNames;
		converter.ConvertToMNMesh(usdMesh, reconverted, MaxUsd::PrimvarMappingOptions{}, channelNames);
		TestUtils::CompareMaxMeshNormals(roof, reconverted);
	}	
	
#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/Normals/FaceVarying_NotIndexed_Ordered_");
	exportPath.append(std::to_string(ordered));
	exportPath.append("_Primvar_");
	exportPath.append(std::to_string(asPrimvar));
	exportPath.append(".usda");
	stage->Export(exportPath);
#endif
}

// Test normal I/O with max normals where normals do not require indexing
// and converted as primvars. Make sure that the ordering does not matter. 
TEST(MeshNormalsTests, FaceVarying_Ordered_NotIndexed_AsPrimvar)
{
	FaceVarying_NotIndexed_Test(true, true);
}

TEST(MeshNormalsTests, FaceVarying_Unordered_NotIndexed_AsPrimvar)
{
	FaceVarying_NotIndexed_Test(true, false);
}

// Test normal I/O with max normals where normals do not require indexing
// and converted as attributes. Make sure that the ordering does not matter. 
TEST(MeshNormalsTests, FaceVarying_Ordered_NotIndexed_AsAttribute)
{
	FaceVarying_NotIndexed_Test(false, true);
}

TEST(MeshNormalsTests, FaceVarying_Unordered_NotIndexed_AsAttribute)
{
	FaceVarying_NotIndexed_Test(false, false);
}

void VertexNormals_Test(bool asPrimvar, bool indexed)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/object");
	pxr::UsdGeomMesh usdMesh;
	MNMesh roof = TestUtils::CreateRoofShape();

	roof.SpecifyNormals();
	auto normals = roof.GetSpecifiedNormals();

	// 2 quads angled to form a roof.
	normals->SetNumFaces(2);
	normals->SetNumNormals(8);
	auto leftSideNormal = Normalize(Point3(-1, 0, 1));
	auto topNormal = Normalize(Point3(0, 0, 1));
	auto rightSideNormal = Normalize(Point3(1, 0, 1));

	if (indexed)
	{
		normals->Normal(0) = leftSideNormal;
		normals->Normal(1) = topNormal;
		normals->Normal(2) = rightSideNormal;

		normals->SetNormalIndex(0, 0, 0);
		normals->SetNormalIndex(0, 1, 1);
		normals->SetNormalIndex(0, 2, 1);
		normals->SetNormalIndex(0, 3, 0);
		normals->SetNormalIndex(1, 0, 1);
		normals->SetNormalIndex(1, 1, 2);
		normals->SetNormalIndex(1, 2, 2);
		normals->SetNormalIndex(1, 3, 1);
	}
	else
	{
		normals->Normal(0) = leftSideNormal;
		normals->Normal(1) = topNormal;
		normals->Normal(2) = topNormal;
		normals->Normal(3) = leftSideNormal;
		normals->Normal(4) = rightSideNormal;
		normals->Normal(5) = rightSideNormal;

		normals->SetNormalIndex(0, 0, 0);
		normals->SetNormalIndex(0, 1, 1);
		normals->SetNormalIndex(0, 2, 2);
		normals->SetNormalIndex(0, 3, 3);
		normals->SetNormalIndex(1, 0, 1);
		normals->SetNormalIndex(1, 1, 4);
		normals->SetNormalIndex(1, 2, 5);
		normals->SetNormalIndex(1, 3, 2);
	}
	
	normals->SetAllExplicit();
	normals->CheckNormals();
	roof.InvalidateGeomCache();

	MaxUsd::MeshConverter converter;
	MaxUsd::MaxMeshConversionOptions options;

	options.SetNormalsMode(asPrimvar ? MaxUsd::MaxMeshConversionOptions::NormalsMode::AsPrimvar
									 : MaxUsd::MaxMeshConversionOptions::NormalsMode::AsAttribute);

	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{&roof}, stage, path, options, usdMesh, pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);

	pxr::VtVec3fArray usdNormals;
	pxr::UsdGeomPrimvar normalPrimvar;
	if (asPrimvar)
	{
		normalPrimvar = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar( pxr::UsdImagingTokens->primvarsNormals);

		EXPECT_EQ(normalPrimvar.GetInterpolation(), pxr::UsdGeomTokens->vertex);
		EXPECT_EQ(normalPrimvar.IsIndexed(), indexed);
		normalPrimvar.Get(&usdNormals);
	}
	else 
	{
		EXPECT_EQ(usdMesh.GetNormalsInterpolation(), pxr::UsdGeomTokens->vertex);
		usdMesh.GetNormalsAttr().Get(&usdNormals);
	}
	
	const auto leftSideNormalUsd = pxr::GfVec3f(leftSideNormal.x, leftSideNormal.y, leftSideNormal.z);
	const auto topNormalUsd = pxr::GfVec3f(topNormal.x, topNormal.y, topNormal.z);;
	const auto rightSideNormalUsd = pxr::GfVec3f(rightSideNormal.x, rightSideNormal.y, rightSideNormal.z);
	
	if (indexed && asPrimvar)
	{
		EXPECT_EQ(usdNormals[0], leftSideNormalUsd);
		EXPECT_EQ(usdNormals[1], topNormalUsd);
		EXPECT_EQ(usdNormals[2], rightSideNormalUsd);
		pxr::VtIntArray normalIndices;
		normalPrimvar.GetIndices(&normalIndices);
		EXPECT_EQ(normalIndices.size(), 6);
	}
	else {
		// Finally, make sure we got the right normals.
		EXPECT_EQ(usdNormals.size(), 6);

		EXPECT_EQ(usdNormals[0], leftSideNormalUsd);
		EXPECT_EQ(usdNormals[1], topNormalUsd);
		EXPECT_EQ(usdNormals[2], topNormalUsd);
		EXPECT_EQ(usdNormals[3], leftSideNormalUsd);
		EXPECT_EQ(usdNormals[4], rightSideNormalUsd);
		EXPECT_EQ(usdNormals[5], rightSideNormalUsd);
	}

	pxr::TfToken subdivScheme;
	usdMesh.GetSubdivisionSchemeAttr().Get(&subdivScheme);
	EXPECT_EQ(subdivScheme, pxr::UsdGeomTokens->none);

	// If as attribute and indexed, we expanded the normals to avoid needing an index.
	// No need to test the round trip, covered by the tests without indexing.
	if (asPrimvar || !indexed)
	{
		// Test conversion back to MAX.
		MNMesh reconvertedMesh;
		std::map<int, std::string> channelNames;
		converter.ConvertToMNMesh(usdMesh, reconvertedMesh, MaxUsd::PrimvarMappingOptions{}, channelNames);
		TestUtils::CompareMaxMeshNormals(roof, reconvertedMesh);
	}	

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/Normals/Vertex_Indexed_");
	exportPath.append(std::to_string(indexed));
	exportPath.append("_Primvar_");
	exportPath.append(std::to_string(asPrimvar));
	exportPath.append(".usda");
	stage->Export(exportPath);
#endif
}

// Test normal I/O where normals should be indexed and converted as primvars
// with vertex interpolation.
TEST(MeshNormalsTests, Vertex_Indexed_AsPrimvar)
{
	VertexNormals_Test(true, true);
}

// Test normal I/O where normals should converted as attributes
// with vertex interpolation.
TEST(MeshNormalsTests, Vertex_Indexed_AsAttribute)
{
	VertexNormals_Test(false, true);
}

// Test normal I/O where normals should converted as primvar
// with vertex interpolation and no indexing.
TEST(MeshNormalsTests, Vertex_NotIndexed_AsPrimvar)
{
	VertexNormals_Test(true, false);
}

// Test normal I/O where normals should converted as attributes
// with vertex interpolation.
TEST(MeshNormalsTests, Vertex_NotIndexed_AsAttribute)
{
	VertexNormals_Test(false, false);
}

void UniformNormals_ToMax_Test(bool asPrimvar) {
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/Uniform_Attribute_ToMax");

	// Build a USD cube with uniform normals.
	pxr::UsdGeomMesh usdMesh;
	MNMesh cube = TestUtils::CreateCube(true);
	MaxUsd::MeshConverter converter;
	MaxUsd::MaxMeshConversionOptions options;
	// We will produce the normals ourselves, as uniform.
	options.SetNormalsMode(MaxUsd::MaxMeshConversionOptions::NormalsMode::None);
	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{&cube}, stage, path, options, usdMesh, pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);
	pxr::VtVec3fArray usdNormals;
	usdNormals.reserve(6);
	std::vector<Point3> normals;
	normals.push_back(Point3(0, 0, -1));
	normals.push_back(Point3(0, 0, 1));
	normals.push_back(Point3(0, -1, 0));
	normals.push_back(Point3(1, 0, 0));
	normals.push_back(Point3(0, 1, 0));
	normals.push_back(Point3(-1, 0, 0));

	for (const auto& n : normals)
	{
		usdNormals.push_back(pxr::GfVec3f(n.x, n.y, n.z));
	}
	pxr::UsdAttribute normalAttr;
	
	if (asPrimvar)
	{
		auto primVarApi = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim());
		auto primvar = primVarApi.CreatePrimvar(pxr::UsdImagingTokens->primvarsNormals, pxr::SdfValueTypeNames->Float3Array);
		normalAttr = primvar.GetAttr();
		primvar.SetInterpolation(pxr::UsdGeomTokens->uniform);
	}
	else
	{
		usdMesh.SetNormalsInterpolation(pxr::UsdGeomTokens->uniform);
		normalAttr = usdMesh.CreateNormalsAttr();
	}
	normalAttr.Set(usdNormals);

	usdMesh.CreateSubdivisionSchemeAttr(pxr::VtValue(pxr::UsdGeomTokens->none));
	
	// Now we can test the import : 
	MNMesh maxMesh;
	std::map<int, std::string> channelNames;
	converter.ConvertToMNMesh(usdMesh, maxMesh, MaxUsd::PrimvarMappingOptions{}, channelNames);

	const auto specNormals = maxMesh.GetSpecifiedNormals();
	auto numNormals = specNormals->GetNumNormals();
	EXPECT_EQ(numNormals, 6);
	for (int i = 0; i < numNormals; i++)
	{
		EXPECT_EQ(specNormals->Normal(i), normals[i]);
	}
	for (int i = 0; i < specNormals->GetNumFaces(); i++)
	{
		for (int j = 0; j < specNormals->Face(i).GetDegree(); j++)
		{
			EXPECT_EQ(specNormals->Face(i).GetNormalID(j), i);
		}
	}
	
#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/Normals/UniformNormal_ToMax_Primvar_");
	exportPath.append(std::to_string(asPrimvar));
	exportPath.append(".usda");
	stage->Export(exportPath);
#endif
}

// Test import of uniform interpolation normals, when defined as primvars.
TEST(MeshNormalsTests, Uniform_ToMax_AsPrimvar)
{
	UniformNormals_ToMax_Test(true);
}

// Test import of uniform interpolation normals, when defined as attributes.
TEST(MeshNormalsTests, Uniform_ToMax_AsAttribute)
{
	UniformNormals_ToMax_Test(false);
}

void VertexNormalsUnusedVertices_Test(bool asPrimvar)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/object");
	pxr::UsdGeomMesh usdMesh;
	MNMesh roof;
	roof.setNumFaces(2);
	roof.setNumVerts(7);
	roof.V(0)->p = Point3(-1.f, -1.f, 0.f);
	roof.V(1)->p = Point3(0.f, -1.f, 1.f);
	roof.V(2)->p = Point3(0.f, 1.f, 1.f);

	// Unused
	roof.V(3)->p = Point3(0.f, 0.f, 0.f);

	roof.V(4)->p = Point3(-1.f, 1.f, 0.f);
	roof.V(5)->p = Point3(1.f, -1.f, 0.f);
	roof.V(6)->p = Point3(1.f, 1.f, 0.f);
	roof.F(0)->SetDeg(4);
	roof.F(0)->vtx[0] = 0;
	roof.F(0)->vtx[1] = 1;
	roof.F(0)->vtx[2] = 2;
	roof.F(0)->vtx[3] = 4;
	roof.F(1)->SetDeg(4);
	roof.F(1)->vtx[0] = 1;
	roof.F(1)->vtx[1] = 5;
	roof.F(1)->vtx[2] = 6;
	roof.F(1)->vtx[3] = 2;
	roof.FillInMesh();
	
	roof.SpecifyNormals();
	auto normals = roof.GetSpecifiedNormals();

	// 2 quads angled to form a roof.
	normals->SetNumFaces(2);
	normals->SetNumNormals(8);
	auto leftSideNormal = Normalize(Point3(-1, 0, 1));
	auto topNormal = Normalize(Point3(0, 0, 1));
	auto rightSideNormal = Normalize(Point3(1, 0, 1));

	normals->Normal(0) = leftSideNormal;
	normals->Normal(1) = topNormal;
	normals->Normal(2) = topNormal;
	normals->Normal(3) = leftSideNormal;
	normals->Normal(4) = rightSideNormal;
	normals->Normal(5) = rightSideNormal;

	normals->SetNormalIndex(0, 0, 0);
	normals->SetNormalIndex(0, 1, 1);
	normals->SetNormalIndex(0, 2, 2);
	normals->SetNormalIndex(0, 3, 3);
	normals->SetNormalIndex(1, 0, 1);
	normals->SetNormalIndex(1, 1, 4);
	normals->SetNormalIndex(1, 2, 5);
	normals->SetNormalIndex(1, 3, 2);
	
	normals->SetAllExplicit();
	normals->CheckNormals();
	roof.InvalidateGeomCache();

	MaxUsd::MeshConverter converter;
	MaxUsd::MaxMeshConversionOptions options;

	options.SetNormalsMode(asPrimvar ? MaxUsd::MaxMeshConversionOptions::NormalsMode::AsPrimvar
									 : MaxUsd::MaxMeshConversionOptions::NormalsMode::AsAttribute);

	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{&roof}, stage, path, options, usdMesh, pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);

	pxr::VtVec3fArray usdNormals;
	pxr::UsdGeomPrimvar normalPrimvar;
	if (asPrimvar)
	{
		normalPrimvar = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar( pxr::UsdImagingTokens->primvarsNormals);

		EXPECT_EQ(normalPrimvar.GetInterpolation(), pxr::UsdGeomTokens->vertex);
		normalPrimvar.Get(&usdNormals);
	}
	else
	{
		EXPECT_EQ(usdMesh.GetNormalsInterpolation(), pxr::UsdGeomTokens->vertex);
		usdMesh.GetNormalsAttr().Get(&usdNormals);
	}

	const auto leftSideNormalUsd = pxr::GfVec3f(leftSideNormal.x, leftSideNormal.y, leftSideNormal.z);
	const auto topNormalUsd = pxr::GfVec3f(topNormal.x, topNormal.y, topNormal.z);;
	const auto rightSideNormalUsd = pxr::GfVec3f(rightSideNormal.x, rightSideNormal.y, rightSideNormal.z);

	// Finally, make sure we got the right normals.
	ASSERT_EQ(usdNormals.size(), 6);

	EXPECT_EQ(usdNormals[0], leftSideNormalUsd);
	EXPECT_EQ(usdNormals[1], topNormalUsd);
	EXPECT_EQ(usdNormals[2], topNormalUsd);
	EXPECT_EQ(usdNormals[3], leftSideNormalUsd);
	EXPECT_EQ(usdNormals[4], rightSideNormalUsd);
	EXPECT_EQ(usdNormals[5], rightSideNormalUsd);

	pxr::TfToken subdivScheme;
	usdMesh.GetSubdivisionSchemeAttr().Get(&subdivScheme);
	EXPECT_EQ(subdivScheme, pxr::UsdGeomTokens->none);

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/Normals/VertexNormals_Unused_Vertices");
	exportPath.append("_Primvar_");
	exportPath.append(std::to_string(asPrimvar));
	exportPath.append(".usda");
	stage->Export(exportPath);
#endif
}

// Test import and export of vertex normals as primvar with unused vertices.
TEST(MeshNormalsTests, Vertex_UnusedVertices_AsPrimvar)
{
	VertexNormalsUnusedVertices_Test(true);
}

// Test import and export of vertex normals as attribute with unused vertices.
TEST(MeshNormalsTests, Vertex_UnusedVertices_AsAttribute)
{
	VertexNormalsUnusedVertices_Test(false);
}

// Test time sampled normals specified as attribute (animated Primvars are already tested in the ChannelBuilder tests).
TEST(MeshNormalsTests, TimeSampledNormals_AsAttribute)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/timeSampledNormalsAttribute");
	auto animatedMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));

	// Create a simple mesh.
	const pxr::VtVec3fArray points = { {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f} , {0.f, 0.f, 1.f} };
	const pxr::VtIntArray faceCounts = { 3 };
	const pxr::VtIntArray indices = { 0,1,2 };
	animatedMesh.CreatePointsAttr().Set(points);
	animatedMesh.CreateFaceVertexCountsAttr().Set(faceCounts);
	animatedMesh.CreateFaceVertexIndicesAttr().Set(indices);

	// Normal attribute interpolation is not animatable.
	animatedMesh.SetNormalsInterpolation(pxr::UsdGeomTokens->vertex);
	const auto normalAttribute = animatedMesh.CreateNormalsAttr();

	// Specify different vertex normals at different timeCodes.
	const pxr::VtVec3fArray normalsDefault = { {1.f, 1.f, 1.f}, {2.f, 2.f, 2.f} , {3.f, 3.f, 4.f} };
	normalAttribute.Set(normalsDefault);
	const pxr::VtVec3fArray normals1 = { {4.f, 4.f, 4.f}, {5.f, 5.f, 5.f} , {6.f, 6.f, 6.f} };
	normalAttribute.Set(normals1, 1);
	const pxr::VtVec3fArray normals2 = { {7.f, 7.f, 7.f}, {8.f, 8.f, 8.f} , {9.f, 9.f, 9.f} };
	normalAttribute.Set(normals2, 2);

	// Test the normal conversion process at different timeCodes to make sure that the specified
	// timeCode is respected.
	auto testAtTimeCode = [&](const pxr::VtVec3fArray& normals, const pxr::UsdTimeCode& timeCode)
	{
		MNMesh maxMesh;
		MaxUsd::MeshConverter converter;
		std::map<int, std::string> channelNames;
		converter.ConvertToMNMesh(
				animatedMesh, maxMesh, MaxUsd::PrimvarMappingOptions{}, channelNames, nullptr, timeCode);
		const auto maxNormals = maxMesh.GetSpecifiedNormals();
		for (int i = 0; i < maxNormals->GetNumNormals(); ++i)
		{
			EXPECT_FLOAT_EQ(normals[i][0], maxNormals->Normal(i).x);
			EXPECT_FLOAT_EQ(normals[i][1], maxNormals->Normal(i).y);
			EXPECT_FLOAT_EQ(normals[i][2], maxNormals->Normal(i).z);
		}
	};
	testAtTimeCode(normalsDefault, pxr::UsdTimeCode::Default());
	testAtTimeCode(normals1, 1);
	testAtTimeCode(normals2, 2);

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/MeshConversionTest/animatedNormalsAttribute.usda");
	stage->Export(exportPath);
#endif
}

TEST(MeshNormalsTests, LeftHandedFaceOrientation)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/object");

	// Export a simple quad to a USD mesh.	
	auto maxQuad = TestUtils::CreateQuad();
	MaxUsd::MeshConverter converter;
	pxr::UsdGeomMesh usdMesh;
	MaxUsd::MaxMeshConversionOptions options;
	options.SetNormalsMode(MaxUsd::MaxMeshConversionOptions::NormalsMode::None);
	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{ &maxQuad }, stage, path, options, usdMesh, pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);
	usdMesh.CreateOrientationAttr().Set(pxr::UsdGeomTokens->leftHanded);

	// 1) Test that normal faces built from vertex-interpolated normals are correctly flipped.

	usdMesh.SetNormalsInterpolation(pxr::UsdGeomTokens->vertex);
	usdMesh.GetNormalsAttr().Set(pxr::VtVec3fArray{ pxr::GfVec3f(0, 1, 2), pxr::GfVec3f(3, 4, 5) , pxr::GfVec3f(6, 7, 8),pxr::GfVec3f(9, 10, 11) });

	MNMesh importedQuadVertex;
	std::map<int, std::string> channelNames1;
	converter.ConvertToMNMesh(usdMesh, importedQuadVertex, MaxUsd::PrimvarMappingOptions{}, channelNames1);
	auto& quadFaceFromVertex = importedQuadVertex.GetSpecifiedNormals()->Face(0);

	// Expect te reverse order after import, always starting at 0.
	EXPECT_EQ(0, quadFaceFromVertex.GetNormalID(0));
	EXPECT_EQ(3, quadFaceFromVertex.GetNormalID(1));
	EXPECT_EQ(2, quadFaceFromVertex.GetNormalID(2));
	EXPECT_EQ(1, quadFaceFromVertex.GetNormalID(3));

	// 2) Test that normal faces built from faceVarying interpolated normals are correctly flipped.

	usdMesh.SetNormalsInterpolation(pxr::UsdGeomTokens->faceVarying);
	usdMesh.GetNormalsAttr().Set(pxr::VtVec3fArray{ pxr::GfVec3f(0, 1, 2), pxr::GfVec3f(3, 4, 5) , pxr::GfVec3f(6, 7, 8),pxr::GfVec3f(9, 10, 11) });

	MNMesh importedQuadFaceVaring;
	std::map<int, std::string> channelNames2;
	converter.ConvertToMNMesh(usdMesh, importedQuadFaceVaring, MaxUsd::PrimvarMappingOptions{}, channelNames2);
	auto& quadFaceFromFaceVarying = importedQuadFaceVaring.GetSpecifiedNormals()->Face(0);

	// Expect the reverse order after import, always starting at 0.
	EXPECT_EQ(0, quadFaceFromFaceVarying.GetNormalID(0));
	EXPECT_EQ(3, quadFaceFromFaceVarying.GetNormalID(1));
	EXPECT_EQ(2, quadFaceFromFaceVarying.GetNormalID(2));
	EXPECT_EQ(1, quadFaceFromFaceVarying.GetNormalID(3));

	// 3) Test that normal faces built from uniform interpolated primvars are not affected (all the points on a face
	// share the same data, nothing to flip).

	usdMesh.SetNormalsInterpolation(pxr::UsdGeomTokens->uniform);
	usdMesh.GetNormalsAttr().Set(pxr::VtVec3fArray{ pxr::GfVec3f(0, 1, 2)});

	MNMesh importedQuadUniform;
	std::map<int, std::string> channelNames3;
	converter.ConvertToMNMesh(usdMesh, importedQuadUniform, MaxUsd::PrimvarMappingOptions{}, channelNames3);
	auto& quadFaceFromUniform = importedQuadUniform.GetSpecifiedNormals()->Face(0);

	EXPECT_EQ(0, quadFaceFromUniform.GetNormalID(0));
	EXPECT_EQ(0, quadFaceFromUniform.GetNormalID(1));
	EXPECT_EQ(0, quadFaceFromUniform.GetNormalID(2));
	EXPECT_EQ(0, quadFaceFromUniform.GetNormalID(3));

	// 4) Test that map faces built from constant interpolated primvars are not affected (all the points on a mesh
	// share the same data, nothing to flip).

	usdMesh.SetNormalsInterpolation(pxr::UsdGeomTokens->constant);
	usdMesh.GetNormalsAttr().Set(pxr::VtVec3fArray{ pxr::GfVec3f(0, 1, 2) });
	
	MNMesh importedQuadConstant;
	std::map<int, std::string> channelNames4;
	converter.ConvertToMNMesh(usdMesh, importedQuadConstant, MaxUsd::PrimvarMappingOptions{}, channelNames4);
	auto& quadFaceFromConstant = importedQuadConstant.GetSpecifiedNormals()->Face(0);

	// Expect te reverse order after import, always starting at 0.
	EXPECT_EQ(0, quadFaceFromConstant.GetNormalID(0));
	EXPECT_EQ(0, quadFaceFromConstant.GetNormalID(1));
	EXPECT_EQ(0, quadFaceFromConstant.GetNormalID(2));
	EXPECT_EQ(0, quadFaceFromConstant.GetNormalID(3));

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/MeshConversionTest/LeftHandedOrientationNormals.usda");
	stage->Export(exportPath);
#endif
}

void ConvertNormalsToUsdTimeCode(bool asAttribute)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/OutputTimeSampledNormals");

	// Sets normals on a quad mesh. The seed is used to generate different normals values.
	auto setNormals = [](MNMesh& quad, float seed) {
		// Setup explicit normals
		quad.SpecifyNormals();
		auto specifiedNormals = quad.GetSpecifiedNormals();

		specifiedNormals->SetParent(&quad);
		specifiedNormals->SetNumFaces(1);
		specifiedNormals->SetNumNormals(4);

		specifiedNormals->Normal(0) = Point3(seed, 0.f, 0.f);
		specifiedNormals->Normal(1) = Point3(seed, seed, 0.f);
		specifiedNormals->Normal(2) = Point3(seed, seed, seed);
		specifiedNormals->Normal(3) = Point3(0.f, seed, seed);

		specifiedNormals->Face(0).SetDegree(4);
		specifiedNormals->Face(0).SpecifyAll();
		for (int i = 0; i < 4; i++)
		{
			specifiedNormals->SetNormalIndex(0, i, i);
		}
		specifiedNormals->SetAllExplicit();
		specifiedNormals->CheckNormals();
		quad.InvalidateGeomCache();
	};

	// Build a few quads with different vertex normals.
	MNMesh quad1 = TestUtils::CreateQuad();
	setNormals(quad1, 1.0f);
	MNMesh quad2 = TestUtils::CreateQuad();
	setNormals(quad2, 2.0f);
	MNMesh quad3 = TestUtils::CreateQuad();
	setNormals(quad3, 3.0f);

	// Export the quads to different USD timecodes.
	MaxUsd::MeshConverter converter;
	pxr::UsdGeomMesh usdMesh;
	MaxUsd::MaxMeshConversionOptions options;
	std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
	options.SetNormalsMode(asAttribute ? MaxUsd::MaxMeshConversionOptions::NormalsMode::AsAttribute
									   : MaxUsd::MaxMeshConversionOptions::NormalsMode::AsPrimvar);
	MaxUsd::MeshConverter::ObjectChannelIntervals intervals;
	// Default
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{ &quad1 }, stage, path, options, usdMesh, pxr::UsdTimeCode::Default(), materialIdToFacesMap, false, intervals);
	// TimeCode 1
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{ &quad2 }, stage, path, options, usdMesh, pxr::UsdTimeCode(1), materialIdToFacesMap, false, intervals);
	// TimeCode 2
	converter.ConvertToUSDMesh(MaxUsd::MeshFacade{ &quad3 }, stage, path, options, usdMesh, pxr::UsdTimeCode(2), materialIdToFacesMap, false, intervals);

	// Now validate that the normals were correctly exported at each timecode.
	pxr::UsdAttribute normalsAttribute = asAttribute
			? usdMesh.GetNormalsAttr()
			: pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar( pxr::UsdImagingTokens->primvarsNormals).GetAttr();
	
	pxr::VtVec3fArray normals1;
	normalsAttribute.Get(&normals1, pxr::UsdTimeCode::Default());
	const pxr::VtVec3fArray expectedNormals1 = { { 1.f, 0.f, 0.f }, { 1.f, 1.f, 0.f }, { 1.f, 1.f, 1.f },
		{ 0.f, 1.f, 1.f } };
	EXPECT_EQ(expectedNormals1, normals1);

	pxr::VtVec3fArray normals2;
	normalsAttribute.Get(&normals2, pxr::UsdTimeCode(1));
	const pxr::VtVec3fArray expectedNormals2 = { { 2.f, 0.f, 0.f }, { 2.f, 2.f, 0.f }, { 2.f, 2.f, 2.f },
		{ 0.f, 2.f, 2.f } };
	EXPECT_EQ(expectedNormals2, normals2);

	pxr::VtVec3fArray normals3;
	normalsAttribute.Get(&normals3, pxr::UsdTimeCode(2));
	const pxr::VtVec3fArray expectedNormals3 = { { 3.f, 0.f, 0.f }, { 3.f, 3.f, 0.f }, { 3.f, 3.f, 3.f },
		{ 0.f, 3.f, 3.f } };
	EXPECT_EQ(expectedNormals3, normals3);

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/MeshConversionTest/Output_TimeSampledNormals_as_attr_");
	exportPath.append(std::to_string(asAttribute));
	exportPath.append(".usda");
	stage->Export(exportPath);
#endif
}

TEST(MeshNormalsTests, ConvertNormalsAsAttributeToUsdTimeCode_AsAttribute)
{
	ConvertNormalsToUsdTimeCode(true);	
}

TEST(MeshNormalsTests, ConvertNormalsAsAttributeToUsdTimeCode_AsPrimvar)
{
	ConvertNormalsToUsdTimeCode(false);
}
