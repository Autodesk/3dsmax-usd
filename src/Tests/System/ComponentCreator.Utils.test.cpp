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

#include <MaxUsdObjects/ComponentCreator/MaxComponentManager.h>
#include <MaxUsdObjects/ComponentCreator/UtilComponentCreator.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <UsdLayerEditor/layerLocking.h>

#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <GetCOREInterface.h>
#include <algorithm>
#include <filesystem>
#include <inode.h>

namespace fs = std::filesystem;
namespace cc = AdskUsdComponentCreator;
namespace ccutil = MaxUsd::ComponentCreator;

namespace {

// A plain (non-component) anonymous USD stage.
pxr::UsdStageRefPtr MakeNonComponentStage()
{
    auto layer = pxr::SdfLayer::CreateAnonymous(".usda");
    return pxr::UsdStage::Open(layer);
}

// A real component stage in `dir` via ComponentAPI::CreateFromNothing (raw stage, not a
// USDStageObject -- these tests exercise the library/manager directly).
pxr::UsdStageRefPtr MakeComponentStage(const fs::path& dir)
{
    // Unique per call: these tests remove their temp folder while their layers stay loaded, so a
    // later test handed the same folder name would resolve to the previous one's layers.
    static int        callCount = 0;
    const std::string name = "TestComp" + std::to_string(++callCount);

    cc::Options opts;
    opts.SetComponentFolder(dir.string());
    opts.SetComponentName(name);
    opts.SetComponentFilename(name);
    opts.SetFileExtension("usda");

    auto info = cc::ComponentAPI::CreateFromNothing(opts);
    if (!info) {
        return nullptr;
    }
    info->OpenAll();
    return info->GetStage();
}

// A USDStageObject scene node backed by a plain (non-component) USD stage from test data.
// A plain stage is not a component, so no component temp folder is involved.
INode* MakePlainStageObjectNode()
{
    const auto             filePath = GetTestDataPath().append("box_sample.usda");
    const std::set<INode*> before = CCTest::SnapshotStageObjectNodes();
    auto*                  stageObj = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
    GetCOREInterface()->CreateObjectNode(stageObj);
    stageObj->SetRootLayer(filePath.generic_wstring().c_str(), L"/");
    return CCTest::FindNewStageObjectNode(before);
}

// Force a live, callback-registered manager so scene-node-add notifications auto-track
// components (CreateForTesting-based tests may have left a callback-free instance).
void EnsureLiveManager()
{
    MaxComponentManager::Shutdown();
    MaxComponentManager::Instance();
}

} // namespace

// ---------------------------------------------------------------------------
// UtilComponentCreator helpers (pure -- no singleton, no Max scene state).
// ---------------------------------------------------------------------------

TEST(IsAdskUsdComponent, ReturnsFalseForNullStage)
{
    EXPECT_FALSE(ccutil::IsAdskUsdComponent(nullptr));
}

TEST(IsAdskUsdComponent, ReturnsFalseForPlainStage)
{
    EXPECT_FALSE(ccutil::IsAdskUsdComponent(MakeNonComponentStage()));
}

TEST(IsAdskUsdComponent, ReturnsTrueForComponentStage)
{
    auto tmpDir = ccutil::AllocateComponentTempFolder();
    auto stage = MakeComponentStage(tmpDir);
    ASSERT_NE(stage, nullptr);
    EXPECT_TRUE(ccutil::IsAdskUsdComponent(stage));
    ccutil::RemoveComponentTempFolder(tmpDir);
}

TEST(IsUnsavedAdskUsdComponent, ReturnsTrueForUnsavedComponent)
{
    auto tmpDir = ccutil::AllocateComponentTempFolder();
    auto stage = MakeComponentStage(tmpDir);
    ASSERT_NE(stage, nullptr);
    EXPECT_TRUE(ccutil::IsUnsavedAdskUsdComponent(stage));
    ccutil::RemoveComponentTempFolder(tmpDir);
}

