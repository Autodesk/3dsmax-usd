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
#include <MaxUsdObjects/ComponentCreator/MaxHost.h>
#include <MaxUsdObjects/ComponentCreator/UtilComponentCreator.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/USDIOController.h>
#include <MaxUsd/Utilities/ScopeGuard.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/relationship.h>
#include <pxr/usd/usd/stage.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Containers.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <GetCOREInterface.h>
#include <set>
#include <stdmat.h>

namespace cc = AdskUsdComponentCreator;

using namespace CCTest;

namespace {

// Assert the mesh's material:binding was rebound under /root/mtl and resolves to a prim
// (otherwise the material would not show in the viewport).
void CheckMaterialBinding(const pxr::UsdStageRefPtr& stage, const char* meshPath)
{
    const pxr::UsdPrim mesh = stage->GetPrimAtPath(pxr::SdfPath(meshPath));
    ASSERT_TRUE(mesh.IsValid());
    const pxr::UsdRelationship binding = mesh.GetRelationship(pxr::TfToken("material:binding"));
    ASSERT_TRUE(binding) << "mesh has no material:binding relationship";
    pxr::SdfPathVector targets;
    binding.GetTargets(&targets);
    ASSERT_FALSE(targets.empty()) << "material:binding has no targets";
    EXPECT_TRUE(targets.front().HasPrefix(pxr::SdfPath("/root/mtl")))
        << "material:binding should point under /root/mtl, got " << targets.front().GetString();
    EXPECT_TRUE(stage->GetPrimAtPath(targets.front()).IsValid())
        << "material:binding target does not resolve to a prim: " << targets.front().GetString();
}

// Is there a Material-typed prim somewhere under the given scope path?
bool HasMaterialUnder(const pxr::UsdStageRefPtr& stage, const char* scope)
{
    const pxr::SdfPath scopePath(scope);
    for (const auto& p : stage->Traverse()) {
        if (p.GetTypeName() == pxr::TfToken("Material") && p.GetPath().HasPrefix(scopePath)) {
            return true;
        }
    }
    return false;
}

} // namespace

// Creating a component from a node with a material assigned must place the material under
// /root/mtl (flat), not /root/mtl/mtl, and must not drop it.
TEST(ComponentCreatorMaterialsInComponent, MaterialLandsUnderRootMtl)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();
    INode*                 cube = MakeCubeNode(L"MtlCube");
    ASSERT_NE(cube, nullptr);
    cube->SetMtl(NewDefaultStdMat());

    MaxUsd::ComponentCreator::CreateComponentFromNodes(
        { cube }, AdskUsdComponentCreator::Options {});

    USDStageObject* comp = StageObjectOf(FindNewStageObjectNode(before));
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);

    // Geometry stays flat under /root/geo.
    EXPECT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/MtlCube")).IsValid())
        << "geometry should be flat at /root/geo/MtlCube. Stage:\n"
        << DumpStage(stage);

    // The material must exist under /root/mtl ...
    EXPECT_TRUE(HasMaterialUnder(stage, "/root/mtl"))
        << "material was dropped -- nothing under /root/mtl. Stage:\n"
        << DumpStage(stage);

    // ... and must NOT be double-nested under an extra /root/mtl/mtl scope.
    EXPECT_FALSE(stage->GetPrimAtPath(pxr::SdfPath("/root/mtl/mtl")).IsValid())
        << "material must not be nested under an extra /root/mtl/mtl scope. Stage:\n"
        << DumpStage(stage);

    CheckMaterialBinding(stage, "/root/geo/MtlCube");
}

