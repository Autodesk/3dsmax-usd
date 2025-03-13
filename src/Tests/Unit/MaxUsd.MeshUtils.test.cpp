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

#include <mesh.h>
#include <mnmesh.h>

#include "TestUtils.h"
#include <MaxUsd/Utilities/MeshUtils.h>

using namespace MaxUsd;

TEST(MeshUtilsTest, SetupEdgeVisibilityFromTriNormals)
{
	// Create a quad, as a trimesh. 
	auto quad = TestUtils::CreateQuad();
	Mesh quadTriMesh;
	quad.OutToTri(quadTriMesh);

	auto getEdgeVis = [](const Mesh& mesh, std::vector<bool>& edgeVis)
	{
		edgeVis.clear();
		const auto numFaces = mesh.getNumFaces();
		for (int i = 0; i < numFaces; i++)
		{
			Face& face = mesh.faces[i];
			edgeVis.push_back(face.getEdgeVis(0) > 0);
			edgeVis.push_back(face.getEdgeVis(1) > 0);
			edgeVis.push_back(face.getEdgeVis(2) > 0);
		}
	};

	auto setEdgeDisplay = [](Mesh& mesh, int visibility)
	{
		for (int i = 0; i < mesh.getNumFaces(); i++)
		{
			Face& face = mesh.faces[i];
			face.setEdgeVis(0,visibility);
			face.setEdgeVis(1, visibility);
			face.setEdgeVis(2, visibility);
		}
	};

	// Make sure that edges between coplanar triangles do not get shown
	// For the quad we created, this means no change to edge visibility.
	std::vector<bool> edgeVisibility;
	getEdgeVis(quadTriMesh, edgeVisibility);
	EXPECT_EQ(4, std::count(edgeVisibility.begin(), edgeVisibility.end(), true));
	MeshUtils::SetupEdgeVisibility(quadTriMesh, true);
	std::vector<bool> edgeVisAfterCall;
	getEdgeVis(quadTriMesh, edgeVisAfterCall);
	EXPECT_TRUE(std::equal(edgeVisibility.begin(), edgeVisibility.end(), edgeVisAfterCall.begin()));

	// Now we move one of the vertices... causing the two triangles to no longer be coplanar...
	quadTriMesh.getVert(0).z += 1.0f;
	MeshUtils::SetupEdgeVisibility(quadTriMesh, true);
	// All edges should now be visible.
	getEdgeVis(quadTriMesh, edgeVisibility);
	EXPECT_TRUE(std::all_of(edgeVisibility.begin(), edgeVisibility.end(), [](bool visibility)
	{
		return visibility;
	}));

	// Reset the quad trimesh and assign different materials to the two triangles.
	quad.OutToTri(quadTriMesh);
	quadTriMesh.faces[0].setMatID(0);
	quadTriMesh.faces[1].setMatID(1);
	MeshUtils::SetupEdgeVisibility(quadTriMesh, true);
	getEdgeVis(quadTriMesh, edgeVisibility);
	EXPECT_TRUE(std::all_of(edgeVisibility.begin(), edgeVisibility.end(), [](bool visibility)
	{
		return visibility;
	}));

	// Make sure that edges already visible are not hidden between coplanar faces.
	quad.OutToTri(quadTriMesh);
	setEdgeDisplay(quadTriMesh, EDGE_VIS);
	MeshUtils::SetupEdgeVisibility(quadTriMesh, true);
	getEdgeVis(quadTriMesh, edgeVisibility);
	EXPECT_TRUE(std::all_of(edgeVisibility.begin(), edgeVisibility.end(), [](bool visibility)
	{
		return visibility;
	}));

	// Make sure the degenerate triangles are not considered/modified.
	quad.OutToTri(quadTriMesh);
	auto verts = quadTriMesh.faces[1].getAllVerts();
	quadTriMesh.faces[1].setVerts(verts[0], verts[1], verts[1]); // Face[1] is degenerated.
	setEdgeDisplay(quadTriMesh, EDGE_INVIS);
	MeshUtils::SetupEdgeVisibility(quadTriMesh, true);
	getEdgeVis(quadTriMesh, edgeVisibility);
	EXPECT_TRUE(std::all_of(edgeVisibility.begin(), edgeVisibility.begin() + 3, [](bool visibility)
	{
		return visibility;
	}));
	EXPECT_FALSE(std::all_of(edgeVisibility.begin() + 3, edgeVisibility.end(), [](bool visibility)
	{
		return visibility;
	}));

	// Test non-manifold mesh cases...
	auto createNonManifoldMesh = []() -> Mesh
	{
		Mesh nonManifoldMesh;
		nonManifoldMesh.setNumFaces(3);
		nonManifoldMesh.setNumVerts(5);
		nonManifoldMesh.verts[0] = Point3(-1.f, -1.f, 0.f);
		nonManifoldMesh.verts[1] = Point3(1.f, -1.f, 0.f);
		nonManifoldMesh.verts[2] = Point3(1.f, 1.f, 0.f);
		nonManifoldMesh.verts[3] = Point3(-1.f, 1.f, 0.f);
		nonManifoldMesh.verts[4] = Point3(0.f, 0.f, 1.f);
		nonManifoldMesh.faces[0].setVerts(0, 1, 2); // coplanar
		nonManifoldMesh.faces[1].setVerts(0, 2, 3); // coplanar
		nonManifoldMesh.faces[2].setVerts(0, 4, 2);  // perpendicular
		return nonManifoldMesh;
	};

	// 3 triangles, two of them coplanar - and the last one perpendicular. All 3 sharing an edge.
	Mesh nonManifoldMesh = createNonManifoldMesh();
	// Set all edges to invisible, to see if they will be set correctly.
	setEdgeDisplay(nonManifoldMesh, EDGE_INVIS);
	MeshUtils::SetupEdgeVisibility(nonManifoldMesh, true);
	// Now only 2 edges should be hidden, the edges between the two coplanar triangles.	
	getEdgeVis(nonManifoldMesh, edgeVisibility);
	EXPECT_EQ(7, std::count(edgeVisibility.begin(), edgeVisibility.end(), true));
	EXPECT_FALSE(edgeVisibility[2]);
	EXPECT_FALSE(edgeVisibility[3]);

	// Make sure that the perpendicular triangle being of a different material doesn't
	// change the result.
	setEdgeDisplay(nonManifoldMesh, EDGE_INVIS);
	nonManifoldMesh.faces[0].setMatID(0);
	nonManifoldMesh.faces[1].setMatID(0);
	nonManifoldMesh.faces[2].setMatID(1);
	MeshUtils::SetupEdgeVisibility(nonManifoldMesh, true);
	getEdgeVis(nonManifoldMesh, edgeVisibility);
	EXPECT_EQ(7, std::count(edgeVisibility.begin(), edgeVisibility.end(), true));
	EXPECT_FALSE(edgeVisibility[2]);
	EXPECT_FALSE(edgeVisibility[3]);
	
	// Make sure material boundaries still work in case there is continuity on the intersection
	// edge, but between triangles that are not coplanar.
	setEdgeDisplay(nonManifoldMesh, EDGE_INVIS);
	nonManifoldMesh.faces[0].setMatID(1);
	nonManifoldMesh.faces[1].setMatID(0);
	nonManifoldMesh.faces[2].setMatID(1);
	MeshUtils::SetupEdgeVisibility(nonManifoldMesh, true);
	getEdgeVis(nonManifoldMesh, edgeVisibility);
	EXPECT_TRUE(std::all_of(edgeVisibility.begin(), edgeVisibility.end(), [](bool visibility)
	{
		return visibility;
	}));
}

TEST(MeshUtilsTest, SetupEdgeVisibilityShowAll)
{
	// Create a quad, as a trimesh. 
	auto quad = TestUtils::CreateCube(false);
	Mesh cubeTriMesh;
	quad.OutToTri(cubeTriMesh);

	auto getEdgeVis = [](const Mesh& mesh, std::vector<bool>& edgeVis)
	{
		edgeVis.clear();
		const auto numFaces = mesh.getNumFaces();
		for (int i = 0; i < numFaces; i++)
		{
			Face& face = mesh.faces[i];
			edgeVis.push_back(face.getEdgeVis(0) > 0);
			edgeVis.push_back(face.getEdgeVis(1) > 0);
			edgeVis.push_back(face.getEdgeVis(2) > 0);
		}
	};
	
	std::vector<bool> edgeVisibility;
	MeshUtils::SetupEdgeVisibility(cubeTriMesh, false);
	getEdgeVis(cubeTriMesh, edgeVisibility);
	EXPECT_TRUE(std::all_of(edgeVisibility.begin(), edgeVisibility.end(), [](bool visibility)
	{
		return visibility;
	}));
}