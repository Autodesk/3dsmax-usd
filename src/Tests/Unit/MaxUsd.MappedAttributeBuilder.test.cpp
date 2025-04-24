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
#include <MaxUsd/MappedAttributeBuilder.h>
#include <MaxUsd/MeshConversion/MeshConverter.h>

#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

#include <MaxUsd/Utilities/TypeUtils.h>

using namespace MaxUsd;

TEST(MappedAttributeBuilderTests, GetValueTypeName)
{
	EXPECT_EQ(MappedAttributeBuilder::GetValueTypeName(MappedAttributeBuilder::Type::TexCoord2fArray),
			pxr::SdfValueTypeNames->TexCoord2fArray);
	EXPECT_EQ(MappedAttributeBuilder::GetValueTypeName(MappedAttributeBuilder::Type::TexCoord3fArray),
			pxr::SdfValueTypeNames->TexCoord3fArray);
	EXPECT_EQ(MappedAttributeBuilder::GetValueTypeName(MappedAttributeBuilder::Type::FloatArray),
			pxr::SdfValueTypeNames->FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetValueTypeName(MappedAttributeBuilder::Type::Float2Array),
			pxr::SdfValueTypeNames->Float2Array);
	EXPECT_EQ(MappedAttributeBuilder::GetValueTypeName(MappedAttributeBuilder::Type::Float3Array),
			pxr::SdfValueTypeNames->Float3Array);
	EXPECT_EQ(MappedAttributeBuilder::GetValueTypeName(MappedAttributeBuilder::Type::Color3fArray),
			pxr::SdfValueTypeNames->Color3fArray);
	EXPECT_EQ(MappedAttributeBuilder::GetValueTypeName(MappedAttributeBuilder::Type(-1)),
			pxr::SdfValueTypeNames->Float3Array);
}

TEST(MappedAttributeBuilderTests, GetTypeDimension)
{
	EXPECT_EQ(MappedAttributeBuilder::GetTypeDimension(MappedAttributeBuilder::Type::TexCoord2fArray), 2);
	EXPECT_EQ(MappedAttributeBuilder::GetTypeDimension(MappedAttributeBuilder::Type::TexCoord3fArray), 3);
	EXPECT_EQ(MappedAttributeBuilder::GetTypeDimension(MappedAttributeBuilder::Type::FloatArray), 1);
	EXPECT_EQ(MappedAttributeBuilder::GetTypeDimension(MappedAttributeBuilder::Type::Float2Array), 2);
	EXPECT_EQ(MappedAttributeBuilder::GetTypeDimension(MappedAttributeBuilder::Type::Float3Array), 3);
	EXPECT_EQ(MappedAttributeBuilder::GetTypeDimension(MappedAttributeBuilder::Type::Color3fArray), 3);
	EXPECT_EQ(MappedAttributeBuilder::GetTypeDimension(MappedAttributeBuilder::Type(-1)), 3);
}

TEST(MappedAttributeBuilderTests, GetHigherDimensionType)
{
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::FloatArray, -1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::FloatArray, 1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::FloatArray, 2),
			MappedAttributeBuilder::Type::Float2Array);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::FloatArray, 3),
			MappedAttributeBuilder::Type::Float3Array);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::FloatArray, 4),
			MappedAttributeBuilder::Type::Float3Array);

	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Float2Array, -1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Float2Array, 1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Float2Array, 2),
			MappedAttributeBuilder::Type::Float2Array);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Float2Array, 3),
			MappedAttributeBuilder::Type::Float3Array);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Float2Array, 4),
			MappedAttributeBuilder::Type::Float3Array);

	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Float3Array, -1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Float3Array, 1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Float3Array, 2),
			MappedAttributeBuilder::Type::Float2Array);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Float3Array, 3),
			MappedAttributeBuilder::Type::Float3Array);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Float3Array, 4),
			MappedAttributeBuilder::Type::Float3Array);

	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::TexCoord2fArray, -1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::TexCoord2fArray, 1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::TexCoord2fArray, 2),
			MappedAttributeBuilder::Type::TexCoord2fArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::TexCoord2fArray, 3),
			MappedAttributeBuilder::Type::TexCoord3fArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::TexCoord2fArray, 4),
			MappedAttributeBuilder::Type::TexCoord3fArray);

	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::TexCoord3fArray, -1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::TexCoord3fArray, 1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::TexCoord3fArray, 2),
			MappedAttributeBuilder::Type::TexCoord2fArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::TexCoord3fArray, 3),
			MappedAttributeBuilder::Type::TexCoord3fArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::TexCoord3fArray, 4),
			MappedAttributeBuilder::Type::TexCoord3fArray);

	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Color3fArray, -1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Color3fArray, 1),
			MappedAttributeBuilder::Type::FloatArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Color3fArray, 2),
			MappedAttributeBuilder::Type::Float2Array);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Color3fArray, 3),
			MappedAttributeBuilder::Type::Color3fArray);
	EXPECT_EQ(MappedAttributeBuilder::GetEquivalentType(MappedAttributeBuilder::Type::Color3fArray, 4),
			MappedAttributeBuilder::Type::Color3fArray);
}

