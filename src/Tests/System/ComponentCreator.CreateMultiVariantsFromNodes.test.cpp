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
#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ScopeGuard.h>

#include <usdUfe/ufe/SetVariantSelectionCommand.h>

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/variantSets.h>

#include <ufe/path.h>
#include <ufe/undoableCommandMgr.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/ComponentFileNames.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <algorithm>
#include <inode.h>

using namespace CCTest;
namespace cc = AdskUsdComponentCreator;

TEST(ComponentCreatorCreateMultiVariantsFromNodes, EmptyNodesIsNoOp)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateMultiVariantsComponentFromCubes({});
    EXPECT_EQ(comp, nullptr) << "no component stage should be created for an empty node list";
}

// Two nodes -> exactly two variants in the first variant set.
TEST(ComponentCreatorCreateMultiVariantsFromNodes, VariantCount)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateMultiVariantsComponentFromCubes({ L"CubeA", L"CubeB" });
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);
    ASSERT_FALSE(desc->GetVariantSets().empty());

    const auto& firstVs = desc->GetVariantSets().begin()->second;
    EXPECT_EQ(firstVs->GetVariants().size(), 2u) << "two nodes must produce two variants";
}

// Every variant must actually contain its node's geometry, not just exist by name. Selecting
// each variant and loading payloads should expose a Mesh under /root/geo.
TEST(ComponentCreatorCreateMultiVariantsFromNodes, EachVariantHasGeometry)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateMultiVariantsComponentFromCubes({ L"CubeA", L"CubeB" });
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    ASSERT_FALSE(desc->GetVariantSets().empty());

    const std::string  setName = desc->GetVariantSets().begin()->first;
    const pxr::UsdPrim root = stage->GetPrimAtPath(pxr::SdfPath("/root"));
    ASSERT_TRUE(root.IsValid());
    pxr::UsdVariantSet             vset = root.GetVariantSet(setName);
    const std::vector<std::string> names = vset.GetVariantNames();
    ASSERT_EQ(names.size(), 2u);

    // Selecting each variant and loading payloads must expose a Mesh under /root/geo -- every
    // variant carries its own node's geometry, not just an entry in the variant set.
    for (const std::string& name : names) {
        vset.SetVariantSelection(name);
        stage->Load(pxr::SdfPath("/root"));
        int meshCount = 0;
        for (const auto& p : stage->Traverse()) {
            if (p.GetTypeName() == pxr::TfToken("Mesh")
                && p.GetPath().HasPrefix(pxr::SdfPath("/root/geo"))) {
                ++meshCount;
            }
        }
        EXPECT_GT(meshCount, 0) << "variant '" << name << "' has no Mesh under /root/geo";
    }
}

// A variant selection made through the UFE command manager
// must actually change the component's composed variant selection.
TEST(ComponentCreatorCreateMultiVariantsFromNodes, SetVariantSelectionViaCommandSticks)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateMultiVariantsComponentFromCubes({ L"CubeA", L"CubeB" });
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    ASSERT_FALSE(desc->GetVariantSets().empty());

    const std::string  setName = desc->GetVariantSets().begin()->first;
    const pxr::UsdPrim root = stage->GetDefaultPrim();
    ASSERT_TRUE(root.IsValid());
    pxr::UsdVariantSet             vset = root.GetVariantSet(setName);
    const std::vector<std::string> names = vset.GetVariantNames();
    ASSERT_EQ(names.size(), 2u);

    // Pick the variant that is NOT currently selected.
    const std::string current = vset.GetVariantSelection();
    const std::string target = (names[0] == current) ? names[1] : names[0];
    ASSERT_NE(target, current);

    // Drive the selection the same way the prim context menu does: a UsdUfe
    // SetVariantSelectionCommand executed through the UFE command manager.
    const Ufe::Path path = MaxUsd::ufe::getUsdPrimUfePath(comp, root.GetPath());
    const auto      cmd = UsdUfe::SetVariantSelectionCommand::create(path, root, setName, target);
    ASSERT_TRUE(cmd);
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
    CCTest::PumpIdle();

    EXPECT_EQ(root.GetVariantSet(setName).GetVariantSelection(), target)
        << "variant selection made via the command system must change the composed selection";
}

