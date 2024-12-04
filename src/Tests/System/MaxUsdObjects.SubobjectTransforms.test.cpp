//
// Copyright 2023 Autodesk
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

#include <MaxUsd/Utilities/ScopeGuard.h>

#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/metrics.h>

#include <maxscript/maxscript.h>
#include <ufe/globalSelection.h>
#include <ufe/hierarchy.h>
#include <ufe/observableSelection.h>

// Base class for testing prim subobject transform in the stage object. The focus is on the final
// position of the prim, not the xformOp setup. Simulates call order of subobject transform methods
// from 3dsmax. The test scene is designed to cover all alot of scenarios, it is a hierarchy of 3
// boxes, each level has translation, rotation and scaling applied.
class SubObjectTransformTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        // Keep track of current units so we can set them back properly. Just making sure we always
        // run the test with the same units.
        GetSystemUnitInfo(&unitType, &unitScale);
        SetSystemUnitInfo(UNITS_INCHES, 1.0f);

        auto       testDataPath = GetTestDataPath();
        const auto filePath = testDataPath.append("subobject_transform.usda");
        stageObject = static_cast<USDStageObject*>(
            GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
        node = GetCOREInterface()->CreateObjectNode(stageObject);
        stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

        HdMaxEngine             testEngine;
        MockRenderItemContainer renderItems;

        pxr::HdChangeTracker dummyTracker;
        auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
        displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);

        // Select the leaf box (combines transforms from the 2 ancestor boxes and its own).
        const auto     globalUfeSel = Ufe::GlobalSelection::get();
        Ufe::Selection newSelection;

        primPath = pxr::SdfPath("/root/Box001/Box002/Box003");

        auto ufeItem
            = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, primPath));
        newSelection.append(ufeItem);
        globalUfeSel->replaceWith(newSelection);

        // Give a transform to the node, with some translate and rotation, to make the tests
        // interesting (the rotation of the node in space matters when figuring out how to move
        // things later, along the right axis).
        Matrix3 nodeTm;
        nodeTm.SetRow(0, { 0.f, 0.f, -1.f });
        nodeTm.SetRow(1, { 0.f, 1.f, 0.f });
        nodeTm.SetRow(2, { 1.f, 0.f, 0.f });
        nodeTm.SetRow(3, { 39.3701f, 0.f, 0.f });
        node->SetNodeTM(0, nodeTm);

        // Switch to Prim sub-object mode.
        GetCOREInterface()->SelectNode(node);
        GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
        GetCOREInterface()->SetSubObjectLevel(1);

        // If currently holding, stop. The SubObjectTransformTest tests call undo and we only
        // want to undo the transform changes.
        if (theHold.Holding()) {
            theHold.Accept(L"Test setup");
        }
    }
    void TearDown() override
    {
        GetCOREInterface()->FileReset(TRUE);
        SetSystemUnitInfo(unitType, unitScale);
    }
    int   unitType;
    float unitScale;

    pxr::SdfPath    primPath;
    INode*          node;
    USDStageObject* stageObject;
};

TEST_F(SubObjectTransformTest, Move)
{
    // Simulate a sub-object move operation initiated from the UI. The default coord system will
    // make it move in world space, so we compare the start/end world space transforms and expect
    // they will differ by the translation we will perform.

    const TimeValue time = 0;
    stageObject->TransformStart(time);

    const auto prim = stageObject->GetUSDStage()->GetPrimAtPath(primPath);
    const auto primPreTransform = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);
    EXPECT_TRUE(primPreTransform.GetTrans().Equals(
        { 66.0295792f, 18.3343010f, -26.6595020f }, MAX_FLOAT_EPSILON));

    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate(primPreTransform.GetTrans());

    Point3 translation = { 0.f, 0.f, 10.f };
    auto   parentTm = node->GetNodeTM(time);

    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    const auto primPostTransform
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);

    // Verify that the prim indeed moved 10 units in the 3dsMax scene's space.
    auto primPreTransformInv = primPreTransform;
    primPreTransformInv.Invert();
    const auto delta = primPreTransformInv * primPostTransform;

    Matrix3 expected;
    expected.SetTranslate(translation);
    EXPECT_TRUE(delta.Equals(expected, MAX_FLOAT_EPSILON));

    // Test undoing the translation.
    ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded);
    const auto transformAfterUndo
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);
    EXPECT_TRUE(primPreTransform.Equals(transformAfterUndo, MAX_FLOAT_EPSILON));
}

TEST_F(SubObjectTransformTest, Rotate)
{
    // Simulate a sub-object rotate operation initiated from the UI.The default coord system will
    // make it rotate around the max scene's axises. So we compare the start/end world space
    // transforms.

    const TimeValue time = 0;

    stageObject->TransformStart(time);

    const auto prim = stageObject->GetUSDStage()->GetPrimAtPath(primPath);
    const auto primPreTransform = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);

    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate(primPreTransform.GetTrans());

    Quat    rotation;
    AngAxis angAxis;
    angAxis.Set(1.f, 0.0f, 0.f, 1.5708f); // 90 degree rotation around X.
    rotation.Set(angAxis);

    auto parentTm = node->GetNodeTM(time);

    stageObject->Rotate(time, parentTm, tmAxis, rotation, FALSE);
    stageObject->TransformFinish(time);

    const auto primPostTransform
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);

    // Verify that the prim indeed moved 10 units in the 3dsMax scene's space.
    auto primPreTransformInv = primPreTransform;
    primPreTransformInv.Invert();
    const auto delta = primPreTransformInv * primPostTransform;

    Matrix3 rotationMatrix;
    rotation.MakeMatrix(rotationMatrix);
    auto invTmAxis = tmAxis;
    invTmAxis.Invert();

    const Matrix3 expected = invTmAxis * rotationMatrix * tmAxis;

    EXPECT_TRUE(delta.Equals(expected, MAX_FLOAT_EPSILON));

    // Test undoing the rotation.
    ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded);
    const auto transformAfterUndo
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);
    EXPECT_TRUE(primPreTransform.Equals(transformAfterUndo, MAX_FLOAT_EPSILON));
}

