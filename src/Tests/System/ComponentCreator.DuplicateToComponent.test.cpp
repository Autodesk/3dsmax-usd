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
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Containers.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <GetCOREInterface.h>

using namespace CCTest;
namespace cc = AdskUsdComponentCreator;

// Duplicating to a component with no variant targeted for edition must fail (return
// false).
TEST(ComponentCreatorDuplicateToComponent, NoVariantTargetReturnsFalse)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);
    // Precondition: a freshly created component has no variant targeted for edition.
    ASSERT_TRUE(cc::ComponentAPI::GetVariantTarget(desc).empty())
        << "precondition: no variant targeted right after create";

    INode*     cube = MakeCubeNode(L"DupNoTargetCube");
    const bool ok = MaxUsd::ComponentCreator::DuplicateINodesToComponent(
        desc, { cube }, MaxUsd::USDSceneBuilderOptions {});
    EXPECT_FALSE(ok) << "duplicate with no variant target must return false";
}

// With the default variant targeted, duplicating a Max node succeeds and the
// geometry lands under /root/geo.
TEST(ComponentCreatorDuplicateToComponent, VariantTargetedLandsInGeo)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);

    // Target the default variant for editing.
    const auto& vs = desc->GetVariantSets().begin()->second;
    const auto  variantDesc = vs->GetVariantDescription(vs->GetDefaultVariant());
    ASSERT_TRUE(variantDesc);
    ASSERT_TRUE(cc::ComponentAPI::SetVariantTarget(desc, cc::VariantNesting { vs, variantDesc }))
        << "could not target the default variant";

    INode*     cube = MakeCubeNode(L"DupCube1");
    const bool ok = MaxUsd::ComponentCreator::DuplicateINodesToComponent(
        desc, { cube }, MaxUsd::USDSceneBuilderOptions {});
    EXPECT_TRUE(ok) << "duplicate with a variant targeted must succeed";

    // The cube should land FLAT at /root/geo/DupCube1 -- not nested beneath an extra
    // export-root Xform (/root/geo/root/DupCube1).
    const pxr::SdfPath geoScope("/root/geo");
    ASSERT_TRUE(stage->GetPrimAtPath(geoScope).IsValid()) << "/root/geo scope missing";
    const pxr::UsdPrim cubePrim = stage->GetPrimAtPath(pxr::SdfPath("/root/geo/DupCube1"));
    if (!cubePrim.IsValid() || cubePrim.GetTypeName() != pxr::TfToken("Mesh")) {
        std::string dump;
        for (const auto& p : stage->Traverse()) {
            dump += p.GetPath().GetString() + " (" + p.GetTypeName().GetString() + ")\n";
        }
        ADD_FAILURE() << "Expected Mesh at /root/geo/DupCube1 (flat). Full stage:\n" << dump;
    }
    EXPECT_FALSE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/root")).IsValid())
        << "geometry must not be nested under an extra /root/geo/root export wrapper";
}

// "Inherit Stage Object Transform" applies to components too: the duplicated geometry is
// authored relative to the component's stage object instead of in world space.
TEST(ComponentCreatorDuplicateToComponent, InheritStageObjectTransformOffsetsGeometry)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto& vs = desc->GetVariantSets().begin()->second;
    const auto  variantDesc = vs->GetVariantDescription(vs->GetDefaultVariant());
    ASSERT_TRUE(variantDesc);
    ASSERT_TRUE(cc::ComponentAPI::SetVariantTarget(desc, cc::VariantNesting { vs, variantDesc }));

    const TimeValue t = GetCOREInterface()->GetTime();

    // Move the component's stage object off the origin; both cubes then sit exactly on it.
    const INodeTab stageNodes = MaxUsd::GetReferencingNodes(comp);
    ASSERT_GT(stageNodes.Count(), 0);
    INode*  stageNode = stageNodes[0];
    Matrix3 stageTm;
    stageTm.SetTrans(Point3(150.f, 20.f, -30.f));
    stageNode->SetNodeTM(t, stageTm);

    const auto placeCubeOnStageObject = [t, &stageTm](const wchar_t* name) {
        INode*  cube = MakeCubeNode(name);
        Matrix3 tm = stageTm;
        cube->SetNodeTM(t, tm);
        return cube;
    };

    // Without the option the cube keeps its world placement.
    INode* worldCube = placeCubeOnStageObject(L"WorldCube");
    ASSERT_TRUE(MaxUsd::ComponentCreator::DuplicateINodesToComponent(
        desc, { worldCube }, MaxUsd::USDSceneBuilderOptions {}));

    // With it the cube is authored relative to the stage object -- here, at its origin.
    INode* inheritCube = placeCubeOnStageObject(L"InheritCube");
    ASSERT_TRUE(MaxUsd::ComponentCreator::DuplicateINodesToComponent(
        desc, { inheritCube }, MaxUsd::USDSceneBuilderOptions {}, stageNode->GetObjectTM(t)));

    ASSERT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/InheritCube")).IsValid())
        << "duplicated prim missing. Stage:\n"
        << DumpStage(stage);
    ASSERT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/WorldCube")).IsValid())
        << "duplicated prim missing. Stage:\n"
        << DumpStage(stage);
    EXPECT_TRUE(pxr::GfIsClose(
        LocalTransform(stage, "/root/geo/InheritCube"),
        ExpectedLocalTransform(stage, Point3(0.f, 0.f, 0.f)),
        1e-4))
        << "with the option on, geometry sitting on the stage object must land at its origin";
    EXPECT_TRUE(pxr::GfIsClose(
        LocalTransform(stage, "/root/geo/WorldCube"),
        ExpectedLocalTransform(stage, stageTm.GetTrans()),
        1e-4))
        << "without the option, geometry keeps its world placement";
}
