//
// Copyright 2024 Autodesk
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

#include <MaxUsd/Utilities/ScopeGuard.h>
#include <RenderDelegate/SelectionRenderItem.h>
#include <RenderDelegate/HdMaxEngine.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>
#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>

#include <ufe/hierarchy.h>

#include <MaxUsdObjects/Objects/UsdCameraObject.h>

// The FOV approximation is wildly imprecise VS floating-point imprecision.
constexpr float FOV_EPSILON = 0.005f;

// Internally, the camera translation code we use on import, is also used for the internal
// 3dsMax physical camera that the USDCameraObject maintains. The conversion code itself is
// tested in details in camera i/o tests. Here, we just make sure that the time code is wired
// up correctly. Not that it is not possible to do this from maxscript, as the generic camera
// interface, is not exposed.
TEST(USDCamera, TranslateAtTime)
{
	// Reset scene after test.
	const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { GetCOREInterface()->FileReset(TRUE); });

	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("animated_camera.usda");
	const auto stageObject =
			static_cast<USDStageObject*>(GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
	const auto node = GetCOREInterface()->CreateObjectNode(stageObject);
	stageObject->GetParamBlock(0)->SetValueByName(L"AnimationMode", 0, 0);
	stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

	// By default, the usd cameras are generated.
	const auto cameraNode = GetCOREInterface()->GetINodeByName(L"PhysCamera001");
	ASSERT_NE(cameraNode, nullptr);

	const auto usdCamera = dynamic_cast<USDCameraObject*>(cameraNode->GetObjectRef());
	ASSERT_NE(usdCamera, nullptr);

	constexpr float expected1 = 0.839f;
	constexpr float expected2 = 0.804f;
	constexpr float expected3 = 0.730f;
	constexpr float expected4 = 0.642f;

	Interval interval = FOREVER;
	const auto fovAt0 = usdCamera->GetFOV(0 * GetTicksPerFrame(), interval);
	EXPECT_NEAR(fovAt0, expected1, FOV_EPSILON);

	const auto fovAt10 = usdCamera->GetFOV(10 * GetTicksPerFrame(), interval);
	EXPECT_NEAR(fovAt10, expected2, FOV_EPSILON);

	const auto fovAt20 = usdCamera->GetFOV(20 * GetTicksPerFrame(), interval);
	EXPECT_NEAR(fovAt20, expected3, FOV_EPSILON);

	const auto fovAt30 = usdCamera->GetFOV(30 * GetTicksPerFrame(), interval);
	EXPECT_NEAR(fovAt30, expected4, FOV_EPSILON);

	// Offset the animation by 10 seconds.
	stageObject->GetParamBlock(0)->SetValueByName(L"AnimationMode", 1, 0);
	stageObject->GetParamBlock(0)->SetValueByName(L"CustomAnimationStartFrame", 10.f, 0);

	const auto fovAt0Offset = usdCamera->GetFOV(0 * GetTicksPerFrame(), interval);
	EXPECT_NEAR(fovAt0Offset, expected1, FOV_EPSILON);

	const auto fovAt10Offset = usdCamera->GetFOV(10 * GetTicksPerFrame(), interval);
	EXPECT_NEAR(fovAt10Offset, expected1, FOV_EPSILON); // Same value as above, anim start is offset by 10.

	const auto fovAt20Offset = usdCamera->GetFOV(20 * GetTicksPerFrame(), interval);
	EXPECT_NEAR(fovAt20Offset, expected2, FOV_EPSILON);

	const auto fovAt30Offset = usdCamera->GetFOV(30 * GetTicksPerFrame(), interval);
	EXPECT_NEAR(fovAt30Offset, expected3, FOV_EPSILON);
}

TEST(USDCamera, AttributeEdit)
{
	// Reset scene after test.
	const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { GetCOREInterface()->FileReset(TRUE); });

	auto testDataPath = GetTestDataPath();
	const auto filePath = testDataPath.append("animated_camera.usda");
	const auto stageObject =
			static_cast<USDStageObject*>(GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
	const auto node = GetCOREInterface()->CreateObjectNode(stageObject);
	stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

	const auto cameraNode = GetCOREInterface()->GetINodeByName(L"PhysCamera001");
	ASSERT_NE(cameraNode, nullptr);

	const auto maxUsdCamera = dynamic_cast<USDCameraObject*>(cameraNode->GetObjectRef());
	ASSERT_NE(maxUsdCamera, nullptr);

	auto usdCam = maxUsdCamera->GetUsdCamera();

	constexpr float expected1 = 0.839f;
	Interval interval = FOREVER;
	const auto val1 = maxUsdCamera->GetFOV(0, interval);
	EXPECT_NEAR(val1, expected1, FOV_EPSILON);


	auto stage = usdCam.GetPrim().GetStage();
	stage->SetEditTarget(stage->GetSessionLayer());

	// Change the value on the session layer. First edit on another layer, 
	// causes a resync of the camera path as there is now composition.
	usdCam.GetFocalLengthAttr().Set(54.114174f);

	constexpr float expected2 = 0.642f;
	const auto val2 = maxUsdCamera->GetFOV(0, interval);
	EXPECT_NEAR(val2, expected2, FOV_EPSILON);

	// Change it again, now attribute only changed.
	usdCam.GetFocalLengthAttr().Set(47.072845f);

	constexpr float expected3 = 0.730f;
	const auto val3 = maxUsdCamera->GetFOV(0, interval);
	EXPECT_NEAR(val3, expected3, FOV_EPSILON);
}
