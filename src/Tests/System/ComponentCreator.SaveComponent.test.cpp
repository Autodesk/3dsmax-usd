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
#include <MaxUsdObjects/ComponentCreator/UtilComponentCreator.h>
#include <MaxUsdObjects/LayerEditor/USDLayerManager.h>
#include <MaxUsdObjects/Objects/MaxUsdNotify.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Interfaces/IUSDStageProvider.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/stage.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <GetCOREInterface.h>
#include <algorithm>
#include <filesystem>
#include <hold.h>
#include <inode.h>
#include <notify.h>
#include <string>

using namespace CCTest;

// The dialog-free save core relocates a freshly-created (temp) component to a chosen folder and
// repoints its USDStageObject at the new root layer.
TEST(ComponentCreatorSave, MoveComponentForSaveRelocatesAndRepoints)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"CCSaveCube");
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    ASSERT_TRUE(MaxUsd::ComponentCreator::IsUnsavedAdskUsdComponent(stage))
        << "freshly created component should be unsaved (root in temp)";

    namespace fs = std::filesystem;
    const std::string oldRoot
        = fs::path(stage->GetRootLayer()->GetRealPath()).make_preferred().string();

    const std::string name = "CCSavedComp";
    const fs::path    dest = fs::temp_directory_path() / "cc_save_test_out";
    std::error_code   ec;
    fs::remove_all(dest / name, ec); // ensure an empty, conflict-free target
    fs::create_directories(dest, ec);

    const std::string newRoot
        = MaxUsd::ComponentCreator::MoveComponentForSave(stage, dest.string(), name);

    ASSERT_FALSE(newRoot.empty()) << "MoveComponentForSave returned empty (move failed)";
    EXPECT_TRUE(fs::exists(newRoot)) << "new root layer not on disk: " << newRoot;
    EXPECT_NE(newRoot.find(name), std::string::npos);

    // The stage object was repointed at the relocated component.
    const pxr::UsdStageRefPtr relocated = comp->GetUSDStage();
    ASSERT_TRUE(relocated);
    const std::string relocatedRoot
        = fs::path(relocated->GetRootLayer()->GetRealPath()).make_preferred().string();
    EXPECT_EQ(relocatedRoot, fs::path(newRoot).make_preferred().string())
        << "stage not repointed to the moved root layer";
    EXPECT_NE(relocatedRoot, oldRoot) << "stage still points at the original temp root";

    fs::remove_all(dest, ec);
}

// The variant target lives in the stage's session layer, and the first save relocates the
// component and reloads its stage (new, empty session layer). The target must survive that.
TEST(ComponentCreatorSave, VariantTargetSurvivesMoveComponentForSave)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"CCTargetCube");
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);

    const auto desc
        = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto target = AdskUsdComponentCreator::ComponentAPI::GetVariantTarget(desc);
    ASSERT_FALSE(target.empty()) << "the created component has no variant target to preserve";
    const auto targetSelections = AdskUsdComponentCreator::GetVariantSelectionsFromNesting(target);

    namespace fs = std::filesystem;
    const std::string name = "CCTargetSavedComp";
    const fs::path    dest = fs::temp_directory_path() / "cc_target_test_out";
    std::error_code   ec;
    fs::remove_all(dest / name, ec);
    fs::create_directories(dest, ec);
    const auto destCleanup = MaxUsd::MakeScopeGuard([]() {}, [&dest]() {
        std::error_code cleanupEc;
        fs::remove_all(dest, cleanupEc);
    });

    ASSERT_FALSE(
        MaxUsd::ComponentCreator::MoveComponentForSave(stage, dest.string(), name).empty());

    const pxr::UsdStageRefPtr relocated = comp->GetUSDStage();
    ASSERT_TRUE(relocated);
    const auto relocatedDesc
        = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(relocated);
    ASSERT_TRUE(relocatedDesc);
    const auto nesting = relocatedDesc->GetVariantNestingFromSelections(targetSelections);
    ASSERT_FALSE(nesting.empty()) << "targeted variant missing from the relocated component";
    EXPECT_TRUE(AdskUsdComponentCreator::ComponentAPI::IsVariantTarget(relocatedDesc, nesting))
        << "variant target lost when the component was saved";
}

// Scene-save no longer partitions components out: an unsaved (temp) component appears in the set
// of stages offered to the batch save dialog.
TEST(ComponentCreatorSave, SceneSaveIncludesUnsavedComponents)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"CCCollectCube");
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    ASSERT_TRUE(MaxUsd::ComponentCreator::IsUnsavedAdskUsdComponent(stage));

    const auto infos = USDLayerManager::Instance()->CollectStagesToSave();
    const bool found
        = std::any_of(infos.begin(), infos.end(), [&](const UsdLayerEditor::StageSavingInfo& i) {
              return i.stage == stage;
          });
    EXPECT_TRUE(found) << "unsaved component missing from CollectStagesToSave()";
}

