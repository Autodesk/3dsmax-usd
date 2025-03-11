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

#include "TestUtils.h"

// MAXX-63363: VS2019 v142: <experimental/filesystem> is deprecated and superseded by the C++17 <filesystem> header
#if _MSVC_LANG > 201402L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif
#include <numeric>

#include <mnmesh.h>
#include <MNNormalSpec.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

namespace TestUtils {


void CompareVertices(const MNMesh& maxMesh, const pxr::UsdGeomMesh& usdMesh)
{
	pxr::VtVec3fArray vertices;
	usdMesh.GetPointsAttr().Get(&vertices);
	ASSERT_EQ(vertices.size(), maxMesh.VNum());
	for (int i = 0; i < maxMesh.VNum(); i++)
	{
		EXPECT_EQ(vertices[i][0], maxMesh.V(i)->p.x) << "Mismatch for X dimension on vertex #" << i;
		EXPECT_EQ(vertices[i][1], maxMesh.V(i)->p.y) << "Mismatch for Y dimension on vertex #" << i;
		EXPECT_EQ(vertices[i][2], maxMesh.V(i)->p.z) << "Mismatch for Z dimension on vertex #" << i;
	}
}

void CompareFaceVertexCount(const MNMesh& maxMesh, const pxr::UsdGeomMesh& usdMesh)
{
	pxr::VtIntArray faceVertexCount;
	usdMesh.GetFaceVertexCountsAttr().Get(&faceVertexCount);
	ASSERT_EQ(faceVertexCount.size(), maxMesh.FNum());
	for (int i = 0; i < maxMesh.FNum(); i++)
	{
		EXPECT_EQ(faceVertexCount[i], maxMesh.F(i)->deg) << "Face vertex count mismatch on face #" << i;
	}
}

void CompareFaceVertices(const MNMesh& maxMesh, const pxr::UsdGeomMesh& usdMesh)
{
	pxr::VtIntArray faceVertices;
	usdMesh.GetFaceVertexIndicesAttr().Get(&faceVertices);
	const auto indexCount = std::accumulate<MNFace*, int, int(int, MNFace&)>(
			maxMesh.f, maxMesh.f + maxMesh.FNum(), 0, [](int total, MNFace& face) { return total + face.deg; });
	ASSERT_EQ(faceVertices.size(), indexCount);

	std::vector<int> indices;

	for (int i = 0; i < maxMesh.FNum(); i++)
	{
		for (int j = 0; j < maxMesh.F(i)->deg; j++)
		{
			EXPECT_EQ(faceVertices[i * 4 + j], maxMesh.F(i)->vtx[j])
					<< "Face vertex index mismatch on face #" << i << " vertex # " << j;
			indices.push_back(maxMesh.F(i)->vtx[j]);
		}
	}
}

void CompareUsdAndMaxMeshes(const MNMesh& maxMesh, const pxr::UsdGeomMesh& usdMesh)
{
	CompareFaceVertexCount(maxMesh, usdMesh);
	CompareVertices(maxMesh, usdMesh);
	CompareFaceVertices(maxMesh, usdMesh);
}

void CompareMaxMeshNormals(MNMesh& maxMesh1, MNMesh& maxMesh2)
{
	auto specNormals1 = maxMesh1.GetSpecifiedNormals();
	auto specNormals2 = maxMesh2.GetSpecifiedNormals();

	ASSERT_EQ(specNormals1 == nullptr, specNormals2 == nullptr);

	// Compare normals
	auto numNormal1 = specNormals1->GetNumNormals();
	auto numNormal2 = specNormals2->GetNumNormals();
	ASSERT_EQ(numNormal1, numNormal2);
	for (int i = 0; i < numNormal1; ++i)
	{
		EXPECT_EQ(specNormals1->Normal(i), specNormals2->Normal(i));
	}

	// Compare normals face indices.
	auto numFace1 = specNormals1->GetNumFaces();
	auto numFace2 = specNormals2->GetNumFaces();
	ASSERT_EQ(numFace1, numFace2);

	for (int i = 0; i < numFace1; i++)
	{
		auto deg1 = specNormals1->Face(i).GetDegree();
		auto deg2 = specNormals2->Face(i).GetDegree();

		ASSERT_EQ(deg1, deg2);
		for (int j = 0; j < specNormals1->Face(i).GetDegree(); j++)
		{
			EXPECT_EQ(specNormals1->Face(i).GetNormalID(j), specNormals2->Face(i).GetNormalID(j));
		}
	}
}

void CompareUSDMatrices(const pxr::GfMatrix4d& matrix1, const pxr::GfMatrix4d& matrix2)
{
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			EXPECT_EQ(matrix1[i][j], matrix2[i][j]);
		}
	}
}

