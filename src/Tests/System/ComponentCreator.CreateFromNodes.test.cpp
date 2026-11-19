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

#include <MaxUsdObjects/ComponentCreator/Commands/CreateComponentFromNodesCmd.h>
#include <MaxUsdObjects/ComponentCreator/MaxVariantEditor.h>
#include <MaxUsdObjects/ComponentCreator/UtilComponentCreator.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ScopeGuard.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>

#include <AdskUsdComponentCreator/Commands/AddVariantSetCommand.h>
#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <cstddef>
#include <set>
#include <units.h>

using namespace CCTest;

// Creating a component from a node produces a USD Stage Object whose stage is a
// valid component, with the source geometry under /root/geo and at least one
// variant set.
TEST(ComponentCreatorCreateFromNodes, GeometryInStageAndHasVariantSet)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();

    INode* cube = MakeCubeNode(L"CCTracerCube");
    MaxUsd::ComponentCreator::CreateComponentFromNodes(
        { cube }, AdskUsdComponentCreator::Options {});

    INode* newNode = FindNewStageObjectNode(before);
    ASSERT_NE(newNode, nullptr) << "create-from-nodes did not create a USD Stage Object";

    auto* stageObj = StageObjectOf(newNode);
    ASSERT_NE(stageObj, nullptr);
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
    ASSERT_TRUE(stage);

    EXPECT_TRUE(MaxUsd::ComponentCreator::IsAdskUsdComponent(stage))
        << "created stage is not a valid Adsk USD component";

    // The exported geometry should land FLAT at /root/geo/<nodeName>
    // -- not nested beneath an extra export-root Xform (/root/geo/root/...).
    // The component node-export must use the absolute-root export prim path so the
    // node prims become direct children of /root/geo.
    const pxr::SdfPath geoScopePath("/root/geo");
    ASSERT_TRUE(stage->GetPrimAtPath(geoScopePath).IsValid())
        << "/root/geo scope missing from component stage";

    const pxr::UsdPrim cubePrim = stage->GetPrimAtPath(pxr::SdfPath("/root/geo/CCTracerCube"));
    if (!cubePrim.IsValid() || cubePrim.GetTypeName() != pxr::TfToken("Mesh")) {
        std::string dump;
        for (const auto& p : stage->Traverse()) {
            dump += p.GetPath().GetString() + " (" + p.GetTypeName().GetString() + ")\n";
        }
        ADD_FAILURE() << "Expected Mesh at /root/geo/CCTracerCube (flat). Full stage:\n" << dump;
    }
    EXPECT_FALSE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/root")).IsValid())
        << "geometry must not be nested under an extra /root/geo/root export wrapper";

    // The resulting component should have at least one variant set.
    const auto desc = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc) << "could not build ComponentDescription from stage metadata";
    EXPECT_FALSE(desc->GetVariantSets().empty()) << "component has no variant sets";

    // The create is undoable: undo removes the component, redo recreates it with its geometry.
    MaxUndo();
    EXPECT_EQ(FindNewStageObjectNode(before), nullptr)
        << "undo should remove the created component";
    MaxRedo();
    USDStageObject* redone = StageObjectOf(FindNewStageObjectNode(before));
    ASSERT_NE(redone, nullptr) << "redo should recreate the component";
    EXPECT_TRUE(Valid(redone->GetUSDStage(), "/root/geo/CCTracerCube"))
        << "geometry should be present again after redo";
}

// An empty node list must create nothing.
TEST(ComponentCreatorCreateFromNodes, EmptyNodesIsNoOp)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();
    MaxUsd::ComponentCreator::CreateComponentFromNodes({}, AdskUsdComponentCreator::Options {});
    EXPECT_EQ(SnapshotStageObjectNodes(), before)
        << "empty node list must not create a USD Stage Object";
}

// After creation the first variant set has a non-empty default variant, and that
// default is the targeted variant.
TEST(ComponentCreatorCreateFromNodes, DefaultAndTargetVariantIsSet)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();
    INode*                 cube = MakeCubeNode(L"CCDefaultVarCube");
    MaxUsd::ComponentCreator::CreateComponentFromNodes(
        { cube }, AdskUsdComponentCreator::Options {});

    INode* newNode = FindNewStageObjectNode(before);
    ASSERT_NE(newNode, nullptr);
    auto* stageObj = StageObjectOf(newNode);
    ASSERT_NE(stageObj, nullptr);
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
    ASSERT_TRUE(stage);

    const auto desc = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    auto& vsets = desc->GetVariantSets();
    ASSERT_FALSE(vsets.empty());
    const auto& vsDesc = vsets.begin()->second;

    const std::string defaultVariant = vsDesc->GetDefaultVariant();
    EXPECT_FALSE(defaultVariant.empty()) << "variant set has no default variant";

    const auto variantDesc = vsDesc->GetVariantDescription(defaultVariant);
    ASSERT_TRUE(variantDesc) << "no descriptor for the default variant";

    const AdskUsdComponentCreator::VariantNesting nesting { vsDesc, variantDesc };
    EXPECT_TRUE(AdskUsdComponentCreator::ComponentAPI::IsVariantTarget(desc, nesting))
        << "default variant must be the targeted variant after create-from-nodes";

    // The create is undoable: undo removes the component, redo recreates it with the default
    // variant targeted again.
    MaxUndo();
    EXPECT_EQ(FindNewStageObjectNode(before), nullptr)
        << "undo should remove the created component";
    MaxRedo();
    USDStageObject* redone = StageObjectOf(FindNewStageObjectNode(before));
    ASSERT_NE(redone, nullptr) << "redo should recreate the component";
    const auto redoneDesc = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(
        redone->GetUSDStage());
    ASSERT_TRUE(redoneDesc);
    ASSERT_FALSE(redoneDesc->GetVariantSets().empty());
    const auto& redoneVs = redoneDesc->GetVariantSets().begin()->second;
    const auto  redoneVar = redoneVs->GetVariantDescription(redoneVs->GetDefaultVariant());
    ASSERT_TRUE(redoneVar);
    EXPECT_TRUE(AdskUsdComponentCreator::ComponentAPI::IsVariantTarget(
        redoneDesc, AdskUsdComponentCreator::VariantNesting { redoneVs, redoneVar }))
        << "default variant should still be the targeted variant after redo";
}

