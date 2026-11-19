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
#include <usdUfe/ufe/UsdUndoInsertChildCommand.h>
#include <usdUfe/undo/UsdUndoManager.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include <maxscript/maxscript.h>
#include <ufe/undoableCommandMgr.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>

using namespace CCTest;
namespace cc = AdskUsdComponentCreator;

namespace {

// Reparent `child` under `parent` (append) via the UFE insert-child command.
void Reparent(
    USDStageObject*            comp,
    const pxr::UsdStageRefPtr& stage,
    const pxr::SdfPath&        child,
    const pxr::SdfPath&        parent)
{
    // Track the edit-target layer for undo (as MaxUsdEditCommand does for app UFE edits).
    // The component reparent copies the prim into the edit-target (session) layer but only
    // tracks the source layers it removes from, so without this the destination copy is not
    // captured and undo cannot reverse it.
    UsdUfe::UsdUndoManager::instance().trackLayerStates(stage->GetEditTarget().GetLayer());
    Ufe::UndoableCommandMgr::instance().executeCmd(UsdUfe::UsdUndoInsertChildCommand::create(
        Item(comp, stage, parent), Item(comp, stage, child), /*pos=*/nullptr));
}

} // namespace

TEST(ComponentCreatorReparentInComponent, ReparentPrim)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    stage->DefinePrim(pxr::SdfPath("/root/geo/Child"), pxr::TfToken("Xform"));
    stage->DefinePrim(pxr::SdfPath("/root/geo/NewParent"), pxr::TfToken("Xform"));

    Reparent(comp, stage, pxr::SdfPath("/root/geo/Child"), pxr::SdfPath("/root/geo/NewParent"));
    EXPECT_FALSE(Valid(stage, "/root/geo/Child"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewParent/Child"));

    MaxUndo();
    EXPECT_TRUE(Valid(stage, "/root/geo/Child"));
    EXPECT_FALSE(Valid(stage, "/root/geo/NewParent/Child"));

    MaxRedo();
    EXPECT_FALSE(Valid(stage, "/root/geo/Child"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewParent/Child"));
}

TEST(ComponentCreatorReparentInComponent, ReparentPrimWithChildren)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    stage->DefinePrim(pxr::SdfPath("/root/geo/Parent"), pxr::TfToken("Xform"));
    stage->DefinePrim(pxr::SdfPath("/root/geo/Parent/Child1"), pxr::TfToken("Xform"));
    stage->DefinePrim(pxr::SdfPath("/root/geo/Parent/Child2"), pxr::TfToken("Sphere"));
    stage->DefinePrim(pxr::SdfPath("/root/geo/NewGrandparent"), pxr::TfToken("Xform"));

    Reparent(
        comp, stage, pxr::SdfPath("/root/geo/Parent"), pxr::SdfPath("/root/geo/NewGrandparent"));
    EXPECT_FALSE(Valid(stage, "/root/geo/Parent"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewGrandparent/Parent"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewGrandparent/Parent/Child1"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewGrandparent/Parent/Child2"));

    MaxUndo();
    EXPECT_TRUE(Valid(stage, "/root/geo/Parent"));
    EXPECT_TRUE(Valid(stage, "/root/geo/Parent/Child1"));
    EXPECT_TRUE(Valid(stage, "/root/geo/Parent/Child2"));
    EXPECT_FALSE(Valid(stage, "/root/geo/NewGrandparent/Parent"));

    MaxRedo();
    EXPECT_FALSE(Valid(stage, "/root/geo/Parent"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewGrandparent/Parent/Child1"));
    EXPECT_TRUE(Valid(stage, "/root/geo/NewGrandparent/Parent/Child2"));
}

// The destination-scope restriction (validateComponentNamespaceOperation with
// isDestination=true on the reparent target) validates in the InsertChild constructor,
// so create() throws when the target is outside the component scopes.
TEST(ComponentCreatorReparentInComponent, ReparentRestrictionToOutsideComponentScope)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    stage->DefinePrim(pxr::SdfPath("/root/geo/Child"), pxr::TfToken("Xform"));
    stage->DefinePrim(pxr::SdfPath("/root/OutsideScope"), pxr::TfToken("Xform"));

    bool threw = false;
    try {
        Reparent(comp, stage, pxr::SdfPath("/root/geo/Child"), pxr::SdfPath("/root/OutsideScope"));
    } catch (const std::exception&) {
        threw = true;
    }

    EXPECT_TRUE(threw)
        << "reparenting to a destination outside the component scopes must be rejected";
    EXPECT_TRUE(Valid(stage, "/root/geo/Child"));
    EXPECT_FALSE(Valid(stage, "/root/OutsideScope/Child"));
}

TEST(ComponentCreatorReparentInComponent, ReparentRestrictionComponentScopeItself)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    stage->DefinePrim(pxr::SdfPath("/root/NewParent"), pxr::TfToken("Xform"));

    bool threw = false;
    try {
        Reparent(comp, stage, pxr::SdfPath("/root/geo"), pxr::SdfPath("/root/NewParent"));
    } catch (const std::exception&) {
        threw = true;
    }

    EXPECT_TRUE(threw) << "reparenting the component geo scope itself must be rejected";
    EXPECT_TRUE(Valid(stage, "/root/geo"));
    EXPECT_FALSE(Valid(stage, "/root/NewParent/geo"));
}
