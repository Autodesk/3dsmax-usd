//
// Copyright 2025 Autodesk
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

#include "TestHelpers.h"

#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <RenderDelegate/HdMaxEngine.h>
#include <RenderDelegate/SelectionRenderItem.h>

#include <MaxUsd/Utilities/UiUtils.h>

#include <usdUfe/utils/uiCallback.h>

#include <pxr/usd/usd/payloads.h>
#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/metrics.h>

#include <maxscript/maxscript.h>
#include <ufe/globalSelection.h>
#include <ufe/hierarchy.h>
#include <ufe/observableSelection.h>

#include <Hold.h>
#include <Max.h>

class SubObjectCloneTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        // Keep track of current units so we can set them back properly. Just making sure we always
        // run the test with the same units.
        GetSystemUnitInfo(&unitType, &unitScale);
        SetSystemUnitInfo(UNITS_INCHES, 1.0f);

        stageObject = static_cast<USDStageObject*>(
            GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
        node = GetCOREInterface()->CreateObjectNode(stageObject);

        HdMaxEngine             testEngine;
        MockRenderItemContainer renderItems;

        pxr::HdChangeTracker dummyTracker;
        auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
        displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);

        const auto     globalUfeSel = Ufe::GlobalSelection::get();
        Ufe::Selection newSelection;

        // Setup a stage with only a sphere.

        primPath = pxr::SdfPath("/sphere");
        stage = stageObject->GetUSDStage();
        stage->DefinePrim(primPath, pxr::TfToken("Sphere"));

        auto ufeItem
            = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, primPath));
        newSelection.append(ufeItem);
        globalUfeSel->replaceWith(newSelection);

        // Switch to Prim sub-object mode.
        GetCOREInterface()->SelectNode(node);
        GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
        GetCOREInterface()->SetSubObjectLevel(1);

        // If currently holding, stop. The SubObjectCloneTest tests call undo and we only
        // want to undo the transform changes.
        if (theHold.Holding()) {
            theHold.Accept(L"Test setup");
        }
        // Fake the shift key being pressed for the duration of the test.
        MaxUsd::Ui::SetIsShiftPressedFunction([] { return true; });
    }
    void TearDown() override
    {
        QuickReset();
        SetSystemUnitInfo(unitType, unitScale);

        MaxUsd::Ui::SetIsShiftPressedFunction({});
    }
    int   unitType;
    float unitScale;

    pxr::SdfPath        primPath;
    INode*              node;
    USDStageObject*     stageObject;
    pxr::UsdStageRefPtr stage;
};

TEST_F(SubObjectCloneTest, CloneWithInternalRef)
{
    const TimeValue time = 0;

    theHold.Begin();

    stageObject->TransformStart(time);

    const auto globalUfeSel = Ufe::GlobalSelection::get();
    const auto clonePath = pxr::SdfPath { "/sphere1" };
    const auto clonedPrim = stage->GetPrimAtPath(clonePath);
    ASSERT_TRUE(clonedPrim);
    const auto layer = stage->GetEditTarget().GetLayer();
    const auto spec = layer->GetPrimAtPath(clonePath);
    const auto refs = spec->GetReferenceList().GetPrependedItems();

    ASSERT_EQ(1, refs.size());
    EXPECT_TRUE(refs[0].IsInternal());
    EXPECT_EQ(refs[0].GetPrimPath(), primPath);

    // The cloned prim should now be the selection.
    EXPECT_EQ(globalUfeSel->size(), 1);
    EXPECT_TRUE(globalUfeSel->contains(MaxUsd::ufe::getUfePath(clonedPrim)));
    const auto primPreTransform
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim, time, false);
    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate(primPreTransform.GetTrans());

    Point3 translation = { 0.f, 0.f, 10.f };
    auto   parentTm = node->GetNodeTM(time);

    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    theHold.Accept(L"Clone");

    const auto primPostTransform
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim, time, false);

    // Verify that the prim indeed moved 10 units in the 3dsMax scene's space.
    auto primPreTransformInv = primPreTransform;
    primPreTransformInv.Invert();
    const auto delta = primPreTransformInv * primPostTransform;
    Matrix3    expected;
    expected.SetTranslate(translation);
    EXPECT_TRUE(delta.Equals(expected, MAX_FLOAT_EPSILON));

    // Test undoing the clone.
    ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded);

    auto deletedClone = stage->GetPrimAtPath(clonePath);
    EXPECT_FALSE(deletedClone);

    const auto srcPath = pxr::SdfPath { "/sphere" };
    const auto srcPrim = stage->GetPrimAtPath(srcPath);
    ASSERT_TRUE(srcPrim);
    EXPECT_EQ(globalUfeSel->size(), 1);
    EXPECT_TRUE(globalUfeSel->contains(MaxUsd::ufe::getUfePath(srcPrim)));
}