// Test scale operation
TEST_F(SubObjectTransformTest, Scale)
{
    // Simulate a sub-object rotate operation initiated from the UI.The default coord system will
    // make it scale around the max scene's axises. So we compare the start/end world space
    // transforms.

    const TimeValue time = 0;

    stageObject->TransformStart(time);

    const auto prim = stageObject->GetUSDStage()->GetPrimAtPath(primPath);
    const auto primPreTransform = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);

    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate(primPreTransform.GetTrans());

    Point3 scale { 5.f, 5.f, 5.f };
    auto   parentTm = node->GetNodeTM(time);

    stageObject->Scale(time, parentTm, tmAxis, scale, FALSE);
    stageObject->TransformFinish(time);

    const auto primPostTransform
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);

    // Verify the scaling that was applied.
    auto primPreTransformInv = primPreTransform;
    primPreTransformInv.Invert();
    const auto delta = primPreTransformInv * primPostTransform;
    Matrix3    scaleMatrix;
    scaleMatrix.SetScale(scale);
    auto invTmAxis = tmAxis;
    invTmAxis.Invert();
    const Matrix3 expected = invTmAxis * scaleMatrix * tmAxis;
    EXPECT_TRUE(delta.Equals(expected, MAX_FLOAT_EPSILON));

    // Test undoing the scaling.
    ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded);
    const auto transformAfterUndo
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);
    EXPECT_TRUE(primPreTransform.Equals(transformAfterUndo, MAX_FLOAT_EPSILON));
}

// Test that different units between max and stage are accounted for when transforming prims.
TEST_F(SubObjectTransformTest, TransformUnits)
{
    // Our test file is in inches (metersPerUnit = 0.0254) and Z up.
    // We will temporarily change that programatically and observe the effect when moving a prim.
    auto       stage = stageObject->GetUSDStage();
    double     currentMeterPerUnit;
    const auto resetStageUnits = MaxUsd::MakeScopeGuard(
        [stage, &currentMeterPerUnit]() {
            currentMeterPerUnit = pxr::UsdGeomGetStageMetersPerUnit(stage);
        },
        [stage, &currentMeterPerUnit]() {
            pxr::UsdGeomSetStageMetersPerUnit(stage, currentMeterPerUnit);
        });

    // Test centimeters as stage units
    pxr::UsdGeomSetStageMetersPerUnit(stage, 0.01);

    const TimeValue time = 0;
    stageObject->TransformStart(time);

    auto       prim = stageObject->GetUSDStage()->GetPrimAtPath(primPath);
    const auto primPreTransform = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);
    Matrix3    tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate({ 66.0295792f, 18.3343010f, -26.6595020f });
    // Moving 10 inches.
    Point3 translation = { 0.f, 0.f, 10.f };
    auto   parentTm = node->GetNodeTM(time);

    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    const auto primPostTransform
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);

    // Verify that the prim indeed moved 10 units in the 3dsMax scene's space.
    auto primPreTransformInv = primPreTransform;
    primPreTransformInv.Invert();
    const auto delta = primPreTransformInv * primPostTransform;

    Matrix3 expected;
    expected.SetTranslate(
        translation); // Should move 10 units in 3dsMax, whatever the units in the stage.
    EXPECT_TRUE(delta.Equals(expected, MAX_FLOAT_EPSILON));
}

// Test that different up-axises between max and stage are accounted for when transforming prims.
TEST_F(SubObjectTransformTest, TransformUpAxis)
{
    // Our test file is in inches (metersPerUnit = 0.0254) and Z up.
    // We will temporarily change that programatically and observe the effect when moving a prim.
    auto         stage = stageObject->GetUSDStage();
    pxr::TfToken currentUpAxis;
    const auto   resetStageUnits = MaxUsd::MakeScopeGuard(
        [stage, &currentUpAxis]() { currentUpAxis = pxr::UsdGeomGetStageUpAxis(stage); },
        [stage, &currentUpAxis]() { pxr::UsdGeomSetStageUpAxis(stage, currentUpAxis); });

    // Test Y-up as up axis, differing from the max scene.
    pxr::UsdGeomSetStageUpAxis(stage, pxr::TfToken("Y"));

    const TimeValue time = 0;
    stageObject->TransformStart(time);

    auto       prim = stageObject->GetUSDStage()->GetPrimAtPath(primPath);
    const auto primPreTransform = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);
    Matrix3    tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate({ 66.0295792f, 18.3343010f, -26.6595020f });

    // Moving 10 inches.
    Point3 translation = { 0.f, 0.f, 10.f };
    auto   parentTm = node->GetNodeTM(time);

    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    const auto primPostTransform
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);

    // Verify that the prim indeed moved 10 units in the 3dsMax scene's space.
    auto primPreTransformInv = primPreTransform;
    primPreTransformInv.Invert();
    const auto delta = primPreTransformInv * primPostTransform;

    Matrix3 expected;
    // Moves 10 units toward Zup, whatever the up axis in the stage.
    expected.SetTranslate({ 0.f, 0.f, 10.f });
    EXPECT_TRUE(delta.Equals(expected, MAX_FLOAT_EPSILON));
}

// Base class for test of the xformOp configuration following manipulation in 3dsMax. Much simple
// scene, with a single box. However there are multiple xformOp in its local transform stack.
class SubObjectXformOpTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        // Keep track of current units so we can set them back properly. Just making sure we always
        // run the the with the same units.
        GetSystemUnitInfo(&unitType, &unitScale);
        SetSystemUnitInfo(UNITS_INCHES, 1.0f);

        auto       testDataPath = GetTestDataPath();
        const auto filePath = testDataPath.append("subobject_pivot_transform.usda");
        stageObject = static_cast<USDStageObject*>(
            GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
        node = GetCOREInterface()->CreateObjectNode(stageObject);
        stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

        HdMaxEngine             testEngine;
        MockRenderItemContainer renderItems;

        pxr::HdChangeTracker dummyTracker;
        auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
        displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);

        // Select the box.
        const auto     globalUfeSel = Ufe::GlobalSelection::get();
        Ufe::Selection newSelection;

        primPath = pxr::SdfPath("/root/Box001");

        auto ufeItem
            = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, primPath));
        newSelection.append(ufeItem);
        globalUfeSel->replaceWith(newSelection);

        // Switch to Prim sub-object mode.
        GetCOREInterface()->SelectNode(node);
        GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
        GetCOREInterface()->SetSubObjectLevel(1);

        // If currently holding, stop. The SubObjectTransformTest tests call undo and we only
        // want to undo the transform changes.
        if (theHold.Holding()) {
            theHold.Accept(L"Test setup");
        }
    }
    void TearDown() override
    {
        GetCOREInterface()->FileReset(TRUE);
        SetSystemUnitInfo(unitType, unitScale);
    }
    int   unitType;
    float unitScale;

    pxr::SdfPath    primPath;
    INode*          node;
    USDStageObject* stageObject;
};

