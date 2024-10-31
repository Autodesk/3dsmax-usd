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

#include <QtCore/QCoreApplication>
#include <gtest/gtest.h>

class ExplorerObserverTest : public UfeUiBaseTest
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

namespace {

// Simulate a new object added from a runtime.
void _addSceneItem(const Ufe::Path& path)
{
    const auto newObject = Ufe::Hierarchy::createItem(path);
    Ufe::Scene::instance().notify(Ufe::ObjectAdd { newObject });
}

// Simulate the removal of an object.
void _removeSceneItem(const Ufe::Path& path)
{
    UfeUiTest::TestHierarchy::removeChild(path);
    const auto removedObject = Ufe::Hierarchy::createItem(path);
    Ufe::Scene::instance().notify(Ufe::ObjectPostDelete { removedObject });
}

} // namespace

TEST_F(ExplorerObserverTest, ExplorerObserver_objectAdded)
{
    const auto pseudoRoot = _testExplorer->treeModel()->root()->child(0);
    ASSERT_EQ(pseudoRoot->childCount(), 3);

    // Keep track of current children, to make sure those are kept, and not re-created.
    const auto c1 = pseudoRoot->child(0);
    const auto c2 = pseudoRoot->child(1);
    const auto c3 = pseudoRoot->child(2);

    const auto newObjectPath = UfeUiTest::getUfePath("/root/new");
    _addSceneItem(newObjectPath);

    EXPECT_EQ(pseudoRoot->childCount(), 4);
    EXPECT_EQ(c1, pseudoRoot->child(0));
    EXPECT_EQ(c2, pseudoRoot->child(1));
    EXPECT_EQ(c3, pseudoRoot->child(2));

    const auto idx = _testExplorer->treeModel()->getIndexFromPath(newObjectPath);
    ASSERT_TRUE(idx.isValid());
    const auto treeItem = _testExplorer->treeModel()->treeItem(idx);
    ASSERT_NE(nullptr, treeItem);
    EXPECT_EQ(newObjectPath, treeItem->sceneItem()->path());
    EXPECT_EQ(treeItem, pseudoRoot->child(3));

    // Add a few more!
    _addSceneItem(UfeUiTest::getUfePath("/root/foo"));
    _addSceneItem(UfeUiTest::getUfePath("/root/bar"));
    _addSceneItem(UfeUiTest::getUfePath("/root/baz"));

    EXPECT_EQ(pseudoRoot->childCount(), 7);
}

TEST_F(ExplorerObserverTest, ExplorerObserver_allObjectsRemoved)
{
    const auto pseudoRoot = _testExplorer->treeModel()->root();
    ASSERT_EQ(pseudoRoot->child(0)->childCount(), 3);

    // Remove A1,A2,A3 from the UFE scene..
    _removeSceneItem(UfeUiTest::TestHierarchy::A1);
    _removeSceneItem(UfeUiTest::TestHierarchy::A2);
    _removeSceneItem(UfeUiTest::TestHierarchy::A3);

    ASSERT_EQ(pseudoRoot->child(0)->childCount(), 0);
}

TEST_F(ExplorerObserverTest, ExplorerObserver_objectsRemoved)
{
    const auto pseudoRoot = _testExplorer->treeModel()->root()->child(0);
    ASSERT_EQ(pseudoRoot->childCount(), 3);

    const auto a1 = pseudoRoot->child(0);
    const auto a2 = pseudoRoot->child(1);
    const auto b2 = pseudoRoot->child(0)->child(1);
    const auto c2 = pseudoRoot->child(0)->child(1)->child(1);

    // Remove some object at each level.
    _removeSceneItem(UfeUiTest::TestHierarchy::C1);
    _removeSceneItem(UfeUiTest::TestHierarchy::B1);
    _removeSceneItem(UfeUiTest::TestHierarchy::A3);

    ASSERT_EQ(pseudoRoot->childCount(), 2);
    EXPECT_EQ(pseudoRoot->child(0), a1);
    EXPECT_EQ(pseudoRoot->child(1), a2);
    ASSERT_EQ(pseudoRoot->child(0)->childCount(), 1);
    EXPECT_EQ(pseudoRoot->child(0)->child(0), b2);
    ASSERT_EQ(pseudoRoot->child(0)->child(0)->childCount(), 1);
    EXPECT_EQ(pseudoRoot->child(0)->child(0)->child(0), c2);
}