TEST_F(SubObjectCloneTest, MultiClone)
{
    // Create more spheres and add them to the selection.
    const auto srcPath1 = pxr::SdfPath { "/sphere1" };
    const auto srcPath2 = pxr::SdfPath { "/sphere2" };
    auto       sphere1Prim = stage->DefinePrim(srcPath1, pxr::TfToken("Sphere"));
    auto       sphere2Prim = stage->DefinePrim(srcPath2, pxr::TfToken("Sphere"));

    const auto globalUfeSel = Ufe::GlobalSelection::get();
    globalUfeSel->append(Ufe::Hierarchy::createItem(MaxUsd::ufe::getUfePath(sphere1Prim)));
    globalUfeSel->append(Ufe::Hierarchy::createItem(MaxUsd::ufe::getUfePath(sphere2Prim)));

    theHold.Begin();

    const TimeValue time = 0;
    stageObject->TransformStart(time);

    const auto clonePath1 = pxr::SdfPath { "/sphere3" };
    const auto clonedPrim1 = stage->GetPrimAtPath(clonePath1);
    const auto clonePath2 = pxr::SdfPath { "/sphere4" };
    const auto clonedPrim2 = stage->GetPrimAtPath(clonePath2);
    const auto clonePath3 = pxr::SdfPath { "/sphere5" };
    const auto clonedPrim3 = stage->GetPrimAtPath(clonePath3);
    ASSERT_TRUE(clonedPrim1);
    ASSERT_TRUE(clonedPrim2);
    ASSERT_TRUE(clonedPrim3);

    EXPECT_EQ(globalUfeSel->size(), 3);
    EXPECT_TRUE(globalUfeSel->contains(MaxUsd::ufe::getUfePath(clonedPrim1)));
    EXPECT_TRUE(globalUfeSel->contains(MaxUsd::ufe::getUfePath(clonedPrim1)));
    EXPECT_TRUE(globalUfeSel->contains(MaxUsd::ufe::getUfePath(clonedPrim1)));

    // Transform will be the same for all 3.
    const auto primPreTransform
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim1, time, false);

    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate(primPreTransform.GetTrans());

    Point3 translation = { 0.f, 0.f, 10.f };
    auto   parentTm = node->GetNodeTM(time);

    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    theHold.Accept(L"Multi Clone");

    const auto primPostTransform1
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim1, time, false);
    const auto primPostTransform2
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim2, time, false);
    const auto primPostTransform3
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim3, time, false);

    Matrix3 expected;
    expected.SetTranslate(translation);

    // Verify that the prim indeed moved 10 units in the 3dsMax scene's space.
    auto primPreTransformInv = primPreTransform;
    primPreTransformInv.Invert();

    const auto delta1 = primPreTransformInv * primPostTransform1;
    EXPECT_TRUE(delta1.Equals(expected, MAX_FLOAT_EPSILON));
    const auto delta2 = primPreTransformInv * primPostTransform2;
    EXPECT_TRUE(delta2.Equals(expected, MAX_FLOAT_EPSILON));
    const auto delta3 = primPreTransformInv * primPostTransform3;
    EXPECT_TRUE(delta3.Equals(expected, MAX_FLOAT_EPSILON));

    // Test undoing the clones.
    ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded);

    auto deletedClone1 = stage->GetPrimAtPath(clonePath1);
    EXPECT_FALSE(deletedClone1);
    auto deletedClone2 = stage->GetPrimAtPath(clonePath2);
    EXPECT_FALSE(deletedClone2);
    auto deletedClone3 = stage->GetPrimAtPath(clonePath3);
    EXPECT_FALSE(deletedClone3);

    EXPECT_EQ(globalUfeSel->size(), 3);
    EXPECT_TRUE(globalUfeSel->contains(MaxUsd::ufe::getUfePath(stage->GetPrimAtPath(primPath))));
    EXPECT_TRUE(globalUfeSel->contains(MaxUsd::ufe::getUfePath(stage->GetPrimAtPath(srcPath1))));
    EXPECT_TRUE(globalUfeSel->contains(MaxUsd::ufe::getUfePath(stage->GetPrimAtPath(srcPath2))));
}

