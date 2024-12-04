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

#include <ufe/globalSelection.h>
#include <ufe/hierarchy.h>
#include <ufe/observableSelection.h>

// Utility function to test a render item's selection buffer state, fully selected or not (all ones
// or zeros)
void TestSelectionBufferState(
    const MaxSDK::Graphics::RenderItemHandle& renderItem,
    bool                                      selected,
    bool                                      decorated = true)
{
    const auto geom = GetRenderItemGeometry(renderItem, decorated, selected);
    auto       selBuffer = geom->GetVertexBuffer(HdMaxRenderData::SelectionBuffer);
    const auto selData
        = reinterpret_cast<Point3*>(selBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

    // While unselected, the selection buffer is expected to be all zeros, when selected, ones.
    auto expected = selected ? Point3(1.f, 1.f, 1.f) : Point3 {};

    EXPECT_TRUE(std::all_of(
        selData, selData + selBuffer.GetNumberOfVertices(), [&expected](const Point3& p) {
            return p.Equals(expected);
        }));

    selBuffer.Unlock();
};

// Testing basic selection display, looking at the selection buffer.
// Making sure that when something is selected, the buffer is filled with ones.
// Also test display behavior in and out of the prim sub-object mode.
TEST(SelectionBuffer, SelectDisplay)
{
    // Reset scene after test.
    const auto resetGuard
        = MaxUsd::MakeScopeGuard([]() {}, []() { GetCOREInterface()->FileReset(TRUE); });

    auto       testDataPath = GetTestDataPath();
    const auto filePath = testDataPath.append("selection_geometry.usda");
    const auto stageObject = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
    const auto node = GetCOREInterface()->CreateObjectNode(stageObject);
    stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

    HdMaxEngine             testEngine;
    MockRenderItemContainer renderItems;

    pxr::HdChangeTracker dummyTracker;
    auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
    displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);

    // The scene is composed of a single box.

    // Render, nothing is selected.
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });
    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());
    TestSelectionBufferState(renderItems.GetRenderItem(0), false);

    // Select the box.
    const auto     globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    auto           ufeItem = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath("/root/Box001")));
    newSelection.append(ufeItem);
    globalUfeSel->replaceWith(newSelection);

    // Render, the box is now selected - but we are not in the prim sub object mode, so
    // the selection should not be displayed.
    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });
    TestSelectionBufferState(renderItems.GetRenderItem(0), false);

    // Switch to Prim sub-object mode -> now showing selection.
    GetCOREInterface()->SelectNode(node);
    GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
    GetCOREInterface()->SetSubObjectLevel(1);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });
    TestSelectionBufferState(renderItems.GetRenderItem(0), true);

    // Switch back to object level -> no longer display selection.
    GetCOREInterface()->SetSubObjectLevel(0);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });
    TestSelectionBufferState(renderItems.GetRenderItem(0), false);

    // Toggle back to sub-object -> display selection again...
    GetCOREInterface()->SetSubObjectLevel(1);
    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });
    TestSelectionBufferState(renderItems.GetRenderItem(0), true);

    // Clear the UFE selection -> selection is cleared in the buffer.
    globalUfeSel->replaceWith({});

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });
    TestSelectionBufferState(renderItems.GetRenderItem(0), false);
}

