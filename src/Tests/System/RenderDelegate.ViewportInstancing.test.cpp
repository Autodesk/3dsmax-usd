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
#include <MaxUsd/Utilities/TypeUtils.h>
#include <RenderDelegate/HdMaxEngine.h>

TEST(ViewportInstancing, SceneGraphInstances)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("scene_graph_instances.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	TestRender(stage, testEngine, renderItems, 0);

	// File contains a box, instanced 4 times.
	// Two different materials are used, so we should get 2 "prototypes".
	ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Shaded, renderItems.GetRenderItem(0).GetVisibilityGroup());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Shaded, renderItems.GetRenderItem(1).GetVisibilityGroup());

	auto renderData = testEngine.GetRenderDelegate()->GetRenderDataIdMap();

	// Box001 and Box003 share the same prototype.
	auto it1 = renderData.find(pxr::SdfPath("/scene_graph_instances/Box001.proto_Box001_id0"));
	ASSERT_TRUE(it1 != renderData.end());
	auto& prototype1RenderData = testEngine.GetRenderDelegate()->GetRenderData(it1->second);
	auto transforms1 = prototype1RenderData.instancer->GetTransforms();
	EXPECT_EQ(2, transforms1.size());
	const auto expectedTransformBox001 = MaxUsd::ToMaxMatrix3(pxr::UsdGeomImageable(stage->GetPrimAtPath(pxr::SdfPath("/scene_graph_instances/Box001"))).ComputeLocalToWorldTransform(0));
	EXPECT_TRUE(expectedTransformBox001.Equals(transforms1[0]));
	const auto expectedTransformBox003 = MaxUsd::ToMaxMatrix3(pxr::UsdGeomImageable(stage->GetPrimAtPath(pxr::SdfPath("/scene_graph_instances/Box003"))).ComputeLocalToWorldTransform(0));
	EXPECT_TRUE(expectedTransformBox003.Equals(transforms1[1]));

	// Box002 and Box004 share the same prototype.
	auto it2 = renderData.find(pxr::SdfPath("/scene_graph_instances/Box002.proto_Box001_id0"));
	ASSERT_TRUE(it2 != renderData.end());
	auto& prototype2RenderData = testEngine.GetRenderDelegate()->GetRenderData(it2->second);
	auto transforms2 = prototype2RenderData.instancer->GetTransforms();
	EXPECT_EQ(2, transforms2.size());
	const auto expectedTransformBox002 = MaxUsd::ToMaxMatrix3(pxr::UsdGeomImageable(stage->GetPrimAtPath(pxr::SdfPath("/scene_graph_instances/Box002"))).ComputeLocalToWorldTransform(0));
	EXPECT_TRUE(expectedTransformBox002.Equals(transforms2[0]));
	const auto expectedTransformBox004 = MaxUsd::ToMaxMatrix3(pxr::UsdGeomImageable(stage->GetPrimAtPath(pxr::SdfPath("/scene_graph_instances/Box004"))).ComputeLocalToWorldTransform(0));
	EXPECT_TRUE(expectedTransformBox004.Equals(transforms2[1]));	
}

TEST(ViewportInstancing, SceneGraphInstancesWithSubsets)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("instances_with_material_bound_subsets.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	TestRender(stage, testEngine, renderItems, 0);

	// File contains a box instanced twice. Each of the faces has a different material. We therefor expect 6 render
	// items, one for each face, which contains 2 instances.
	ASSERT_EQ(6, renderItems.GetNumberOfRenderItems());
	
	auto renderData = testEngine.GetRenderDelegate()->GetRenderDataIdMap();

	auto it1 = renderData.find(pxr::SdfPath("/instances_with_material_bound_subsets/Box001.proto_Box001_id0"));
	ASSERT_TRUE(it1 != renderData.end());
	auto& prototypeRenderData = testEngine.GetRenderDelegate()->GetRenderData(it1->second);
	auto transforms = prototypeRenderData.instancer->GetTransforms();
	EXPECT_EQ(2, transforms.size());
	EXPECT_EQ(6, prototypeRenderData.shadedSubsets.size());
	const auto expectedTransformBox001 = MaxUsd::ToMaxMatrix3(pxr::UsdGeomImageable(stage->GetPrimAtPath(pxr::SdfPath("/instances_with_material_bound_subsets/Box001"))).ComputeLocalToWorldTransform(0));
	EXPECT_TRUE(expectedTransformBox001.Equals(transforms[0]));
	const auto expectedTransformBox002 = MaxUsd::ToMaxMatrix3(pxr::UsdGeomImageable(stage->GetPrimAtPath(pxr::SdfPath("/instances_with_material_bound_subsets/Box002"))).ComputeLocalToWorldTransform(0));
	EXPECT_TRUE(expectedTransformBox002.Equals(transforms[1]));
}