TEST(GetComponentLayersToSave, ReturnsEmptyForNull)
{
    EXPECT_TRUE(ccutil::GetComponentLayersToSave(nullptr).empty());
}

TEST(GetComponentLayersToSave, ReturnsEmptyForNonComponentStage)
{
    EXPECT_TRUE(ccutil::GetComponentLayersToSave(MakeNonComponentStage()).empty());
}

TEST(GetComponentLayersToSave, ReturnsNonEmptyForComponentStage)
{
    // A freshly created component has dirty (unsaved) layers, so the set of layers a
    // save would touch is non-empty.
    auto tmpDir = ccutil::AllocateComponentTempFolder();
    auto stage = MakeComponentStage(tmpDir);
    ASSERT_NE(stage, nullptr);
    auto layers = ccutil::GetComponentLayersToSave(stage);
    EXPECT_FALSE(layers.empty());
    ccutil::RemoveComponentTempFolder(tmpDir);
}

TEST(AllocateComponentTempFolder, CreatesUniqueDirectories)
{
    auto a = ccutil::AllocateComponentTempFolder();
    auto b = ccutil::AllocateComponentTempFolder();
    EXPECT_NE(a, b);
    EXPECT_TRUE(fs::exists(a));
    EXPECT_TRUE(fs::exists(b));
    ccutil::RemoveComponentTempFolder(a);
    ccutil::RemoveComponentTempFolder(b);
}

TEST(RemoveComponentTempFolder, DeletesDirectory)
{
    auto dir = ccutil::AllocateComponentTempFolder();
    EXPECT_TRUE(fs::exists(dir));
    ccutil::RemoveComponentTempFolder(dir);
    EXPECT_FALSE(fs::exists(dir));
}

TEST(RemoveComponentTempFolder, NoOpOnNonExistentPath)
{
    EXPECT_NO_THROW(ccutil::RemoveComponentTempFolder(fs::path("nonexistent_xyz_12345")));
}

// When a parent and one of its descendants are both selected, only the parent is kept so the
// same geometry is not included twice.
TEST(RemoveDescendantINodes, DropsDescendantsKeepsTopLevel)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    INode* parent = CCTest::MakeCubeNode(L"Parent");
    INode* child = CCTest::MakeCubeNode(L"Child");
    parent->AttachChild(child);
    INode* other = CCTest::MakeCubeNode(L"Other");

    const auto out = MaxUsd::RemoveDescendantINodes({ parent, child, other });
    EXPECT_EQ(out.size(), 2u);
    EXPECT_NE(std::find(out.begin(), out.end(), parent), out.end());
    EXPECT_NE(std::find(out.begin(), out.end(), other), out.end());
    EXPECT_EQ(std::find(out.begin(), out.end(), child), out.end()) << "child must be dropped";
}

// Prim-path equivalent: descendants of another selected path are dropped.
TEST(RemoveDescendantPaths, DropsDescendantsKeepsTopLevel)
{
    const std::vector<pxr::SdfPath> in {
        pxr::SdfPath("/A"), pxr::SdfPath("/A/B"), pxr::SdfPath("/A/B/C"), pxr::SdfPath("/D")
    };
    const auto out = MaxUsd::RemoveDescendantPaths(in);
    EXPECT_EQ(out.size(), 2u);
    EXPECT_NE(std::find(out.begin(), out.end(), pxr::SdfPath("/A")), out.end());
    EXPECT_NE(std::find(out.begin(), out.end(), pxr::SdfPath("/D")), out.end());
}

// Creating a component locks its root layer AND registers it with the layer-editor lock registry
// (not just the raw permission bit), so the lock shows in the Layer Editor and survives
// save/reload.
TEST(MaxComponentManagerLock, CreatedComponentRootIsRegisteredLocked)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CCTest::CreateComponentFromCube(L"LockCube");
    ASSERT_NE(comp, nullptr);
    const auto root = comp->GetUSDStage()->GetRootLayer();
    ASSERT_TRUE(root);
    EXPECT_FALSE(root->PermissionToEdit());
    EXPECT_TRUE(UsdLayerEditor::isLayerLocked(root))
        << "component root must be in the layer-editor lock registry, not just permission-locked";
}