// Regression: create's undo/redo must reuse the same component (same temp folder / layer
// identifiers) rather than rebuilding at a fresh temp folder, so a later variant edit still targets
// live layers. Create, add a variant set, undo twice (variant then create), redo twice; the variant
// set must return on the redone component. Before the fix, redo of create rebuilt a brand-new
// component and the variant-set redo was stranded on the old (deleted) layers.
TEST(ComponentCreatorCreateFromNodes, VariantEditSurvivesCreateUndoRedo)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();
    INode*                 cube = MakeCubeNode(L"CCUndoRedoCube");
    MaxUsd::ComponentCreator::CreateComponentFromNodes(
        { cube }, AdskUsdComponentCreator::Options {});

    USDStageObject* comp = StageObjectOf(FindNewStageObjectNode(before));
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);

    auto desc = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const std::size_t baseVsCount = desc->GetVariantSets().size();

    // Add a variant set -- its own undoable step on the Max undo stack, after the create.
    auto addCmd = AdskUsdComponentCreator::AddVariantSetCommand::ExecuteWithUndo(
        desc, {}, "UndoRedoVarSet");
    ASSERT_TRUE(addCmd);
    ASSERT_TRUE(addCmd->IsSuccessful());

    // Undo the variant, then the create.
    MaxUndo();
    MaxUndo();
    EXPECT_EQ(FindNewStageObjectNode(before), nullptr) << "create undo should remove the component";

    // Redo the create, then the variant.
    MaxRedo();
    MaxRedo();

    USDStageObject* redone = StageObjectOf(FindNewStageObjectNode(before));
    ASSERT_NE(redone, nullptr) << "redo should bring the component back";
    const auto redoneDesc = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(
        redone->GetUSDStage());
    ASSERT_TRUE(redoneDesc);
    EXPECT_EQ(redoneDesc->GetVariantSets().size(), baseVsCount + 1)
        << "the added variant set must be restored after the create undo/redo round-trip\n"
        << DumpStage(redone->GetUSDStage());
}

// The Variant Manager must clear its displayed component when that component's stage is deleted
// from the scene -- including when the create is undone (which deletes the node). Otherwise it
// keeps showing a component that no longer exists.
TEST(ComponentCreatorCreateFromNodes, VariantEditorClearsWhenComponentDeleted)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();
    INode*                 cube = MakeCubeNode(L"CCEditorClearCube");
    MaxUsd::ComponentCreator::CreateComponentFromNodes(
        { cube }, AdskUsdComponentCreator::Options {});
    ASSERT_NE(FindNewStageObjectNode(before), nullptr);

    // Creating a component opens the Variant Manager on it.
    ASSERT_TRUE(MaxVariantEditor::IsCreated());
    ASSERT_TRUE(MaxVariantEditor::Instance()->GetCurrentStage())
        << "the editor should be showing the newly created component";

    // Undo the create -> deletes the component node -> the editor must drop the stale component.
    MaxUndo();
    EXPECT_FALSE(MaxVariantEditor::Instance()->GetCurrentStage())
        << "the variant editor must clear when its component's stage is deleted";
}

// A component is authored into layers of its own rather than copied wholesale from the exported
// stage, so the scene's units and up axis have to be carried across. Without them the component
// composes against USD's defaults and reads at the wrong scale in any scene that is not authored
// in centimetres.
TEST(ComponentCreatorCreateFromNodes, ComponentKeepsSceneUnitsAndUpAxis)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();
    INode*                 cube = MakeCubeNode(L"CCUnitsCube");
    MaxUsd::ComponentCreator::CreateComponentFromNodes(
        { cube }, AdskUsdComponentCreator::Options {});

    INode* newNode = FindNewStageObjectNode(before);
    ASSERT_NE(newNode, nullptr);
    auto* stageObj = StageObjectOf(newNode);
    ASSERT_NE(stageObj, nullptr);
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
    ASSERT_TRUE(stage);

    EXPECT_NEAR(pxr::UsdGeomGetStageMetersPerUnit(stage), GetSystemUnitScale(UNITS_METERS), 1e-6)
        << "the component does not carry the 3ds Max scene's unit scale";
    EXPECT_EQ(pxr::UsdGeomGetStageUpAxis(stage), pxr::UsdGeomTokens->z)
        << "the component does not carry the exported up axis";
}