// Testing that any pivot xform op found on the xformable's stack is respected.
TEST_F(SubObjectXformOpTest, PrimWithPivot)
{
    // Simulate a sub-object rotate operation initiated from the UI.

    const TimeValue time = 0;

    stageObject->TransformStart(time);

    const auto prim = stageObject->GetUSDStage()->GetPrimAtPath(primPath);
    const auto primPreTransform = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);

    Quat    rotation;
    AngAxis angAxis;
    angAxis.Set(1.f, 0.0f, 0.f, 1.5708f); // 90 degree rotation around X.
    rotation.Set(angAxis);
    auto parentTm = node->GetNodeTM(time);

    Matrix3 pivot; // The is the pivot set on the box in the usd file, 10 units up.
    pivot.SetTranslate({ 0, 0, 10 });

    stageObject->Rotate(time, parentTm, pivot, rotation, FALSE);
    stageObject->TransformFinish(time);

    const auto primPostTransform
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);

    auto primPreTransformInv = primPreTransform;
    primPreTransformInv.Invert();
    const auto delta = primPreTransformInv * primPostTransform;

    Matrix3 rotationMatrix;
    rotation.MakeMatrix(rotationMatrix);

    auto invPivot = pivot;
    invPivot.Invert();

    const Matrix3 expected = invPivot * rotationMatrix * pivot;
    EXPECT_TRUE(delta.Equals(expected, MAX_FLOAT_EPSILON));
}

TEST_F(SubObjectXformOpTest, CreateOrReuseXformOp)
{
    const auto prim = stageObject->GetUSDStage()->GetPrimAtPath(primPath);
    const auto xformable = pxr::UsdGeomXformable(prim);

    // Initially there are 4 ops on the stack :
    // transform, transform:t1, the pivot, and its inverse.
    bool resetStack;
    EXPECT_EQ(4, xformable.GetOrderedXformOps(&resetStack).size());

    const TimeValue time = 0;
    stageObject->TransformStart(time);

    const auto primPreTransform = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);

    auto    parentTm = node->GetNodeTM(time);
    Point3  translation = { 0.f, 0.f, 10.f };
    Matrix3 tmAxis;
    tmAxis.SetTranslate(primPreTransform.GetTrans());

    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);

    stageObject->TransformFinish(time);

    // Now we should have one more op, and it should be named t2.
    const auto ops = xformable.GetOrderedXformOps(&resetStack);
    EXPECT_EQ(5, ops.size());

    const auto lastOp = ops.back();
    EXPECT_EQ(lastOp.GetName(), pxr::TfToken("xformOp:transform:t2"));
    EXPECT_EQ(lastOp.GetNumTimeSamples(), 0); // authoring at the default time code.

    // Make sure its in the right place.
    const auto primPostTransform
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);
    auto primPreTransformInv = primPreTransform;
    primPreTransformInv.Invert();
    const auto delta = primPreTransformInv * primPostTransform;
    Matrix3    expected;
    expected.SetTranslate(translation);
    EXPECT_TRUE(delta.Equals(expected, MAX_FLOAT_EPSILON));

    // Move it another 10 units
    const auto primPreTransform2
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);

    stageObject->TransformStart(time);
    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    // Should have found and reused t2, not created a new op.
    auto ops2 = xformable.GetOrderedXformOps(&resetStack);
    EXPECT_EQ(5, ops2.size());
    auto lastOp2 = ops2.back();
    EXPECT_EQ(lastOp2.GetName(), pxr::TfToken("xformOp:transform:t2"));
    EXPECT_EQ(lastOp.GetNumTimeSamples(), 0); // authoring at the default time code.

    // Make sure its in the right place.
    const auto primPostTransform2
        = USDStageObject::GetMaxScenePrimTransform(node, prim, time, false);
    auto primPreTransformInv2 = primPreTransform2;
    primPreTransformInv2.Invert();
    const auto delta2 = primPreTransformInv2 * primPostTransform2;
    EXPECT_TRUE(delta2.Equals(expected, MAX_FLOAT_EPSILON));
}

TEST_F(SubObjectXformOpTest, MoveInDifferentCoordSystem)
{
    const TimeValue time = 0;
    // Give the node a non-trivial transform.
    Matrix3 transform;
    transform.SetScale({ 2.f, 2.f, 2.f });
    transform.RotateX(1.5708f);
    transform.SetTrans({ 0, 0, 5 });

    node->SetNodeTM(time, transform);

    const auto stage = stageObject->GetUSDStage();
    const auto prim = stage->GetPrimAtPath(primPath);
    const auto xformable = pxr::UsdGeomXformable(prim);

    stageObject->TransformStart(time);

    pxr::GfMatrix4d preTransform;
    bool            resetStack;
    xformable.GetLocalTransformation(
        &preTransform, &resetStack, MaxUsd::GetCurrentUsdTimeCode(stage));

    auto   parentTm = node->GetNodeTM(time);
    Point3 translation = { 0.f, 0.f, 10.f };

    // Setup the axis to simulate COORDS_LOCAL (transforming the prim in its local coordinate
    // system).
    Matrix3 tmAxis;
    tmAxis.SetRow(0, { 1.f, 0.f, 0.f });
    tmAxis.SetRow(1, { 0.f, 0.f, 1.f });
    tmAxis.SetRow(2, { 0.f, -1.f, 0.f });
    tmAxis.SetRow(3, { 0.f, -20.f, 5.f });

    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    // Make sure its in the right place. Here, unlike in other tests, we compare the local
    // transformation, because we set the axis transform to simulate the local coordsystem.
    pxr::GfMatrix4d postTransform;
    xformable.GetLocalTransformation(
        &postTransform, &resetStack, MaxUsd::GetCurrentUsdTimeCode(stage));

    const auto delta = MaxUsd::ToMaxMatrix3(preTransform.GetInverse() * postTransform);
    Matrix3    expected;

    // We applied a scaling of 2 on the node, so expect the local translation to compensate.
    expected.SetTranslate(translation / 2.0f);
    EXPECT_TRUE(delta.Equals(expected, MAX_FLOAT_EPSILON));
}