TEST(ViewportInstancing, PointInstances)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("point_instances.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	TestRender(stage, testEngine, renderItems, 0);

	// File contains a box instanced 3 times via a point instancer, using 2 prototypes (expect
	// two render items)
	ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
	auto renderData = testEngine.GetRenderDelegate()->GetRenderDataIdMap();

	pxr::VtArray<pxr::GfMatrix4d> expectedTransforms;
	auto instancer = pxr::UsdGeomPointInstancer(stage->GetPrimAtPath(pxr::SdfPath("/Instancer")));
	instancer.ComputeInstanceTransformsAtTime(&expectedTransforms, 0, 0);

	// First prototype, 1 instance.
	auto it1 = renderData.find(pxr::SdfPath("/Instancer.proto0_cube_id0"));
	ASSERT_TRUE(it1 != renderData.end());
	auto& prototype1RenderData = testEngine.GetRenderDelegate()->GetRenderData(it1->second);
	auto transforms1 = prototype1RenderData.instancer->GetTransforms();
	EXPECT_EQ(1, transforms1.size());
	EXPECT_EQ(1, prototype1RenderData.shadedSubsets.size());
	EXPECT_TRUE(MaxUsd::ToMaxMatrix3(expectedTransforms[0]).Equals(transforms1[0]));

	// Second prototype, 2 instances.
	auto it2 = renderData.find(pxr::SdfPath("/Instancer.proto1_cube_id0"));
	ASSERT_TRUE(it2 != renderData.end());
	auto& prototype2RenderData = testEngine.GetRenderDelegate()->GetRenderData(it2->second);
	auto transforms2 = prototype2RenderData.instancer->GetTransforms();	
	EXPECT_EQ(2, transforms2.size());
	EXPECT_EQ(1, prototype2RenderData.shadedSubsets.size());
	EXPECT_TRUE(MaxUsd::ToMaxMatrix3(expectedTransforms[1]).Equals(transforms2[0]));
	EXPECT_TRUE(MaxUsd::ToMaxMatrix3(expectedTransforms[2]).Equals(transforms2[1]));	
}

TEST(ViewportInstancing, WireframeInstances)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("scene_graph_instances.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	TestRender(stage, testEngine, renderItems, 0, nullptr, { pxr::HdReprTokens->wire });

	// File contains a box, instanced 4 times.
	// Two different materials are used, so we should get 2 "prototypes".
	ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe, renderItems.GetRenderItem(0).GetVisibilityGroup());
	ASSERT_EQ(MaxSDK::Graphics::RenderItemVisibilityGroup::RenderItemVisible_Wireframe, renderItems.GetRenderItem(1).GetVisibilityGroup());
}

TEST(ViewportInstancing, InstancesAnimatedTransform)
{
	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("scene_graph_instances_animated.usda");
	const auto stage = pxr::UsdStage::Open(MaxUsd::MaxStringToUsdString(filePath.c_str()));

	HdMaxEngine testEngine;
	MockRenderItemContainer renderItems;

	// Frame 0 :
	TestRender(stage, testEngine, renderItems, 0);
	// File contains a box, instanced twice.
	ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());
	auto renderData = testEngine.GetRenderDelegate()->GetRenderDataIdMap();
	// Box001 and Box002 share the same prototype.
	auto it1 = renderData.find(pxr::SdfPath("/scene_graph_instances_animated/Box001.proto_Box001_id0"));
	ASSERT_TRUE(it1 != renderData.end());
	auto& prototype1RenderData = testEngine.GetRenderDelegate()->GetRenderData(it1->second);
	auto transforms1 = prototype1RenderData.instancer->GetTransforms();
	EXPECT_EQ(2, transforms1.size());
	auto expectedTransformBox001 = MaxUsd::ToMaxMatrix3(pxr::UsdGeomImageable(stage->GetPrimAtPath(pxr::SdfPath("/scene_graph_instances_animated/Box001"))).ComputeLocalToWorldTransform(0));
	EXPECT_TRUE(expectedTransformBox001.Equals(transforms1[0]));
	auto expectedTransformBox002 = MaxUsd::ToMaxMatrix3(pxr::UsdGeomImageable(stage->GetPrimAtPath(pxr::SdfPath("/scene_graph_instances_animated/Box002"))).ComputeLocalToWorldTransform(0));
	EXPECT_TRUE(expectedTransformBox002.Equals(transforms1[1]));
	renderItems.ClearAllRenderItems();
	TestRender(stage, testEngine, renderItems, 1);

	// Frame 1 :	
	// File contains a box, instanced twice.
	ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());
	renderData = testEngine.GetRenderDelegate()->GetRenderDataIdMap();
	// Box001 and Box002 share the same prototype.
	it1 = renderData.find(pxr::SdfPath("/scene_graph_instances_animated/Box001.proto_Box001_id0"));
	ASSERT_TRUE(it1 != renderData.end());
	auto& prototype2RenderData = testEngine.GetRenderDelegate()->GetRenderData(it1->second);
	auto transforms2 = prototype2RenderData.instancer->GetTransforms();
	EXPECT_EQ(2, transforms2.size());
	expectedTransformBox001 = MaxUsd::ToMaxMatrix3(pxr::UsdGeomImageable(stage->GetPrimAtPath(pxr::SdfPath("/scene_graph_instances_animated/Box001"))).ComputeLocalToWorldTransform(1));
	EXPECT_TRUE(expectedTransformBox001.Equals(transforms2[0]));
	expectedTransformBox002 = MaxUsd::ToMaxMatrix3(pxr::UsdGeomImageable(stage->GetPrimAtPath(pxr::SdfPath("/scene_graph_instances_animated/Box002"))).ComputeLocalToWorldTransform(1));
	EXPECT_TRUE(expectedTransformBox002.Equals(transforms2[1]));	
}