MNMesh CreatePlane(int rows, int cols)
{
	if (rows <= 0 || cols <= 0)
	{
		return MNMesh();
	}

	MNMesh plane;
	const int NB_FACES = rows * cols;
	const int NB_VERTS = (rows + 1) * (cols + 1);
	
	plane.setNumFaces(NB_FACES);
	plane.setNumVerts(NB_VERTS);
	
	auto count = 0;
	for (auto y = 0; y <= cols; ++y)
	{
		for (auto x = 0; x <= rows; ++x)
		{
			plane.V(count)->p = Point3(x, y, 0);
			++count;
		}
	}

	int row = 0;
	for (auto n = 0; n < NB_FACES; ++n)
	{
		row = n / rows;
		plane.F(n)->SetDeg(4);
		plane.F(n)->vtx[0] = n + row;
		plane.F(n)->vtx[1] = plane.F(n)->vtx[0] + cols + 1;
		plane.F(n)->vtx[2] = plane.F(n)->vtx[0] + cols + 2;
		plane.F(n)->vtx[3] = plane.F(n)->vtx[0] + 1;
	}

	plane.FillInMesh();

	return plane;
}

MNMesh CreateCube(bool specififyNormals)
{
	// Cube from a BOXOBJ_CLASS_ID object.
	MNMesh cube;
	cube.setNumFaces(6);
	cube.setNumVerts(8);
	cube.V(0)->p = Point3(-1.f, -1.f, -1.f);
	cube.V(1)->p = Point3(1.f, -1.f, -1.f);
	cube.V(2)->p = Point3(-1.f, 1.f, -1.f);
	cube.V(3)->p = Point3(1.f, 1.f, -1.f);
	cube.V(4)->p = Point3(-1.f, -1.f, 1.f);
	cube.V(5)->p = Point3(1.f, -1.f, 1.f);
	cube.V(6)->p = Point3(-1.f, 1.f, 1.f);
	cube.V(7)->p = Point3(1.f, 1.f, 1.f);
	cube.F(0)->SetDeg(4);
	cube.F(0)->vtx[0] = 0;
	cube.F(0)->vtx[1] = 2;
	cube.F(0)->vtx[2] = 3;
	cube.F(0)->vtx[3] = 1;
	cube.F(1)->SetDeg(4);
	cube.F(1)->vtx[0] = 4;
	cube.F(1)->vtx[1] = 5;
	cube.F(1)->vtx[2] = 7;
	cube.F(1)->vtx[3] = 6;
	cube.F(2)->SetDeg(4);
	cube.F(2)->vtx[0] = 0;
	cube.F(2)->vtx[1] = 1;
	cube.F(2)->vtx[2] = 5;
	cube.F(2)->vtx[3] = 4;
	cube.F(3)->SetDeg(4);
	cube.F(3)->vtx[0] = 1;
	cube.F(3)->vtx[1] = 3;
	cube.F(3)->vtx[2] = 7;
	cube.F(3)->vtx[3] = 5;
	cube.F(4)->SetDeg(4);
	cube.F(4)->vtx[0] = 3;
	cube.F(4)->vtx[1] = 2;
	cube.F(4)->vtx[2] = 6;
	cube.F(4)->vtx[3] = 7;
	cube.F(5)->SetDeg(4);
	cube.F(5)->vtx[0] = 2;
	cube.F(5)->vtx[1] = 0;
	cube.F(5)->vtx[2] = 4;
	cube.F(5)->vtx[3] = 6;
	cube.FillInMesh();

	if (specififyNormals)
	{		
		// Setup explicit normals
		cube.SpecifyNormals();
		auto specifiedNormals = cube.GetSpecifiedNormals();

		specifiedNormals->SetParent(&cube);
		specifiedNormals->SetNumFaces(6);
		specifiedNormals->SetNumNormals(6);

		specifiedNormals->Normal(0) = Point3(0, 0, -1);
		specifiedNormals->Normal(1) = Point3(0, 0, 1);
		specifiedNormals->Normal(2) = Point3(0, -1, 0);
		specifiedNormals->Normal(3) = Point3(1, 0, 0);
		specifiedNormals->Normal(4) = Point3(0, 1, 0);
		specifiedNormals->Normal(5) = Point3(-1, 0, 0);

		for (int i = 0; i < cube.FNum(); i++)
		{
			specifiedNormals->Face(i).SetDegree(cube.F(i)->deg);
			specifiedNormals->Face(i).SpecifyAll();
			for (int j = 0; j < cube.F(i)->deg; j++)
			{
				specifiedNormals->SetNormalIndex(i, j, i);
			}
		}
		specifiedNormals->SetAllExplicit();
		specifiedNormals->CheckNormals();
		cube.InvalidateGeomCache();
	}
	return cube;
}

