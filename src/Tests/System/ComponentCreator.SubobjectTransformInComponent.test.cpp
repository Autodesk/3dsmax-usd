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

#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/UiUtils.h>

#include <usdUfe/ufe/UsdSceneItem.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/propertySpec.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include <ufe/globalSelection.h>
#include <ufe/observableSelection.h>
#include <ufe/selection.h>

#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <algorithm>
#include <cmath>
#include <GetCOREInterface.h>
#include <hold.h>

using namespace CCTest;
namespace cc = AdskUsdComponentCreator;

namespace {

// First Mesh prim under /root/geo (robust to the export-root nesting divergence).
pxr::UsdPrim FindGeoMesh(const pxr::UsdStageRefPtr& stage)
{
    const pxr::SdfPath geo("/root/geo");
    for (const auto& p : stage->Traverse()) {
        if (p.GetTypeName() == pxr::TfToken("Mesh") && p.GetPath().HasPrefix(geo)) {
            return p;
        }
    }
    return pxr::UsdPrim();
}

// Build a single-cube component, target its default variant (so /root/geo composes and
// forwarding has a destination), and select the geo mesh through the UFE global
// selection (what sub-object Prim selection does). Returns the stage object plus the
// path of the prim's xformOp:transform override that the gizmo authors. Asserts on
// failure via gtest, so callers should ASSERT_NE(comp, nullptr).
USDStageObject* SetupComponentWithSelectedMesh(
    const wchar_t*       cubeName,
    pxr::UsdStageRefPtr& stage,
    pxr::SdfPath&        xformAttrPath)
{
    USDStageObject* comp = CreateComponentFromCube(cubeName);
    if (!comp) {
        return nullptr;
    }
    stage = comp->GetUSDStage();
    if (!stage) {
        return nullptr;
    }

    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    EXPECT_TRUE(desc);
    if (!desc) {
        return nullptr;
    }
    const auto& vs = desc->GetVariantSets().begin()->second;
    const auto  variantDesc = vs->GetVariantDescription(vs->GetDefaultVariant());
    EXPECT_TRUE(variantDesc);
    EXPECT_TRUE(cc::ComponentAPI::SetVariantTarget(desc, cc::VariantNesting { vs, variantDesc }));

    const pxr::UsdPrim mesh = FindGeoMesh(stage);
    EXPECT_TRUE(mesh.IsValid()) << "no Mesh prim under /root/geo to transform";
    if (!mesh.IsValid()) {
        return nullptr;
    }
    xformAttrPath = mesh.GetPath().AppendProperty(pxr::TfToken("xformOp:transform"));

    const Ufe::Path ufePath = MaxUsd::ufe::getUsdPrimUfePath(comp, mesh.GetPath());
    const auto      item = UsdUfe::UsdSceneItem::create(ufePath, mesh);
    EXPECT_TRUE(item);
    Ufe::GlobalSelection::get()->clear();
    Ufe::GlobalSelection::get()->append(item);
    return comp;
}

// Every Mesh prim under /root/geo (a component built from several nodes has more than one).
std::vector<pxr::UsdPrim> FindGeoMeshes(const pxr::UsdStageRefPtr& stage)
{
    const pxr::SdfPath        geo("/root/geo");
    std::vector<pxr::UsdPrim> meshes;
    for (const auto& p : stage->Traverse()) {
        if (p.GetTypeName() == pxr::TfToken("Mesh") && p.GetPath().HasPrefix(geo)) {
            meshes.push_back(p);
        }
    }
    return meshes;
}

// The opinions on `attrPath` authored outside the session layer -- i.e. what edit forwarding
// pushed into the targeted variant.
pxr::SdfPropertySpecHandleVector
ForwardedSpecs(const pxr::UsdStageRefPtr& stage, const pxr::SdfPath& attrPath)
{
    const auto attr = stage->GetAttributeAtPath(attrPath);
    if (!attr) {
        return {};
    }
    pxr::SdfPropertySpecHandleVector forwarded;
    for (const auto& spec : attr.GetPropertyStack(pxr::UsdTimeCode::Default())) {
        if (spec->GetLayer() != stage->GetSessionLayer()) {
            forwarded.push_back(spec);
        }
    }
    return forwarded;
}

// The strongest matrix opinion on `attrPath` authored outside the session layer. False when
// nothing has been forwarded (yet).
bool ForwardedMatrix(
    const pxr::UsdStageRefPtr& stage,
    const pxr::SdfPath&        attrPath,
    pxr::GfMatrix4d&           value)
{
    const auto specs = ForwardedSpecs(stage, attrPath);
    if (specs.empty()) {
        return false;
    }
    const pxr::VtValue defaultValue = specs.front()->GetDefaultValue();
    if (!defaultValue.IsHolding<pxr::GfMatrix4d>()) {
        return false;
    }
    value = defaultValue.UncheckedGet<pxr::GfMatrix4d>();
    return true;
}

// Composed value of a matrix xformOp (identity when unauthored).
pxr::GfMatrix4d ComposedMatrix(const pxr::UsdStageRefPtr& stage, const pxr::SdfPath& attrPath)
{
    pxr::GfMatrix4d value(1.0);
    if (const auto attr = stage->GetAttributeAtPath(attrPath)) {
        attr.Get(&value, pxr::UsdTimeCode::Default());
    }
    return value;
}

bool IsAlmostEqual(const pxr::GfMatrix4d& a, const pxr::GfMatrix4d& b)
{
    return pxr::GfIsClose(a, b, 1e-6);
}

// Drive an interactive translation of `steps` gizmo steps and leave the drag open -- the caller
// commits it (TransformFinish) or throws it away (TransformCancel). The event loop is pumped
// between steps, as it is during a real drag.
void DragMove(USDStageObject* comp, TimeValue t, int steps)
{
    comp->TransformStart(t);
    for (int step = 1; step <= steps; ++step) {
        Matrix3 partm;  // identity by default
        Matrix3 tmAxis; // identity by default
        Point3  val(10.f * step, 0.f, 0.f);
        comp->Move(t, partm, tmAxis, val, FALSE);
        PumpIdle();
    }
}

// A single known undo entry, pushed right before the operation under test: `node` moves to
// z = kSentinelZ. Once the undo that reverts the operation has run, one further undo must bring
// the node back -- which only holds if the operation took exactly one undo entry.
constexpr float kSentinelZ = 123.f;

void PushUndoSentinel(INode* node)
{
    if (theHold.Holding()) {
        theHold.Accept(L"Test setup");
    }
    Matrix3 tm = node->GetNodeTM(0);
    tm.SetTrans(Point3(0.f, 0.f, kSentinelZ));
    theHold.Begin();
    node->SetNodeTM(0, tm);
    theHold.Accept(L"Undo sentinel");
}

bool UndoSentinelStillSet(INode* node)
{
    return std::abs(node->GetNodeTM(0).GetTrans().z - kSentinelZ) < 1e-3f;
}

} // namespace