// Testing selection within a consolidated mesh (selected prims will have corresponding parts of the
// selection buffer filled with ones).
TEST(SelectionBuffer, ConsolidatedMeshSelection)
{
    // Reset scene after test.
    const auto resetGuard
        = MaxUsd::MakeScopeGuard([]() {}, []() { GetCOREInterface()->FileReset(TRUE); });

    auto       testDataPath = GetTestDataPath();
    const auto filePath = testDataPath.append("selection_consolidated_geometry.usda");
    const auto stageObject = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
    const auto node = GetCOREInterface()->CreateObjectNode(stageObject);
    stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

    HdMaxEngine             testEngine;
    MockRenderItemContainer renderItems;

    // The scene is composed a box and a sphere.

    // Setup consolidation. Use a static delay of 0 so that consolidation happens immediately.
    HdMaxConsolidator::Config consolidationConfig;
    consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
    consolidationConfig.maxTriangles = 20000;
    consolidationConfig.maxCellSize = 200000;
    consolidationConfig.maxInstanceCount = 1000;
    consolidationConfig.staticDelay = 0;
    pxr::HdChangeTracker dummyTracker;
    auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
    displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);
    consolidationConfig.displaySettings = displaySettings;

    // Render, nothing is selected.
    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    // Single render item, as everything is consolidated.
    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());

    TestSelectionBufferState(renderItems.GetRenderItem(0), false, false);

    // Select the sphere and switch to prim sub-object.
    const auto&    globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    const auto     ufeItem = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath("/root/Sphere001")));
    newSelection.append(ufeItem);
    globalUfeSel->replaceWith(newSelection);

    GetCOREInterface()->SelectNode(node);
    GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
    GetCOREInterface()->SetSubObjectLevel(1);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());
    const auto renderItem = renderItems.GetRenderItem(0);

    const auto geom = GetRenderItemGeometry(renderItem, false, true);
    auto       selBuffer = geom->GetVertexBuffer(HdMaxRenderData::SelectionBuffer);
    const auto selData
        = reinterpret_cast<Point3*>(selBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

    // First 24 verts are those of the box, unselected.
    EXPECT_TRUE(
        std::all_of(selData, selData + 24, [](const Point3& p) { return p.Equals(Point3 {}); }));
    // The rest belong to the sphere, selected.
    EXPECT_TRUE(
        std::all_of(selData + 24, selData + selBuffer.GetNumberOfVertices(), [](const Point3& p) {
            return p.Equals(Point3 { 1.f, 1.f, 1.f });
        }));

    selBuffer.Unlock();

    // Clear the UFE selection -> selection is cleared in the buffer.
    globalUfeSel->replaceWith({});

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);
    TestSelectionBufferState(renderItems.GetRenderItem(0), false, false);
}

// Testing selection within a consolidated mesh built from instances.
TEST(SelectionBuffer, ConsolidatedInstancedGeometry)
{
    // Reset scene after test.
    const auto resetGuard
        = MaxUsd::MakeScopeGuard([]() {}, []() { GetCOREInterface()->FileReset(TRUE); });

    auto       testDataPath = GetTestDataPath();
    const auto filePath = testDataPath.append("selection_instanced_geometry.usda");
    const auto stageObject = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
    const auto node = GetCOREInterface()->CreateObjectNode(stageObject);
    stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

    HdMaxEngine             testEngine;
    MockRenderItemContainer renderItems;

    // The scene is composed a 3 instanced boxes, with the following consolidation settings, they
    // will all be consolidated.

    HdMaxConsolidator::Config consolidationConfig;
    consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
    consolidationConfig.maxTriangles = 20000;
    consolidationConfig.maxCellSize = 200000;
    consolidationConfig.maxInstanceCount = 1000;
    consolidationConfig.staticDelay = 0;
    pxr::HdChangeTracker dummyTracker;
    auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
    displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);
    consolidationConfig.displaySettings = displaySettings;

    // Render, nothing is selected.
    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    // Single render item, as everything is consolidated.
    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());

    TestSelectionBufferState(renderItems.GetRenderItem(0), false, false);

    // Select the 2nd box and switch to prim sub-object.
    const auto&    globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    const auto     ufeItem = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath("/root/Box002")));
    newSelection.append(ufeItem);
    globalUfeSel->replaceWith(newSelection);
    stageObject->UpdatePrimSelectionDisplay();
    GetCOREInterface()->SelectNode(node);
    GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
    GetCOREInterface()->SetSubObjectLevel(1);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    const auto renderItem = renderItems.GetRenderItem(0);

    const auto geom = GetRenderItemGeometry(renderItem, false, true);
    auto       selBuffer = geom->GetVertexBuffer(HdMaxRenderData::SelectionBuffer);
    const auto selData
        = reinterpret_cast<Point3*>(selBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

    // First 24 verts are those of the first box, unselected.
    EXPECT_TRUE(
        std::all_of(selData, selData + 24, [](const Point3& p) { return p.Equals(Point3 {}); }));
    // The next 24 verts are those of the second box, selected
    EXPECT_TRUE(std::all_of(selData + 24, selData + 48, [](const Point3& p) {
        return p.Equals(Point3 { 1.f, 1.f, 1.f });
    }));
    // The rest is the last box, unselected
    EXPECT_TRUE(
        std::all_of(selData + 48, selData + selBuffer.GetNumberOfVertices(), [](const Point3& p) {
            return p.Equals(Point3 {});
        }));

    selBuffer.Unlock();

    // Clear the UFE selection -> selection is cleared in the buffer.
    globalUfeSel->replaceWith({});

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);
    TestSelectionBufferState(renderItems.GetRenderItem(0), false, false);
}

