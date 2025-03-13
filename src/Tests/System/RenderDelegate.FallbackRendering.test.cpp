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
#include <maxscript/foundation/numbers.h>

#include <MaxUsdObjects/Objects/USDStageObject.h>
#include <MaxUsd/Utilities/ScopeGuard.h>

// Test that the primvar mapping configuration on a UsdStageObject is respected when
// generating render meshes.
TEST(FallbackRender, RenderMeshPrimvarMapping)
{
	// Reset scene after test.
	const auto resetGuard = MaxUsd::MakeScopeGuard(
	[]() { }, []() { GetCOREInterface()->FileReset(TRUE); });
	
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("unmappedpv.usda");
	
	auto stageObject = static_cast<USDStageObject*>(GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));

	auto node = GetCOREInterface()->CreateObjectNode(stageObject);
	stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

	BOOL needDelete;

	// GetRenderMesh() returns a mesh for the overall stage, i.e. all the individual prim meshes are merged into a single mesh.
	ViewMock viewMock{};
	auto renderMesh = stageObject->GetRenderMesh(0, node, viewMock, needDelete);

	// Map1 is not used but it is mapped to "1". It is mapped, so it should still be loaded.
	ASSERT_TRUE(renderMesh->mapSupport(1));
	auto& map1 = renderMesh->Map(1);
	EXPECT_EQ(map1.getNumVerts(), 48);
	EXPECT_TRUE(map1.tv[0].Equals(Point3(1.f,0.f,0.f)));
	// Foo and bar are unmapped, so they should not be found in the resulting render mesh.
	for (int i = 2; i < MAX_MESHMAPS; ++i) // Uvs are from 1 to 100.
	{
		ASSERT_FALSE(renderMesh->mapSupport(i));
	}

	// Test mapping / unmapping and effect on render mesh.
	stageObject->SetPrimvarChannelMapping(L"foo", Integer::intern(10));
	stageObject->SetPrimvarChannelMapping(L"bar", Integer::intern(20));
	stageObject->SetPrimvarChannelMapping(L"map1", &undefined);

	renderMesh = stageObject->GetRenderMesh(0, node, viewMock, needDelete);

	// Validate output render mesh.
	ASSERT_FALSE(renderMesh->mapSupport(1));
;	ASSERT_TRUE(renderMesh->mapSupport(10));
	auto& map10 = renderMesh->Map(10);
	ASSERT_EQ(map10.getNumVerts(), 24); // foo only present in one of the boxes, so only 24 verts
	// Test some meaningful values to make sure we loaded the right uvs.
	EXPECT_TRUE(map10.tv[0].Equals(Point3{}));
	EXPECT_TRUE(map10.tv[14].Equals(Point3(1.f, 1.f, 0.f)));

	EXPECT_TRUE(renderMesh->mapSupport(20));
	auto& map20 = renderMesh->Map(20);
	EXPECT_EQ(map20.getNumVerts(), 24); // bar only present in one of the boxes, so only 24 verts
	// Test some meaningful values to make sure we loaded the right uvs.
	EXPECT_TRUE(map20.tv[0].Equals(Point3(0.65894926f, 0.23953691f, 0.f) ));
	EXPECT_TRUE(map20.tv[23].Equals(Point3(0.39848992f, 0.333660007f, 0.f)));

	// Test mapping both foo and bar to the same channel.
	stageObject->ClearMappedPrimvars();
	stageObject->SetPrimvarChannelMapping(L"foo", Integer::intern(50));
	stageObject->SetPrimvarChannelMapping(L"bar", Integer::intern(50));

	renderMesh = stageObject->GetRenderMesh(0, node, viewMock, needDelete);
	for (int i = 2; i < MAX_MESHMAPS; ++i)
	{
		ASSERT_EQ(i == 50, renderMesh->mapSupport(i)); // Only channel 50 should be set.
	}

	auto& map50 = renderMesh->Map(50);
	EXPECT_EQ(map50.getNumVerts(), 48);
	// Test some meaningful values to make sure we loaded the right uvs.
	EXPECT_TRUE(map50.tv[0].Equals(Point3(0.f, 0.f, 0.f)));
	EXPECT_TRUE(map50.tv[47].Equals(Point3(0.398489833f, 0.333660007f, 0.f)));
}