// Moving a prim in sub-object mode must forward the resulting xformOp override out of
// the component's session layer and into the targeted variant.
TEST(ComponentCreatorSubobjectTransformInComponent, MoveForwardsToVariant)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCube(L"XfCube");
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);

    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    // Target the default variant so /root/geo composes and forwarding has a destination.
    const auto& vs = desc->GetVariantSets().begin()->second;
    const auto  variantDesc = vs->GetVariantDescription(vs->GetDefaultVariant());
    ASSERT_TRUE(variantDesc);
    ASSERT_TRUE(cc::ComponentAPI::SetVariantTarget(desc, cc::VariantNesting { vs, variantDesc }));

    const pxr::UsdPrim mesh = FindGeoMesh(stage);
    ASSERT_TRUE(mesh.IsValid()) << "no Mesh prim under /root/geo to transform";
    const pxr::SdfPath meshPath = mesh.GetPath();
    const pxr::SdfPath xformAttrPath = meshPath.AppendProperty(pxr::TfToken("xformOp:transform"));

    // Select the prim through the UFE global selection (what sub-object Prim selection does).
    const Ufe::Path ufePath = MaxUsd::ufe::getUsdPrimUfePath(comp, meshPath);
    const auto      item = UsdUfe::UsdSceneItem::create(ufePath, mesh);
    ASSERT_TRUE(item);
    Ufe::GlobalSelection::get()->clear();
    Ufe::GlobalSelection::get()->append(item);

    const TimeValue t = GetCOREInterface()->GetTime();
    comp->TransformStart(t);

    // Drive a translation move, as the viewport gizmo would.
    Matrix3 partm;  // identity by default
    Matrix3 tmAxis; // identity by default
    Point3  val(50.f, 0.f, 0.f);
    comp->Move(t, partm, tmAxis, val, FALSE);

    // The interactive move authors the override into the session layer (edit target).
    EXPECT_TRUE(stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath))
        << "interactive sub-object move should author into the session layer";

    comp->TransformFinish(t);
    PumpIdle();

    // After the forward the edit must have moved into the variant, leaving the session layer clean
    // (mirrors the add-prim forwarding behaviour).
    EXPECT_FALSE(stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath))
        << "sub-object move must be forwarded out of the session layer into the variant";
}

