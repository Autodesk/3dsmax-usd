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
#include <usdUfe/ufe/UsdUndoAddNewPrimCommand.h>
#include <usdUfe/undo/UsdUndoBlock.h>
#include <usdUfe/undo/UsdUndoableItem.h>

#include <pxr/usd/sdf/layer.h>
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

} // namespace

// A pxr-API edit (no UsdUndoBlock) is not forwarded and stays on the session layer
// (the component edit target).
TEST(ComponentCreatorAddPrimInComponent, AddPrimViaApiLandsInSessionLayer)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    const pxr::SdfPath primPath("/root/geo/ApiPrim");
    const pxr::UsdPrim newPrim = stage->DefinePrim(primPath, pxr::TfToken("Xform"));
    EXPECT_TRUE(newPrim.IsValid());

    EXPECT_TRUE(stage->GetSessionLayer()->GetPrimAtPath(primPath))
        << "prim authored via the pxr API should land in the session layer";
    EXPECT_FALSE(stage->GetRootLayer()->GetPrimAtPath(primPath))
        << "prim authored via the pxr API should not be in the locked root layer";
}

// A prim added through a UFE command (UsdUndoAddNewPrimCommand) under /root/geo is forwarded out
// of the session layer to the targeted variant -- the same result as the direct-edit test above,
// but via the command-capture forward path (MaxUfeUndoableCommandMgr). Undo/redo of this path is
// covered by AddForwardedPrimUndoesAsOneStep; here we only assert the forward.
TEST(ComponentCreatorAddPrimInComponent, AddPrimViaUfeCommandForwardsToVariantLayer)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    // Add an Xform under /root/geo through the UFE undoable command system (so
    // MaxUfeUndoableCommandMgr opens an edit-forward capture scope around it).
    const pxr::UsdPrim geoPrim = stage->GetPrimAtPath(pxr::SdfPath("/root/geo"));
    ASSERT_TRUE(geoPrim.IsValid());
    const Ufe::Path geoUfePath = MaxUsd::ufe::getUsdPrimUfePath(comp, pxr::SdfPath("/root/geo"));
    const auto      parentItem = UsdUfe::UsdSceneItem::create(geoUfePath, geoPrim);
    ASSERT_TRUE(parentItem);

    const auto cmd = UsdUfe::UsdUndoAddNewPrimCommand::create(parentItem, "UfeAdd", "Xform");
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
    const pxr::SdfPath primPath = cmd->newPrim().GetPath();
    ASSERT_FALSE(primPath.IsEmpty());
    PumpIdle();

    // After forwarding: gone from the session layer, still visible via the variant.
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(primPath))
        << "prim added via UFE command should have been forwarded out of the session layer";
    EXPECT_TRUE(stage->GetPrimAtPath(primPath).IsValid())
        << "prim should remain visible in the composed stage after forwarding";
}

// Command-driven (capture-scope) forward path: a single undo must revert both the command edit
// and its forwarded variant edit (they share one undo entry); redo restores both.
TEST(ComponentCreatorAddPrimInComponent, AddForwardedPrimUndoesAsOneStep)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    // Build the /root/geo parent scene item and add a new Xform child through the
    // UFE undoable command system (so MaxUfeUndoableCommandMgr opens an edit-forward
    // capture scope around it and attaches the resulting forward to its undo entry).
    const pxr::UsdPrim geoPrim = stage->GetPrimAtPath(pxr::SdfPath("/root/geo"));
    ASSERT_TRUE(geoPrim.IsValid());
    const Ufe::Path geoUfePath = MaxUsd::ufe::getUsdPrimUfePath(comp, pxr::SdfPath("/root/geo"));
    const auto      parentItem = UsdUfe::UsdSceneItem::create(geoUfePath, geoPrim);
    ASSERT_TRUE(parentItem);

    const auto cmd = UsdUfe::UsdUndoAddNewPrimCommand::create(parentItem, "ForwardTest", "Xform");
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
    const pxr::SdfPath primPath = cmd->newPrim().GetPath();
    ASSERT_FALSE(primPath.IsEmpty());
    PumpIdle();

    // After the command (the forward runs synchronously once it commits): the prim
    // has left the session layer for the variant, but is still composed/visible.
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(primPath))
        << "forwarded prim should no longer be in the session layer";
    EXPECT_TRUE(stage->GetPrimAtPath(primPath).IsValid())
        << "forwarded prim should remain visible in the composed stage";

    // One undo must revert the command AND its forward together: prim gone everywhere.
    ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded);
    PumpIdle();
    EXPECT_FALSE(stage->GetPrimAtPath(primPath).IsValid())
        << "a single undo must revert both the command edit and its forward";
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(primPath))
        << "undo must also clear the session layer (no leftover from the original edit)";

    // Redo must replay both: prim visible again, in the variant (not the session).
    ExecuteMAXScriptScript(L"max redo", MAXScript::ScriptSource::NonEmbedded);
    PumpIdle();
    EXPECT_TRUE(stage->GetPrimAtPath(primPath).IsValid()) << "redo must restore the forwarded prim";
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(primPath))
        << "redo must restore the prim to the variant, not the session layer";
}