// Testing selection of instances.
TEST(SelectionBuffer, InstancedGeometry)
{
    // Reset scene after test.
    const auto resetGuard
        = MaxUsd::MakeScopeGuard([]() {}, []() { GetCOREInterface()->FileReset(TRUE); });

    auto       testDataPath = GetTestDataPath();
    const auto filePath = testDataPath.append("selection_instanced_geometry.usda");
    const auto stageObject = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
    const auto node = GetCOREInterface()->CreateObjectNode(stageObject);
    stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

    HdMaxEngine             testEngine;
    MockRenderItemContainer renderItems;

    // The scene is composed a 3 instanced boxes

    // Disable consolidation explicitly to make sure instancing is used.
    HdMaxConsolidator::Config consolidationConfig;
    consolidationConfig.strategy = HdMaxConsolidator::Strategy::Off;

    // Render, nothing is selected.
    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    // Single instanced render item carrying all 3 instances.
    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());

    // Select the 2nd box and switch to prim sub-object.
    const auto&    globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    const auto     ufeItemBBox002 = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath("/root/Box002")));
    newSelection.append(ufeItemBBox002);
    globalUfeSel->replaceWith(newSelection);
    stageObject->UpdatePrimSelectionDisplay();
    GetCOREInterface()->SelectNode(node);
    GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
    GetCOREInterface()->SetSubObjectLevel(1);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    // Instance selection display is implemented using a different instance render item. So we now
    // expect 2 items.
    ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());

    // Selecting another box doesn't add a new render item, both selected boxes will display
    // their selection from the same same instance render item.
    const auto ufeItemBBox003 = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath("/root/Box003")));

    newSelection.append(ufeItemBBox003);
    globalUfeSel->replaceWith(newSelection);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());

    // Clear the UFE selection -> back to a single instancing render item.
    globalUfeSel->replaceWith({});
    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());
}

// Testing that selecting a parent prim displays children as selected.
TEST(SelectionBuffer, SelectHierarchy)
{
    // Reset scene after test.
    const auto resetGuard
        = MaxUsd::MakeScopeGuard([]() {}, []() { GetCOREInterface()->FileReset(TRUE); });

    auto       testDataPath = GetTestDataPath();
    const auto filePath = testDataPath.append("selection_hierarchy.usda");
    const auto stageObject = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
    const auto node = GetCOREInterface()->CreateObjectNode(stageObject);
    stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

    HdMaxEngine             testEngine;
    MockRenderItemContainer renderItems;

    pxr::HdChangeTracker dummyTracker;
    auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
    displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);

    // The scene is a hierarchy of boxes like so :
    // /root (Xform)
    //   /Box001 (Xform)
    //     /Box001_Shape (Mesh)
    //     /Box002 (Mesh)
    //     /Box003 (Xform)
    //        /Box003_Shape (Mesh)
    //        /Box004 (Mesh)

    // There are 4 meshes, this will result in 4 render items (consolidation not being used).

    // Render, nothing is selected.
    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });

    ASSERT_EQ(4, renderItems.GetNumberOfRenderItems());
    for (int i = 0; i < 4; ++i) {
        TestSelectionBufferState(renderItems.GetRenderItem(i), false);
    }

    // Select the root prim.
    const auto&    globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    const auto     rootItem = Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath("/root")));
    newSelection.append(rootItem);
    globalUfeSel->replaceWith(newSelection);

    // Switch to Prim sub-object mode -> now everything should show selected.
    GetCOREInterface()->SelectNode(node);
    GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
    GetCOREInterface()->SetSubObjectLevel(1);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });

    ASSERT_EQ(4, renderItems.GetNumberOfRenderItems());
    for (int i = 0; i < 4; ++i) {
        TestSelectionBufferState(renderItems.GetRenderItem(i), true);
    }

    // Clear the UFE selection -> selection is cleared in the buffer.
    globalUfeSel->replaceWith({});

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });

    ASSERT_EQ(4, renderItems.GetNumberOfRenderItems());
    for (int i = 0; i < 4; ++i) {
        TestSelectionBufferState(renderItems.GetRenderItem(i), false);
    }

    // Now test with a sub-tree. Selecting Box003 should also display selection for it child, Box004
    newSelection.clear();
    newSelection.append(Ufe::Hierarchy::createItem(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath("/root/Box001/Box003"))));
    globalUfeSel->replaceWith(newSelection);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });

    ASSERT_EQ(4, renderItems.GetNumberOfRenderItems());
    for (int i = 0; i < 4; ++i) {
        // Box003 and Box004 are at the last 2 indices.
        TestSelectionBufferState(renderItems.GetRenderItem(i), i > 1);
    }
}