// Rotating a prim in sub-object mode must forward the resulting xformOp:transform out of the
// session layer, exactly like the move above. (Move/rotate/scale all funnel through the same
// interactive-transform path, so this is parity coverage for the rotate gizmo.)
TEST(ComponentCreatorSubobjectTransformInComponent, RotateForwardsToVariant)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    pxr::UsdStageRefPtr stage;
    pxr::SdfPath        xformAttrPath;
    USDStageObject*     comp = SetupComponentWithSelectedMesh(L"RotCube", stage, xformAttrPath);
    ASSERT_NE(comp, nullptr);

    const TimeValue t = GetCOREInterface()->GetTime();
    comp->TransformStart(t);

    // Drive a rotation about Z, as the viewport gizmo would.
    Matrix3 partm;  // identity by default
    Matrix3 tmAxis; // identity by default
    Quat    val(AngAxis(Point3(0.f, 0.f, 1.f), 0.5f));
    comp->Rotate(t, partm, tmAxis, val, FALSE);

    EXPECT_TRUE(stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath))
        << "interactive sub-object rotate should author into the session layer";

    comp->TransformFinish(t);
    PumpIdle();

    EXPECT_FALSE(stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath))
        << "sub-object rotate must be forwarded out of the session layer into the variant";
}

// Scaling a prim in sub-object mode must forward the resulting xformOp:transform out of the
// session layer, exactly like the move/rotate above (parity coverage for the scale gizmo).
TEST(ComponentCreatorSubobjectTransformInComponent, ScaleForwardsToVariant)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    pxr::UsdStageRefPtr stage;
    pxr::SdfPath        xformAttrPath;
    USDStageObject*     comp = SetupComponentWithSelectedMesh(L"ScaleCube", stage, xformAttrPath);
    ASSERT_NE(comp, nullptr);

    const TimeValue t = GetCOREInterface()->GetTime();
    comp->TransformStart(t);

    // Drive a uniform scale, as the viewport gizmo would.
    Matrix3 partm;  // identity by default
    Matrix3 tmAxis; // identity by default
    Point3  val(2.f, 2.f, 2.f);
    comp->Scale(t, partm, tmAxis, val, FALSE);

    EXPECT_TRUE(stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath))
        << "interactive sub-object scale should author into the session layer";

    comp->TransformFinish(t);
    PumpIdle();

    EXPECT_FALSE(stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath))
        << "sub-object scale must be forwarded out of the session layer into the variant";
}