// Creates a simple roof-ish shape formed by two connecting quads.
MNMesh CreateRoofShape()
{
	MNMesh roof;
	roof.setNumFaces(2);
	roof.setNumVerts(6);
	roof.V(0)->p = Point3(-1.f, -1.f, 0.f);
	roof.V(1)->p = Point3(0.f, -1.f, 1.f);
	roof.V(2)->p = Point3(0.f, 1.f, 1.f);
	roof.V(3)->p = Point3(-1.f, 1.f, 0.f);
	roof.V(4)->p = Point3(1.f, -1.f, 0.f);
	roof.V(5)->p = Point3(1.f, 1.f, 0.f);
	roof.F(0)->SetDeg(4);
	roof.F(0)->vtx[0] = 0;
	roof.F(0)->vtx[1] = 1;
	roof.F(0)->vtx[2] = 2;
	roof.F(0)->vtx[3] = 3;
	roof.F(1)->SetDeg(4);
	roof.F(1)->vtx[0] = 1;
	roof.F(1)->vtx[1] = 4;
	roof.F(1)->vtx[2] = 5;
	roof.F(1)->vtx[3] = 2;
	roof.FillInMesh();
	return roof;
}

MNMesh CreateQuad()
{
	MNMesh quad;
	quad.setNumFaces(1);
	quad.setNumVerts(4);
	quad.V(0)->p = Point3(-1.f, -1.f, 0.f);
	quad.V(1)->p = Point3(1.f, -1.f, 00.f);
	quad.V(2)->p = Point3(1.f, 1.f, 0.f);
	quad.V(3)->p = Point3(-1.f, 1.f, 0.f);
	quad.F(0)->SetDeg(4);
	quad.F(0)->vtx[0] = 0;
	quad.F(0)->vtx[1] = 1;
	quad.F(0)->vtx[2] = 2;
	quad.F(0)->vtx[3] = 3;
	quad.FillInMesh();
	return quad;
}

std::string GetOutputDirectory()
{
	auto tempDir = fs::temp_directory_path();
	tempDir /= "usd-component-tests";
	return tempDir.u8string();
}

} // namespace TestUtils