TEST_F(SubObjectCloneTest, Cancel)
{
    theHold.Begin();

    const TimeValue time = 0;
    stageObject->TransformStart(time);

    const auto globalUfeSel = Ufe::GlobalSelection::get();
    const auto clonePath = pxr::SdfPath { "/sphere1" };
    const auto clonedPrim = stage->GetPrimAtPath(clonePath);
    ASSERT_TRUE(clonedPrim);
    EXPECT_EQ(globalUfeSel->size(), 1);
    EXPECT_TRUE(globalUfeSel->contains(MaxUsd::ufe::getUfePath(clonedPrim)));
    const auto primPreTransform
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim, time, false);
    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate(primPreTransform.GetTrans());

    Point3 translation = { 0.f, 0.f, 10.f };
    auto   parentTm = node->GetNodeTM(time);

    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformCancel(time);

    auto cancelledClone = stage->GetPrimAtPath(clonePath);
    EXPECT_FALSE(cancelledClone);

    const auto srcPath = pxr::SdfPath { "/sphere" };
    const auto srcPrim = stage->GetPrimAtPath(srcPath);
    ASSERT_TRUE(srcPrim);
    EXPECT_EQ(globalUfeSel->size(), 1);
    EXPECT_TRUE(globalUfeSel->contains(MaxUsd::ufe::getUfePath(srcPrim)));
}

TEST_F(SubObjectCloneTest, CloneCallbacks)
{
    class TestCallback : public UsdUfe::UICallback
    {
    public:
        TestCallback()
            : UICallback()
        {
        }
        void operator()(const pxr::VtDictionary& context, pxr::VtDictionary& callbackData) override
        {
            this->context = context;
            this->data = callbackData;
        }
        pxr::VtDictionary context;
        pxr::VtDictionary data;
    };

    auto startCb = std::make_shared<TestCallback>();
    registerUICallback(PXR_NS::TfToken("onUsdPrimCloneStart"), startCb);
    auto finishCb = std::make_shared<TestCallback>();
    registerUICallback(PXR_NS::TfToken("onUsdPrimCloneFinish"), finishCb);
    auto cancelCb = std::make_shared<TestCallback>();
    registerUICallback(PXR_NS::TfToken("onUsdPrimCloneCancel"), cancelCb);

    const TimeValue time = 0;
    stageObject->TransformStart(time);

    EXPECT_EQ("internalReference", startCb->context["mode"]);
    EXPECT_EQ(1, startCb->data["items"].GetArraySize());
    auto startItems = startCb->data["items"].Get<pxr::VtStringArray>();
    EXPECT_TRUE(startItems[0].find("/sphere") != std::string::npos);

    Matrix3 tmAxis = Matrix3::Identity;
    Point3  translation = { 0.f, 0.f, 10.f };
    auto    parentTm = node->GetNodeTM(time);

    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    EXPECT_EQ("internalReference", finishCb->context["mode"]);
    EXPECT_EQ(1, finishCb->data["cloned_items"].GetArraySize());
    auto finishItems = finishCb->data["cloned_items"].Get<pxr::VtStringArray>();
    EXPECT_TRUE(finishItems[0].find("/sphere1") != std::string::npos);

    stageObject->TransformStart(time);
    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformCancel(time);

    EXPECT_EQ("internalReference", cancelCb->context["mode"]);
    auto cancelItems = cancelCb->data["cloned_items"].Get<pxr::VtStringArray>();
    EXPECT_EQ(1, cancelItems.size());
    EXPECT_TRUE(cancelItems[0].find("/sphere2") != std::string::npos);

    unregisterUICallback(PXR_NS::TfToken("onUsdPrimCloneStart"), startCb);
    unregisterUICallback(PXR_NS::TfToken("onUsdPrimCloneFinish"), finishCb);
    unregisterUICallback(PXR_NS::TfToken("onUsdPrimCloneCancel"), cancelCb);
}