TEST(MappedAttributeBuilderTests, ConstantPrimvar)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/cube");
	auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	auto cube = TestUtils::CreateCube(false);

	std::vector<Point3> values;
	values.emplace_back(1.f, 1.f, 1.f);
	// All face-vertex pointing to the same data.
	auto mappedData = std::make_shared<MappedAttributeBuilder::MappedData>(
			values.data(), values.size(), std::make_shared<std::vector<int>>(24,0));

	MappedAttributeBuilder primvarBuilder(MaxUsd::MeshFacade{ &cube }, mappedData);

	MappedAttributeBuilder::Config config{ pxr::TfToken("testPrimvar"), MappedAttributeBuilder::Type::Float3Array };
	primvarBuilder.BuildPrimvar(usdMesh, config, pxr::UsdTimeCode::Default(), false);

	auto primvar = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(config.GetPrimvarName());

	ASSERT_TRUE(primvar.IsDefined());
	EXPECT_EQ(primvar.GetInterpolation(), pxr::UsdGeomTokens->constant);
	EXPECT_EQ(primvar.IsIndexed(), false);
	/// MappedAttributeBuilder::Type::Float3Array will resolve to a pxr::VtVec3fArray
	pxr::VtVec3fArray primValues;
	primvar.Get(&primValues);
	EXPECT_EQ(primValues.size(), 1);
	EXPECT_EQ(primValues[0], pxr::GfVec3f(values[0].x, values[0].y, values[0].z));

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/MappedAttributeBuilder/ConstantPrimvar.usda");
	stage->Export(exportPath);
#endif
}

TEST(MappedAttributeBuilderTests, VertexPrimvar_Indexed)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/cube");
	auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	auto cube = TestUtils::CreateCube(false);

	std::vector<Point3> values(4);
	float nextValue = 0.f;
	std::generate(values.begin(), values.end(), [&nextValue]() { return Point3(nextValue++, 0.f, 0.f); });

	auto faceDataIndices = std::make_shared<std::vector<int>>(24);
	int faceVertex = 0;
	for (int i = 0; i < cube.FNum(); i++)
	{
		const auto face = cube.F(i);
		for (int j = 0; j < face->deg; j++)
		{
			int vertexIndex = face->vtx[j];
			(*faceDataIndices)[faceVertex++] = vertexIndex % 2; // even/odd vertices share the same data
		}
	}

	MappedAttributeBuilder primvarBuilder(MaxUsd::MeshFacade{ &cube },
			std::make_shared<MappedAttributeBuilder::MappedData>(values.data(), values.size(), faceDataIndices));
	MappedAttributeBuilder::Config config{ pxr::TfToken("testPrimvar"), MappedAttributeBuilder::Type::Float3Array };
	primvarBuilder.BuildPrimvar(usdMesh, config, pxr::UsdTimeCode::Default(), false);

	auto primvar = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(pxr::TfToken(config.GetPrimvarName()));

	ASSERT_TRUE(primvar.IsDefined());
	EXPECT_EQ(primvar.GetInterpolation(), pxr::UsdGeomTokens->vertex);
	EXPECT_TRUE(primvar.IsIndexed());
	/// MappedAttributeBuilder::Type::Float3Array will resolve to a pxr::VtVec3fArray
	pxr::VtVec3fArray primvarValues;
	primvar.Get(&primvarValues);
	EXPECT_EQ(primvarValues.size(), 4);

	for (int i = 0; i < primvarValues.size(); i++)
	{
		EXPECT_EQ(primvarValues[i], pxr::GfVec3f(values[i].x, values[i].y, values[i].z));
	}

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/MappedAttributeBuilder/VertexPrimvar_Indexed.usda");
	stage->Export(exportPath);
