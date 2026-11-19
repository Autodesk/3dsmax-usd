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

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/Utilities/ScopeGuard.h>

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usd/variantSets.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/root.h>
#include <pxr/usd/usdSkel/skeleton.h>

#include <AdskUsdComponentCreator/Commands/AddVariantCommand.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Descriptions/VariantSetDescription.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <set>
#include <vector>

using namespace CCTest;

namespace {

// The first prim of type T found anywhere on the stage, or an invalid prim.
template <class T> pxr::UsdPrim FindFirstOfType(const pxr::UsdStageRefPtr& stage)
{
    for (const auto& prim : stage->Traverse()) {
        if (prim.IsA<T>()) {
            return prim;
        }
    }
    return pxr::UsdPrim();
}

} // namespace

// A component created from a skinned mesh and its bones, with skin translation enabled,
// keeps the mesh geometry and encapsulates the skinned content under a UsdSkelRoot.
// USD requires a skinned mesh and its skeleton to share a SkelRoot ancestor, so one has
// to be established even though the source stage is exported flat at the pseudo-root.
TEST(ComponentCreatorSkinnedNodes, SkinnedMeshKeepsGeometryAndGetsSkelRoot)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();

    INode* mesh = nullptr;
    INode* boneRoot = nullptr;
    MakeSkinnedSetup(mesh, boneRoot);
    ASSERT_NE(mesh, nullptr) << "skinned mesh was not created";
    ASSERT_NE(boneRoot, nullptr) << "bone chain was not created";

    MaxUsd::USDSceneBuilderOptions exportOptions;
    exportOptions.SetTranslateSkin(true);
    exportOptions.SetTimeMode(MaxUsd::USDSceneBuilderOptions::TimeMode::AnimationRange);

    MaxUsd::ComponentCreator::CreateComponentFromNodes(
        { mesh, boneRoot }, AdskUsdComponentCreator::Options {}, exportOptions);

    INode* newNode = FindNewStageObjectNode(before);
    ASSERT_NE(newNode, nullptr) << "create-from-nodes did not create a USD Stage Object";

    auto* stageObj = StageObjectOf(newNode);
    ASSERT_NE(stageObj, nullptr);
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
    ASSERT_TRUE(stage);

    // The component prim ends up being what encloses the skinned content, so the geometry keeps
    // the same flat placement it has without skinning rather than sitting a level deeper.
    const pxr::UsdPrim meshPrim = stage->GetPrimAtPath(pxr::SdfPath("/root/geo/CCSkinMesh"));
    ASSERT_TRUE(meshPrim.IsValid()) << "skinned mesh missing from the component's geometry scope\n"
                                    << DumpStage(stage);

    // The skinned-mesh writer gives up on a node whose SkelRoot cannot be resolved, leaving the
    // prim behind with its transform but no geometry -- so the mesh has to be checked for points,
    // not merely for existence.
    pxr::VtVec3fArray points;
    pxr::UsdGeomMesh(meshPrim).GetPointsAttr().Get(&points);
    EXPECT_FALSE(points.empty()) << "the skinned mesh was exported with no points\n"
                                 << DumpStage(stage);

    const pxr::UsdPrim skelPrim = FindFirstOfType<pxr::UsdSkelSkeleton>(stage);
    ASSERT_TRUE(skelPrim.IsValid()) << "no UsdSkelSkeleton in the component stage -- the skeleton "
                                       "was not exported\n"
                                    << DumpStage(stage);

    // USD only resolves skinning when the mesh and its skeleton resolve to the same
    // SkelRoot ancestor; siblings under separate roots leave the mesh unskinned.
    const pxr::UsdSkelRoot meshRoot = pxr::UsdSkelRoot::Find(meshPrim);
    const pxr::UsdSkelRoot skelRoot = pxr::UsdSkelRoot::Find(skelPrim);
    ASSERT_TRUE(meshRoot) << "skinned mesh is not encapsulated under a UsdSkelRoot\n"
                          << DumpStage(stage);
    ASSERT_TRUE(skelRoot) << "skeleton is not encapsulated under a UsdSkelRoot\n"
                          << DumpStage(stage);
    EXPECT_EQ(meshRoot.GetPath(), skelRoot.GetPath())
        << "the skinned mesh and its skeleton are under different SkelRoots\n"
        << DumpStage(stage);

    // A binding that lists joints the skeleton does not have leaves the influences resolving
    // against the wrong joints, which deforms the mesh into a wrong pose rather than failing.
    pxr::VtTokenArray bindingJoints;
    pxr::VtTokenArray skeletonJoints;
    pxr::UsdSkelBindingAPI(meshPrim).GetJointsAttr().Get(&bindingJoints);
    pxr::UsdSkelSkeleton(skelPrim).GetJointsAttr().Get(&skeletonJoints);
    EXPECT_FALSE(skeletonJoints.empty()) << "the skeleton has no joints\n" << DumpStage(stage);
    if (!bindingJoints.empty()) {
        EXPECT_EQ(bindingJoints.size(), skeletonJoints.size())
            << "the mesh binding and the skeleton disagree on the joint set\n"
            << DumpStage(stage);
    }

    EXPECT_EQ(meshRoot.GetPath(), pxr::SdfPath("/root"))
        << "the component prim is not the enclosing SkelRoot\n"
        << DumpStage(stage);

    // Samples are authored at frame numbers, so the component has to carry the scene's frame rate
    // or they resolve at the wrong instants and the animation drifts.
    EXPECT_DOUBLE_EQ(stage->GetTimeCodesPerSecond(), 4800.0 / double(GetTicksPerFrame()))
        << "the component does not carry the scene's frame rate";
    EXPECT_TRUE(stage->HasAuthoredTimeCodeRange())
        << "the component does not carry the exported time range";

    // A payload left on a different rate than the component composing it has its samples rescaled
    // by the ratio, which slides every frame off the pose it was authored at.
    const pxr::UsdPrim animPrim = FindFirstOfType<pxr::UsdSkelAnimation>(stage);
    ASSERT_TRUE(animPrim.IsValid()) << "no SkelAnimation in the component stage\n"
                                    << DumpStage(stage);
    std::vector<double> samples;
    animPrim.GetAttribute(pxr::TfToken("rotations")).GetTimeSamples(&samples);
    ASSERT_FALSE(samples.empty()) << "the skeleton animation has no time samples";
    EXPECT_DOUBLE_EQ(samples.back(), stage->GetEndTimeCode())
        << "the animation samples do not line up with the component's time range";
}

