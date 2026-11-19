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

#include <MaxUsd/MaxTokens.h>
#include <MaxUsd/USDIOController.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Containers.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <GetCOREInterface.h>
#include <set>
#include <string>

using namespace CCTest;
namespace cc = AdskUsdComponentCreator;

// Adding a parent node pulls in its child hierarchy (the child is exported too), like the create
// path -- not just the parent's own mesh.
TEST(ComponentCreatorAddToComponentFromNodes, AddedNodeIncludesChildHierarchy)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"Base");
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string defVar = vs->GetDefaultVariant();

    INode* parent = MakeCubeNode(L"AddedParent");
    INode* child = MakeCubeNode(L"AddedChild");
    parent->AttachChild(child);
    // Add only the parent; its child must come along.
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { parent },
        cc::VariantSelections { { vsName, defVar } },
        false,
        MaxUsd::USDSceneBuilderOptions {}));

    int meshCount = 0;
    for (const auto& p : stage->Traverse()) {
        if (p.GetTypeName() == pxr::TfToken("Mesh")
            && p.GetPath().HasPrefix(pxr::SdfPath("/root/geo"))) {
            ++meshCount;
        }
    }
    EXPECT_GE(meshCount, 3)
        << "Base + added parent + its child must all be present under /root/geo";
}

// Adding with no component description must return false.
TEST(ComponentCreatorAddToComponentFromNodes, NoComponentDescReturnsFalse)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    INode*     cube = MakeCubeNode(L"AddNoDesc");
    const bool ok = MaxUsd::ComponentCreator::AddINodesToComponent(
        nullptr,
        { cube },
        cc::VariantSelections { { "model", "default" } },
        false,
        MaxUsd::USDSceneBuilderOptions {});
    EXPECT_FALSE(ok) << "adding with a null component description must return false";
}

// Adding to the default variant succeeds and does not create a new stage object.
TEST(ComponentCreatorAddToComponentFromNodes, SanityReturnsTrueNoNewProxy)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"AddBase");
    ASSERT_NE(comp, nullptr);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(comp->GetUSDStage());
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string defVar = vs->GetDefaultVariant();

    INode*                 cube2 = MakeCubeNode(L"AddCube2");
    const std::set<INode*> before = SnapshotStageObjectNodes();
    const bool             ok = MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { cube2 },
        cc::VariantSelections { { vsName, defVar } },
        false,
        MaxUsd::USDSceneBuilderOptions {});
    EXPECT_TRUE(ok) << "adding nodes to the default variant should succeed";
    EXPECT_EQ(SnapshotStageObjectNodes(), before) << "add must not create a new stage object";
}

// Adding to the default variant leaves the default variant name unchanged.
TEST(ComponentCreatorAddToComponentFromNodes, DefaultVariantUnchanged)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"AddDef");
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string originalDefault = vs->GetDefaultVariant();

    INode* cube2 = MakeCubeNode(L"AddDefCube2");
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { cube2 },
        cc::VariantSelections { { vsName, originalDefault } },
        false,
        MaxUsd::USDSceneBuilderOptions {}));

    const auto desc2 = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc2);
    EXPECT_EQ(desc2->GetVariantSets().begin()->second->GetDefaultVariant(), originalDefault);
}

// Max has no multi-variant create yet, so the second variant is authored directly
// through the library API for setup.
TEST(ComponentCreatorAddToComponentFromNodes, DefaultVariantUnchangedWhenAddingToNonDefault)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"AddNonDef");
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string originalDefault = vs->GetDefaultVariant();

    // Author a second variant so we can target a non-default one.
    const std::string secondVariant = "zSecondVariant";
    cc::ComponentAPI::AddVariant(desc, {}, vs, secondVariant);

    INode* cube2 = MakeCubeNode(L"AddNonDefCube2");
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { cube2 },
        cc::VariantSelections { { vsName, secondVariant } },
        false,
        MaxUsd::USDSceneBuilderOptions {}));

    const auto desc2 = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc2);
    EXPECT_EQ(desc2->GetVariantSets().begin()->second->GetDefaultVariant(), originalDefault)
        << "adding to a non-default variant must not change the default";
}

