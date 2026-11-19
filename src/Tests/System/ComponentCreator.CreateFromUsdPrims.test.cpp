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
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ScopeGuard.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Options/Options.h>

using namespace CCTest;
namespace cc = AdskUsdComponentCreator;
namespace ccc = MaxUsd::ComponentCreator;

namespace {

// Create a component from the given prim sources and return its new USD Stage Object.
USDStageObject* CreateFromPrims(const std::vector<ccc::UsdPrimSource>& sources)
{
    const std::set<INode*> before = SnapshotStageObjectNodes();
    ccc::CreateComponentFromUsdPrims(sources, cc::Options {});
    return StageObjectOf(FindNewStageObjectNode(before));
}

} // namespace

TEST(ComponentCreatorCreateFromUsdPrims, EmptyIsNoOp)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();
    ccc::CreateComponentFromUsdPrims({}, cc::Options {});
    EXPECT_EQ(FindNewStageObjectNode(before), nullptr)
        << "no component stage should be created for an empty source list";
}

// A source prim lands under /root/geo/<name>.
TEST(ComponentCreatorCreateFromUsdPrims, GeometryInStage)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const auto src = pxr::UsdStage::CreateInMemory();
    src->DefinePrim(pxr::SdfPath("/pCube1"), pxr::TfToken("Cube"));

    const std::set<INode*> before = SnapshotStageObjectNodes();
    USDStageObject*        comp = CreateFromPrims({ { src, { pxr::SdfPath("/pCube1") } } });
    ASSERT_NE(comp, nullptr);
    EXPECT_TRUE(Valid(comp->GetUSDStage(), "/root/geo/pCube1"));

    // The create is undoable: undo removes the component, redo recreates it with its geometry.
    MaxUndo();
    EXPECT_EQ(FindNewStageObjectNode(before), nullptr)
        << "undo should remove the created component";
    MaxRedo();
    USDStageObject* redone = StageObjectOf(FindNewStageObjectNode(before));
    ASSERT_NE(redone, nullptr) << "redo should recreate the component";
    EXPECT_TRUE(Valid(redone->GetUSDStage(), "/root/geo/pCube1"))
        << "geometry should be present again after redo";
}

// One variant set with one variant.
TEST(ComponentCreatorCreateFromUsdPrims, HasVariantSet)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const auto src = pxr::UsdStage::CreateInMemory();
    src->DefinePrim(pxr::SdfPath("/pCube1"), pxr::TfToken("Cube"));

    USDStageObject* comp = CreateFromPrims({ { src, { pxr::SdfPath("/pCube1") } } });
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);
    ASSERT_EQ(desc->GetVariantSets().size(), 1u);
    EXPECT_EQ(desc->GetVariantSets().begin()->second->GetVariants().size(), 1u);
    EXPECT_TRUE(Valid(comp->GetUSDStage(), "/root/geo/pCube1"));
}

// Two prims (one source stage): default variant set and targeted, both prims under /root/geo.
TEST(ComponentCreatorCreateFromUsdPrims, DefaultAndTargetVariantIsSet)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const auto src = pxr::UsdStage::CreateInMemory();
    src->DefinePrim(pxr::SdfPath("/pCube1"), pxr::TfToken("Cube"));
    src->DefinePrim(pxr::SdfPath("/pCone1"), pxr::TfToken("Cone"));

    const std::set<INode*> before = SnapshotStageObjectNodes();
    USDStageObject*        comp
        = CreateFromPrims({ { src, { pxr::SdfPath("/pCube1"), pxr::SdfPath("/pCone1") } } });
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    const auto                desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    ASSERT_FALSE(desc->GetVariantSets().empty());

    const auto&       firstVs = desc->GetVariantSets().begin()->second;
    const std::string defaultName = firstVs->GetDefaultVariant();
    ASSERT_FALSE(defaultName.empty());
    const auto defaultVar = firstVs->GetVariantDescription(defaultName);
    ASSERT_TRUE(defaultVar);
    EXPECT_TRUE(
        cc::ComponentAPI::IsVariantTarget(desc, cc::VariantNesting { firstVs, defaultVar }));

    EXPECT_TRUE(Valid(stage, "/root/geo/pCube1"));
    EXPECT_TRUE(Valid(stage, "/root/geo/pCone1"));

    // The create is undoable: undo removes the component, redo recreates it with the default
    // variant targeted again.
    MaxUndo();
    EXPECT_EQ(FindNewStageObjectNode(before), nullptr)
        << "undo should remove the created component";
    MaxRedo();
    USDStageObject* redone = StageObjectOf(FindNewStageObjectNode(before));
    ASSERT_NE(redone, nullptr) << "redo should recreate the component";
    const auto redoneDesc
        = cc::ComponentDescription::CreateFromStageMetadata(redone->GetUSDStage());
    ASSERT_TRUE(redoneDesc);
    ASSERT_FALSE(redoneDesc->GetVariantSets().empty());
    const auto& redoneVs = redoneDesc->GetVariantSets().begin()->second;
    const auto  redoneVar = redoneVs->GetVariantDescription(redoneVs->GetDefaultVariant());
    ASSERT_TRUE(redoneVar);
    EXPECT_TRUE(
        cc::ComponentAPI::IsVariantTarget(redoneDesc, cc::VariantNesting { redoneVs, redoneVar }))
        << "default variant should still be targeted after redo";
}

// Same-named prims from two source stages both land in the component, the second uniquified.
TEST(ComponentCreatorCreateFromUsdPrims, PrimsWithSameNameFromMultiStages)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const auto src1 = pxr::UsdStage::CreateInMemory();
    src1->DefinePrim(pxr::SdfPath("/pCube1"), pxr::TfToken("Cube"));
    const auto src2 = pxr::UsdStage::CreateInMemory();
    src2->DefinePrim(pxr::SdfPath("/pCube1"), pxr::TfToken("Cube"));

    USDStageObject* comp = CreateFromPrims(
        { { src1, { pxr::SdfPath("/pCube1") } }, { src2, { pxr::SdfPath("/pCube1") } } });
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();

    EXPECT_TRUE(Valid(stage, "/root/geo/pCube1"));
    EXPECT_TRUE(Valid(stage, "/root/geo/pCube2"))
        << "the second same-named prim should be uniquified to pCube2";
}