// Base class for tests involving moving multiple prims at the same time.
class SubObjectTransformMultipleTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        // Keep track of current units so we can set them back properly. Just making sure we always
        // run the test with the same units.
        GetSystemUnitInfo(&unitType, &unitScale);
        SetSystemUnitInfo(UNITS_INCHES, 1.0f);

        auto       testDataPath = GetTestDataPath();
        const auto filePath = testDataPath.append("subobject_transform_multiple.usda");
        stageObject = static_cast<USDStageObject*>(
            GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
        node = GetCOREInterface()->CreateObjectNode(stageObject);
        stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

        HdMaxEngine             testEngine;
        MockRenderItemContainer renderItems;

        pxr::HdChangeTracker dummyTracker;
        auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
        displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);

        box1Path = pxr::SdfPath("/root/Box001");
        box2Path = pxr::SdfPath("/root/Box002");
        box3Path = pxr::SdfPath("/root/Box003");
        box1item
            = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, box1Path));
        box2item
            = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, box2Path));
        box3item
            = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, box3Path));

        Matrix3 nodeTm;
        nodeTm.SetTranslate({ 0, 10, 0 });

        node->SetNodeTM(0, nodeTm);

        // Switch to Prim sub-object mode.
        GetCOREInterface()->SelectNode(node);
        GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
        GetCOREInterface()->SetSubObjectLevel(1);

        // If currently holding, stop. The SubObjectTransformTest tests call undo and we only
        // want to undo the transform changes.
        if (theHold.Holding()) {
            theHold.Accept(L"Test setup");
        }
    }
    void TearDown() override
    {
        GetCOREInterface()->FileReset(TRUE);
        SetSystemUnitInfo(unitType, unitScale);
    }
    int   unitType;
    float unitScale;

    pxr::SdfPath box1Path;
    pxr::SdfPath box2Path;
    pxr::SdfPath box3Path;

    Ufe::SceneItem::Ptr box1item;
    Ufe::SceneItem::Ptr box2item;
    Ufe::SceneItem::Ptr box3item;

    INode*          node;
    USDStageObject* stageObject;
};

class SubObjAxisCallbackMock : public SubObjAxisCallback
{
public:
    void Center(Point3 center, int id) override { this->center = center; }
    void TM(Matrix3 tm, int id) override { this->tm = tm; }
    int  Type() override { return 0; }

    Point3  center;
    Matrix3 tm;
};

// Check that centers are correctly computed with single and multi-selection.
TEST_F(SubObjectTransformMultipleTest, Centers)
{
    const auto globalUfeSel = Ufe::GlobalSelection::get();

    Ufe::Selection         newSelection;
    SubObjAxisCallbackMock cb;

    newSelection.append(box1item);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);
    EXPECT_TRUE(cb.center.Equals({ 10.f, 10.f, 10.f }));

    newSelection.clear();
    newSelection.append(box2item);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);
    EXPECT_TRUE(cb.center.Equals({ -10.f, 10.f, 10.f }));

    newSelection.clear();
    newSelection.append(box1item);
    newSelection.append(box2item);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);
    EXPECT_TRUE(cb.center.Equals({ 0.f, 10.f, 10.f }));
}

// Check that axises are correctly computed with single and multi-selection.
TEST_F(SubObjectTransformMultipleTest, TMs)
{
    const auto globalUfeSel = Ufe::GlobalSelection::get();

    Ufe::Selection         newSelection;
    SubObjAxisCallbackMock cb;

    newSelection.append(box2item);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectTMs(&cb, 0, node, nullptr);

    Matrix3 box2tm;
    box2tm.SetTranslate({ -10.f, 10.f, 10.f });
    EXPECT_TRUE(cb.tm.Equals(box2tm));

    newSelection.clear();
    newSelection.append(box3item);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectTMs(&cb, 0, node, nullptr);

    Matrix3 box3tm
        = { { -0.0, -1.0, 0.0 }, { 0.0, 0.0, 1.0 }, { -1.0, 0.0, 0.0 }, { 0.0, 10.0, 10.0 } };

    EXPECT_TRUE(cb.tm.Equals(box3tm));

    newSelection.clear();
    newSelection.append(box2item);
    newSelection.append(box3item);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectTMs(&cb, 0, node, nullptr);

    Matrix3 box2and3tm = { { 0.0, -1.0, 0.0 },
                           { 0.707106709, 0.0, 0.707106769 },
                           { -0.707106829, 0.0, 0.707106769 },
                           { -5.0, 10.0, 10.0 } };
    EXPECT_TRUE(cb.tm.Equals(box2and3tm));
}

TEST_F(SubObjectTransformMultipleTest, Move)
{
    const auto     globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    newSelection.append(box1item);
    newSelection.append(box2item);
    globalUfeSel->replaceWith(newSelection);

    SubObjAxisCallbackMock cb;
    stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);

    const TimeValue time = 0;
    stageObject->TransformStart(time);

    const auto box1 = stageObject->GetUSDStage()->GetPrimAtPath(box1Path);
    const auto primPreTransformBox1
        = USDStageObject::GetMaxScenePrimTransform(node, box1, time, false);

    const auto box2 = stageObject->GetUSDStage()->GetPrimAtPath(box2Path);
    const auto primPreTransformBox2
        = USDStageObject::GetMaxScenePrimTransform(node, box2, time, false);

    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate(cb.center);

    Point3 translation = { 0.f, 0.f, 5.f };
    auto   parentTm = node->GetNodeTM(time);

    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    const auto primPostTransformBox1
        = USDStageObject::GetMaxScenePrimTransform(node, box1, time, false);

    auto primPreTransformInvBox1 = primPreTransformBox1;
    primPreTransformInvBox1.Invert();
    const auto deltaBox1 = primPreTransformInvBox1 * primPostTransformBox1;

    const auto primPostTransformBox2
        = USDStageObject::GetMaxScenePrimTransform(node, box2, time, false);

    auto primPreTransformInvBox2 = primPreTransformBox2;
    primPreTransformInvBox2.Invert();
    const auto deltaBox2 = primPreTransformInvBox2 * primPostTransformBox2;

    Matrix3 expectedDelta;
    expectedDelta.SetTranslate(translation);
    EXPECT_TRUE(deltaBox1.Equals(expectedDelta, MAX_FLOAT_EPSILON));
    EXPECT_TRUE(deltaBox2.Equals(expectedDelta, MAX_FLOAT_EPSILON));
}