// Two forwarded edits stacked on the undo queue must unwind and replay one at a time
// (undo -> undo -> redo -> redo), which the single-edit tests above don't cover. Each edit
// adds a distinct prim through the UFE command path (so each is its own forwarded undo
// entry); after each step we assert exactly which prims are composed and that nothing is
// stranded in the session layer.
TEST(ComponentCreatorAddPrimInComponent, StackedForwardsUndoRedoWalk)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    const pxr::UsdPrim geoPrim = stage->GetPrimAtPath(pxr::SdfPath("/root/geo"));
    ASSERT_TRUE(geoPrim.IsValid());
    const Ufe::Path geoUfePath = MaxUsd::ufe::getUsdPrimUfePath(comp, pxr::SdfPath("/root/geo"));
    const auto      parentItem = UsdUfe::UsdSceneItem::create(geoUfePath, geoPrim);
    ASSERT_TRUE(parentItem);

    // Add prim A through a UFE command, let it forward.
    const auto cmdA = UsdUfe::UsdUndoAddNewPrimCommand::create(parentItem, "StackA", "Xform");
    Ufe::UndoableCommandMgr::instance().executeCmd(cmdA);
    const pxr::SdfPath pathA = cmdA->newPrim().GetPath();
    ASSERT_FALSE(pathA.IsEmpty());
    PumpIdle();

    // Add prim B through a second UFE command, let it forward.
    const auto cmdB = UsdUfe::UsdUndoAddNewPrimCommand::create(parentItem, "StackB", "Xform");
    Ufe::UndoableCommandMgr::instance().executeCmd(cmdB);
    const pxr::SdfPath pathB = cmdB->newPrim().GetPath();
    ASSERT_FALSE(pathB.IsEmpty());
    PumpIdle();

    // Both forwarded: present in the composed stage, neither left in the session layer.
    EXPECT_TRUE(stage->GetPrimAtPath(pathA).IsValid());
    EXPECT_TRUE(stage->GetPrimAtPath(pathB).IsValid());
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(pathA));
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(pathB));

    // Undo once: B (the most recent edit) is reverted; A is still in effect.
    ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded);
    PumpIdle();
    EXPECT_TRUE(stage->GetPrimAtPath(pathA).IsValid()) << "first edit must still be in effect";
    EXPECT_FALSE(stage->GetPrimAtPath(pathB).IsValid()) << "second edit must be undone";
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(pathA));

    // Undo again: A is reverted; nothing composed, nothing stranded in the session layer.
    ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded);
    PumpIdle();
    EXPECT_FALSE(stage->GetPrimAtPath(pathA).IsValid()) << "first edit must now be undone too";
    EXPECT_FALSE(stage->GetPrimAtPath(pathB).IsValid());
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(pathA));

    // Redo once: A returns to the variant (not the session layer); B still absent.
    ExecuteMAXScriptScript(L"max redo", MAXScript::ScriptSource::NonEmbedded);
    PumpIdle();
    EXPECT_TRUE(stage->GetPrimAtPath(pathA).IsValid()) << "redo must replay the first edit first";
    EXPECT_FALSE(stage->GetPrimAtPath(pathB).IsValid());
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(pathA));

    // Redo again: B returns to the variant; both composed, session layer still clean.
    ExecuteMAXScriptScript(L"max redo", MAXScript::ScriptSource::NonEmbedded);
    PumpIdle();
    EXPECT_TRUE(stage->GetPrimAtPath(pathA).IsValid());
    EXPECT_TRUE(stage->GetPrimAtPath(pathB).IsValid()) << "redo must replay the second edit";
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(pathB));
}