// An in-progress drag must not forward: the override belongs to the session layer until the
// transform is committed.
TEST(ComponentCreatorSubobjectTransformInComponent, InteractiveDragDoesNotForwardUntilFinish)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    pxr::UsdStageRefPtr stage;
    pxr::SdfPath        xformAttrPath;
    USDStageObject*     comp = SetupComponentWithSelectedMesh(L"DragCube", stage, xformAttrPath);
    ASSERT_NE(comp, nullptr);

    pxr::GfMatrix4d forwardedBefore;
    const bool      hadForwardedBefore = ForwardedMatrix(stage, xformAttrPath, forwardedBefore);

    const TimeValue t = GetCOREInterface()->GetTime();
    DragMove(comp, t, 3);

    EXPECT_TRUE(stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath))
        << "an interactive move should author into the session layer";
    pxr::GfMatrix4d forwardedDuring;
    const bool      hasForwardedDuring = ForwardedMatrix(stage, xformAttrPath, forwardedDuring);
    EXPECT_EQ(hasForwardedDuring, hadForwardedBefore)
        << "an in-progress drag must not forward into the variant";
    if (hadForwardedBefore && hasForwardedDuring) {
        EXPECT_TRUE(IsAlmostEqual(forwardedDuring, forwardedBefore))
            << "an in-progress drag must not change what the variant holds";
    }
    const pxr::GfMatrix4d draggedValue = ComposedMatrix(stage, xformAttrPath);

    comp->TransformFinish(t);
    PumpIdle();

    EXPECT_FALSE(stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath))
        << "committing the transform must forward the override out of the session layer";
    pxr::GfMatrix4d forwardedAfter;
    ASSERT_TRUE(ForwardedMatrix(stage, xformAttrPath, forwardedAfter))
        << "committing the transform must author the override in the variant";
    EXPECT_TRUE(IsAlmostEqual(forwardedAfter, draggedValue))
        << "the forwarded value must be the value the drag ended on";
}

// A multi-step drag and its forward are one operation: a single undo reverts all of it, and the
// undo entry right underneath is still the one that preceded the drag.
TEST(ComponentCreatorSubobjectTransformInComponent, MultiStepDragForwardsOnceAndUndoesInOneStep)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    INode* sentinel = MakeCubeNode(L"UndoSentinel");
    ASSERT_NE(sentinel, nullptr);

    pxr::UsdStageRefPtr stage;
    pxr::SdfPath        xformAttrPath;
    USDStageObject*     comp = SetupComponentWithSelectedMesh(L"StepCube", stage, xformAttrPath);
    ASSERT_NE(comp, nullptr);

    const pxr::GfMatrix4d initialValue = ComposedMatrix(stage, xformAttrPath);
    PushUndoSentinel(sentinel);
    ASSERT_TRUE(UndoSentinelStillSet(sentinel));

    const TimeValue t = GetCOREInterface()->GetTime();
    DragMove(comp, t, 5);
    const pxr::GfMatrix4d draggedValue = ComposedMatrix(stage, xformAttrPath);
    ASSERT_FALSE(IsAlmostEqual(draggedValue, initialValue))
        << "the drag should have moved the prim";
    comp->TransformFinish(t);
    PumpIdle();

    EXPECT_FALSE(stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath));
    pxr::GfMatrix4d forwarded;
    ASSERT_TRUE(ForwardedMatrix(stage, xformAttrPath, forwarded));
    EXPECT_TRUE(IsAlmostEqual(forwarded, draggedValue));

    // One undo for the whole drag. Per-move forwarding used to leave five forward entries stacked
    // on top of the transform, so this undo only popped the last of them.
    MaxUndo();
    PumpIdle();
    EXPECT_TRUE(IsAlmostEqual(ComposedMatrix(stage, xformAttrPath), initialValue))
        << "a single undo must revert the whole drag, forward included";
    EXPECT_TRUE(UndoSentinelStillSet(sentinel)) << "one undo must not reach past the drag";

    MaxUndo();
    PumpIdle();
    EXPECT_FALSE(UndoSentinelStillSet(sentinel))
        << "the drag and its forward must occupy exactly one undo entry";
}