TEST_F(SubObjectCloneTest, CloneNoChain)
{
    const TimeValue time = 0;

    theHold.Begin();

    // Initial clone
    stageObject->TransformStart(time);
    const auto globalUfeSel = Ufe::GlobalSelection::get();
    const auto clonePath = pxr::SdfPath { "/sphere1" };
    const auto clonedPrim = stage->GetPrimAtPath(clonePath);
    const auto layer = stage->GetEditTarget().GetLayer();
    const auto spec = layer->GetPrimAtPath(clonePath);
    const auto refs = spec->GetReferenceList().GetPrependedItems();
    const auto primPreTransform
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim, time, false);
    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate(primPreTransform.GetTrans());
    Point3 translation = { 0.f, 0.f, 10.f };
    auto   parentTm = node->GetNodeTM(time);
    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    theHold.Accept(L"Clone");

    ASSERT_EQ(1, refs.size());
    EXPECT_TRUE(refs[0].IsInternal());
    EXPECT_EQ(refs[0].GetPrimPath(), primPath);

    auto ufeItem
        = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, clonePath));
    Ufe::Selection cloneSel;
    cloneSel.append(ufeItem);
    globalUfeSel->replaceWith(cloneSel);

    // Clone the clone
    theHold.Begin();
    stageObject->TransformStart(time);
    const auto clonePath2 = pxr::SdfPath { "/sphere2" };
    const auto clonedPrim2 = stage->GetPrimAtPath(clonePath2);
    const auto spec2 = layer->GetPrimAtPath(clonePath2);
    const auto refs2 = spec2->GetReferenceList().GetPrependedItems();
    const auto primPreTransform2
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim2, time, false);
    Matrix3 tmAxis2 = Matrix3::Identity;
    tmAxis2.SetTranslate(primPreTransform2.GetTrans());
    stageObject->Move(time, parentTm, tmAxis2, translation, FALSE);
    stageObject->TransformFinish(time);
    theHold.Accept(L"Clone");

    ASSERT_EQ(1, refs2.size());
    EXPECT_TRUE(refs2[0].IsInternal());
    EXPECT_EQ(refs2[0].GetPrimPath(), primPath); // Referencing the initial prim, not the clone's

    pxr::GfMatrix4d tr = pxr::UsdGeomImageable(clonedPrim2)
                             .ComputeLocalToWorldTransform(pxr::UsdTimeCode::Default());
    const auto trans = tr.ExtractTranslation();
    EXPECT_FLOAT_EQ(0, trans[0]);
    EXPECT_FLOAT_EQ(0, trans[1]);
    EXPECT_FLOAT_EQ(20, trans[2]);
}

