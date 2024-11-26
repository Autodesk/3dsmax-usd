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

#include <UFEUI/treeItem.h>
#include <UFEUI/treeModel.h>

#include <ufe/hierarchy.h>
#include <ufe/subject.h>

#include <gtest/gtest.h>

class TreeItemTest : public UfeUiBaseTest
{
};

TEST_F(TreeItemTest, TreeItem_parenting)
{
    auto treeModel = UfeUiTest::createEmptyModel();

    UfeUi::TreeItem root { treeModel.get(),
                           Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root")) };

    EXPECT_EQ(0, root.childCount());

    auto child1
        = root.appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/child1")));
    EXPECT_EQ(&root, child1->parentItem());
    auto child2
        = root.appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/child2")));
    EXPECT_EQ(&root, child2->parentItem());
    auto child3
        = root.appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/child3")));
    EXPECT_EQ(&root, child3->parentItem());

    auto subChild1 = child1->appendChild(
        Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/child1/subChild1")));
    EXPECT_EQ(child1, subChild1->parentItem());
    auto subChild2 = child1->appendChild(
        Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/child1/subChild2")));
    EXPECT_EQ(child1, subChild2->parentItem());

    EXPECT_EQ(3, root.childCount());
    EXPECT_EQ(2, child1->childCount());

    EXPECT_EQ(nullptr, root.child(-1));
    EXPECT_EQ(child1, root.child(0));
    EXPECT_EQ(child2, root.child(1));
    EXPECT_EQ(child3, root.child(2));
    EXPECT_EQ(nullptr, root.child(3));

    EXPECT_EQ(subChild1, child1->child(0));
    EXPECT_EQ(subChild2, child1->child(1));

    root.removeChild(child2);
    child1->removeChild(subChild1);

    EXPECT_EQ(2, root.childCount());
    EXPECT_EQ(1, child1->childCount());

    // No longer there, noop.
    root.removeChild(child2);
    EXPECT_EQ(2, root.childCount());

    root.clearChildren();
    EXPECT_EQ(0, root.childCount());
}

TEST_F(TreeItemTest, TreeItem_row)
{
    const auto treeModel = UfeUiTest::createEmptyModel();

    UfeUi::TreeItem root { treeModel.get(),
                           Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root")) };
    EXPECT_EQ(0, root.row());

    const auto child1
        = root.appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/child1")));
    EXPECT_EQ(0, child1->row());
    const auto child2
        = root.appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/child2")));
    EXPECT_EQ(1, child2->row());
    const auto child3
        = root.appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/child3")));
    EXPECT_EQ(2, child3->row());

    const auto subChild1 = child1->appendChild(
        Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/child1/subChild1")));
    EXPECT_EQ(0, subChild1->row());
    const auto subChild2 = child1->appendChild(
        Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/child1/subChild2")));
    EXPECT_EQ(1, subChild2->row());

    root.removeChild(child1);
    EXPECT_EQ(0, child2->row());
    EXPECT_EQ(1, child3->row());
}
TEST_F(TreeItemTest, TreeItem_disabled)
{
    const auto treeModel = UfeUiTest::createEmptyModel();

    UfeUi::TreeItem root { treeModel.get(),
                           Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root")) };

    auto A1 = root.appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/A1")));
    auto A2 = root.appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/A2")));
    auto A3 = root.appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/A3")));

    auto B1 = A1->appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/A1/B1")));
    auto B2 = A1->appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/A1/B2")));

    auto C1 = B2->appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/A1/B2/C1")));
    auto C2 = B2->appendChild(Ufe::Hierarchy::createItem(UfeUiTest::getUfePath("/root/A1/B2/C2")));

    auto checkHidden = [](const std::vector<UfeUi::TreeItem*>& items, bool value) {
        for (const auto& item : items) {
            EXPECT_EQ(!item->computedVisibility(), value);
        }
    };

    // Starting point, all visible.
    checkHidden({ &root, A1, A2, A3, B1, B2, C1, C2 }, false);
    // Invis top level item, which should hide everything.
    Ufe::Object3d::object3d(root.sceneItem())->setVisibility(false);
    // Still all visible, via caching.
    checkHidden({ &root, A1, A2, A3, B1, B2, C1, C2 }, false);
    // Clear the cache (propagates down)
    root.clearStateCache();
    // Check again, all hidden now.
    checkHidden({ &root, A1, A2, A3, B1, B2, C1, C2 }, true);

    // Show again the root, and test inv in the middle of the hierarchy.
    Ufe::Object3d::object3d(root.sceneItem())->setVisibility(true);
    Ufe::Object3d::object3d(B2->sceneItem())->setVisibility(false);
    root.clearStateCache();
    checkHidden({ B2, C1, C2 }, true);
    checkHidden({ &root, A1, A2, B1 }, false);
    // Changing the vis of C1 has no effect, value is inherited.
    C1->clearStateCache();
    Ufe::Object3d::object3d(C1->sceneItem())->setVisibility(true);
    checkHidden({ C1 }, true);
    // Change B2 again, values are still cached at it's level
    Ufe::Object3d::object3d(B2->sceneItem())->setVisibility(true);
    checkHidden({ B2, C1, C2 }, true);
    checkHidden({ &root, A1, A2, B1 }, false);
    B2->clearStateCache();
    // All visible now.
    checkHidden({ &root, A1, A2, A3, B1, B2, C1, C2 }, false);

    // Test a with a leaf item.
    Ufe::Object3d::object3d(C1->sceneItem())->setVisibility(false);
    // Still on cached values.
    checkHidden({ &root, A1, A2, A3, B1, B2, C1, C2 }, false);
    C1->clearStateCache();
    checkHidden({ C1 }, true);
    checkHidden({ &root, A1, A2, A3, B1, B2, C2 }, false);
}
TEST_F(TreeItemTest, TreeItem_findDescendants)
{
    const auto                  model = UfeUi::TreeModel::create({}, nullptr);
    Ufe::Hierarchy::ChildFilter childFilter;
    model->buildTreeFrom(
        model->root(),
        Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::root),
        "",
        {},
        childFilter,
        false);
    const auto root = model->root();

    const auto none = root->findDescendants([](const UfeUi::TreeItem* item) { return false; });
    EXPECT_TRUE(none.empty());

    const auto all = root->findDescendants([](const UfeUi::TreeItem* item) { return true; });
    EXPECT_EQ(7, all.size());

    const auto containsA = root->findDescendants([](const UfeUi::TreeItem* item) {
        return item->sceneItem()->path().string().find("A1") != std::string::npos;
    });
    // A1 + all children
    EXPECT_EQ(5, containsA.size());

    // Not found, the search is stopped at items returning false.
    const auto containsB = root->findDescendants([](const UfeUi::TreeItem* item) {
        return item->sceneItem()->path().string().find("B") != std::string::npos;
    });
    EXPECT_EQ(0, containsB.size());
}