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

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ScopeGuard.h>

#include <usdUfe/ufe/UsdUndoDeleteCommand.h>
#include <usdUfe/undo/UsdUndoManager.h>

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include <maxscript/maxscript.h>
#include <ufe/undoableCommand.h>
#include <ufe/undoableCommandMgr.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <hold.h>
#include <string>

using namespace CCTest;
namespace cc = AdskUsdComponentCreator;

namespace {

void DeletePrim(const pxr::UsdStageRefPtr& stage, const pxr::SdfPath& path)
{
    // Track the edit-target layer for undo (as MaxUsdEditCommand does for app UFE edits):
    // UsdUndoDeleteCommand does not track layer states itself, so without this its undo
    // would capture nothing.
    UsdUfe::UsdUndoManager::instance().trackLayerStates(stage->GetEditTarget().GetLayer());
    Ufe::UndoableCommandMgr::instance().executeCmd(
        UsdUfe::UsdUndoDeleteCommand::create(stage->GetPrimAtPath(path)));
}

} // namespace

TEST(ComponentCreatorDeleteInComponent, DeleteLeafPrim)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    ASSERT_TRUE(stage->DefinePrim(pxr::SdfPath("/root/geo/TestPrim"), pxr::TfToken("Xform")));

    DeletePrim(stage, pxr::SdfPath("/root/geo/TestPrim"));
    EXPECT_FALSE(Valid(stage, "/root/geo/TestPrim"));

    MaxUndo();
    EXPECT_TRUE(Valid(stage, "/root/geo/TestPrim"));

    MaxRedo();
    EXPECT_FALSE(Valid(stage, "/root/geo/TestPrim"));
}

// Delete a hierarchy (behavior-only). Instrumented with a layer dump on undo failure
// to diagnose the subtree-undo behavior.
TEST(ComponentCreatorDeleteInComponent, DeleteHierarchy)
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
    ASSERT_TRUE(Valid(stage, "/root/geo/Parent/Child2"));
    const std::string afterDefine = DumpStage(stage);

    DeletePrim(stage, pxr::SdfPath("/root/geo/Parent"));
    const std::string afterDelete = DumpStage(stage);
    EXPECT_FALSE(Valid(stage, "/root/geo/Parent"));
    EXPECT_FALSE(Valid(stage, "/root/geo/Parent/Child1"));
    EXPECT_FALSE(Valid(stage, "/root/geo/Parent/Child2"));

    MaxUndo();
    const std::string afterUndo = DumpStage(stage);
    EXPECT_TRUE(
        Valid(stage, "/root/geo/Parent") && Valid(stage, "/root/geo/Parent/Child1")
        && Valid(stage, "/root/geo/Parent/Child2"))
        << "undo did not restore the hierarchy.\n=== after define ===\n"
        << afterDefine << "=== after delete ===\n"
        << afterDelete << "=== after undo ===\n"
        << afterUndo;

    MaxRedo();
    EXPECT_FALSE(Valid(stage, "/root/geo/Parent"));
}

// Deletes several distinct subtrees in one composite command (behavior-only) and
// undoes/redoes them as a single step.
TEST(ComponentCreatorDeleteInComponent, DeleteMultiplePrims)
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
    stage->DefinePrim(pxr::SdfPath("/root/geo/Sibling"), pxr::TfToken("Cube"));

    // Delete Child1, Parent and Sibling as ONE operation (a UFE composite command),
    // matching how a multi-selection delete is a single undo unit. Delete the child
    // before its parent so each target is still valid when removed.
    Ufe::UndoableCommandMgr::instance().executeCmd(Ufe::CompositeUndoableCommand::create(
        { UsdUfe::UsdUndoDeleteCommand::create(
              stage->GetPrimAtPath(pxr::SdfPath("/root/geo/Parent/Child1"))),
          UsdUfe::UsdUndoDeleteCommand::create(
              stage->GetPrimAtPath(pxr::SdfPath("/root/geo/Parent"))),
          UsdUfe::UsdUndoDeleteCommand::create(
              stage->GetPrimAtPath(pxr::SdfPath("/root/geo/Sibling"))) }));

    EXPECT_FALSE(Valid(stage, "/root/geo/Parent"));
    EXPECT_FALSE(Valid(stage, "/root/geo/Parent/Child1"));
    EXPECT_FALSE(Valid(stage, "/root/geo/Parent/Child2"));
    EXPECT_FALSE(Valid(stage, "/root/geo/Sibling"));

    MaxUndo();
    EXPECT_TRUE(Valid(stage, "/root/geo/Parent"));
    EXPECT_TRUE(Valid(stage, "/root/geo/Sibling"));

    MaxRedo();
    EXPECT_FALSE(Valid(stage, "/root/geo/Parent"));
}