#endif
}

void VertexPrimvar_Test(bool ordered)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/cube");
	auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	auto cube = TestUtils::CreateCube(false);

	std::vector<Point3> values(8);
	float nextValue = 0.f;
	// All values are different. One piece of data per vertex, we can avoid an index by using the same order as vertices (as if the
	// indexing was [0,1,2,3,4,5,7].
	std::generate(values.begin(), values.end(), [&nextValue]() { return Point3(nextValue++, 0.f, 0.f); });

	auto faceDataIndices = std::make_shared<std::vector<int>>(24);

	std::vector<Point3> expectedDataArray;
	expectedDataArray.reserve(8);

	// Test simple case, data is already in the same order as the vertices (i.e. vertex indices == data indices)
	if (ordered)
	{
		expectedDataArray = values;
		int faceVertex = 0;
		for (int i = 0; i < cube.FNum(); i++)
		{
			const auto face = cube.F(i);
			for (int j = 0; j < face->deg; j++)
			{
				int vertexIndex = face->vtx[j];
				(*faceDataIndices)[faceVertex++] = vertexIndex;
			}
		}
	}
	// Data is ordered differently, but the count is the same so it can just be reordered to avoid the need for an
	// index.
	else
	{
		std::map<int, int> vertexIndexToDataIndex;
		int faceVertex = 0;
		int nextDataIndex = 0;
		for (int i = 0; i < cube.FNum(); i++)
		{
			const auto face = cube.F(i);
			for (int j = 0; j < face->deg; j++)
			{
				int vertexIndex = face->vtx[j];
				int dataIndex;
				// Make sure we use the same data index for the same vertex.
				const auto it = vertexIndexToDataIndex.find(vertexIndex);
				if (it != vertexIndexToDataIndex.end())
				{
					dataIndex = it->second;
				}
				else
				{
					dataIndex = nextDataIndex++;
					vertexIndexToDataIndex[vertexIndex] = dataIndex;
				}
				(*faceDataIndices)[faceVertex++] = dataIndex;
			}
		}
		// The ordered map's keys are the vertex indices, so this will be the order outputted to
		// the primvar to match the vertices.
		for (auto const& entry : vertexIndexToDataIndex)
		{
			expectedDataArray.push_back(values[entry.second]);
		}
	}

	MappedAttributeBuilder primvarBuilder(
			MaxUsd::MeshFacade{ &cube }, std::make_shared<MappedAttributeBuilder::MappedData>(values.data(), values.size(), faceDataIndices));
	MappedAttributeBuilder::Config config{ pxr::TfToken("testPrimvar"), MappedAttributeBuilder::Type::Float3Array };
	primvarBuilder.BuildPrimvar(usdMesh, config, pxr::UsdTimeCode::Default(), false);
	auto primvar = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(config.GetPrimvarName());

	ASSERT_TRUE(primvar.IsDefined());
	EXPECT_EQ(primvar.GetInterpolation(), pxr::UsdGeomTokens->vertex);
	// No indexing used!
	EXPECT_FALSE(primvar.IsIndexed());
	/// MappedAttributeBuilder::Type::Float3Array will resolve to a pxr::VtVec3fArray
	pxr::VtVec3fArray primvarValues;
	primvar.Get(&primvarValues);
	EXPECT_EQ(primvarValues.size(), 8);

	for (int i = 0; i < primvarValues.size(); i++)
	{
		EXPECT_EQ(
				primvarValues[i], pxr::GfVec3f(expectedDataArray[i].x, expectedDataArray[i].y, expectedDataArray[i].z));
	}

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/MappedAttributeBuilder/VertexPrimvar_Indexed_ordered_");
	exportPath.append(std::to_string(ordered));
	exportPath.append(".usda");
	stage->Export(exportPath);
