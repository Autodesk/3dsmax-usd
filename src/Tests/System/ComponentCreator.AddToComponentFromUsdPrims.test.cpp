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

#include <MaxUsdObjects/ComponentCreator/UtilComponentCreator.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ScopeGuard.h>

#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>

using namespace CCTest;
namespace cc = AdskUsdComponentCreator;
namespace ccc = MaxUsd::ComponentCreator;

namespace {

// A source in-memory stage with a single Cube prim at /<name>.
pxr::UsdStageRefPtr MakeSourceStageWithCube(const char* name)
{
    auto src = pxr::UsdStage::CreateInMemory();
    src->DefinePrim(pxr::SdfPath(std::string("/") + name), pxr::TfToken("Cube"));
    return src;
}

// The (variantSetName, variantName) of the component's default variant.
cc::VariantSelections DefaultSelection(const cc::ComponentDescription::Ptr& desc)
{
    const auto& vs = *desc->GetVariantSets().begin();
    return cc::VariantSelections { { vs.first, vs.second->GetDefaultVariant() } };
}

} // namespace

TEST(ComponentCreatorAddToComponentFromUsdPrims, NoComponentDescReturnsFalse)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    auto src = MakeSourceStageWithCube("Cube1");
    EXPECT_FALSE(ccc::AddPrimsToComponent(
        nullptr,
        { { src, { pxr::SdfPath("/Cube1") } } },
        cc::VariantSelections { { "model", "extra" } },
        /*isReplacing=*/false))
        << "adding with a null component description must return false";
}

TEST(ComponentCreatorAddToComponentFromUsdPrims, SanityReturnsTrueNoNewProxy)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);

    auto                   src = MakeSourceStageWithCube("pCube2");
    const std::set<INode*> before = SnapshotStageObjectNodes();
    const bool             ok = ccc::AddPrimsToComponent(
        desc,
        { { src, { pxr::SdfPath("/pCube2") } } },
        DefaultSelection(desc),
        /*isReplacing=*/false);
    EXPECT_TRUE(ok) << "adding prims to the default variant should succeed";
    EXPECT_EQ(SnapshotStageObjectNodes(), before) << "add must not create a new stage object";
}

// Adding a prim with purpose "guide" to the default variant places the geometry under a
// /root/geo/guide Scope whose (inherited) imageable purpose is guide, rather than flat under
// /root/geo.
TEST(ComponentCreatorAddToComponentFromUsdPrims, AddWithPurpose)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);

    auto                   src = MakeSourceStageWithCube("pCube2");
    const std::set<INode*> before = SnapshotStageObjectNodes();
    const bool             ok = ccc::AddPrimsToComponent(
        desc,
        { { src, { pxr::SdfPath("/pCube2") } } },
        DefaultSelection(desc),
        /*isReplacing=*/false,
        /*purpose=*/"guide");
    EXPECT_TRUE(ok) << "adding prims with a purpose should succeed";
    EXPECT_EQ(SnapshotStageObjectNodes(), before) << "add must not create a new stage object";
    PumpIdle(); // flush any deferred edit-forward before inspecting/leaving the test

    const pxr::UsdPrim scopePrim = stage->GetPrimAtPath(pxr::SdfPath("/root/geo/guide"));
    ASSERT_TRUE(scopePrim.IsValid()) << "/root/geo/guide scope should exist";
    EXPECT_EQ(scopePrim.GetTypeName(), pxr::TfToken("Scope"));

    int underGuide = 0;
    for (const auto& p : stage->Traverse()) {
        if (p.GetName() == pxr::TfToken("pCube2")
            && p.GetPath().HasPrefix(pxr::SdfPath("/root/geo/guide"))) {
            ++underGuide;
            EXPECT_EQ(pxr::UsdGeomImageable(p).ComputePurpose(), pxr::UsdGeomTokens->guide)
                << "added prim under the guide scope should compute purpose = guide";
        }
    }
    EXPECT_GE(underGuide, 1) << "the added prim should live under /root/geo/guide";
}

// Adding to the default variant must not change which variant is the default.
TEST(ComponentCreatorAddToComponentFromUsdPrims, DefaultVariantUnchanged)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    const auto                desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const std::string originalDefault = desc->GetVariantSets().begin()->second->GetDefaultVariant();
    ASSERT_FALSE(originalDefault.empty());

    auto src = MakeSourceStageWithCube("pCube2");
    ASSERT_TRUE(ccc::AddPrimsToComponent(
        desc,
        { { src, { pxr::SdfPath("/pCube2") } } },
        DefaultSelection(desc),
        /*isReplacing=*/false));

    const auto updated = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(updated);
    EXPECT_EQ(updated->GetVariantSets().begin()->second->GetDefaultVariant(), originalDefault)
        << "default variant must not change after add (is_default_variant=false)";
}

// Build a 2-variant component (AppleNode alphabetically first -> default), add a prim
// to the non-default (ZebraNode) variant, and confirm the default is unchanged.
TEST(ComponentCreatorAddToComponentFromUsdPrims, DefaultVariantUnchangedWhenAddingToNonDefault)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateMultiVariantsComponentFromCubes({ L"ZebraNode", L"AppleNode" });
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    const auto                desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);

    const auto&       vs = *desc->GetVariantSets().begin();
    const std::string vsName = vs.first;
    const std::string originalDefault = vs.second->GetDefaultVariant();
    ASSERT_EQ(originalDefault, "AppleNode");

    // Add to the NON-default "ZebraNode" variant.
    auto src = MakeSourceStageWithCube("pExtra");
    ASSERT_TRUE(ccc::AddPrimsToComponent(
        desc,
        { { src, { pxr::SdfPath("/pExtra") } } },
        cc::VariantSelections { { vsName, "ZebraNode" } },
        /*isReplacing=*/false));

    const auto updated = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(updated);
    EXPECT_EQ(updated->GetVariantSets().begin()->second->GetDefaultVariant(), originalDefault)
        << "default variant must not change when adding to a non-default variant";
}