TEST_F(SubObjectCloneTest, CloneChainForAttributeDiff)
{
    const TimeValue time = 0;

    theHold.Begin();

    // Initial clone
    stageObject->TransformStart(time);
    const auto globalUfeSel = Ufe::GlobalSelection::get();
    const auto clonePath = pxr::SdfPath { "/sphere1" };
    const auto clonedPrim = stage->GetPrimAtPath(clonePath);
    const auto layer = stage->GetEditTarget().GetLayer();
    const auto spec = layer->GetPrimAtPath(clonePath);
    const auto refs = spec->GetReferenceList().GetPrependedItems();
    const auto primPreTransform
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim, time, false);
    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate(primPreTransform.GetTrans());
    Point3 translation = { 0.f, 0.f, 10.f };
    auto   parentTm = node->GetNodeTM(time);
    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    theHold.Accept(L"Clone");

    ASSERT_EQ(1, refs.size());
    EXPECT_TRUE(refs[0].IsInternal());
    EXPECT_EQ(refs[0].GetPrimPath(), primPath);

    auto ufeItem
        = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, clonePath));
    Ufe::Selection cloneSel;
    cloneSel.append(ufeItem);
    globalUfeSel->replaceWith(cloneSel);

    // Edit and clone the clone

    auto sphere = pxr::UsdGeomSphere(clonedPrim);
    sphere.CreateRadiusAttr().Set(10.0);

    theHold.Begin();
    stageObject->TransformStart(time);

    const auto clonePath2 = pxr::SdfPath { "/sphere2" };
    const auto clonedPrim2 = stage->GetPrimAtPath(clonePath2);
    const auto spec2 = layer->GetPrimAtPath(clonePath2);
    const auto refs2 = spec2->GetReferenceList().GetPrependedItems();
    const auto primPreTransform2
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim2, time, false);
    Matrix3 tmAxis2 = Matrix3::Identity;
    tmAxis2.SetTranslate(primPreTransform2.GetTrans());
    stageObject->Move(time, parentTm, tmAxis2, translation, FALSE);
    stageObject->TransformFinish(time);
    theHold.Accept(L"Clone");

    ASSERT_EQ(1, refs2.size());
    EXPECT_TRUE(refs2[0].IsInternal());
    EXPECT_EQ(refs2[0].GetPrimPath(), clonePath); // Referencing the first clone's path.
}

TEST_F(SubObjectCloneTest, CloneChainForArc)
{
    const TimeValue time = 0;

    theHold.Begin();

    // Initial clone
    stageObject->TransformStart(time);
    const auto globalUfeSel = Ufe::GlobalSelection::get();
    const auto clonePath = pxr::SdfPath { "/sphere1" };
    const auto clonedPrim = stage->GetPrimAtPath(clonePath);
    const auto layer = stage->GetEditTarget().GetLayer();
    const auto spec = layer->GetPrimAtPath(clonePath);
    const auto refs = spec->GetReferenceList().GetPrependedItems();
    const auto primPreTransform
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim, time, false);
    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate(primPreTransform.GetTrans());
    Point3 translation = { 0.f, 0.f, 10.f };
    auto   parentTm = node->GetNodeTM(time);
    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    theHold.Accept(L"Clone");

    ASSERT_EQ(1, refs.size());
    EXPECT_TRUE(refs[0].IsInternal());
    EXPECT_EQ(refs[0].GetPrimPath(), primPath);

    auto ufeItem
        = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, clonePath));
    Ufe::Selection cloneSel;
    cloneSel.append(ufeItem);
    globalUfeSel->replaceWith(cloneSel);

    // Add an arc and clone the clone

    clonedPrim.GetPayloads().AddPayload("/foo/bar");

    theHold.Begin();
    stageObject->TransformStart(time);

    const auto clonePath2 = pxr::SdfPath { "/sphere2" };
    const auto clonedPrim2 = stage->GetPrimAtPath(clonePath2);
    const auto spec2 = layer->GetPrimAtPath(clonePath2);
    const auto refs2 = spec2->GetReferenceList().GetPrependedItems();
    const auto primPreTransform2
        = USDStageObject::GetMaxScenePrimTransform(node, clonedPrim2, time, false);
    Matrix3 tmAxis2 = Matrix3::Identity;
    tmAxis2.SetTranslate(primPreTransform2.GetTrans());
    stageObject->Move(time, parentTm, tmAxis2, translation, FALSE);
    stageObject->TransformFinish(time);
    theHold.Accept(L"Clone");

    ASSERT_EQ(1, refs2.size());
    EXPECT_TRUE(refs2[0].IsInternal());
    EXPECT_EQ(refs2[0].GetPrimPath(), clonePath); // Referencing the first clone's path.
}