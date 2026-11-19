//
// Copyright 2026 Autodesk
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

#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ScopeGuard.h>

#include <pxr/usd/sdf/path.h>

// StageObjectMap is keyed by the stage object path (first UFE segment). getStage must strip
// longer paths down to that segment so prim-level paths still resolve the stage.
TEST(UfeUtils, GetStage)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    auto       testDataPath = GetTestDataPath();
    const auto filePath = testDataPath.append("selection_geometry.usda");
    const auto stageObject = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
    ASSERT_NE(stageObject, nullptr);
    GetCOREInterface()->CreateObjectNode(stageObject);
    stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

    const auto expectedStage = stageObject->GetUSDStage();
    ASSERT_TRUE(expectedStage);

    EXPECT_EQ(MaxUsd::ufe::getStage(Ufe::Path {}), nullptr);

    const auto stagePath = MaxUsd::ufe::getUsdStageObjectPath(stageObject);
    ASSERT_FALSE(stagePath.empty());
    EXPECT_EQ(MaxUsd::ufe::getStage(stagePath), expectedStage);

    const auto primPath
        = MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath("/root/Box001"));
    ASSERT_GE(primPath.nbSegments(), 2u);
    EXPECT_EQ(MaxUsd::ufe::getStage(primPath), expectedStage);

    EXPECT_EQ(MaxUsd::ufe::getStagePath(expectedStage), stagePath);
}