// Mirror of the create case for the duplicate/add path: duplicating a material-bearing node
// into a targeted variant must also land the material flat under /root/mtl (not
// /root/mtl/mtl) with the binding rebound.
TEST(ComponentCreatorMaterialsInComponent, DuplicatedMaterialLandsUnderRootMtl)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);

    // Target the default variant so duplicate has a destination.
    const auto& vs = desc->GetVariantSets().begin()->second;
    const auto  variantDesc = vs->GetVariantDescription(vs->GetDefaultVariant());
    ASSERT_TRUE(variantDesc);
    ASSERT_TRUE(cc::ComponentAPI::SetVariantTarget(desc, cc::VariantNesting { vs, variantDesc }));

    INode* cube = MakeCubeNode(L"DupMtlCube");
    ASSERT_NE(cube, nullptr);
    cube->SetMtl(NewDefaultStdMat());
    ASSERT_TRUE(MaxUsd::ComponentCreator::DuplicateINodesToComponent(
        desc, { cube }, MaxUsd::USDSceneBuilderOptions {}));

    EXPECT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/DupMtlCube")).IsValid())
        << "duplicated geometry should be flat at /root/geo/DupMtlCube. Stage:\n"
        << DumpStage(stage);
    EXPECT_TRUE(HasMaterialUnder(stage, "/root/mtl"))
        << "duplicated material was dropped -- nothing under /root/mtl. Stage:\n"
        << DumpStage(stage);
    EXPECT_FALSE(stage->GetPrimAtPath(pxr::SdfPath("/root/mtl/mtl")).IsValid())
        << "duplicated material must not nest under an extra /root/mtl/mtl scope. Stage:\n"
        << DumpStage(stage);

    CheckMaterialBinding(stage, "/root/geo/DupMtlCube");
}

TEST(ComponentCreatorMaterialsInComponent, DuplicateHonoursExportOptions)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);

    const auto& vs = desc->GetVariantSets().begin()->second;
    const auto  variantDesc = vs->GetVariantDescription(vs->GetDefaultVariant());
    ASSERT_TRUE(variantDesc);
    ASSERT_TRUE(cc::ComponentAPI::SetVariantTarget(desc, cc::VariantNesting { vs, variantDesc }));

    INode* cube = MakeCubeNode(L"NoMtlCube");
    ASSERT_NE(cube, nullptr);
    cube->SetMtl(NewDefaultStdMat());

    MaxUsd::USDSceneBuilderOptions exportOptions;
    exportOptions.SetTranslateMaterials(false);
    ASSERT_TRUE(
        MaxUsd::ComponentCreator::DuplicateINodesToComponent(desc, { cube }, exportOptions));

    EXPECT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/NoMtlCube")).IsValid())
        << "the duplicate itself should still have happened. Stage:\n"
        << DumpStage(stage);
    EXPECT_FALSE(HasMaterialUnder(stage, "/root/mtl"))
        << "materials were disabled in the export options, so none should have been exported. "
           "Stage:\n"
        << DumpStage(stage);
}

TEST(ComponentCreatorMaterialsInComponent, AddSelectionHonoursExportToStageUiOptions)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);

    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string defVar = vs->GetDefaultVariant();

    INode* cube = MakeCubeNode(L"UiOptCube");
    ASSERT_NE(cube, nullptr);
    cube->SetMtl(NewDefaultStdMat());
    GetCOREInterface()->ClearNodeSelection();
    GetCOREInterface()->SelectNode(cube);

    auto*                                io = MaxUsd::GetUSDIOController();
    const MaxUsd::USDSceneBuilderOptions previousUiOptions
        = io->GetExportUIOptions(MaxUsd::USDSceneBuilderOptions::Type::ToStage);
    const auto uiOptionsGuard = MaxUsd::MakeScopeGuard([]() {}, [io, &previousUiOptions]() {
        io->SetExportUIOptions(previousUiOptions, MaxUsd::USDSceneBuilderOptions::Type::ToStage);
    });

    MaxUsd::USDSceneBuilderOptions uiOptions = previousUiOptions;
    uiOptions.SetTranslateMaterials(false);
    io->SetExportUIOptions(uiOptions, MaxUsd::USDSceneBuilderOptions::Type::ToStage);

    ASSERT_TRUE(MaxHost::Instance()->AddSelectionToComponent(
        desc, cc::VariantSelections { { vsName, defVar } }, /*isReplacing=*/false, std::string()));

    EXPECT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/UiOptCube")).IsValid())
        << "the add itself should still have happened. Stage:\n"
        << DumpStage(stage);
    EXPECT_FALSE(HasMaterialUnder(stage, "/root/mtl"))
        << "materials were disabled in the export to stage UI options, so none should have been "
           "exported. Stage:\n"
        << DumpStage(stage);
}