// Every variant's payload has to carry the scene's frame rate, not just the ones composed at
// creation: composition rescales the samples of any layer whose rate differs from the layer
// including it, so a variant left on the default rate plays stretched once it is selected.
TEST(ComponentCreatorSkinnedNodes, EveryVariantKeepsTheSceneFrameRate)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();

    INode* first = MakeCubeNode(L"CCAnimCubeA");
    INode* second = MakeCubeNode(L"CCAnimCubeB");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    AnimateNodeRotation(L"CCAnimCubeA");
    AnimateNodeRotation(L"CCAnimCubeB");

    MaxUsd::USDSceneBuilderOptions exportOptions;
    exportOptions.SetTimeMode(MaxUsd::USDSceneBuilderOptions::TimeMode::AnimationRange);

    MaxUsd::ComponentCreator::CreateMultiVariantsComponentFromNodes(
        { first, second }, AdskUsdComponentCreator::Options {}, exportOptions);

    INode* newNode = FindNewStageObjectNode(before);
    ASSERT_NE(newNode, nullptr);
    auto* stageObj = StageObjectOf(newNode);
    ASSERT_NE(stageObj, nullptr);
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
    ASSERT_TRUE(stage);

    const pxr::UsdPrim componentPrim = stage->GetPrimAtPath(pxr::SdfPath("/root"));
    ASSERT_TRUE(componentPrim.IsValid());
    pxr::UsdVariantSets varSets = componentPrim.GetVariantSets();
    const std::vector<std::string> setNames = varSets.GetNames();
    ASSERT_FALSE(setNames.empty()) << "component has no variant sets\n" << DumpStage(stage);
    pxr::UsdVariantSet varSet = varSets.GetVariantSet(setNames.front());
    const std::vector<std::string> variants = varSet.GetVariantNames();
    ASSERT_GE(variants.size(), 2u) << "expected a multi-variant component\n" << DumpStage(stage);

    for (const std::string& variant : variants) {
        ASSERT_TRUE(varSet.SetVariantSelection(variant));
        const pxr::UsdPrim cube = FindFirstOfType<pxr::UsdGeomMesh>(stage);
        ASSERT_TRUE(cube.IsValid()) << "no mesh in variant " << variant << "\n" << DumpStage(stage);
        std::vector<double> samples;
        pxr::UsdGeomXformable(cube).GetTimeSamples(&samples);
        ASSERT_FALSE(samples.empty()) << "variant " << variant << " has no animation";
        EXPECT_DOUBLE_EQ(samples.back(), stage->GetEndTimeCode())
            << "variant " << variant << " does not line up with the component's time range";
    }
}