// Testing that prims added or removed from view are correctly handled.
TEST(SelectionBuffer, SelectedPrimAddRemove)
{
    // Reset scene after test.
    const auto resetGuard
        = MaxUsd::MakeScopeGuard([]() {}, []() { GetCOREInterface()->FileReset(TRUE); });

    auto       testDataPath = GetTestDataPath();
    const auto filePath = testDataPath.append("selection_hierarchy.usda");
    const auto stageObject = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
    const auto node = GetCOREInterface()->CreateObjectNode(stageObject);
    stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

    HdMaxEngine             testEngine;
    MockRenderItemContainer renderItems;

    pxr::HdChangeTracker dummyTracker;
    auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
    displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);

    // The scene is a hierarchy of boxes like so :
    // /root (Xform)
    //   /Box001 (Xform)
    //     /Box001_Shape (Mesh)
    //     /Box002 (Mesh)
    //     /Box003 (Xform)
    //        /Box003_Shape (Mesh)
    //        /Box004 (Mesh)

    // There are 4 meshes, this will result in 4 render items (consolidation not being used).

    // Render, nothing is selected.
    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });

    ASSERT_EQ(4, renderItems.GetNumberOfRenderItems());
    for (int i = 0; i < 4; ++i) {
        TestSelectionBufferState(renderItems.GetRenderItem(i), false);
    }

    // Switch to Prim sub-object mode so that selection is displayed.
    GetCOREInterface()->SelectNode(node);
    GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
    GetCOREInterface()->SetSubObjectLevel(1);

    // Deactivate the subtree under Box003
    const auto box3Path = pxr::SdfPath("/root/Box001/Box003");

    auto box3 = stageObject->GetUSDStage()->GetPrimAtPath(box3Path);
    box3.SetActive(false);

    // Select box 3 and render.
    const auto&    globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    newSelection.append(
        Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, box3Path)));
    globalUfeSel->replaceWith(newSelection);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });

    // Box3 being inactive, we only have 2 render items.
    ASSERT_EQ(2, renderItems.GetNumberOfRenderItems());
    for (int i = 0; i < 2; ++i) {
        TestSelectionBufferState(renderItems.GetRenderItem(i), false);
    }

    // Reactivate box3, and render again, should show selected.
    box3.SetActive(true);
    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull });

    // Back to 4 render item, Box003 and Box004 are selected even if selection was
    // done while they were inactive.
    ASSERT_EQ(4, renderItems.GetNumberOfRenderItems());
    for (int i = 0; i < 4; ++i) {
        TestSelectionBufferState(renderItems.GetRenderItem(i), i > 1);
    }
}