// Creating a component, saving it, then closing and reopening its Explorer tab, and finally
// undoing and redoing the resulting selection change, used to crash in
// NodeSelectionSet::SelectMultiple.
TEST(ComponentCreatorSave, ExplorerTabToggleThenUndoRedoAll)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    GetCOREInterface()->FlushUndoBuffer();

    theHold.Begin();
    INode* cube = MakeCubeNode(L"CCReopenCube");
    ASSERT_NE(cube, nullptr);
    theHold.Accept(L"Create Cube");

    const std::size_t stageNodesBefore = SnapshotStageObjectNodes().size();
    MaxUsd::ComponentCreator::CreateComponentFromNodes(
        { cube }, AdskUsdComponentCreator::Options {});
    ASSERT_EQ(SnapshotStageObjectNodes().size(), stageNodesBefore + 1);

    USDStageObject* comp = nullptr;
    for (INode* n : SnapshotStageObjectNodes()) {
        comp = StageObjectOf(n);
    }
    ASSERT_NE(comp, nullptr);
    ASSERT_TRUE(comp->GetUSDStage());

    namespace fs = std::filesystem;
    const std::string name = "CCReopenSavedComp";
    const fs::path    dest = fs::temp_directory_path() / "cc_reopen_test_out";
    std::error_code   ec;
    fs::remove_all(dest / name, ec);
    fs::create_directories(dest, ec);
    const auto destCleanup = MaxUsd::MakeScopeGuard([]() {}, [&dest]() {
        std::error_code cleanupEc;
        fs::remove_all(dest, cleanupEc);
    });

    ASSERT_FALSE(
        MaxUsd::ComponentCreator::MoveComponentForSave(comp->GetUSDStage(), dest.string(), name)
            .empty());

    // Close the stage's Explorer tab, then reopen it.
    comp->CloseInUsdExplorer();
    comp->OpenInUsdExplorer();

    // Reopening the tab from the UI goes through selecting the stage node, and 3ds Max records
    // selection changes as their own undo entry.
    INode* compNode = nullptr;
    for (INode* n : SnapshotStageObjectNodes()) {
        compNode = n;
    }
    ASSERT_NE(compNode, nullptr);
    theHold.Begin();
    GetCOREInterface()->SelectNode(compNode);
    theHold.Accept(L"Select");

    // The save above flushed the undo buffer, so the select is the only thing left to undo/redo;
    // this must not crash (the historical NodeSelectionSet::SelectMultiple regression).
    MaxUndo();
    MaxRedo();

    EXPECT_NE(GetCOREInterface()->GetINodeByName(L"CCReopenCube"), nullptr);
    EXPECT_EQ(SnapshotStageObjectNodes().size(), stageNodesBefore + 1);
}

TEST(ComponentCreatorSave, InitialComponentSaveFlushesUndoStack)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    GetCOREInterface()->FlushUndoBuffer();

    const std::set<INode*> before = SnapshotStageObjectNodes();
    USDStageObject*        comp = CreateComponentFromCube(L"CCNoUndoSaveCube");
    ASSERT_NE(comp, nullptr);
    ASSERT_TRUE(comp->GetUSDStage());

    namespace fs = std::filesystem;
    const std::string name = "CCNoUndoSaveComp";
    const fs::path    dest = fs::temp_directory_path() / "cc_no_undo_save_test_out";
    std::error_code   ec;
    fs::remove_all(dest / name, ec);
    fs::create_directories(dest, ec);
    const auto destCleanup = MaxUsd::MakeScopeGuard([]() {}, [&dest]() {
        std::error_code cleanupEc;
        fs::remove_all(dest, cleanupEc);
    });

    const std::string newRoot
        = MaxUsd::ComponentCreator::MoveComponentForSave(comp->GetUSDStage(), dest.string(), name);
    ASSERT_FALSE(newRoot.empty());
    ASSERT_EQ(
        fs::path(comp->GetUSDStage()->GetRootLayer()->GetRealPath()).make_preferred().string(),
        fs::path(newRoot).make_preferred().string())
        << "the save did not repoint the stage at the saved location";

    // Saving flushed the undo buffer, so this has nothing to undo.
    MaxUndo();

    INode* componentNode = FindNewStageObjectNode(before);
    ASSERT_NE(componentNode, nullptr) << "undo removed the component";
    USDStageObject* stageObj = StageObjectOf(componentNode);
    ASSERT_NE(stageObj, nullptr);
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
    ASSERT_TRUE(stage && stage->GetRootLayer());
    EXPECT_EQ(
        fs::path(stage->GetRootLayer()->GetRealPath()).make_preferred().string(),
        fs::path(newRoot).make_preferred().string())
        << "undo should be a no-op after save; the component moved from its saved location";
}
