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

#include "ComponentCreatorTestHelpers.h"
#include "TestHelpers.h"

#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ScopeGuard.h>

#include <usdUfe/ufe/UsdSceneItem.h>
#include <usdUfe/ufe/UsdUndoRenameCommand.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include <maxscript/maxscript.h>
#include <ufe/path.h>
#include <ufe/undoableCommandMgr.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>

using namespace CCTest;
namespace cc = AdskUsdComponentCreator;

TEST(ComponentCreatorRenameInComponent, RenamePrim)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    ASSERT_TRUE(stage->DefinePrim(pxr::SdfPath("/root/geo/OldName"), pxr::TfToken("Xform")));

    Ufe::UndoableCommandMgr::instance().executeCmd(UsdUfe::UsdUndoRenameCommand::create(
        Item(comp, stage, pxr::SdfPath("/root/geo/OldName")), Ufe::PathComponent("NewName")));
    EXPECT_FALSE(Valid(stage, "/root/geo/OldName"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewName"));

    MaxUndo();
    EXPECT_TRUE(Valid(stage, "/root/geo/OldName"));
    EXPECT_FALSE(Valid(stage, "/root/geo/NewName"));

    MaxRedo();
    EXPECT_FALSE(Valid(stage, "/root/geo/OldName"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewName"));
}

TEST(ComponentCreatorRenameInComponent, RenamePrimWithChildren)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    stage->DefinePrim(pxr::SdfPath("/root/geo/OldParent"), pxr::TfToken("Xform"));
    stage->DefinePrim(pxr::SdfPath("/root/geo/OldParent/Child1"), pxr::TfToken("Xform"));
    stage->DefinePrim(pxr::SdfPath("/root/geo/OldParent/Child2"), pxr::TfToken("Sphere"));

    Ufe::UndoableCommandMgr::instance().executeCmd(UsdUfe::UsdUndoRenameCommand::create(
        Item(comp, stage, pxr::SdfPath("/root/geo/OldParent")), Ufe::PathComponent("NewParent")));
    EXPECT_FALSE(Valid(stage, "/root/geo/OldParent"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewParent"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewParent/Child1"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewParent/Child2"));

    MaxUndo();
    EXPECT_TRUE(Valid(stage, "/root/geo/OldParent"));
    EXPECT_TRUE(Valid(stage, "/root/geo/OldParent/Child1"));
    EXPECT_TRUE(Valid(stage, "/root/geo/OldParent/Child2"));
    EXPECT_FALSE(Valid(stage, "/root/geo/NewParent"));

    MaxRedo();
    EXPECT_FALSE(Valid(stage, "/root/geo/OldParent"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewParent/Child1"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewParent/Child2"));
}

// The component validation runs in the rename command's constructor, so create()
// itself throws. The scope prim must keep its original name.
TEST(ComponentCreatorRenameInComponent, RenameRestrictionComponentScopeItself)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    ASSERT_TRUE(Valid(stage, "/root/geo"));

    bool threw = false;
    try {
        Ufe::UndoableCommandMgr::instance().executeCmd(UsdUfe::UsdUndoRenameCommand::create(
            Item(comp, stage, pxr::SdfPath("/root/geo")), Ufe::PathComponent("renamed_geo")));
    } catch (const std::exception&) {
        threw = true;
    }

    EXPECT_TRUE(threw) << "renaming the component geo scope itself must be rejected";
    EXPECT_TRUE(Valid(stage, "/root/geo"));
    EXPECT_FALSE(Valid(stage, "/root/renamed_geo"));
}