#endif
}

TEST(MappedAttributeBuilderTests, VertexPrimvar_Ordered)
{
	VertexPrimvar_Test(true);
}

TEST(MappedAttributeBuilderTests, VertexPrimvar_Unordered)
{
	VertexPrimvar_Test(false);
}

TEST(MappedAttributeBuilderTests, FaceVaryingPrimvarIndexed)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/cube");
	auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	auto cube = TestUtils::CreateCube(false);

	std::vector<Point3> values;
	values.emplace_back(1.f, 1.f, 1.f);
	values.emplace_back(2.f, 2.f, 2.f);
	// The first face-vertex uses values[0], all others values[1]. This means one of the corners of the cube will
	// not have all its face-vertices pointing to the same data.
	auto faceDataIndices = std::make_shared<std::vector<int>>(24, 1);
	(*faceDataIndices)[0] = 0;

	MappedAttributeBuilder primvarBuilder(
			MaxUsd::MeshFacade{ &cube }, std::make_shared<MappedAttributeBuilder::MappedData>(values.data(), values.size(), faceDataIndices));

	MappedAttributeBuilder::Config config{ pxr::TfToken("testPrimvar"), MappedAttributeBuilder::Type::Float3Array };
	primvarBuilder.BuildPrimvar(usdMesh, config, pxr::UsdTimeCode::Default(), false);

	auto primvar = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(pxr::TfToken(config.GetPrimvarName()));

	ASSERT_TRUE(primvar.IsDefined());
	EXPECT_EQ(primvar.GetInterpolation(), pxr::UsdGeomTokens->faceVarying);
	EXPECT_EQ(primvar.IsIndexed(), true);
	/// MappedAttributeBuilder::Type::Float3Array will resolve to a pxr::VtVec3fArray
	pxr::VtVec3fArray primvarValues;
	primvar.Get(&primvarValues);
	EXPECT_EQ(primvarValues.size(), 2);
	EXPECT_EQ(primvarValues[0], pxr::GfVec3f(values[0].x, values[0].y, values[0].z));
	EXPECT_EQ(primvarValues[1], pxr::GfVec3f(values[1].x, values[1].y, values[1].z));

	pxr::VtIntArray primvarIndices;
	primvar.GetIndices(&primvarIndices);
	EXPECT_EQ(primvarIndices[0], 0);
	EXPECT_TRUE(std::all_of(primvarIndices.begin() + 1, primvarIndices.end(), [](int idx) { return idx == 1; }));


#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/MappedAttributeBuilder/FaceVaryingPrimvarIndexed.usda");
	stage->Export(exportPath);
#endif
}

void FaceVaryingPrimvar_Test(bool ordered)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/cube");
	auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	auto cube = TestUtils::CreateCube(false);

	std::vector<Point3> values(24);
	float nextValue = 0.f;
	std::generate(values.begin(), values.end(), [&nextValue]() { return Point3(nextValue++, 0.f, 0.f); });

	auto faceDataIndices = std::make_shared<std::vector<int>>(24);
	// As long as we have 24 values, i.e. one piece of data for each face-vertex on the cube, we should
	// not need indexing. We just need to make sure the data is reordered if the order on data indices doesn't
	// match the order in face indices.
	// If testing unordered, go in reverse order.
	int faceVertex = ordered ? 0 : int(faceDataIndices->size()) - 1;
	int increment = ordered ? 1 : -1;
	for (int i = 0; i < cube.FNum(); i++)
	{
		const auto face = cube.F(i);
		for (int j = 0; j < face->deg; j++)
		{
			(*faceDataIndices)[faceVertex] = faceVertex;
			faceVertex += increment;
		}
	}

	MappedAttributeBuilder primvarBuilder(
			MaxUsd::MeshFacade{ &cube }, std::make_shared<MappedAttributeBuilder::MappedData>(values.data(), values.size(), faceDataIndices));

	MappedAttributeBuilder::Config config{ pxr::TfToken("testPrimvar"), MappedAttributeBuilder::Type::Float3Array };
	primvarBuilder.BuildPrimvar(usdMesh, config, pxr::UsdTimeCode::Default(), false);

	auto primvar = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(config.GetPrimvarName());

	ASSERT_TRUE(primvar.IsDefined());
	EXPECT_EQ(primvar.GetInterpolation(), pxr::UsdGeomTokens->faceVarying);
	EXPECT_EQ(primvar.IsIndexed(), false);

	pxr::VtVec3fArray primvarValues;
	primvar.Get(&primvarValues);
	EXPECT_EQ(primvarValues.size(), 24);

	for (int i = ordered ? 0 : int(faceDataIndices->size()) - 1; i < primvarValues.size(); i += increment)
	{
		EXPECT_EQ(primvarValues[i], pxr::GfVec3f(values[i].x, values[i].y, values[i].z));
	}

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/MappedAttributeBuilder/FaceVaryingPrimvar_Ordered_");
	exportPath.append(std::to_string(ordered));
	exportPath.append(".usda");
	stage->Export(exportPath);