// ---------------------------------------------------------------------------
// MaxComponentManager (process-global singleton).
// ---------------------------------------------------------------------------

namespace {

// Installs a callback-free test manager for the duration of a test, then restores a
// live (callback-registered) singleton so the rest of the in-process suite and the app
// keep auto-tracking component stages.
struct TestManager
{
    MaxComponentManager* mgr;
    TestManager()
        : mgr(MaxComponentManager::CreateForTesting())
    {
    }
    ~TestManager()
    {
        MaxComponentManager::Shutdown();
        MaxComponentManager::Instance();
    }
};

} // namespace

// Adding a node fires the scene-add notification the live manager listens on. Only a
// component stage should be set up (tracked): its root layer ends up locked and registered
// with the layer-editor lock registry. The other two node kinds must be left untouched.

// 1) Non-USD node (a plain mesh): no stage, nothing to track.
TEST(MaxComponentManagerOnNodeAdded, NonUsdNodeIsNotTracked)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });
    EnsureLiveManager();

    INode* node = CCTest::MakeCubeNode(L"OnAddNonUsd");
    ASSERT_NE(node, nullptr);
    // A mesh node is not a USD stage object, so the manager has nothing to set up: it
    // never appears among the scene's stage-object nodes.
    EXPECT_EQ(CCTest::SnapshotStageObjectNodes().count(node), 0u);
}

// 2) USD stage node that is NOT a component: reaches the manager but must not be locked.
// A plain stage is not a component, so no component temp folder is created.
TEST(MaxComponentManagerOnNodeAdded, NonComponentUsdStageIsNotTracked)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });
    EnsureLiveManager();

    INode* node = MakePlainStageObjectNode();
    ASSERT_NE(node, nullptr);
    auto* obj = CCTest::StageObjectOf(node);
    ASSERT_NE(obj, nullptr);
    const auto root = obj->GetUSDStage()->GetRootLayer();
    ASSERT_TRUE(root);

    // TODO: assert directly on tracking once ComponentManager exposes it, e.g.
    // EXPECT_FALSE(MaxComponentManager::Instance()->IsComponentTracked(obj->GetUSDStage()));
    // Until then we use the lock side-effect as a proxy for "not set up as a component".
    EXPECT_FALSE(UsdLayerEditor::isLayerLocked(root))
        << "a non-component USD stage must not be set up as a component";
    EXPECT_TRUE(root->PermissionToEdit());
}

// 3) Component USD stage node: the manager tracks it -- root locked and registered. Creating
// the component also materializes it in its own temp folder, which we verify and clean up.
TEST(MaxComponentManagerOnNodeAdded, ComponentUsdStageIsTracked)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });
    EnsureLiveManager();

    USDStageObject* comp = CCTest::CreateComponentFromCube(L"OnAddComponent");
    ASSERT_NE(comp, nullptr);
    const auto root = comp->GetUSDStage()->GetRootLayer();
    ASSERT_TRUE(root);

    const fs::path componentFolder = fs::path(root->GetIdentifier()).parent_path();
    EXPECT_TRUE(fs::exists(componentFolder))
        << "a created component must be materialized in a temp folder: " << componentFolder;

    // TODO: assert directly on tracking once ComponentManager exposes it, e.g.
    // EXPECT_TRUE(MaxComponentManager::Instance()->IsComponentTracked(comp->GetUSDStage()));
    // Until then we use the lock side-effect as a proxy for "tracked as a component".
    EXPECT_TRUE(UsdLayerEditor::isLayerLocked(root))
        << "an added component stage must be registered-locked by the manager";
    EXPECT_FALSE(root->PermissionToEdit());

    ccutil::RemoveComponentTempFolder(componentFolder);
}