TEST(ComponentCreatorDeleteInComponent, DeleteRestrictionComponentScopeItself)
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

    DeletePrim(stage, pxr::SdfPath("/root/geo"));

    EXPECT_TRUE(Valid(stage, "/root/geo"))
        << "deleting the component geo scope itself must be rejected";
}

TEST(ComponentCreatorDeleteInComponent, DeletePrimInsideVariant)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"DelVarCube");
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);

    // Locate the cube mesh prim created inside the component (lands flat under the geo
    // scope, e.g. /root/geo/DelVarCube). Found by name so the test is independent of
    // the exact geo-scope layout.
    pxr::SdfPath cubePath;
    for (const auto& p : stage->Traverse()) {
        if (p.GetName() == pxr::TfToken("DelVarCube")) {
            cubePath = p.GetPath();
            break;
        }
    }
    ASSERT_FALSE(cubePath.IsEmpty()) << "cube prim not found in component stage:\n"
                                     << DumpStage(stage);

    // Precondition: it is authored inside the variant, not the session layer -- this
    // is exactly the case the old restriction blocked.
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(cubePath))
        << "cube should be authored inside the variant, not the session layer";

    Ufe::UndoableCommandMgr::instance().executeCmd(
        UsdUfe::UsdUndoDeleteCommand::create(stage->GetPrimAtPath(cubePath)));
    EXPECT_FALSE(Valid(stage, cubePath.GetString().c_str()))
        << "a prim inside the component variant should be deletable";

    MaxUndo();
    EXPECT_TRUE(Valid(stage, cubePath.GetString().c_str())) << "undo should restore the prim";

    MaxRedo();
    EXPECT_FALSE(Valid(stage, cubePath.GetString().c_str()));
}

// Real multi-selection case: a component built from several cubes (all variant-
// resident), then deleting them all in one composite command, undone/redone as a
// single step. This is the user-facing multi-element delete -- distinct from
// DeleteMultiplePrims above, which uses session-only prims.
TEST(ComponentCreatorDeleteInComponent, DeleteMultipleVariantPrims)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCubes({ L"DelMultiA", L"DelMultiB" });
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);

    // Locate both cube mesh prims created inside the component's variant.
    pxr::SdfPath pathA, pathB;
    for (const auto& p : stage->Traverse()) {
        if (p.GetName() == pxr::TfToken("DelMultiA")) {
            pathA = p.GetPath();
        } else if (p.GetName() == pxr::TfToken("DelMultiB")) {
            pathB = p.GetPath();
        }
    }
    ASSERT_FALSE(pathA.IsEmpty()) << "DelMultiA not found:\n" << DumpStage(stage);
    ASSERT_FALSE(pathB.IsEmpty()) << "DelMultiB not found:\n" << DumpStage(stage);
    EXPECT_FALSE(stage->GetSessionLayer()->GetPrimAtPath(pathA))
        << "cubes should be authored inside the variant, not the session layer";

    // Delete both as ONE composite command (a single multi-selection undo unit),
    // exactly as the UI does.
    Ufe::UndoableCommandMgr::instance().executeCmd(Ufe::CompositeUndoableCommand::create(
        { UsdUfe::UsdUndoDeleteCommand::create(stage->GetPrimAtPath(pathA)),
          UsdUfe::UsdUndoDeleteCommand::create(stage->GetPrimAtPath(pathB)) }));
    EXPECT_FALSE(Valid(stage, pathA.GetString().c_str()));
    EXPECT_FALSE(Valid(stage, pathB.GetString().c_str()));

    // A single undo must restore both.
    MaxUndo();
    EXPECT_TRUE(Valid(stage, pathA.GetString().c_str()) && Valid(stage, pathB.GetString().c_str()))
        << "a single undo must restore the whole multi-selection delete";

    MaxRedo();
    EXPECT_FALSE(Valid(stage, pathA.GetString().c_str()));
    EXPECT_FALSE(Valid(stage, pathB.GetString().c_str()));
}