TEST_F(ExplorerObserverTest, ExplorerObserver_objectsRemovedNoop)
{
    const auto pseudoRoot = _testExplorer->treeModel()->root()->child(0);
    ASSERT_EQ(pseudoRoot->childCount(), 3);

    // Remove an object at each level, going down the hierarchy
    _removeSceneItem(UfeUiTest::TestHierarchy::A1);
    _removeSceneItem(UfeUiTest::TestHierarchy::B1); // Already removed, child of A1
    _removeSceneItem(UfeUiTest::TestHierarchy::C1); // Already removed child of B2 (child of A1)

    ASSERT_EQ(pseudoRoot->childCount(), 2);
    ASSERT_EQ(pseudoRoot->child(0)->childCount(), 0);
    ASSERT_EQ(pseudoRoot->child(1)->childCount(), 0);
}

TEST_F(ExplorerObserverTest, ExplorerObserver_subTreeInvalidate)
{
    const auto pseudoRoot = _testExplorer->treeModel()->root()->child(0);

    ASSERT_EQ(pseudoRoot->childCount(), 3);

    // Keep track of current children, to make sure those are kept, and not re-created.
    const auto c1 = pseudoRoot->child(0);
    const auto c2 = pseudoRoot->child(1);
    const auto c3 = pseudoRoot->child(2);

    // Build a brand new subtree under A1.
    auto fooPath = UfeUiTest::getUfePath("/root/A1/foo");
    auto barPath = UfeUiTest::getUfePath("/root/A1/foo/bar");
    auto bazPath = UfeUiTest::getUfePath("/root/A1/foo/baz");

    UfeUiTest::TestHierarchy::clearChildren(UfeUiTest::TestHierarchy::A1);
    UfeUiTest::TestHierarchy::addChild(UfeUiTest::TestHierarchy::A1, fooPath);
    UfeUiTest::TestHierarchy::addChild(fooPath, barPath);
    UfeUiTest::TestHierarchy::addChild(fooPath, bazPath);

    Ufe::Scene::instance().notify(
        Ufe::SubtreeInvalidate { Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::A1) });

    ASSERT_TRUE(
        _testExplorer->treeModel()->getIndexFromPath(UfeUiTest::TestHierarchy::A1).isValid());

    // Previous subtree is gone.
    ASSERT_FALSE(
        _testExplorer->treeModel()->getIndexFromPath(UfeUiTest::TestHierarchy::B1).isValid());
    ASSERT_FALSE(
        _testExplorer->treeModel()->getIndexFromPath(UfeUiTest::TestHierarchy::B2).isValid());
    ASSERT_FALSE(
        _testExplorer->treeModel()->getIndexFromPath(UfeUiTest::TestHierarchy::C1).isValid());
    ASSERT_FALSE(
        _testExplorer->treeModel()->getIndexFromPath(UfeUiTest::TestHierarchy::C2).isValid());

    // New subtree!
    ASSERT_TRUE(_testExplorer->treeModel()->getIndexFromPath(fooPath).isValid());
    ASSERT_TRUE(_testExplorer->treeModel()->getIndexFromPath(barPath).isValid());
    ASSERT_TRUE(_testExplorer->treeModel()->getIndexFromPath(bazPath).isValid());

    EXPECT_EQ(c1, pseudoRoot->child(0));
    EXPECT_EQ(c2, pseudoRoot->child(1));
    EXPECT_EQ(c3, pseudoRoot->child(2));

    // Change it back!
    UfeUiTest::TestHierarchy::resetTestHierarchy();
    Ufe::Scene::instance().notify(
        Ufe::SubtreeInvalidate { Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::A1) });

    ASSERT_TRUE(
        _testExplorer->treeModel()->getIndexFromPath(UfeUiTest::TestHierarchy::A1).isValid());

    // Base test subtree is back.
    ASSERT_TRUE(
        _testExplorer->treeModel()->getIndexFromPath(UfeUiTest::TestHierarchy::B1).isValid());
    ASSERT_TRUE(
        _testExplorer->treeModel()->getIndexFromPath(UfeUiTest::TestHierarchy::B2).isValid());
    ASSERT_TRUE(
        _testExplorer->treeModel()->getIndexFromPath(UfeUiTest::TestHierarchy::C1).isValid());
    ASSERT_TRUE(
        _testExplorer->treeModel()->getIndexFromPath(UfeUiTest::TestHierarchy::C2).isValid());

    ASSERT_FALSE(_testExplorer->treeModel()->getIndexFromPath(fooPath).isValid());
    ASSERT_FALSE(_testExplorer->treeModel()->getIndexFromPath(barPath).isValid());
    ASSERT_FALSE(_testExplorer->treeModel()->getIndexFromPath(bazPath).isValid());

    EXPECT_EQ(c1, pseudoRoot->child(0));
    EXPECT_EQ(c2, pseudoRoot->child(1));
    EXPECT_EQ(c3, pseudoRoot->child(2));
}