TEST(MaxComponentManager, AddComponentIsIdempotent)
{
    TestManager tm;
    auto        tmpDir = ccutil::AllocateComponentTempFolder();
    auto        stage = MakeComponentStage(tmpDir);
    ASSERT_NE(stage, nullptr);

    EXPECT_NO_THROW(tm.mgr->AddComponent(stage));
    EXPECT_NO_THROW(tm.mgr->AddComponent(stage));

    ccutil::RemoveComponentTempFolder(tmpDir);
}

TEST(MaxComponentManager, SaveComponentPersistsRootLayer)
{
    TestManager tm;
    auto        tmpDir = ccutil::AllocateComponentTempFolder();
    auto        stage = MakeComponentStage(tmpDir);
    ASSERT_NE(stage, nullptr);

    tm.mgr->AddComponent(stage);
    const auto root = stage->GetRootLayer();
    ASSERT_NE(root, nullptr);
    // Freshly created component is unsaved, so its root layer is dirty -- otherwise the save
    // below would be a no-op and this test would pass vacuously.
    ASSERT_TRUE(root->IsDirty()) << "a freshly created component's root layer should be dirty";

    EXPECT_NO_THROW(tm.mgr->SaveComponent(stage));

    // SaveComponent flushes the component's dirty layers to disk; SdfLayer::Save clears the
    // dirty bit synchronously (not via the notice-driven bookkeeping), so this is reliable here.
    EXPECT_FALSE(root->IsDirty()) << "root layer should be clean after SaveComponent";
    EXPECT_TRUE(fs::exists(root->GetRealPath()))
        << "component root layer should be written to disk after SaveComponent";

    ccutil::RemoveComponentTempFolder(tmpDir);
}

TEST(MaxComponentManager, RootLayerIsLockedAfterAdd)
{
    TestManager tm;
    auto        tmpDir = ccutil::AllocateComponentTempFolder();
    auto        stage = MakeComponentStage(tmpDir);
    ASSERT_NE(stage, nullptr);

    auto root = stage->GetRootLayer();
    ASSERT_NE(root, nullptr);
    root->SetPermissionToEdit(true);

    tm.mgr->AddComponent(stage);
    EXPECT_FALSE(root->PermissionToEdit());

    ccutil::RemoveComponentTempFolder(tmpDir);
}

TEST(MaxComponentManager, RemoveComponentUnlocksRootLayer)
{
    TestManager tm;
    auto        tmpDir = ccutil::AllocateComponentTempFolder();
    auto        stage = MakeComponentStage(tmpDir);
    ASSERT_NE(stage, nullptr);

    tm.mgr->AddComponent(stage);
    tm.mgr->RemoveComponent(stage);

    auto root = stage->GetRootLayer();
    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(root->PermissionToEdit());

    ccutil::RemoveComponentTempFolder(tmpDir);
}

