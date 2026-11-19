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
#include <AdskUsdComponentCreator/ComponentFileNames.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Options/Options.h>

using namespace CCTest;
namespace cc = AdskUsdComponentCreator;
namespace ccc = MaxUsd::ComponentCreator;

namespace {

USDStageObject* CreateFromPrims(const std::vector<ccc::UsdPrimSource>& sources)
{
    const std::set<INode*> before = SnapshotStageObjectNodes();
    ccc::CreateMultiVariantsComponentFromUsdPrims(sources, cc::Options {});
    return StageObjectOf(FindNewStageObjectNode(before));
}

} // namespace

TEST(ComponentCreatorCreateMultiVariantsFromUsdPrims, EmptyIsNoOp)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();
    ccc::CreateMultiVariantsComponentFromUsdPrims({}, cc::Options {});
    EXPECT_EQ(FindNewStageObjectNode(before), nullptr);
}

// Two prims -> two variants, only one visible at a time.
TEST(ComponentCreatorCreateMultiVariantsFromUsdPrims, VariantCount)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const auto src = pxr::UsdStage::CreateInMemory();
    src->DefinePrim(pxr::SdfPath("/pCube1"), pxr::TfToken("Cube"));
    src->DefinePrim(pxr::SdfPath("/pCone1"), pxr::TfToken("Cone"));

    USDStageObject* comp
        = CreateFromPrims({ { src, { pxr::SdfPath("/pCube1"), pxr::SdfPath("/pCone1") } } });
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    const auto                desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto& firstVs = desc->GetVariantSets().begin()->second;
    EXPECT_EQ(firstVs->GetVariants().size(), 2u);
    // Both prims became their own variant.
    EXPECT_TRUE(firstVs->GetVariantDescription("pCube1"));
    EXPECT_TRUE(firstVs->GetVariantDescription("pCone1"));

    // Each prim is in a separate variant, so only the default (alphabetically-first,
    // pCone1) is visible in the composed stage; the other is gated out.
    EXPECT_FALSE(Valid(stage, "/root/geo/pCube1"));
    EXPECT_TRUE(Valid(stage, "/root/geo/pCone1"));
}

TEST(ComponentCreatorCreateMultiVariantsFromUsdPrims, TargetedVariantIsDefault)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const auto src = pxr::UsdStage::CreateInMemory();
    src->DefinePrim(pxr::SdfPath("/pCube1"), pxr::TfToken("Cube"));
    src->DefinePrim(pxr::SdfPath("/pCone1"), pxr::TfToken("Cone"));

    USDStageObject* comp
        = CreateFromPrims({ { src, { pxr::SdfPath("/pCube1"), pxr::SdfPath("/pCone1") } } });
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);

    const auto&       firstVs = desc->GetVariantSets().begin()->second;
    const std::string defaultName = firstVs->GetDefaultVariant();
    ASSERT_FALSE(defaultName.empty());
    const auto defaultVar = firstVs->GetVariantDescription(defaultName);
    ASSERT_TRUE(defaultVar);
    EXPECT_TRUE(
        cc::ComponentAPI::IsVariantTarget(desc, cc::VariantNesting { firstVs, defaultVar }));
}

TEST(ComponentCreatorCreateMultiVariantsFromUsdPrims, DefaultIsAlphabeticallyFirst)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const auto src = pxr::UsdStage::CreateInMemory();
    src->DefinePrim(pxr::SdfPath("/MangoNode"), pxr::TfToken("Cube"));
    src->DefinePrim(pxr::SdfPath("/AvocadoNode"), pxr::TfToken("Cube"));
    src->DefinePrim(pxr::SdfPath("/KiwiNode"), pxr::TfToken("Cube"));

    USDStageObject* comp = CreateFromPrims({ { src,
                                               { pxr::SdfPath("/MangoNode"),
                                                 pxr::SdfPath("/AvocadoNode"),
                                                 pxr::SdfPath("/KiwiNode") } } });
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);

    const auto& firstVs = desc->GetVariantSets().begin()->second;
    EXPECT_EQ(firstVs->GetVariants().size(), 3u);

    cc::Options opts;
    opts.Validate();
    const std::string expectedDefault
        = cc::GenerateVariantNameFromObjectName(opts, "AvocadoNode", { "AvocadoNode" }, {});
    EXPECT_EQ(firstVs->GetDefaultVariant(), expectedDefault);
}

// Selecting a prim and one of its descendants must collapse to a single (top-level) variant --
// the descendant is filtered out, mirroring the node path's RemoveDescendantINodes.
TEST(ComponentCreatorCreateMultiVariantsFromUsdPrims, DescendantPrimIsFilteredOut)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const auto src = pxr::UsdStage::CreateInMemory();
    src->DefinePrim(pxr::SdfPath("/Parent"), pxr::TfToken("Xform"));
    src->DefinePrim(pxr::SdfPath("/Parent/Child"), pxr::TfToken("Cube"));

    USDStageObject* comp
        = CreateFromPrims({ { src, { pxr::SdfPath("/Parent"), pxr::SdfPath("/Parent/Child") } } });
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);
    ASSERT_FALSE(desc->GetVariantSets().empty());
    EXPECT_EQ(desc->GetVariantSets().begin()->second->GetVariants().size(), 1u)
        << "the descendant prim must be filtered out, leaving a single variant";
}

// Same-named prims from two source stages produce two distinct variants.
TEST(ComponentCreatorCreateMultiVariantsFromUsdPrims, PrimsWithSameNameFromMultiStages)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const auto src1 = pxr::UsdStage::CreateInMemory();
    src1->DefinePrim(pxr::SdfPath("/pCube1"), pxr::TfToken("Cube"));
    const auto src2 = pxr::UsdStage::CreateInMemory();
    src2->DefinePrim(pxr::SdfPath("/pCube1"), pxr::TfToken("Cube"));

    USDStageObject* comp = CreateFromPrims(
        { { src1, { pxr::SdfPath("/pCube1") } }, { src2, { pxr::SdfPath("/pCube1") } } });
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);
    EXPECT_EQ(desc->GetVariantSets().begin()->second->GetVariants().size(), 2u)
        << "two same-named prims from different stages should yield two variants";
}