// The default variant remains the targeted variant after adding to it.
TEST(ComponentCreatorAddToComponentFromNodes, TargetedVariantUnchangedAfterAdd)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"AddTgt");
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string defVar = vs->GetDefaultVariant();

    const auto variantDesc = vs->GetVariantDescription(defVar);
    ASSERT_TRUE(variantDesc);
    EXPECT_TRUE(cc::ComponentAPI::IsVariantTarget(desc, cc::VariantNesting { vs, variantDesc }))
        << "precondition: default variant is targeted after create";

    INode* cube2 = MakeCubeNode(L"AddTgtCube2");
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { cube2 },
        cc::VariantSelections { { vsName, defVar } },
        false,
        MaxUsd::USDSceneBuilderOptions {}));

    const auto desc2 = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc2);
    const auto& vs2 = desc2->GetVariantSets().begin()->second;
    const auto  variantDesc2 = vs2->GetVariantDescription(vs2->GetDefaultVariant());
    ASSERT_TRUE(variantDesc2);
    EXPECT_TRUE(cc::ComponentAPI::IsVariantTarget(desc2, cc::VariantNesting { vs2, variantDesc2 }))
        << "default variant must remain targeted after add";
}

// Adding a node with purpose "proxy" to the default variant places the geometry under a
// /root/geo/proxy Scope whose imageable purpose is set, rather than flat under /root/geo.
TEST(ComponentCreatorAddToComponentFromNodes, AddWithProxyPurposeCreatesScope)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"PurposeBase");
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string defVar = vs->GetDefaultVariant();

    INode* cube = MakeCubeNode(L"ProxyCube");
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { cube },
        cc::VariantSelections { { vsName, defVar } },
        false,
        MaxUsd::USDSceneBuilderOptions {},
        "proxy"));
    PumpIdle(); // flush any deferred edit-forward before inspecting/leaving the test

    // The purpose scope exists and is a Scope; the added geometry lives under it and its effective
    // (inherited) imageable purpose is proxy.
    const pxr::UsdPrim scopePrim = stage->GetPrimAtPath(pxr::SdfPath("/root/geo/proxy"));
    ASSERT_TRUE(scopePrim.IsValid()) << "/root/geo/proxy scope should exist";
    EXPECT_EQ(scopePrim.GetTypeName(), pxr::TfToken("Scope"));

    int underProxy = 0;
    for (const auto& p : stage->Traverse()) {
        if (p.GetTypeName() == pxr::TfToken("Mesh")
            && p.GetPath().HasPrefix(pxr::SdfPath("/root/geo/proxy"))) {
            ++underProxy;
            EXPECT_EQ(pxr::UsdGeomImageable(p).ComputePurpose(), pxr::UsdGeomTokens->proxy)
                << "added geometry under the proxy scope should compute purpose = proxy";
        }
    }
    EXPECT_GE(underProxy, 1) << "the added cube's mesh should live under /root/geo/proxy";
}

// render and guide purposes produce their own scopes with the matching imageable purpose.
TEST(ComponentCreatorAddToComponentFromNodes, AddWithRenderAndGuidePurpose)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"RGBase");
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string defVar = vs->GetDefaultVariant();

    INode* renderCube = MakeCubeNode(L"RenderCube");
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { renderCube },
        cc::VariantSelections { { vsName, defVar } },
        false,
        MaxUsd::USDSceneBuilderOptions {},
        "render"));
    PumpIdle();
    INode* guideCube = MakeCubeNode(L"GuideCube");
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { guideCube },
        cc::VariantSelections { { vsName, defVar } },
        false,
        MaxUsd::USDSceneBuilderOptions {},
        "guide"));
    PumpIdle();

    const auto computedPurposeUnder = [&stage](const char* scopePath) {
        for (const auto& p : stage->Traverse()) {
            if (p.GetTypeName() == pxr::TfToken("Mesh")
                && p.GetPath().HasPrefix(pxr::SdfPath(scopePath))) {
                return pxr::UsdGeomImageable(p).ComputePurpose();
            }
        }
        return pxr::TfToken();
    };

    ASSERT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/render")).IsValid());
    EXPECT_EQ(computedPurposeUnder("/root/geo/render"), pxr::UsdGeomTokens->render);

    ASSERT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/guide")).IsValid());
    EXPECT_EQ(computedPurposeUnder("/root/geo/guide"), pxr::UsdGeomTokens->guide);
}