TEST_F(SubObjectTransformMultipleTest, Rotate)
{
    const auto     globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    newSelection.append(box1item);
    newSelection.append(box2item);
    globalUfeSel->replaceWith(newSelection);

    SubObjAxisCallbackMock cb;
    stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);

    const TimeValue time = 0;
    stageObject->TransformStart(time);

    const auto box1 = stageObject->GetUSDStage()->GetPrimAtPath(box1Path);
    const auto primPreTransformBox1
        = USDStageObject::GetMaxScenePrimTransform(node, box1, time, false);

    const auto box2 = stageObject->GetUSDStage()->GetPrimAtPath(box2Path);
    const auto primPreTransformBox2
        = USDStageObject::GetMaxScenePrimTransform(node, box2, time, false);

    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate({ 0.f, 10.f, 25.3999996f });

    auto parentTm = node->GetNodeTM(time);

    Quat rotation { 0.f, 0.f, -0.707106769f, 0.707106769f };

    stageObject->Rotate(time, parentTm, tmAxis, rotation, FALSE);
    stageObject->TransformFinish(time);

    const auto primPostTransformBox1
        = USDStageObject::GetMaxScenePrimTransform(node, box1, time, false);
    auto primPreTransformInvBox1 = primPreTransformBox1;
    primPreTransformInvBox1.Invert();
    const auto deltaBox1 = primPreTransformInvBox1 * primPostTransformBox1;

    const auto primPostTransformBox2
        = USDStageObject::GetMaxScenePrimTransform(node, box2, time, false);
    auto primPreTransformInvBox2 = primPreTransformBox2;
    primPreTransformInvBox2.Invert();
    const auto deltaBox2 = primPreTransformInvBox2 * primPostTransformBox2;

    // Both boxes should have been rotated the same way, around the same axis by 90 degrees.
    Matrix3 expectedDelta = { { 0, 1, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 10, 10, 0 } };
    EXPECT_TRUE(deltaBox1.Equals(expectedDelta, MAX_FLOAT_EPSILON));
    EXPECT_TRUE(deltaBox2.Equals(expectedDelta, MAX_FLOAT_EPSILON));
}

TEST_F(SubObjectTransformMultipleTest, Scale)
{
    const auto     globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    newSelection.append(box1item);
    newSelection.append(box2item);
    globalUfeSel->replaceWith(newSelection);

    SubObjAxisCallbackMock cb;
    stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);

    const TimeValue time = 0;
    stageObject->TransformStart(time);

    const auto box1 = stageObject->GetUSDStage()->GetPrimAtPath(box1Path);
    const auto primPreTransformBox1
        = USDStageObject::GetMaxScenePrimTransform(node, box1, time, false);

    const auto box2 = stageObject->GetUSDStage()->GetPrimAtPath(box2Path);
    const auto primPreTransformBox2
        = USDStageObject::GetMaxScenePrimTransform(node, box2, time, false);

    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate({ 0.f, 10.f, 25.3999996f });

    auto parentTm = node->GetNodeTM(time);

    Point3 scaling { 5.f, 5.f, 5.f };

    stageObject->Scale(time, parentTm, tmAxis, scaling, FALSE);
    stageObject->TransformFinish(time);

    const auto primPostTransformBox1
        = USDStageObject::GetMaxScenePrimTransform(node, box1, time, false);
    auto primPreTransformInvBox1 = primPreTransformBox1;
    primPreTransformInvBox1.Invert();
    const auto deltaBox1 = primPreTransformInvBox1 * primPostTransformBox1;

    const auto primPostTransformBox2
        = USDStageObject::GetMaxScenePrimTransform(node, box2, time, false);
    auto primPreTransformInvBox2 = primPreTransformBox2;
    primPreTransformInvBox2.Invert();
    const auto deltaBox2 = primPreTransformInvBox2 * primPostTransformBox2;

    // Both boxes should have been scaled the same way, from the same origin.
    // The translate delta in the transform component is (5-1)* -Point3{ 0.f, 10.f, 25.3999996f }
    Matrix3 expectedDelta = { { 5, 0, 0 }, { 0, 5, 0 }, { 0, 0, 5 }, { 0.f, -40.f, -101.6f } };
    EXPECT_TRUE(deltaBox1.Equals(expectedDelta, MAX_FLOAT_EPSILON));
    EXPECT_TRUE(deltaBox2.Equals(expectedDelta, MAX_FLOAT_EPSILON));
}

class GetXformablePrimsFromSelectionTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        // Keep track of current units so we can set them back properly. Just making sure we always
        // run the test with the same units.
        GetSystemUnitInfo(&unitType, &unitScale);
        SetSystemUnitInfo(UNITS_INCHES, 1.0f);

        auto       testDataPath = GetTestDataPath();
        const auto filePath
            = testDataPath.append("subobject_transform_multiple_what_xformable.usda");
        stageObject = static_cast<USDStageObject*>(
            GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
        node = GetCOREInterface()->CreateObjectNode(stageObject);
        stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

        HdMaxEngine             testEngine;
        MockRenderItemContainer renderItems;

        pxr::HdChangeTracker dummyTracker;
        auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
        displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);

        rootItem = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, root));
        fooItem = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, foo));
        barItem = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, bar));
        bazItem = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, baz));
        buzzItem = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, buzz));
        quxItem = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, qux));
        quuxItem = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, quux));
        corgeItem = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, corge));
        graultItem
            = Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, grault));

        // Switch to Prim sub-object mode.
        GetCOREInterface()->SelectNode(node);
        GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
        GetCOREInterface()->SetSubObjectLevel(1);

        // If currently holding, stop. The SubObjectTransformTest tests call undo and we only
        // want to undo the transform changes.
        if (theHold.Holding()) {
            theHold.Accept(L"Test setup");
        }
    }

    void TearDown() override
    {
        GetCOREInterface()->FileReset(TRUE);
        SetSystemUnitInfo(unitType, unitScale);
    }

    int   unitType;
    float unitScale;

    pxr::SdfPath root { "/root" };
    pxr::SdfPath foo { "/root/Foo" };
    pxr::SdfPath bar { "/root/Bar" };
    pxr::SdfPath baz { "/root/Bar/Baz" };
    pxr::SdfPath buzz { "/root/Bar/Buzz" };
    pxr::SdfPath qux { "/root/Bar/Qux" };
    pxr::SdfPath quux { "/root/Bar/Qux/Quux" };
    pxr::SdfPath corge { "/root/Bar/Qux/Corge" };
    pxr::SdfPath grault { "/root/Bar/Grault" };

    Ufe::SceneItem::Ptr rootItem;
    Ufe::SceneItem::Ptr fooItem;
    Ufe::SceneItem::Ptr barItem;
    Ufe::SceneItem::Ptr bazItem;
    Ufe::SceneItem::Ptr buzzItem;
    Ufe::SceneItem::Ptr quxItem;
    Ufe::SceneItem::Ptr quuxItem;
    Ufe::SceneItem::Ptr corgeItem;
    Ufe::SceneItem::Ptr graultItem;

    INode*          node;
    USDStageObject* stageObject;
};

