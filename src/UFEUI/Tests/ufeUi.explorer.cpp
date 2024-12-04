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
#include "testUfeRuntime.h"
#include "ufeUiTestBase.h"
#include "utils.h"

#include <UFEUI/Views/explorer.h>
#include <UFEUI/treeitem.h>

#include <ufe/scene.h>
#include <ufe/sceneNotification.h>

#include <gtest/gtest.h>

class ExplorerTest : public UfeUiBaseTest
{
protected:
    void SetUp() override
    {
        UfeUiBaseTest::SetUp();
        // Explorer widget with our test hierarchy used as base for testing.
        const auto              root = Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root"));
        const UfeUi::TypeFilter typeFilter;
        Ufe::Hierarchy::ChildFilter childFilter;
        _testExplorer = new UfeUi::Explorer(root, {}, typeFilter, childFilter, false, "", {});
    }
    void TearDown() override
    {
        delete _testExplorer;
        _testExplorer = nullptr;
        UfeUiBaseTest::TearDown();
    }
    UfeUi::Explorer* _testExplorer = nullptr;
};

TEST_F(ExplorerTest, ExplorerObserver_updateFilter)
{
    const auto model = _testExplorer->treeModel();
    const auto treeItemRoot = model->root();

    // No filter active.
    EXPECT_EQ(treeItemRoot->sceneItem(), nullptr);
    EXPECT_EQ(treeItemRoot->childCount(), 1);
    EXPECT_EQ(treeItemRoot->child(0)->childCount(), 3);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->childCount(), 2);
    EXPECT_EQ(treeItemRoot->child(0)->child(1)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(0)->child(2)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->child(0)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->child(1)->childCount(), 2);

    // Set the test fitler (filters A2,B2,C2).
    auto filter = UfeUiTest::TestHierarchy::childFilter();
    _testExplorer->setChildFilter(filter);
    EXPECT_EQ(treeItemRoot->sceneItem(), nullptr);
    EXPECT_EQ(treeItemRoot->childCount(), 1);
    EXPECT_EQ(treeItemRoot->child(0)->childCount(), 2);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->childCount(), 1);
    EXPECT_EQ(treeItemRoot->child(0)->child(1)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->child(0)->childCount(), 0);

    // Disable the filter.
    filter.front().value = false;
    _testExplorer->setChildFilter(filter);
    EXPECT_EQ(treeItemRoot->sceneItem(), nullptr);
    EXPECT_EQ(treeItemRoot->childCount(), 1);
    EXPECT_EQ(treeItemRoot->child(0)->childCount(), 3);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->childCount(), 2);
    EXPECT_EQ(treeItemRoot->child(0)->child(1)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(0)->child(2)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->child(0)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->child(1)->childCount(), 2);

    // Re-enable.
    filter.front().value = true;
    _testExplorer->setChildFilter(filter);
    EXPECT_EQ(treeItemRoot->sceneItem(), nullptr);
    EXPECT_EQ(treeItemRoot->childCount(), 1);
    EXPECT_EQ(treeItemRoot->child(0)->childCount(), 2);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->childCount(), 1);
    EXPECT_EQ(treeItemRoot->child(0)->child(1)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->child(0)->childCount(), 0);
}