// IncludedPathsForFlatExport must honour opts.IncludeMaterials(): the exported material scope is
// flattened to its individual material prims when materials are included, and dropped entirely
// when they are excluded. Geometry is unaffected either way. (The callers currently pass the
// default options, but the function must react to the option regardless of the caller.)
TEST(ComponentCreatorUtils, FlatExportPathsHonorIncludeMaterials)
{
    const MaxUsd::USDSceneBuilderOptions exportDefaults;
    const pxr::SdfPath                   mtlScope
        = pxr::SdfPath::AbsoluteRootPath().AppendPath(exportDefaults.GetMaterialPrimPath());

    const auto stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    stage->DefinePrim(pxr::SdfPath("/geoNode"), pxr::TfToken("Xform"));
    stage->DefinePrim(mtlScope, pxr::TfToken("Scope"));
    const pxr::SdfPath matPath = mtlScope.AppendChild(pxr::TfToken("Mat1"));
    stage->DefinePrim(matPath, pxr::TfToken("Material"));

    // Materials included (default): geometry + the individual material prim, but not the /mtl
    // scope itself (it is flattened away to avoid /root/mtl/mtl).
    cc::Options withMaterials;
    ASSERT_TRUE(withMaterials.IncludeMaterials());
    const MaxUsd::USDSceneBuilderOptions exportOptions;
    const auto withPaths
        = ccutil::IncludedPathsForFlatExport(stage, withMaterials, exportOptions);
    EXPECT_EQ(withPaths.count(pxr::SdfPath("/geoNode")), 1u);
    EXPECT_EQ(withPaths.count(matPath), 1u);
    EXPECT_EQ(withPaths.count(mtlScope), 0u);

    // Materials excluded: the whole /mtl scope is dropped; geometry is untouched.
    cc::Options noMaterials;
    noMaterials.SetIncludeMaterials(false);
    const auto noPaths = ccutil::IncludedPathsForFlatExport(stage, noMaterials, exportOptions);
    EXPECT_EQ(noPaths.count(pxr::SdfPath("/geoNode")), 1u);
    EXPECT_EQ(noPaths.count(matPath), 0u);
    EXPECT_EQ(noPaths.count(mtlScope), 0u);
}

// Deleting a component's stage-object node must NOT untrack/unlock the component. We removed
// the OnNodePreDeleted -> RemoveComponent hook: removing on delete would
// drop the manager's strong refs to the component's unsaved (dirty) in-mem layers, so a later
// undo could not restore them. The old hook unlocked the root layer on delete; now it stays
// locked. (Whether a subsequent undo fully restores the in-mem stage is a separate, known
// stage-object/temp-file concern and is not asserted here.)
TEST(MaxComponentManager, NodeDeletionDoesNotUntrackComponent)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = CCTest::SnapshotStageObjectNodes();
    USDStageObject*        comp = CCTest::CreateComponentFromCube(L"DeleteNoUntrackCube");
    ASSERT_NE(comp, nullptr);
    INode* node = CCTest::FindNewStageObjectNode(before);
    ASSERT_NE(node, nullptr);

    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    // Hold a strong ref so the root layer outlives the node deletion for inspection.
    const pxr::SdfLayerRefPtr root = stage->GetRootLayer();
    ASSERT_TRUE(root);
    ASSERT_FALSE(root->PermissionToEdit()) << "a tracked component's root layer should be locked";

    GetCOREInterface()->DeleteNode(node);

    EXPECT_FALSE(root->PermissionToEdit())
        << "deleting the stage-object node must not unlock/untrack the component";
}

// Reviewer's recipe: an unsaved edit made inside a targeted variant must survive deleting the
// component's stage-object node and undoing that delete. This is the end-to-end guarantee behind
// not untracking on delete (the manager keeps the dirty in-mem layers alive).
//
// TODO: DISABLED -- undoing a stage-object deletion does not currently restore the in-mem stage
// (the node is not brought back / the component is recreated from a fresh temp file), so the
// unsaved variant edit is lost on delete->undo. Not untracking on delete (this change) is the
// prerequisite -- the manager now keeps the dirty layers -- but the stage-object undo path still
// needs to restore the node and reconnect them. Re-enable once delete->undo of unsaved
// components is fixed.
TEST(MaxComponentManager, DISABLED_UnsavedVariantEditSurvivesStageObjectDeleteUndo)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = CCTest::SnapshotStageObjectNodes();
    USDStageObject*        comp = CCTest::CreateComponentFromCube(L"UndoEditCube");
    ASSERT_NE(comp, nullptr);
    INode* node = CCTest::FindNewStageObjectNode(before);
    ASSERT_NE(node, nullptr);

    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    CCTest::SetDefaultVariantTarget(desc);

    // Make an unsaved change inside the targeted variant (forwarded on idle). Do NOT save.
    stage->DefinePrim(pxr::SdfPath("/root/geo/UnsavedEdit"), pxr::TfToken("Xform"));
    CCTest::PumpIdle();
    ASSERT_TRUE(CCTest::Valid(stage, "/root/geo/UnsavedEdit"))
        << "precondition: the edit is present before deleting the stage object";

    // Delete the stage-object node, then undo the delete.
    GetCOREInterface()->DeleteNode(node);
    CCTest::MaxUndo();
    CCTest::PumpIdle();

    // Re-acquire the restored stage object and assert the unsaved edit is still there.
    INode* restored = CCTest::FindNewStageObjectNode(before);
    ASSERT_NE(restored, nullptr) << "undo should restore the stage-object node";
    USDStageObject* restoredComp = CCTest::StageObjectOf(restored);
    ASSERT_NE(restoredComp, nullptr);
    const pxr::UsdStageRefPtr restoredStage = restoredComp->GetUSDStage();
    ASSERT_TRUE(restoredStage);

    EXPECT_TRUE(CCTest::Valid(restoredStage, "/root/geo/UnsavedEdit"))
        << "the unsaved variant edit must survive delete + undo\n"
        << CCTest::DumpStage(restoredStage);
}