// Testing that the selection display handles internal instance indices change correctly
TEST(SelectionBuffer, InstancesIndexChange)
{
    // Reset scene after test.
    const auto resetGuard
        = MaxUsd::MakeScopeGuard([]() {}, []() { GetCOREInterface()->FileReset(TRUE); });

    auto       testDataPath = GetTestDataPath();
    const auto filePath = testDataPath.append("selection_instanced_geometry.usda");
    const auto stageObject = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, STAGE_CLASS_ID));
    const auto node = GetCOREInterface()->CreateObjectNode(stageObject);
    stageObject->SetRootLayer(filePath.generic_wstring().c_str(), L"/");

    HdMaxEngine             testEngine;
    MockRenderItemContainer renderItems;

    // The scene is composed a 3 instanced boxes, with the following consolidation settings, they
    // will all be consolidated. The idea is to toggle the second box, this will change the instance
    // index of the third box, and we look at the consolidated instances to know if all went well.

    HdMaxConsolidator::Config consolidationConfig;
    consolidationConfig.strategy = HdMaxConsolidator::Strategy::Static;
    consolidationConfig.maxTriangles = 20000;
    consolidationConfig.maxCellSize = 200000;
    consolidationConfig.maxInstanceCount = 1000;
    consolidationConfig.staticDelay = 0;
    pxr::HdChangeTracker dummyTracker;
    auto&                displaySettings = testEngine.GetRenderDelegate()->GetDisplaySettings();
    displaySettings.SetDisplayMode(HdMaxDisplaySettings::WireColor, dummyTracker);
    consolidationConfig.displaySettings = displaySettings;

    // Render, nothing is selected.
    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    // Single render item, as everything is consolidated.
    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());

    TestSelectionBufferState(renderItems.GetRenderItem(0), false, false);

    // Select the first and last boxes and switch to prim sub-object.

    const auto box1Path = pxr::SdfPath("/root/Box001");
    const auto box3Path = pxr::SdfPath("/root/Box003");

    const auto&    globalUfeSel = Ufe::GlobalSelection::get();
    Ufe::Selection newSelection;
    newSelection.append(
        Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, box1Path)));
    newSelection.append(
        Ufe::Hierarchy::createItem(MaxUsd::ufe::getUsdPrimUfePath(stageObject, box3Path)));
    globalUfeSel->replaceWith(newSelection);

    GetCOREInterface()->SelectNode(node);
    GetCOREInterface()->SetCommandPanelTaskMode(TASK_MODE_MODIFY);
    GetCOREInterface()->SetSubObjectLevel(1);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    auto geom = GetRenderItemGeometry(renderItems.GetRenderItem(0), false, true);
    auto selBuffer = geom->GetVertexBuffer(HdMaxRenderData::SelectionBuffer);
    auto selData = reinterpret_cast<Point3*>(selBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

    // First 24 verts are those of the first box, selected
    EXPECT_TRUE(std::all_of(
        selData, selData + 24, [](const Point3& p) { return p.Equals(Point3 { 1.f, 1.f, 1.f }); }));
    // The next 24 verts are those of the second box, unselected
    EXPECT_TRUE(std::all_of(
        selData + 24, selData + 48, [](const Point3& p) { return p.Equals(Point3 {}); }));
    // The rest is the last box, selected
    EXPECT_TRUE(
        std::all_of(selData + 48, selData + selBuffer.GetNumberOfVertices(), [](const Point3& p) {
            return p.Equals(Point3 { 1.f, 1.f, 1.f });
        }));

    selBuffer.Unlock();

    const auto stage = stageObject->GetUSDStage();
    auto       box2 = stage->GetPrimAtPath(pxr::SdfPath("/root/Box002"));
    box2.SetActive(false);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);

    // Box002 was the only unselected box, now all should be selected.
    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());
    TestSelectionBufferState(renderItems.GetRenderItem(0), true, false);

    // Reactivate the box002 instance and deactivate box001
    box2.SetActive(true);
    auto box1 = stage->GetPrimAtPath(box1Path);
    box1.SetActive(false);

    stageObject->UpdatePrimSelectionDisplay();
    TestRender(
        stageObject->GetUSDStage(),
        *(stageObject->GetHydraEngine()),
        renderItems,
        0,
        nullptr,
        { pxr::HdReprTokens->smoothHull },
        consolidationConfig);
    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());

    geom = GetRenderItemGeometry(renderItems.GetRenderItem(0), false, true);
    selBuffer = geom->GetVertexBuffer(HdMaxRenderData::SelectionBuffer);
    selData = reinterpret_cast<Point3*>(selBuffer.Lock(0, 0, MaxSDK::Graphics::ReadAcess));

    ASSERT_EQ(1, renderItems.GetNumberOfRenderItems());

    // First 24 verts are those of box002, still unselected
    EXPECT_TRUE(
        std::all_of(selData, selData + 24, [](const Point3& p) { return p.Equals(Point3 {}); }));
    // The rest is box3, selected.
    EXPECT_TRUE(
        std::all_of(selData + 24, selData + selBuffer.GetNumberOfVertices(), [](const Point3& p) {
            return p.Equals(Point3 { 1.f, 1.f, 1.f });
        }));
}