// Edit forwarding is paused for the whole of undo AND redo (the forward is replayed by the
// command's own undo entry). A redo that re-forwarded would push a fresh undo entry on top of the
// transform, so the next undo would no longer be the transform's.
TEST(ComponentCreatorSubobjectTransformInComponent, RedoOfForwardedTransformDoesNotDoubleForward)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    INode* sentinel = MakeCubeNode(L"UndoSentinel");
    ASSERT_NE(sentinel, nullptr);

    pxr::UsdStageRefPtr stage;
    pxr::SdfPath        xformAttrPath;
    USDStageObject*     comp = SetupComponentWithSelectedMesh(L"RedoCube", stage, xformAttrPath);
    ASSERT_NE(comp, nullptr);

    const pxr::GfMatrix4d initialValue = ComposedMatrix(stage, xformAttrPath);
    PushUndoSentinel(sentinel);

    const TimeValue t = GetCOREInterface()->GetTime();
    DragMove(comp, t, 1);
    const pxr::GfMatrix4d draggedValue = ComposedMatrix(stage, xformAttrPath);
    comp->TransformFinish(t);
    PumpIdle();

    MaxUndo();
    PumpIdle();
    ASSERT_TRUE(IsAlmostEqual(ComposedMatrix(stage, xformAttrPath), initialValue))
        << "precondition: undo reverted the transform and its forward";

    MaxRedo();
    PumpIdle();
    EXPECT_TRUE(IsAlmostEqual(ComposedMatrix(stage, xformAttrPath), draggedValue))
        << "redo must restore the transform";
    EXPECT_FALSE(stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath))
        << "redo must restore the override to the variant, not the session layer";
    pxr::GfMatrix4d forwardedAfterRedo;
    ASSERT_TRUE(ForwardedMatrix(stage, xformAttrPath, forwardedAfterRedo));
    EXPECT_TRUE(IsAlmostEqual(forwardedAfterRedo, draggedValue));

    MaxUndo();
    PumpIdle();
    EXPECT_TRUE(IsAlmostEqual(ComposedMatrix(stage, xformAttrPath), initialValue))
        << "redo must not push an extra forward undo entry on top of the transform";
    EXPECT_TRUE(UndoSentinelStillSet(sentinel));

    MaxUndo();
    PumpIdle();
    EXPECT_FALSE(UndoSentinelStillSet(sentinel))
        << "the transform must still occupy exactly one undo entry after an undo/redo round trip";
}

// The commit re-authors the drag-start value before the final one to force the layer dirty (the
// drag already wrote the final value, so a plain re-set would not reach the forwarder). That
// transient value must never be what ends up forwarded.
TEST(ComponentCreatorSubobjectTransformInComponent, ForwardedValueIsFinalNotDragStart)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    pxr::UsdStageRefPtr stage;
    pxr::SdfPath        xformAttrPath;
    USDStageObject*     comp = SetupComponentWithSelectedMesh(L"FinalCube", stage, xformAttrPath);
    ASSERT_NE(comp, nullptr);

    const pxr::GfMatrix4d initialValue = ComposedMatrix(stage, xformAttrPath);

    const TimeValue t = GetCOREInterface()->GetTime();
    DragMove(comp, t, 3);
    const pxr::GfMatrix4d draggedValue = ComposedMatrix(stage, xformAttrPath);
    ASSERT_FALSE(IsAlmostEqual(draggedValue, initialValue));
    comp->TransformFinish(t);
    PumpIdle();

    pxr::GfMatrix4d forwarded;
    ASSERT_TRUE(ForwardedMatrix(stage, xformAttrPath, forwarded));
    EXPECT_TRUE(IsAlmostEqual(forwarded, draggedValue))
        << "the variant must hold the final drag value";
    EXPECT_FALSE(IsAlmostEqual(forwarded, initialValue))
        << "the drag-start value re-authored to force the layer dirty must not be forwarded";
    EXPECT_TRUE(IsAlmostEqual(ComposedMatrix(stage, xformAttrPath), draggedValue))
        << "the composed transform must not snap back when the forward runs";
}