// Adding a variant re-runs the creator's setup of the component prim, which retypes it as a plain
// Xform. The skinned content of the existing variant then has no SkelRoot and stops deforming, so
// the SkelRoot has to survive the add, and its undo and redo.
TEST(ComponentCreatorSkinnedNodes, SkelRootSurvivesAddingAVariant)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    const std::set<INode*> before = SnapshotStageObjectNodes();

    INode* mesh = nullptr;
    INode* boneRoot = nullptr;
    MakeSkinnedSetup(mesh, boneRoot);
    ASSERT_NE(mesh, nullptr);
    ASSERT_NE(boneRoot, nullptr);

    MaxUsd::USDSceneBuilderOptions exportOptions;
    exportOptions.SetTranslateSkin(true);
    exportOptions.SetTimeMode(MaxUsd::USDSceneBuilderOptions::TimeMode::AnimationRange);

    MaxUsd::ComponentCreator::CreateComponentFromNodes(
        { mesh, boneRoot }, AdskUsdComponentCreator::Options {}, exportOptions);

    INode* newNode = FindNewStageObjectNode(before);
    ASSERT_NE(newNode, nullptr);
    auto* stageObj = StageObjectOf(newNode);
    ASSERT_NE(stageObj, nullptr);
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
    ASSERT_TRUE(stage);
    const pxr::SdfPath rootPath("/root");
    ASSERT_TRUE(stage->GetPrimAtPath(rootPath).IsA<pxr::UsdSkelRoot>()) << DumpStage(stage);

    const auto desc = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string originalVariant = vs->GetDefaultVariant();

    auto cmd = AdskUsdComponentCreator::AddVariantCommand::ExecuteWithUndo(
        desc, { vs }, vs, "CCSkinSecondVariant");
    ASSERT_TRUE(cmd);
    ASSERT_TRUE(cmd->IsSuccessful());
    EXPECT_TRUE(stage->GetPrimAtPath(rootPath).IsA<pxr::UsdSkelRoot>())
        << "adding a variant retyped the component prim\n"
        << DumpStage(stage);

    pxr::UsdVariantSet varSet
        = stage->GetPrimAtPath(rootPath).GetVariantSets().GetVariantSet(vsName);
    ASSERT_TRUE(varSet.SetVariantSelection(originalVariant));
    const pxr::UsdPrim meshPrim = stage->GetPrimAtPath(pxr::SdfPath("/root/geo/CCSkinMesh"));
    ASSERT_TRUE(meshPrim.IsValid()) << DumpStage(stage);
    EXPECT_TRUE(pxr::UsdSkelRoot::Find(meshPrim))
        << "the original variant's skinned mesh is no longer under a SkelRoot\n"
        << DumpStage(stage);

    MaxUndo();
    EXPECT_TRUE(stage->GetPrimAtPath(rootPath).IsA<pxr::UsdSkelRoot>())
        << "undo lost the SkelRoot\n"
        << DumpStage(stage);

    MaxRedo();
    EXPECT_TRUE(stage->GetPrimAtPath(rootPath).IsA<pxr::UsdSkelRoot>())
        << "redo lost the SkelRoot\n"
        << DumpStage(stage);
}