TEST_F(GetXformablePrimsFromSelectionTest, TestFindXformablesToEdit)
{
    // When moving prims from a multi-selecttion, we need to find what prims to move,
    // those are the xformable at the top of substrees.

    /*
     * /root/Foo (Mesh)
     *      /Bar (Scope)
     *         /Baz (mesh)
     *		   /Buzz (mesh)
     *		   /Qux (Xform)
     *		       /Quux (Mesh)
     *			   /Corge (Mesh)
     *		   /Grault (Mesh)
     **/

    auto containsPath = [](const pxr::SdfPath&                              path,
                           const std::vector<USDStageObject::Transformable> transformable) {
        return transformable.end()
            != std::find_if(
                   transformable.begin(),
                   transformable.end(),
                   [&path](const USDStageObject::Transformable& t) {
                       return path == t.prim.GetPath();
                   });
    };

    const auto     globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    newSelection.append(rootItem);
    newSelection.append(barItem);
    newSelection.append(fooItem);
    newSelection.append(quxItem);
    newSelection.append(corgeItem);
    globalUfeSel->replaceWith(newSelection);
    auto transformables = stageObject->GetTransformablesFromSelection();
    EXPECT_EQ(1, transformables.size());
    EXPECT_TRUE(containsPath(root, transformables));

    newSelection.clear();
    newSelection.append(fooItem);
    newSelection.append(barItem); // Scope, not xformable.
    globalUfeSel->replaceWith(newSelection);
    transformables = stageObject->GetTransformablesFromSelection();
    EXPECT_EQ(1, transformables.size());
    EXPECT_TRUE(containsPath(foo, transformables));

    newSelection.clear();
    newSelection.append(fooItem);
    newSelection.append(barItem); // Scope, not xformable.
    newSelection.append(buzzItem);
    newSelection.append(quxItem);
    newSelection.append(corgeItem); // Qux selected, ignored.
    globalUfeSel->replaceWith(newSelection);
    transformables = stageObject->GetTransformablesFromSelection();
    EXPECT_EQ(3, transformables.size());
    EXPECT_TRUE(containsPath(foo, transformables));
    EXPECT_TRUE(containsPath(buzz, transformables));
    EXPECT_TRUE(containsPath(qux, transformables));

    newSelection.clear();
    newSelection.append(buzzItem);
    newSelection.append(corgeItem);
    newSelection.append(quuxItem);
    globalUfeSel->replaceWith(newSelection);
    transformables = stageObject->GetTransformablesFromSelection();
    EXPECT_EQ(3, transformables.size());
    EXPECT_TRUE(containsPath(buzz, transformables));
    EXPECT_TRUE(containsPath(corge, transformables));
    EXPECT_TRUE(containsPath(quux, transformables));
}

class SubObjectPointInstanceOpTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        // Keep track of current units so we can set them back properly. Just making sure we always
        // run the the with the same units.
        GetSystemUnitInfo(&unitType, &unitScale);
        SetSystemUnitInfo(UNITS_INCHES, 1.0f);

        auto       testDataPath = GetTestDataPath();
        const auto filePath = testDataPath.append("subobject_point_instance_transform.usda");
        stageObject = static_cast<USDStageObject*>(
            GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
        node = GetCOREInterface()->CreateObjectNode(stageObject);
        stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

        HdMaxEngine             testEngine;
        MockRenderItemContainer renderItems;

        pxr::HdChangeTracker dummyTracker;
        auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
        displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);

        // Switch to Prim sub-object mode.
        GetCOREInterface()->SelectNode(node);
        GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
        GetCOREInterface()->SetSubObjectLevel(1);

        // If currently holding, stop. The SubObjectTransformTest tests call undo and we only
        // want to undo the transform changes.
        if (theHold.Holding()) {
            theHold.Accept(L"Test setup");
        }
    }
    void TearDown() override
    {
        GetCOREInterface()->FileReset(TRUE);
        SetSystemUnitInfo(unitType, unitScale);
    }
    int   unitType;
    float unitScale;

    INode*          node;
    USDStageObject* stageObject;
};

TEST_F(SubObjectPointInstanceOpTest, PointInstanceMove)
{
    auto primPath = pxr::SdfPath("/InstancerAllAttrAuth");

    auto testMoveInstances = [this, &primPath](const std::vector<int>& indices) {
        const auto     globalUfeSel = Ufe::GlobalSelection::get();
        Ufe::Selection newSelection;
        for (const auto& idx : indices) {
            auto ufeItem = Ufe::Hierarchy::createItem(
                MaxUsd::ufe::getUsdPrimUfePath(stageObject, primPath, idx));
            newSelection.append(ufeItem);
        }
        globalUfeSel->replaceWith(newSelection);

        SubObjAxisCallbackMock cb;
        stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);

        const TimeValue time = 0;
        stageObject->TransformStart(time);

        const auto prim = stageObject->GetUSDStage()->GetPrimAtPath(primPath);

        const auto transformsPre
            = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);
        Matrix3 tmAxis = Matrix3::Identity;
        tmAxis.SetTranslate(cb.center);

        Point3 translation = { 0.f, 0.f, 5.f };
        auto   parentTm = node->GetNodeTM(time);

        stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
        stageObject->TransformFinish(time);

        const auto transformsPost
            = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);

        for (const auto& idx : indices) {
            auto invTrans = transformsPre[idx];
            invTrans.Invert();
            const auto delta = invTrans * transformsPost[idx];

            Matrix3 expectedDelta;
            expectedDelta.SetTranslate(translation);
            EXPECT_TRUE(delta.Equals(expectedDelta, MAX_FLOAT_EPSILON));
        }

        // Test undo
        ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded);
        const auto transformsPostUndo
            = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);
        for (int i = 0; i < transformsPre.size(); ++i) {
            EXPECT_TRUE(transformsPre[i].Equals(transformsPostUndo[i], MAX_FLOAT_EPSILON));
        }
    };

    // Single
    testMoveInstances({ 0 });
    // Multiple
    testMoveInstances({ 0, 1 });
}