#endif
}

TEST(MappedAttributeBuilderTests, FaceVaryingPrimvar_Ordered)
{
	FaceVaryingPrimvar_Test(true);
}

TEST(MappedAttributeBuilderTests, FaceVaryingPrimvar_Unordered)
{
	FaceVaryingPrimvar_Test(false);
}

// Test that the dimension of the selected primvar types is respected on export.
TEST(MappedAttributeBuilderTests, PrimvarExportDimensions)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/cube");
	auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	auto cube = TestUtils::CreateCube(false);

	std::vector<Point3> values;
	values.push_back(Point3(1.f, 2.f, 3.f));
	MappedAttributeBuilder primvarBuilder(MaxUsd::MeshFacade{ &cube },
			std::make_shared<MappedAttributeBuilder::MappedData>(
					values.data(), values.size(), std::make_shared<std::vector<int>>(24, 0)));

	{
		MappedAttributeBuilder::Config config{ pxr::TfToken("float1"), MappedAttributeBuilder::Type::FloatArray, false };
		primvarBuilder.BuildPrimvar(usdMesh, config, pxr::UsdTimeCode::Default(), false);
		const auto primvar1 = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(pxr::TfToken(config.GetPrimvarName()));
		EXPECT_EQ(pxr::SdfValueTypeNames->FloatArray, primvar1.GetTypeName());
		// Upon setting the primvar values, if the type was not right, nothing will be set.
		pxr::VtFloatArray unidimensionalValues;
		primvar1.Get(&unidimensionalValues);
		EXPECT_FALSE(unidimensionalValues.empty());
	}

	{
		MappedAttributeBuilder::Config config{ pxr::TfToken("float2"), MappedAttributeBuilder::Type::Float2Array, false };
		primvarBuilder.BuildPrimvar(usdMesh, config, pxr::UsdTimeCode::Default(), false);
		const auto primvar2 = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(pxr::TfToken(config.GetPrimvarName()));
		EXPECT_EQ(pxr::SdfValueTypeNames->Float2Array, primvar2.GetTypeName());
		// Upon setting the primvar values, if the type was not right, nothing will be set.
		pxr::VtVec2fArray bidimensionalValues;
		primvar2.Get(&bidimensionalValues);
		EXPECT_FALSE(bidimensionalValues.empty());
	}

	{
		MappedAttributeBuilder::Config config{ pxr::TfToken("float3"), MappedAttributeBuilder::Type::Float3Array, false };
		primvarBuilder.BuildPrimvar(usdMesh, config, pxr::UsdTimeCode::Default(), false);
		const auto primvar3 = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(pxr::TfToken(config.GetPrimvarName()));
		EXPECT_EQ(pxr::SdfValueTypeNames->Float3Array, primvar3.GetTypeName());
		// Upon setting the primvar values, if the type was not right, nothing will be set.
		pxr::VtVec3fArray tridimensionalValues;
		primvar3.Get(&tridimensionalValues);
		EXPECT_FALSE(tridimensionalValues.empty());
	}
#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/MappedAttributeBuilder/PrimvarExportDimensions.usda");
	stage->Export(exportPath);
#endif
}