// Pins the premise behind the forced dirty: by the time the transform is committed the session
// layer already holds the final value, so the commit writes a value that is not changing. Without
// the forced dirty the forwarder never sees a change and the override is stranded in the session.
TEST(ComponentCreatorSubobjectTransformInComponent, CommitForwardsValueDragAlreadyAuthored)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    pxr::UsdStageRefPtr stage;
    pxr::SdfPath        xformAttrPath;
    USDStageObject*     comp = SetupComponentWithSelectedMesh(L"DirtyCube", stage, xformAttrPath);
    ASSERT_NE(comp, nullptr);

    const TimeValue t = GetCOREInterface()->GetTime();
    DragMove(comp, t, 2);

    const auto sessionSpec = stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath);
    ASSERT_TRUE(sessionSpec);
    const pxr::VtValue sessionValue = sessionSpec->GetDefaultValue();
    ASSERT_TRUE(sessionValue.IsHolding<pxr::GfMatrix4d>());
    const pxr::GfMatrix4d draggedValue = ComposedMatrix(stage, xformAttrPath);
    EXPECT_TRUE(IsAlmostEqual(sessionValue.UncheckedGet<pxr::GfMatrix4d>(), draggedValue))
        << "the drag should already have authored the value the commit will re-author";

    comp->TransformFinish(t);
    PumpIdle();

    EXPECT_FALSE(stage->GetSessionLayer()->GetAttributeAtPath(xformAttrPath))
        << "the commit must forward even though it re-authors an unchanged value";
    pxr::GfMatrix4d forwarded;
    ASSERT_TRUE(ForwardedMatrix(stage, xformAttrPath, forwarded));
    EXPECT_TRUE(IsAlmostEqual(forwarded, draggedValue));
}

// A cancelled drag (right-click during a move) commits nothing: no forward into the variant and no
// undo entry left behind.
TEST(ComponentCreatorSubobjectTransformInComponent, CancelledDragForwardsNothing)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    INode* sentinel = MakeCubeNode(L"UndoSentinel");
    ASSERT_NE(sentinel, nullptr);

    pxr::UsdStageRefPtr stage;
    pxr::SdfPath        xformAttrPath;
    USDStageObject*     comp = SetupComponentWithSelectedMesh(L"CancelCube", stage, xformAttrPath);
    ASSERT_NE(comp, nullptr);

    const pxr::GfMatrix4d initialValue = ComposedMatrix(stage, xformAttrPath);

    pxr::GfMatrix4d forwardedBefore;
    const bool      hadForwardedBefore = ForwardedMatrix(stage, xformAttrPath, forwardedBefore);

    PushUndoSentinel(sentinel);

    const TimeValue t = GetCOREInterface()->GetTime();
    DragMove(comp, t, 2);
    comp->TransformCancel(t);
    PumpIdle();

    EXPECT_TRUE(IsAlmostEqual(ComposedMatrix(stage, xformAttrPath), initialValue))
        << "cancelling a drag must leave the prim where it was";
    pxr::GfMatrix4d forwardedAfter;
    const bool      hasForwardedAfter = ForwardedMatrix(stage, xformAttrPath, forwardedAfter);
    EXPECT_EQ(hasForwardedAfter, hadForwardedBefore)
        << "a cancelled drag must not author anything in the variant";
    if (hadForwardedBefore && hasForwardedAfter) {
        EXPECT_TRUE(IsAlmostEqual(forwardedAfter, forwardedBefore));
    }

    // The sentinel is the top undo entry only if the cancelled drag pushed none of its own, so
    // this one undo must reach it.
    MaxUndo();
    PumpIdle();
    EXPECT_FALSE(UndoSentinelStillSet(sentinel))
        << "a cancelled drag must not leave an undo entry behind";
}