TEST(FallbackRender, MultiRenderMeshPrimvarMapping)
{
	// Reset scene after test.
	const auto resetGuard = MaxUsd::MakeScopeGuard(
	[]() { }, []() { GetCOREInterface()->FileReset(TRUE); });

	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("unmappedpv.usda");
	
	auto stageObject = static_cast<USDStageObject*>(GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));

	auto node = GetCOREInterface()->CreateObjectNode(stageObject);
	stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

	BOOL needDelete;
	ViewMock viewMock{};
	auto renderMesh1 = stageObject->GetMultipleRenderMesh(0, node, viewMock, needDelete, 0);
	auto renderMesh2 = stageObject->GetMultipleRenderMesh(0, node, viewMock, needDelete, 1);

	// Map1 is not used but it is mapped to "1". Is is mapped so should still be loaded.
	ASSERT_TRUE(renderMesh1->mapSupport(1));
	auto& Mesh1_map1 = renderMesh1->Map(1);
	EXPECT_TRUE(Mesh1_map1.tv[0].Equals(Point3(1.f,0.f,0.f)));
	EXPECT_EQ(Mesh1_map1.getNumVerts(), 24);
	auto& Mesh2_map1 = renderMesh2->Map(1);
	EXPECT_EQ(Mesh2_map1.getNumVerts(), 24);
	EXPECT_TRUE(Mesh2_map1.tv[0].Equals(Point3(1.f,0.f,0.f)));

	// Foo and bar are unmapped, so they should not be found in the resulting render meshes.
	ASSERT_TRUE(renderMesh2->mapSupport(1));
	for (int i = 2; i < MAX_MESHMAPS; ++i) // Uvs are from 1 to 100.
	{
		ASSERT_FALSE(renderMesh1->mapSupport(i));
		ASSERT_FALSE(renderMesh2->mapSupport(i));
	}

	// Test mapping / unmapping and effect on render meshes.
	stageObject->SetPrimvarChannelMapping(L"foo", Integer::intern(10));
	stageObject->SetPrimvarChannelMapping(L"bar", Integer::intern(20));
	stageObject->SetPrimvarChannelMapping(L"map1", &undefined);

	renderMesh1 = stageObject->GetMultipleRenderMesh(0, node, viewMock, needDelete,0);
	renderMesh2 = stageObject->GetMultipleRenderMesh(0, node, viewMock, needDelete,1);

	// Validate output render mesh 1
	{
		ASSERT_FALSE(renderMesh1->mapSupport(1));
		ASSERT_FALSE(renderMesh1->mapSupport(20)); // bar not defined on the first box

		ASSERT_TRUE(renderMesh1->mapSupport(10)); // foo is defined
		auto& map10 = renderMesh1->Map(10);
		ASSERT_EQ(map10.getNumVerts(), 24);
		// Test some meaningful values to make sure we loaded the right uvs.
		EXPECT_TRUE(map10.tv[0].Equals(Point3(0.f,0.f,0.f)));
		EXPECT_TRUE(map10.tv[14].Equals(Point3(1.f, 1.f, 0.f)));
	}

	// Validate output render mesh 2
	{
		ASSERT_FALSE(renderMesh2->mapSupport(1));
		ASSERT_FALSE(renderMesh2->mapSupport(10)); // foo is defined

		EXPECT_TRUE(renderMesh2->mapSupport(20)); // bar not defined on second box
		auto& map20 = renderMesh2->Map(20);
		EXPECT_EQ(map20.getNumVerts(), 24);
		// Test some meaningful values to make sure we loaded the right uvs.
		EXPECT_TRUE(map20.tv[0].Equals(Point3(0.658949256f,0.239536881f,0.f)));
		EXPECT_TRUE(map20.tv[23].Equals(Point3(0.39848992f, 0.333660007f, 0.f)));
	}

	// Test mapping both foo and bar to the same channel.
	stageObject->ClearMappedPrimvars();
	stageObject->SetPrimvarChannelMapping(L"foo", Integer::intern(50));
	stageObject->SetPrimvarChannelMapping(L"bar", Integer::intern(50));

	renderMesh1 = stageObject->GetMultipleRenderMesh(0, node, viewMock, needDelete, 0);
	renderMesh2 = stageObject->GetMultipleRenderMesh(0, node, viewMock, needDelete, 1);
	for (int i = 2; i < MAX_MESHMAPS; ++i)
	{
		// Only channel 50 should be set.
		ASSERT_EQ(i == 50, renderMesh1->mapSupport(i));
		ASSERT_EQ(i == 50, renderMesh1->mapSupport(i));
	}

	// Validate bar on box 1
	auto& map50_1 = renderMesh1->Map(50);
	EXPECT_EQ(map50_1.getNumVerts(), 24);
	EXPECT_TRUE(map50_1.tv[0].Equals(Point3(0.f,0.f,0.f)));
	EXPECT_TRUE(map50_1.tv[14].Equals(Point3(1.f, 1.f, 0.f)));

	// Validate foo on box 2
	auto& map50_2 = renderMesh2->Map(50);
	EXPECT_EQ(map50_2.getNumVerts(), 24);
	EXPECT_TRUE(map50_2.tv[0].Equals(Point3(0.658949256f, 0.239536881f, 0.f)));
	EXPECT_TRUE(map50_2.tv[23].Equals(Point3(0.398489833f, 0.333660007f, 0.f)));
}