TEST_F(SubObjectPointInstanceOpTest, PointInstanceRotate)
{
    auto primPath = pxr::SdfPath("/InstancerAllAttrAuth");

    auto testRotateInstances = [this, &primPath](const std::vector<int>& indices) {
        const auto     globalUfeSel = Ufe::GlobalSelection::get();
        Ufe::Selection newSelection;
        for (const auto& idx : indices) {
            auto ufeItem = Ufe::Hierarchy::createItem(
                MaxUsd::ufe::getUsdPrimUfePath(stageObject, primPath, idx));
            newSelection.append(ufeItem);
        }
        globalUfeSel->replaceWith(newSelection);

        SubObjAxisCallbackMock cb;
        stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);

        const TimeValue time = 0;
        stageObject->TransformStart(time);

        const auto prim = stageObject->GetUSDStage()->GetPrimAtPath(primPath);

        const auto transformsPre
            = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);
        Matrix3 tmAxis = Matrix3::Identity;
        tmAxis.SetTranslate(cb.center);

        Quat    rotation;
        AngAxis angAxis;
        angAxis.Set(1.f, 0.0f, 0.f, 1.5708f); // 90 degree rotation around X.
        rotation.Set(angAxis);
        auto parentTm = node->GetNodeTM(time);

        stageObject->Rotate(time, parentTm, tmAxis, rotation, FALSE);
        stageObject->TransformFinish(time);

        const auto transformsPost
            = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);

        const auto epsilon = 1E-3f;

        for (const auto& idx : indices) {
            auto invTrans = transformsPre[idx];
            invTrans.Invert();
            const auto delta = invTrans * transformsPost[idx];

            Matrix3 rotationMatrix;
            rotation.MakeMatrix(rotationMatrix);
            auto invTmAxis = tmAxis;
            invTmAxis.Invert();

            const Matrix3 expectedDelta = invTmAxis * rotationMatrix * tmAxis;

            EXPECT_TRUE(delta.Equals(expectedDelta, epsilon));
        }

        // Test undo
        ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded);
        const auto transformsPostUndo
            = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);
        for (int i = 0; i < transformsPre.size(); ++i) {
            EXPECT_TRUE(transformsPre[i].Equals(transformsPostUndo[i], epsilon));
        }
    };

    // Single
    testRotateInstances({ 0 });
    // Multiple
    testRotateInstances({ 0, 1 });
}

TEST_F(SubObjectPointInstanceOpTest, PointInstanceScale)
{
    auto primPath = pxr::SdfPath("/InstancerAllAttrAuth");

    auto testScaleInstances = [this, &primPath](const std::vector<int>& indices) {
        const auto     globalUfeSel = Ufe::GlobalSelection::get();
        Ufe::Selection newSelection;
        for (const auto& idx : indices) {
            auto ufeItem = Ufe::Hierarchy::createItem(
                MaxUsd::ufe::getUsdPrimUfePath(stageObject, primPath, idx));
            newSelection.append(ufeItem);
        }
        globalUfeSel->replaceWith(newSelection);

        SubObjAxisCallbackMock cb;
        stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);

        const TimeValue time = 0;
        stageObject->TransformStart(time);

        const auto prim = stageObject->GetUSDStage()->GetPrimAtPath(primPath);

        const auto transformsPre
            = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);
        Matrix3 tmAxis = Matrix3::Identity;
        tmAxis.SetTranslate(cb.center);

        auto parentTm = node->GetNodeTM(time);

        Point3 scaling { 5.f, 5.f, 5.f };
        stageObject->Scale(time, parentTm, tmAxis, scaling, FALSE);
        stageObject->TransformFinish(time);

        const auto transformsPost
            = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);

        for (const auto& idx : indices) {
            auto invTrans = transformsPre[idx];
            invTrans.Invert();
            const auto delta = invTrans * transformsPost[idx];

            Matrix3 scaleMatrix;
            scaleMatrix.SetScale(scaling);
            auto invTmAxis = tmAxis;
            invTmAxis.Invert();
            const Matrix3 expectedDelta = invTmAxis * scaleMatrix * tmAxis;

            EXPECT_TRUE(delta.Equals(expectedDelta, MAX_FLOAT_EPSILON));
        }

        // Test undo
        ExecuteMAXScriptScript(L"max undo", MAXScript::ScriptSource::NonEmbedded);
        const auto transformsPostUndo
            = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);
        for (int i = 0; i < transformsPre.size(); ++i) {
            EXPECT_TRUE(transformsPre[i].Equals(transformsPostUndo[i], MAX_FLOAT_EPSILON));
        }
    };

    // Single
    testScaleInstances({ 0 });
    // Multiple
    testScaleInstances({ 0, 1 });
}