TEST(MappedAttributeBuilderTests, BuildMappedAttributeAtTimeCode)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/BuildMappedAttributeAtTimeCode");

	auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	auto quad = TestUtils::CreateQuad();

	MappedAttributeBuilder::Config config{ pxr::TfToken("testPrimvar"), MappedAttributeBuilder::Type::Float3Array };
	
	// Sets up the primvar indices and values on the usdMesh at a specific timecode, using a primvar builder..
	auto setupPrimvarAtTimeCode = [&config, &quad, &usdMesh](const std::vector<int>& indices, const std::vector<Point3>& values,
								  const pxr::UsdTimeCode& timeCode) {
		const MappedAttributeBuilder primvarBuilder(MaxUsd::MeshFacade{ &quad },
				std::make_shared<MappedAttributeBuilder::MappedData>(
						values.data(), values.size(), std::make_shared<std::vector<int>>(indices)));
		primvarBuilder.BuildPrimvar(usdMesh, config, timeCode, false);
	};

	// Make sure to use different indices/values at each timecode, to be certain we are not validating against
	// interpolated values later.
	setupPrimvarAtTimeCode(std::vector<int>{ 0, 1, 0, 1 }, std::vector<Point3>{ Point3(0.f, 0.f, 0.f), Point3(1.f, 1.f, 1.f) }, pxr::UsdTimeCode::Default());
	setupPrimvarAtTimeCode(std::vector<int>{ 1, 1, 0, 0 }, std::vector<Point3>{ Point3(2.f, 2.f, 2.f), Point3(3.f, 3.f, 3.f) }, pxr::UsdTimeCode(1));
	setupPrimvarAtTimeCode(std::vector<int>{ 0, 0, 1, 1 }, std::vector<Point3>{ Point3(4.f, 4.f, 4.f), Point3(5.f, 5.f, 5.f) }, pxr::UsdTimeCode(2));

	// Validate that the expected primvar values and indices are found at each timecode...
	auto primvar = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(pxr::TfToken(config.GetPrimvarName()));

	// Default
	pxr::VtVec3fArray primvarValuesAtDefault;
	primvar.Get(&primvarValuesAtDefault, pxr::UsdTimeCode::Default());
	pxr::VtIntArray primvarIndicesAtDefault;
	primvar.GetIndices(&primvarIndicesAtDefault, pxr::UsdTimeCode::Default());
	const auto expectedValuesAtDefault = pxr::VtVec3fArray{ {0.f, 0.f, 0.f }, { 1.f, 1.f, 1.f } };
	EXPECT_EQ(primvarValuesAtDefault, expectedValuesAtDefault);
	const auto expectedIndicesAtDefault = pxr::VtIntArray{ 0, 1, 0, 1 };
	EXPECT_EQ(primvarIndicesAtDefault, expectedIndicesAtDefault);

	// Timecode 1
	pxr::VtVec3fArray primvarValuesAt1;
	primvar.Get(&primvarValuesAt1, pxr::UsdTimeCode(1));
	pxr::VtIntArray primvarIndicesAt1;
	primvar.GetIndices(&primvarIndicesAt1, pxr::UsdTimeCode(1));
	const auto expectedValuesAt1 = pxr::VtVec3fArray{ { 2.f, 2.f, 2.f }, { 3.f, 3.f, 3.f } };
	EXPECT_EQ(primvarValuesAt1, expectedValuesAt1);
	const auto expectedIndicesAt1 = pxr::VtIntArray{ 1, 1, 0, 0 };
	EXPECT_EQ(primvarIndicesAt1, expectedIndicesAt1);

	// Timecode 2
	pxr::VtVec3fArray primvarValuesAt2;
	primvar.Get(&primvarValuesAt2, pxr::UsdTimeCode(2));
	pxr::VtIntArray primvarIndicesAt2;
	primvar.GetIndices(&primvarIndicesAt2, pxr::UsdTimeCode(2));
	const auto expectedValuesAt2 = pxr::VtVec3fArray{ { 4.f, 4.f, 4.f }, { 5.f, 5.f, 5.f } };
	EXPECT_EQ(primvarValuesAt2, expectedValuesAt2);
	const auto expectedIndicesAt2 = pxr::VtIntArray{ 0, 0, 1, 1 };
	EXPECT_EQ(primvarIndicesAt2, expectedIndicesAt2);

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/MappedAttributeBuilder/BuildMappedAttributeAtTimeCode.usda");
	stage->Export(exportPath);