// Adding twice with the same purpose reuses the one scope; both meshes end up under it.
TEST(ComponentCreatorAddToComponentFromNodes, RepeatedPurposeReusesScope)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"ReuseBase");
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string defVar = vs->GetDefaultVariant();

    INode* first = MakeCubeNode(L"ReuseA");
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { first },
        cc::VariantSelections { { vsName, defVar } },
        false,
        MaxUsd::USDSceneBuilderOptions {},
        "proxy"));
    PumpIdle();
    INode* second = MakeCubeNode(L"ReuseB");
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { second },
        cc::VariantSelections { { vsName, defVar } },
        false,
        MaxUsd::USDSceneBuilderOptions {},
        "proxy"));
    PumpIdle();

    // Exactly one proxy scope, with both added cubes' meshes beneath it.
    int proxyScopes = 0;
    int meshesUnderProxy = 0;
    for (const auto& p : stage->Traverse()) {
        if (p.GetTypeName() == pxr::TfToken("Scope")
            && p.GetPath() == pxr::SdfPath("/root/geo/proxy")) {
            ++proxyScopes;
        }
        if (p.GetTypeName() == pxr::TfToken("Mesh")
            && p.GetPath().HasPrefix(pxr::SdfPath("/root/geo/proxy"))) {
            ++meshesUnderProxy;
        }
    }
    EXPECT_EQ(proxyScopes, 1) << "the proxy scope must be reused, not duplicated";
    EXPECT_GE(meshesUnderProxy, 2) << "both added cubes should be under the single proxy scope";
}

// An empty purpose keeps the plain-add layout: geometry flat under /root/geo, no purpose scope.
TEST(ComponentCreatorAddToComponentFromNodes, EmptyPurposeIsPlainAdd)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"PlainBase");
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string defVar = vs->GetDefaultVariant();

    INode* cube = MakeCubeNode(L"PlainCube");
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { cube },
        cc::VariantSelections { { vsName, defVar } },
        false,
        MaxUsd::USDSceneBuilderOptions {},
        ""));

    EXPECT_FALSE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/proxy")).IsValid())
        << "empty purpose must not create a purpose scope";
    // Added geometry stays flat under /root/geo (some Mesh directly beneath it).
    int meshesDirectlyUnderGeo = 0;
    for (const auto& p : stage->Traverse()) {
        if (p.GetTypeName() == pxr::TfToken("Mesh")
            && p.GetPath().GetParentPath() == pxr::SdfPath("/root/geo")) {
            ++meshesDirectlyUnderGeo;
        }
    }
    EXPECT_GE(meshesDirectlyUnderGeo, 1)
        << "plain add should leave geometry directly under /root/geo";
}

// The purpose-add is one undo step that captures the whole operation (add + scope + reparents run
// as one command): one undo removes the added geometry AND the purpose scope, and one redo
// restores both. Uses the UI-free component so MaxUndo/MaxRedo don't drive the Variant Editor
// (which would hang the headless test).
TEST(ComponentCreatorAddToComponentFromNodes, PurposeAddUndoRedo)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string defVar = vs->GetDefaultVariant();

    const auto proxyMeshCount = [&stage]() {
        int n = 0;
        for (const auto& p : stage->Traverse()) {
            if (p.GetTypeName() == pxr::TfToken("Mesh")
                && p.GetPath().HasPrefix(pxr::SdfPath("/root/geo/proxy"))) {
                ++n;
            }
        }
        return n;
    };

    // The add runs as a single undoable command, so no manual grouping is needed here.
    INode*     cube = MakeCubeNode(L"UndoProxyCube");
    const bool ok = MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { cube },
        cc::VariantSelections { { vsName, defVar } },
        false,
        MaxUsd::USDSceneBuilderOptions {},
        "proxy");
    ASSERT_TRUE(ok);
    PumpIdle();

    ASSERT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/proxy")).IsValid())
        << "precondition: purpose scope exists after the add";
    ASSERT_GE(proxyMeshCount(), 1) << "precondition: geometry is under the purpose scope";

    // One undo reverts the whole operation.
    MaxUndo();
    PumpIdle();
    EXPECT_FALSE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/proxy")).IsValid())
        << "one undo should remove the purpose scope";
    EXPECT_EQ(proxyMeshCount(), 0) << "one undo should remove the added geometry";

    // One redo restores it.
    MaxRedo();
    PumpIdle();
    EXPECT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/proxy")).IsValid())
        << "redo should restore the purpose scope";
    EXPECT_GE(proxyMeshCount(), 1) << "redo should restore the added geometry";
}