TEST_F(SubObjectPointInstanceOpTest, PointInstancerMiscSetups)
{
    std::vector<pxr::SdfPath> instancers = { pxr::SdfPath("/InstancerAttrNotAuth"),
                                             pxr::SdfPath("/InstancerAttrAuthEmpty"),
                                             pxr::SdfPath("/InstancerAttrAuthAtTime") };

    for (const auto& path : instancers) {
        auto indices = { 0, 1 };

        const auto     globalUfeSel = Ufe::GlobalSelection::get();
        Ufe::Selection newSelection;
        for (const auto& idx : indices) {
            auto ufeItem = Ufe::Hierarchy::createItem(
                MaxUsd::ufe::getUsdPrimUfePath(stageObject, path, idx));
            newSelection.append(ufeItem);
        }
        globalUfeSel->replaceWith(newSelection);

        SubObjAxisCallbackMock cb;
        stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);
        const TimeValue time = 0;
        stageObject->TransformStart(time);

        const auto prim = stageObject->GetUSDStage()->GetPrimAtPath(path);

        const auto transformsPre
            = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);
        Matrix3 tmAxis = Matrix3::Identity;
        tmAxis.SetTranslate(cb.center);

        Point3 translation = { 0.f, 0.f, 5.f };
        auto   parentTm = node->GetNodeTM(time);

        stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
        stageObject->TransformFinish(time);

        const auto transformsPost
            = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);

        for (const auto& idx : indices) {
            auto invTrans = transformsPre[idx];
            invTrans.Invert();
            const auto delta = invTrans * transformsPost[idx];

            Matrix3 expectedDelta;
            expectedDelta.SetTranslate(translation);
            EXPECT_TRUE(delta.Equals(expectedDelta, MAX_FLOAT_EPSILON));
        }

        const auto instancer = pxr::UsdGeomPointInstancer { prim };

        const auto posAttr = instancer.GetPositionsAttr();
        ASSERT_TRUE(posAttr.IsAuthored());
        pxr::VtVec3fArray pos;
        posAttr.Get(&pos);
        EXPECT_EQ(pos.size(), 2);

        const auto oriAttr = instancer.GetOrientationsAttr();
        ASSERT_TRUE(posAttr.IsAuthored());
        pxr::VtQuathArray ori;
        oriAttr.Get(&ori);
        EXPECT_EQ(ori.size(), 2);

        const auto sclAttr = instancer.GetScalesAttr();
        ASSERT_TRUE(sclAttr.IsAuthored());
        pxr::VtVec3fArray scl;
        sclAttr.Get(&scl);
        EXPECT_EQ(scl.size(), 2);
    }
}

TEST_F(SubObjectPointInstanceOpTest, PointInstancerSubobjectCenters)
{
    const auto globalUfeSel = Ufe::GlobalSelection::get();

    auto instance1 = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath { "/SubobjCentersInstances" }, 0));
    auto instance2 = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath { "/SubobjCentersInstances" }, 1));
    auto prim = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath { "/SubobjCenterPrim" }));

    Ufe::Selection         newSelection;
    SubObjAxisCallbackMock cb;
    // Center from one instance.
    newSelection.append(instance1);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);
    EXPECT_TRUE(cb.center.Equals({ 10.f, 10.f, 10.f }));
    // Center from abg of 2 instances.
    newSelection.append(instance2);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);
    EXPECT_TRUE(cb.center.Equals({ 5.f, 5.f, 5.f }));

    // Test that mixing point instances and regular prims works also
    newSelection.append(prim);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);
    EXPECT_TRUE(cb.center.Equals({ 20.f, 20.f, 20.f }));
}

TEST_F(SubObjectPointInstanceOpTest, PointInstancerSubobjectTMs)
{
    // The math of the SubObjectTms is already tested in SubObjectTransformMultipleTest.TMs, here
    // we just want to make sure point instances are considered properly. So looking only at the
    // translation component is enough.

    const auto globalUfeSel = Ufe::GlobalSelection::get();

    auto instance1 = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath { "/SubobjCentersInstances" }, 0));
    auto instance2 = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath { "/SubobjCentersInstances" }, 1));
    auto prim = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath { "/SubobjCenterPrim" }));

    Ufe::Selection         newSelection;
    SubObjAxisCallbackMock cb;

    newSelection.append(instance1);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectTMs(&cb, 0, node, nullptr);

    Matrix3 instance1tm;
    instance1tm.SetTranslate({ 10.f, 10.f, 10.f });
    EXPECT_TRUE(cb.tm.Equals(instance1tm));

    newSelection.append(instance2);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectTMs(&cb, 0, node, nullptr);

    Matrix3 instance1and2tm;
    instance1and2tm.SetTranslate({ 5.f, 5.f, 5.f });
    EXPECT_TRUE(cb.tm.Equals(instance1and2tm));

    newSelection.append(prim);
    globalUfeSel->replaceWith(newSelection);
    stageObject->GetSubObjectTMs(&cb, 0, node, nullptr);

    Matrix3 instance1and2andPrimtm;
    instance1and2andPrimtm.SetTranslate({ 20.f, 20.f, 20.f });
    EXPECT_TRUE(cb.tm.Equals(instance1and2andPrimtm));
}

TEST_F(SubObjectPointInstanceOpTest, PointInstanceWithProtoXform)
{
    auto protoPath = pxr::SdfPath("/ProtoGreen");
    auto primPath = pxr::SdfPath("/InstancerAllAttrAuth");

    const auto stage = stageObject->GetUSDStage();

    const auto protoPrim = stage->GetPrimAtPath(protoPath);
    const auto protoXform = pxr::UsdGeomXformable { protoPrim };

    protoXform.AddTranslateOp().Set(pxr::GfVec3d { 10, 10, 0 });

    const auto indices = { 0 };

    const auto     globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    for (const auto& idx : indices) {
        auto ufeItem = Ufe::Hierarchy::createItem(
            MaxUsd::ufe::getUsdPrimUfePath(stageObject, primPath, idx));
        newSelection.append(ufeItem);
    }
    globalUfeSel->replaceWith(newSelection);

    SubObjAxisCallbackMock cb;
    stageObject->GetSubObjectCenters(&cb, 0, node, nullptr);

    const TimeValue time = 0;
    stageObject->TransformStart(time);

    const auto prim = stage->GetPrimAtPath(primPath);

    const auto transformsPre
        = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);
    Matrix3 tmAxis = Matrix3::Identity;
    tmAxis.SetTranslate(cb.center);

    Point3 translation = { 0.f, 0.f, 5.f };
    auto   parentTm = node->GetNodeTM(time);

    stageObject->Move(time, parentTm, tmAxis, translation, FALSE);
    stageObject->TransformFinish(time);

    const auto transformsPost
        = USDStageObject::GetMaxScenePointInstancesTransforms(node, prim, indices, 0);

    for (const auto& idx : indices) {
        auto invTrans = transformsPre[idx];
        invTrans.Invert();
        const auto delta = invTrans * transformsPost[idx];

        Matrix3 expectedDelta;
        expectedDelta.SetTranslate(translation);
        EXPECT_TRUE(delta.Equals(expectedDelta, MAX_FLOAT_EPSILON));
    }
}