#endif
}

TEST(MappedAttributeBuilderTests, VertexPrimvar_UnusedVertices)
{
	const auto stage = pxr::UsdStage::CreateInMemory();
	const auto path = pxr::SdfPath("/VertexPrimvarUnusedVertices");

	auto usdMesh = pxr::UsdGeomMesh(stage->DefinePrim(path, pxr::TfToken("Mesh")));
	
	MNMesh meshWithUnusedVerts;
	meshWithUnusedVerts.setNumFaces(1);
	meshWithUnusedVerts.setNumVerts(11);
	// Bunch of unused vertices, purposefully add some at the beginning, middle and end.
	meshWithUnusedVerts.V(0)->p = Point3(99.f, 99.f, 99.f); // Unused
	meshWithUnusedVerts.V(1)->p = Point3(99.f, 99.f, 99.f); // Unused
	meshWithUnusedVerts.V(2)->p = Point3(-1.f, -1.f, 0.f);
	meshWithUnusedVerts.V(3)->p = Point3(1.f, -1.f, 00.f);
	meshWithUnusedVerts.V(4)->p = Point3(99.f, 99.f, 99.f); // Unused
	meshWithUnusedVerts.V(5)->p = Point3(99.f, 99.f, 99.f); // Unused
	meshWithUnusedVerts.V(6)->p = Point3(1.f, 1.f, 0.f);
	meshWithUnusedVerts.V(7)->p = Point3(99.f, 99.f, 99.f); // Unused
	meshWithUnusedVerts.V(8)->p = Point3(-1.f, 1.f, 0.f);
	meshWithUnusedVerts.V(9)->p = Point3(99.f, 99.f, 99.f); // Unused
	meshWithUnusedVerts.V(10)->p = Point3(-99.f, -99.f, -99.f); // Unused
	meshWithUnusedVerts.F(0)->SetDeg(4);
	meshWithUnusedVerts.F(0)->vtx[0] = 2;
	meshWithUnusedVerts.F(0)->vtx[1] = 3;
	meshWithUnusedVerts.F(0)->vtx[2] = 6;
	meshWithUnusedVerts.F(0)->vtx[3] = 8;
	meshWithUnusedVerts.FillInMesh();

	MappedAttributeBuilder::Config config{ pxr::TfToken("testPrimvar"), MappedAttributeBuilder::Type::Float3Array };
	
	auto mappedValues = std::vector<Point3>{ Point3(0.f, 0.f, 0.f), Point3(1.f, 1.f, 1.f) };
	const MappedAttributeBuilder primvarBuilder(MaxUsd::MeshFacade{ &meshWithUnusedVerts },
		std::make_shared<MappedAttributeBuilder::MappedData>(mappedValues.data(), 2,
			std::make_shared<std::vector<int>>(std::vector<int>{ 0, 1, 0, 1})));
	primvarBuilder.BuildPrimvar(usdMesh, config, pxr::UsdTimeCode::Default(), false);

	auto primvar = pxr::UsdGeomPrimvarsAPI(usdMesh.GetPrim()).GetPrimvar(pxr::TfToken(config.GetPrimvarName()));

	pxr::VtVec3fArray primvarValuesAtDefault;
	primvar.Get(&primvarValuesAtDefault, pxr::UsdTimeCode::Default());
	pxr::VtIntArray primvarIndicesAtDefault;
	primvar.GetIndices(&primvarIndicesAtDefault, pxr::UsdTimeCode::Default());
	const auto expectedValuesAtDefault = pxr::VtVec3fArray{ {0.f, 0.f, 0.f }, { 1.f, 1.f, 1.f } };
	EXPECT_EQ(primvarValuesAtDefault, expectedValuesAtDefault);
	const auto expectedIndicesAtDefault = pxr::VtIntArray{ 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0 };
	EXPECT_EQ(primvarIndicesAtDefault, expectedIndicesAtDefault);

#ifdef TEST_OUTPUT_USD_FILES
	std::string exportPath = TestUtils::GetOutputDirectory();
	exportPath.append("/MappedAttributeBuilder/VertexPrimvarUnusedVertices.usda");
	stage->Export(exportPath);
#endif
}