// Transforming a multi-selection goes through one composite command, so both prims forward under
// the same capture scope and the whole thing is a single undo entry.
TEST(ComponentCreatorSubobjectTransformInComponent, MultiSelectionDragForwardsAsOneStep)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    INode* sentinel = MakeCubeNode(L"UndoSentinel");
    ASSERT_NE(sentinel, nullptr);

    USDStageObject* comp = CreateComponentFromCubes({ L"MultiA", L"MultiB" });
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    const std::vector<pxr::UsdPrim> meshes = FindGeoMeshes(stage);
    ASSERT_GE(meshes.size(), 2u) << "expected a component holding at least two meshes";

    std::vector<pxr::SdfPath>    attrPaths;
    std::vector<pxr::GfMatrix4d> initialValues;
    Ufe::GlobalSelection::get()->clear();
    for (size_t i = 0; i < 2; ++i) {
        const pxr::UsdPrim& mesh = meshes[i];
        attrPaths.push_back(mesh.GetPath().AppendProperty(pxr::TfToken("xformOp:transform")));
        initialValues.push_back(ComposedMatrix(stage, attrPaths.back()));
        const auto item = UsdUfe::UsdSceneItem::create(
            MaxUsd::ufe::getUsdPrimUfePath(comp, mesh.GetPath()), mesh);
        ASSERT_TRUE(item);
        Ufe::GlobalSelection::get()->append(item);
    }

    PushUndoSentinel(sentinel);

    const TimeValue t = GetCOREInterface()->GetTime();
    DragMove(comp, t, 2);
    std::vector<pxr::GfMatrix4d> draggedValues;
    for (const auto& attrPath : attrPaths) {
        draggedValues.push_back(ComposedMatrix(stage, attrPath));
    }
    comp->TransformFinish(t);
    PumpIdle();

    for (size_t i = 0; i < attrPaths.size(); ++i) {
        EXPECT_FALSE(stage->GetSessionLayer()->GetAttributeAtPath(attrPaths[i]))
            << "both prims of the multi-selection must be forwarded out of the session layer";
        pxr::GfMatrix4d forwarded;
        ASSERT_TRUE(ForwardedMatrix(stage, attrPaths[i], forwarded));
        EXPECT_TRUE(IsAlmostEqual(forwarded, draggedValues[i]));
    }

    // One undo for the drag and both forwards; the sentinel underneath must survive it.
    MaxUndo();
    PumpIdle();
    for (size_t i = 0; i < attrPaths.size(); ++i) {
        EXPECT_TRUE(IsAlmostEqual(ComposedMatrix(stage, attrPaths[i]), initialValues[i]))
            << "a single undo must revert the whole multi-selection transform";
    }
    EXPECT_TRUE(UndoSentinelStillSet(sentinel)) << "one undo must not reach past the drag";

    // The next undo does reach the sentinel -- which only holds if the drag took one entry.
    MaxUndo();
    PumpIdle();
    EXPECT_FALSE(UndoSentinelStillSet(sentinel))
        << "a multi-selection drag and both its forwards must be one undo entry";
}