// Replacing the targeted variant's content with a new node is one undo step: undo brings back the
// geometry the component was created from, and redo swaps the replacement back in.
TEST(ComponentCreatorAddToComponentFromNodes, ReplaceUndoRestoresOriginalContent)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"ReplaceOriginal");
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string defVar = vs->GetDefaultVariant();

    const auto hasMeshNamed = [&stage](const char* name) {
        for (const auto& p : stage->Traverse()) {
            if (p.GetTypeName() == pxr::TfToken("Mesh") && p.GetName() == pxr::TfToken(name)) {
                return true;
            }
        }
        return false;
    };
    ASSERT_TRUE(hasMeshNamed("ReplaceOriginal")) << DumpStage(stage);
    const auto dumpLayers = [&stage]() {
        std::string out = DumpStage(stage);
        for (const auto& layer : stage->GetUsedLayers()) {
            std::string text;
            layer->ExportToString(&text);
            out += "  layer " + layer->GetIdentifier() + ":\n" + text + "\n";
        }
        return out;
    };

    INode* replacement = MakeCubeNode(L"ReplaceNew");
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { replacement },
        cc::VariantSelections { { vsName, defVar } },
        /*isReplacing=*/true,
        MaxUsd::USDSceneBuilderOptions {}));
    PumpIdle();
    ASSERT_TRUE(hasMeshNamed("ReplaceNew")) << dumpLayers();
    ASSERT_FALSE(hasMeshNamed("ReplaceOriginal")) << dumpLayers();

    MaxUndo();
    PumpIdle();
    EXPECT_TRUE(hasMeshNamed("ReplaceOriginal"))
        << "undo must restore the original content\n"
        << dumpLayers();
    EXPECT_FALSE(hasMeshNamed("ReplaceNew")) << "undo must remove the replacement\n"
                                             << dumpLayers();

    MaxRedo();
    PumpIdle();
    EXPECT_TRUE(hasMeshNamed("ReplaceNew")) << "redo must restore the replacement\n"
                                            << dumpLayers();
    EXPECT_FALSE(hasMeshNamed("ReplaceOriginal")) << dumpLayers();
}

// The creator copies the source stage's up axis onto the component, so an add that exports in
// the export options' axis rather than the component's silently redefines the component. The
// component's axis must win, whatever the add is handed.
TEST(ComponentCreatorAddToComponentFromNodes, AddKeepsComponentUpAxis)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    MaxUsd::USDSceneBuilderOptions yUpOptions;
    yUpOptions.SetUpAxis(MaxUsd::USDSceneBuilderOptions::UpAxis::Y);

    const std::set<INode*> before = SnapshotStageObjectNodes();
    INode*                 cube = MakeCubeNode(L"YUpBase");
    MaxUsd::ComponentCreator::CreateComponentFromNodes({ cube }, cc::Options {}, yUpOptions);
    auto* stageObj = StageObjectOf(FindNewStageObjectNode(before));
    ASSERT_NE(stageObj, nullptr);
    const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
    ASSERT_TRUE(stage);
    ASSERT_EQ(pxr::UsdGeomGetStageUpAxis(stage), pxr::UsdGeomTokens->y)
        << "precondition: the component was created Y-up";

    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto& vs = desc->GetVariantSets().begin()->second;

    // Added with the Z-up default the add flows are handed (the export-to-stage options).
    INode* added = MakeCubeNode(L"YUpAdded");
    ASSERT_TRUE(MaxUsd::ComponentCreator::AddINodesToComponent(
        desc,
        { added },
        cc::VariantSelections { { vs->GetName(), vs->GetDefaultVariant() } },
        false,
        MaxUsd::USDSceneBuilderOptions {}));

    EXPECT_EQ(pxr::UsdGeomGetStageUpAxis(stage), pxr::UsdGeomTokens->y)
        << "adding to a component must not rewrite its up axis";
}

