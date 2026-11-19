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

#pragma once

// Shared helpers for the Component Creator C++ system tests.
// Implemented in ComponentCreatorTestHelpers.cpp.

#include <usdUfe/ufe/UsdSceneItem.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>

#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <set>
#include <string>
#include <vector>

class INode;
class Point3;
class USDStageObject;

namespace CCTest {

// Build a unit cube as an Editable Poly node named `name` (real geometry for the
// export pipeline).
INode* MakeCubeNode(const wchar_t* name);

// Every scene node whose base object is a USDStageObject.
std::set<INode*> SnapshotStageObjectNodes();

// The one stage-object node added since `before` was captured, or null.
INode* FindNewStageObjectNode(const std::set<INode*>& before);

// Resolve the USDStageObject backing a node (or null).
USDStageObject* StageObjectOf(INode* node);

// Create a component from a single new cube and return its USD Stage Object (or
// null). Goes through the real create-from-nodes flow (which targets the default
// variant). Use for the create/add tests.
USDStageObject* CreateComponentFromCube(const wchar_t* cubeName);

// Like CreateComponentFromCube but from several new cubes in one create operation
// (real multi-selection create). Use for multi-element / variant-resident tests.
USDStageObject* CreateComponentFromCubes(const std::vector<const wchar_t*>& cubeNames);

// Create a multi-variant component from several new cubes: each cube becomes its own
// variant in the component's first variant set (real multi-selection multi-variant
// create). Returns the new USD Stage Object (or null).
USDStageObject* CreateMultiVariantsComponentFromCubes(const std::vector<const wchar_t*>& cubeNames);

// Create a "plain" component (one default variant, no variant target) and a USD
// Stage Object pointing at its root layer. Use for the *-in-component tests
// (delete/rename/reparent/add-prim/invariants). Returns the stage object or null.
USDStageObject* MakeComponentStageObject();

// Diagnostic dump of a stage -- composed prims (with type) plus the session and root
// layer contents -- for use in test failure messages.
std::string DumpStage(const pxr::UsdStageRefPtr& stage);

// Target the default variant of the component's first variant set so subsequent edits
// forward into it.
void SetDefaultVariantTarget(const AdskUsdComponentCreator::ComponentDescription::Ptr& desc);

// True if the stage has a valid prim at `path`.
bool Valid(const pxr::UsdStageRefPtr& stage, const char* path);

// Run 3ds Max's native undo / redo (via MAXScript).
void MaxUndo();
void MaxRedo();

// Build a skinned setup: a segmented box driven by a two-bone chain, as in a rigged
// character -- the mesh and the bone root are siblings at the top of the scene, not
// parent and child. Returns the mesh and the bone root, either null on failure.
void MakeSkinnedSetup(INode*& mesh, INode*& boneRoot);

// Key a rotation on the named node over frames 0-10.
void AnimateNodeRotation(const wchar_t* name);

// The UsdSceneItem (UFE path + prim) for `path` in the component's stage object.
UsdUfe::UsdSceneItem::Ptr
Item(USDStageObject* comp, const pxr::UsdStageRefPtr& stage, const pxr::SdfPath& path);

// Pump the Qt event loop so deferred (idle-scheduled) edit-forwards run before asserting.
void PumpIdle();

// The transform authored locally on `primPath`. Identity when the prim is missing or carries no
// xform ops, so assert the prim exists separately when that distinction matters.
pxr::GfMatrix4d LocalTransform(const pxr::UsdStageRefPtr& stage, const char* primPath);

// The local transform expected of a prim exported from an unrotated node placed at
// `maxTranslation`. Exporting Z-up Max content into a Y-up stage bakes the axis change into
// every prim's own transform, so the expectation is rotated, not just permuted.
pxr::GfMatrix4d
ExpectedLocalTransform(const pxr::UsdStageRefPtr& stage, const Point3& maxTranslation);

} // namespace CCTest