// A top-level node's child hierarchy must be included in that node's variant. The parent node
// is named so it is the alphabetically-first (hence default) variant, which composes without
// any variant switching: the composed component must then show both the parent's mesh and the
// child's under /root/geo.
TEST(ComponentCreatorCreateMultiVariantsFromNodes, VariantIncludesChildHierarchy)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    // "AParent" sorts before "ZOther", so the parent's variant is the default and composes.
    INode* parent = MakeCubeNode(L"AParent");
    INode* child = MakeCubeNode(L"Teapot");
    parent->AttachChild(child);
    INode* other = MakeCubeNode(L"ZOther");

    const std::set<INode*> before = SnapshotStageObjectNodes();
    MaxUsd::ComponentCreator::CreateMultiVariantsComponentFromNodes(
        { parent, other }, cc::Options {});
    USDStageObject* comp = StageObjectOf(FindNewStageObjectNode(before));
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    stage->Load(pxr::SdfPath("/root"));

    // The default (parent) variant is composed; it must contain the parent's mesh and the
    // child's (2 meshes under /root/geo).
    int meshCount = 0;
    for (const auto& p : stage->Traverse()) {
        if (p.GetTypeName() == pxr::TfToken("Mesh")
            && p.GetPath().HasPrefix(pxr::SdfPath("/root/geo"))) {
            ++meshCount;
        }
    }
    EXPECT_GE(meshCount, 2)
        << "the parent node's (default) variant must include its child hierarchy "
           "(expected 2 meshes under /root/geo)";
}

// After creation the default variant is targeted.
TEST(ComponentCreatorCreateMultiVariantsFromNodes, TargetedVariantIsDefault)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateMultiVariantsComponentFromCubes({ L"ZebraNode", L"AppleNode" });
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);
    ASSERT_FALSE(desc->GetVariantSets().empty());

    const auto&       firstVs = desc->GetVariantSets().begin()->second;
    const std::string defaultName = firstVs->GetDefaultVariant();
    ASSERT_FALSE(defaultName.empty());
    const auto defaultVar = firstVs->GetVariantDescription(defaultName);
    ASSERT_TRUE(defaultVar);
    EXPECT_TRUE(cc::ComponentAPI::IsVariantTarget(desc, cc::VariantNesting { firstVs, defaultVar }))
        << "the default variant must be targeted after a multi-variant create";
}

// Selecting a node and one of its descendants must collapse to a single (top-level) variant --
// the descendant is filtered out (mirrors the prim path's DescendantPrimIsFilteredOut).
TEST(ComponentCreatorCreateMultiVariantsFromNodes, DescendantNodeIsFilteredOut)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    INode* parent = MakeCubeNode(L"AParent");
    INode* child = MakeCubeNode(L"Child");
    parent->AttachChild(child);

    const std::set<INode*> before = SnapshotStageObjectNodes();
    MaxUsd::ComponentCreator::CreateMultiVariantsComponentFromNodes(
        { parent, child }, cc::Options {});
    USDStageObject* comp = StageObjectOf(FindNewStageObjectNode(before));
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);
    ASSERT_FALSE(desc->GetVariantSets().empty());
    EXPECT_EQ(desc->GetVariantSets().begin()->second->GetVariants().size(), 1u)
        << "the descendant node must be filtered out, leaving a single variant";
}

// With three nodes the alphabetically-first node's variant is the default.
TEST(ComponentCreatorCreateMultiVariantsFromNodes, DefaultIsAlphabeticallyFirst)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp
        = CreateMultiVariantsComponentFromCubes({ L"MangoNode", L"AvocadoNode", L"KiwiNode" });
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);
    ASSERT_FALSE(desc->GetVariantSets().empty());

    const auto& firstVs = desc->GetVariantSets().begin()->second;
    EXPECT_EQ(firstVs->GetVariants().size(), 3u) << "three nodes must produce three variants";

    // The default should be the variant generated from the alphabetically-first node name.
    cc::Options opts;
    opts.Validate();
    const std::string expectedDefault
        = cc::GenerateVariantNameFromObjectName(opts, "AvocadoNode", { "AvocadoNode" }, {});
    EXPECT_EQ(firstVs->GetDefaultVariant(), expectedDefault)
        << "default variant should come from the alphabetically-first node";
}