// "Inherit Stage Object Transform" also governs the Variant Manager's add/replace path
// (MaxHost::AddSelectionToComponent), which resolves the option off the component's own stage
// object node rather than being handed a transform. Both adds below replace the variant's
// content, so the replace flow is covered too.
TEST(ComponentCreatorAddToComponentFromNodes, AddSelectionInheritsStageObjectTransform)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = MakeComponentStageObject();
    ASSERT_NE(comp, nullptr);
    const auto stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    const auto&       vs = desc->GetVariantSets().begin()->second;
    const std::string vsName = vs->GetName();
    const std::string defVar = vs->GetDefaultVariant();

    const TimeValue t = GetCOREInterface()->GetTime();

    // Move the component's stage object off the origin; each cube is then placed exactly on it,
    // so inheriting its transform must land the geometry at the component's origin.
    const INodeTab stageNodes = MaxUsd::GetReferencingNodes(comp);
    ASSERT_GT(stageNodes.Count(), 0);
    Matrix3 stageTm;
    stageTm.SetTrans(Point3(150.f, 20.f, -30.f));
    stageNodes[0]->SetNodeTM(t, stageTm);

    // The add resolves the transform itself, walking description -> stage -> stage object ->
    // node. Check that chain here so a break in it reports as itself, not as a bad transform.
    // The description resolves to the creator's own stage over the component file, not the one
    // the stage object opened, so the root layer -- interned per identifier -- is the link.
    const auto descStage = cc::ComponentDescription::GetStageFromComponentDescription(desc);
    ASSERT_TRUE(descStage);
    EXPECT_EQ(descStage->GetRootLayer(), stage->GetRootLayer())
        << "the description must resolve to a stage over the component's root layer";
    EXPECT_TRUE(stageNodes[0]->GetObjectTM(t).Equals(stageTm))
        << "the node transform the add inherits must be the one the test set";

    auto*                   io = MaxUsd::GetUSDIOController();
    const pxr::VtDictionary previousExtraOptions = io->GetUIExportToStageExtraOptions();
    const auto              extraOptionsGuard
        = MaxUsd::MakeScopeGuard([]() {}, [io, &previousExtraOptions]() {
              io->SetUIExportToStageExtraOptions(previousExtraOptions);
          });

    const auto addSelection = [&](const wchar_t* cubeName, bool inheritTransform) {
        pxr::VtDictionary extraOptions = previousExtraOptions;
        extraOptions.SetValueAtPath(
            pxr::MaxUsdExportTokens->inheritStageObjectTransform,
            pxr::VtValue { inheritTransform });
        io->SetUIExportToStageExtraOptions(extraOptions);

        INode* cube = MakeCubeNode(cubeName);
        cube->SetNodeTM(t, stageTm);
        GetCOREInterface()->ClearNodeSelection();
        GetCOREInterface()->SelectNode(cube);
        return MaxHost::Instance()->AddSelectionToComponent(
            desc,
            cc::VariantSelections { { vsName, defVar } },
            /*isReplacing=*/true,
            std::string());
    };

    ASSERT_TRUE(addSelection(L"AddWorldCube", false));
    ASSERT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/AddWorldCube")).IsValid())
        << "added prim missing. Stage:\n"
        << DumpStage(stage);
    EXPECT_TRUE(pxr::GfIsClose(
        LocalTransform(stage, "/root/geo/AddWorldCube"),
        ExpectedLocalTransform(stage, stageTm.GetTrans()),
        1e-4))
        << "without the option, added geometry keeps its world placement";

    ASSERT_TRUE(addSelection(L"AddInheritCube", true));
    ASSERT_TRUE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/AddInheritCube")).IsValid())
        << "added prim missing. Stage:\n"
        << DumpStage(stage);
    EXPECT_TRUE(pxr::GfIsClose(
        LocalTransform(stage, "/root/geo/AddInheritCube"),
        ExpectedLocalTransform(stage, Point3(0.f, 0.f, 0.f)),
        1e-4))
        << "with the option on, geometry sitting on the stage object must land at its origin"
        << "\n  actual:      " << LocalTransform(stage, "/root/geo/AddInheritCube")
        << "\n  expected:    " << ExpectedLocalTransform(stage, Point3(0.f, 0.f, 0.f))
        << "\n  world space: " << ExpectedLocalTransform(stage, stageTm.GetTrans())
        << "\n  (matching world space means the root transform was never applied)";
    EXPECT_FALSE(stage->GetPrimAtPath(pxr::SdfPath("/root/geo/AddWorldCube")).IsValid())
        << "isReplacing should have replaced the variant's previous content";
}
