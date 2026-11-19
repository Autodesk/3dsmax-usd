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

#include "ComponentCreatorTestHelpers.h" // CCTest::MaxUndo/MaxRedo/PumpIdle
#include "TestHelpers.h"                 // STAGE_CLASS_ID, QuickReset

#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ScopeGuard.h>

#include <usdUfe/ufe/UsdSceneItem.h>
#include <usdUfe/ufe/UsdUndoAddNewPrimCommand.h>
#include <usdUfe/undo/UsdUndoBlock.h>
#include <usdUfe/undo/UsdUndoableItem.h>

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include <maxscript/maxscript.h>
#include <ufe/undoableCommandMgr.h>

#include <AdskUsdEditForward/RuleDef.h>
#include <GetCOREInterface.h>

using namespace CCTest;
namespace ef = AdskUsdEditForward;

namespace {

// Build a plain (non-component) stage object whose session-layer edits forward to a layer
// tagged "TEST". The anonymous TEST target layer comes back through `testLayer` (keep it alive
// for the test). Authors a "/Geo" Xform parent so the UFE add commands have a stable parent,
// and leaves the edit target on the session layer -- the only layer the forwarder watches.
USDStageObject* MakeForwardingStageObject(pxr::SdfLayerRefPtr& testLayer)
{
    auto* stageObj = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
    GetCOREInterface()->CreateObjectNode(stageObj);
    // Builds the Forwarder for this stage (watches the session layer).
    stageObj->CreateInMemoryStage();
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
    EXPECT_TRUE(stage);
    if (!stage) {
        return nullptr;
    }

    // Target sublayer whose identifier ("...:TEST_layer") matches the rule's regex (.*TEST.*).
    testLayer = pxr::SdfLayer::CreateAnonymous("TEST_layer");
    EXPECT_TRUE(testLayer);
    stage->GetRootLayer()->InsertSubLayerPath(testLayer->GetIdentifier());

    // Continuous rule: forward every session-layer edit (input .*) into the TEST layer.
    auto rule = std::make_shared<ef::RuleDef>();
    rule->id = "test_rule";
    rule->inputObjectExpression = ef::RuleExpression(".*");
    rule->targetLayerExpression = ef::RuleExpression(".*TEST.*");
    const ef::RuleDef::RuleSet ruleSet({ rule }, /*continuous=*/true);
    ef::RuleDef::WriteRuleSetToLayerCustomData(stage->GetRootLayer(), ruleSet);

    // Parent prim, authored on the root layer so it composes (gives UFE adds a stable parent).
    {
        pxr::UsdEditContext ctx(stage, stage->GetRootLayer());
        stage->DefinePrim(pxr::SdfPath("/Geo"), pxr::TfToken("Xform"));
    }

    // The forwarder only watches the session layer, so author edits there.
    stage->SetEditTarget(stage->GetSessionLayer());
    return stageObj;
}

// Add a child Xform under /Geo via the UFE command system, so the command mgr opens an
// edit-forward capture scope and attaches the resulting forward to its Max undo entry.
// Returns the new prim's path.
pxr::SdfPath AddChildViaUfe(USDStageObject* stageObj, const std::string& name)
{
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
    const pxr::UsdPrim        parent = stage->GetPrimAtPath(pxr::SdfPath("/Geo"));
    EXPECT_TRUE(parent.IsValid());
    const Ufe::Path parentUfe = MaxUsd::ufe::getUsdPrimUfePath(stageObj, pxr::SdfPath("/Geo"));
    const auto      parentItem = UsdUfe::UsdSceneItem::create(parentUfe, parent);
    EXPECT_TRUE(parentItem);
    const auto cmd = UsdUfe::UsdUndoAddNewPrimCommand::create(parentItem, name, "Xform");
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
    return cmd->newPrim().GetPath();
}

} // namespace

// A single undo must revert BOTH the forward and the original session-layer edit (they share
// one undo entry via the command capture scope): the prim ends up nowhere.
TEST(AdskUsdEditForward, UndoRevertsForwardAndSourceTogether)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    pxr::SdfLayerRefPtr testLayer;
    USDStageObject*     stageObj = MakeForwardingStageObject(testLayer);
    ASSERT_NE(stageObj, nullptr);
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();

    const pxr::SdfPath primPath = AddChildViaUfe(stageObj, "Xform1");
    ASSERT_FALSE(primPath.IsEmpty());
    PumpIdle();

    // Forwarded: in the TEST layer, not the session layer.
    EXPECT_TRUE(testLayer->GetPrimAtPath(primPath)) << "edit should be forwarded to the TEST layer";
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(primPath));

    MaxUndo();
    PumpIdle();

    EXPECT_FALSE(testLayer->GetPrimAtPath(primPath))
        << "undo must remove the forwarded edit from the TEST layer";
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(primPath))
        << "undo must also clear the original session-layer edit (one undo for both)";
}

// Redo after undo must restore the forwarded state: prim back in the TEST layer, still gone
// from the session layer.
TEST(AdskUsdEditForward, RedoRestoresForward)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    pxr::SdfLayerRefPtr testLayer;
    USDStageObject*     stageObj = MakeForwardingStageObject(testLayer);
    ASSERT_NE(stageObj, nullptr);
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();

    const pxr::SdfPath primPath = AddChildViaUfe(stageObj, "Xform1");
    ASSERT_FALSE(primPath.IsEmpty());
    PumpIdle();

    MaxUndo();
    PumpIdle();
    ASSERT_FALSE(testLayer->GetPrimAtPath(primPath)) << "precondition: undo cleared the TEST layer";

    MaxRedo();
    PumpIdle();

    EXPECT_TRUE(testLayer->GetPrimAtPath(primPath)) << "redo must restore the forwarded edit";
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(primPath))
        << "redo must restore the edit to the TEST layer, not the session layer";
}

// An edit authored on the session layer outside a command is not forwarded: it stays on the
// session layer. Only edits made by a UFE command (or a marked interactive transform) are
// forwarded.
TEST(AdskUsdEditForward, EditOutsideCommandStaysInSession)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    pxr::SdfLayerRefPtr testLayer;
    USDStageObject*     stageObj = MakeForwardingStageObject(testLayer);
    ASSERT_NE(stageObj, nullptr);
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();

    const pxr::SdfPath primPath("/Geo/Loose");
    // A raw pxr-API edit, not driven by a UFE command.
    stage->DefinePrim(primPath, pxr::TfToken("Xform"));

    PumpIdle();

    EXPECT_TRUE(stage->GetSessionLayer()->GetPrimAtPath(primPath))
        << "an edit made outside a command must stay on the session layer";
    EXPECT_FALSE(testLayer->GetPrimAtPath(primPath))
        << "an edit made outside a command must not be forwarded";
}