// Shift-dragging clones the selection before transforming it: the clone lands next to its
// source, and the source survives.
TEST(ComponentCreatorSubobjectTransformInComponent, ShiftDragClonesPrimInComponent)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    pxr::UsdStageRefPtr stage;
    pxr::SdfPath        xformAttrPath;
    USDStageObject*     comp = SetupComponentWithSelectedMesh(L"CloneCube", stage, xformAttrPath);
    ASSERT_NE(comp, nullptr);

    const pxr::SdfPath meshPath = xformAttrPath.GetPrimPath();
    const pxr::UsdPrim parent = stage->GetPrimAtPath(meshPath.GetParentPath());
    ASSERT_TRUE(parent);
    std::vector<pxr::SdfPath> childrenBefore;
    for (const auto& child : parent.GetChildren()) {
        childrenBefore.push_back(child.GetPath());
    }

    MaxUsd::Ui::SetIsShiftPressedFunction([]() { return true; });
    const auto shiftGuard
        = MaxUsd::MakeScopeGuard([]() {}, []() { MaxUsd::Ui::SetIsShiftPressedFunction({}); });

    const TimeValue t = GetCOREInterface()->GetTime();
    DragMove(comp, t, 1);
    comp->TransformFinish(t);
    PumpIdle();

    std::vector<pxr::SdfPath> added;
    for (const auto& child : parent.GetChildren()) {
        if (std::find(childrenBefore.begin(), childrenBefore.end(), child.GetPath())
            == childrenBefore.end()) {
            added.push_back(child.GetPath());
        }
    }
    EXPECT_EQ(added.size(), 1u) << "shift-drag should add exactly one clone next to the source";
    EXPECT_TRUE(stage->GetPrimAtPath(meshPath)) << "the source prim must survive the clone";
    EXPECT_EQ(Ufe::GlobalSelection::get()->size(), 1u) << "the clone becomes the new selection";
}

// Same, for a multi-selection: every selected prim is cloned, not just the first.
TEST(ComponentCreatorSubobjectTransformInComponent, ShiftDragClonesMultiSelectionInComponent)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    USDStageObject* comp = CreateComponentFromCubes({ L"CloneA", L"CloneB", L"CloneC" });
    ASSERT_NE(comp, nullptr);
    const pxr::UsdStageRefPtr stage = comp->GetUSDStage();
    ASSERT_TRUE(stage);
    const auto desc = cc::ComponentDescription::CreateFromStageMetadata(stage);
    ASSERT_TRUE(desc);
    SetDefaultVariantTarget(desc);

    std::vector<pxr::UsdPrim> meshes = FindGeoMeshes(stage);
    ASSERT_GE(meshes.size(), 3u) << "expected a component holding at least three meshes";
    meshes.resize(3);

    const pxr::UsdPrim parent = stage->GetPrimAtPath(meshes.front().GetPath().GetParentPath());
    ASSERT_TRUE(parent);
    std::vector<pxr::SdfPath> childrenBefore;
    for (const auto& child : parent.GetChildren()) {
        childrenBefore.push_back(child.GetPath());
    }

    Ufe::GlobalSelection::get()->clear();
    for (const auto& mesh : meshes) {
        const auto item = Item(comp, stage, mesh.GetPath());
        ASSERT_TRUE(item);
        Ufe::GlobalSelection::get()->append(item);
    }

    MaxUsd::Ui::SetIsShiftPressedFunction([]() { return true; });
    const auto shiftGuard
        = MaxUsd::MakeScopeGuard([]() {}, []() { MaxUsd::Ui::SetIsShiftPressedFunction({}); });

    const TimeValue t = GetCOREInterface()->GetTime();
    DragMove(comp, t, 1);
    comp->TransformFinish(t);
    PumpIdle();

    size_t added = 0;
    for (const auto& child : parent.GetChildren()) {
        if (std::find(childrenBefore.begin(), childrenBefore.end(), child.GetPath())
            == childrenBefore.end()) {
            ++added;
        }
    }
    EXPECT_EQ(added, meshes.size()) << "shift-dragging three prims must clone all three";
    for (const auto& mesh : meshes) {
        EXPECT_TRUE(stage->GetPrimAtPath(mesh.GetPath()))
            << "the source prims must survive the clone";
    }
    EXPECT_EQ(Ufe::GlobalSelection::get()->size(), meshes.size())
        << "the clones become the new selection";
}