// The temp folder name comes from what is on disk, not from a session counter. A counter only
// moves forward, so it restarts at 1 on the next launch and re-issues numbers earlier components
// already used -- and the name is user-visible, it becomes the asset name the save dialog offers.
TEST(ComponentCreatorUtils, TempFolderNamingFollowsDiskState)
{
    const auto first = ccutil::AllocateComponentTempFolder();
    ASSERT_TRUE(fs::exists(first));

    // Same disk state must give the same answer.
    ccutil::RemoveComponentTempFolder(first);
    const auto again = ccutil::AllocateComponentTempFolder();
    EXPECT_EQ(again, first) << "allocating after freeing " << first.string() << " gave "
                            << again.string();

    // And a second, while the first is still there, must differ.
    const auto second = ccutil::AllocateComponentTempFolder();
    EXPECT_NE(second, again);
    EXPECT_TRUE(fs::exists(second));

    ccutil::RemoveComponentTempFolder(again);
    ccutil::RemoveComponentTempFolder(second);
}

// The Variant Manager shows variants for any stage, not just components: for a plain stage it
// follows the selected prim, falling back to the nearest ancestor that carries the variant sets.
TEST(ComponentCreatorUtils, FindPrimWithVariantSets)
{
    auto stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    auto parent = stage->DefinePrim(pxr::SdfPath("/root"), pxr::TfToken("Xform"));
    auto child = stage->DefinePrim(pxr::SdfPath("/root/geo"), pxr::TfToken("Xform"));
    auto unrelated = stage->DefinePrim(pxr::SdfPath("/other"), pxr::TfToken("Xform"));
    ASSERT_TRUE(parent && child && unrelated);

    // No variant sets anywhere yet.
    EXPECT_FALSE(ccutil::FindPrimWithVariantSets(stage, pxr::SdfPath("/root/geo")));

    auto variantSet = parent.GetVariantSets().AddVariantSet("model");
    ASSERT_TRUE(variantSet.AddVariant("default"));

    // The prim itself when it has the variant sets, its ancestor when the selection is deeper.
    EXPECT_EQ(ccutil::FindPrimWithVariantSets(stage, pxr::SdfPath("/root")), parent);
    EXPECT_EQ(ccutil::FindPrimWithVariantSets(stage, pxr::SdfPath("/root/geo")), parent);

    // Nothing on an unrelated branch, and nothing without a prim path.
    EXPECT_FALSE(ccutil::FindPrimWithVariantSets(stage, pxr::SdfPath("/other")));
    EXPECT_FALSE(ccutil::FindPrimWithVariantSets(stage, pxr::SdfPath()));
    EXPECT_FALSE(ccutil::FindPrimWithVariantSets(nullptr, pxr::SdfPath("/root")));
}
