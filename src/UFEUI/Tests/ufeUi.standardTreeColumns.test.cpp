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

#include <UFEUI/standardTreeColumns.h>
#include <UFEUI/treeModel.h>

#include <QtGui/qicon.h>
#include <gtest/gtest.h>

class StandardColumnsTest : public UfeUiBaseTest
{
};

TEST_F(StandardColumnsTest, TreeColumn_visIndex)
{
    const VisColumn col { 99 };
    ASSERT_EQ(99, col.visualIndex());
    const VisColumn col1 { 0 };
    ASSERT_EQ(0, col1.visualIndex());
}

// Vis column tests.
TEST_F(StandardColumnsTest, VisColumn_header)
{
    const VisColumn col { 0 };
    QIcon           visible { ":/ufe/Icons/visible.png" };
    ASSERT_TRUE(UfeUiTest::areIconsEqual(
        visible, qvariant_cast<QIcon>(col.columnHeader(Qt::DecorationRole))));
}

TEST_F(StandardColumnsTest, VisColumn_data)
{
    const VisColumn col { 0 };

    const auto model = UfeUiTest::createEmptyModel();
    const auto a1 = UfeUiTest::createTreeItem(model.get(), UfeUiTest::TestHierarchy::A1);
    const auto b1 = a1->appendChild(Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::B1));

    // No text, only an Icon.
    EXPECT_EQ(QVariant {}, col.data(a1.get(), Qt::DisplayRole));

    // There are tree possible icons used :
    const QIcon hiddenInherit = QIcon { ":/ufe/Icons/hiddenInherit.png" };
    ;
    const QIcon hidden = QIcon { ":/ufe/Icons/hidden.png" };
    const QIcon visibleInherit = QIcon { ":/ufe/Icons/visibleInherit.png" };

    // Both a1 and b1 are visible.

    ASSERT_TRUE(UfeUiTest::areIconsEqual(
        visibleInherit, qvariant_cast<QIcon>(col.data(a1.get(), Qt::DecorationRole))));
    ASSERT_TRUE(UfeUiTest::areIconsEqual(
        visibleInherit, qvariant_cast<QIcon>(col.data(b1, Qt::DecorationRole))));
    // Hide a1, now a1 is invisible, and b1 inherits.
    Ufe::Object3d::object3d(a1->sceneItem())->setVisibility(false);
    ASSERT_TRUE(UfeUiTest::areIconsEqual(
        hidden, qvariant_cast<QIcon>(col.data(a1.get(), Qt::DecorationRole))));
    ASSERT_TRUE(UfeUiTest::areIconsEqual(
        hiddenInherit, qvariant_cast<QIcon>(col.data(b1, Qt::DecorationRole))));

    const auto testItem = std::dynamic_pointer_cast<UfeUiTest::TestSceneItem>(a1->sceneItem());
    testItem->setHideable(false);
    ASSERT_EQ(QVariant {}, col.data(a1.get(), Qt::DecorationRole));
}
TEST_F(StandardColumnsTest, VisColumn_setData)
{
    VisColumn  col { 0 };
    const auto model = UfeUiTest::createEmptyModel();
    const auto a1 = UfeUiTest::createTreeItem(model.get(), UfeUiTest::TestHierarchy::A1);

    // Set data not doing anything.
    EXPECT_FALSE(col.setData(a1.get(), QString("foo"), Qt::DisplayRole));
    EXPECT_FALSE(col.setData(a1.get(), Qt::Checked, Qt::CheckStateRole));
}

TEST_F(StandardColumnsTest, VisColumn_flags)
{
    VisColumn     col { 0 };
    const auto    model = UfeUiTest::createEmptyModel();
    const auto    a1 = UfeUiTest::createTreeItem(model.get(), UfeUiTest::TestHierarchy::A1);
    Qt::ItemFlags flags;
    col.flags(a1.get(), flags);
    // No special flags.
    EXPECT_EQ(Qt::ItemFlags {}, flags);
}

// Name column tests.
TEST_F(StandardColumnsTest, NameColumn_header)
{
    const NameColumn col { 0 };
    ASSERT_EQ(QObject::tr("Prim Name"), col.columnHeader(Qt::DisplayRole));
}

TEST_F(StandardColumnsTest, NameColumn_data)
{
    const NameColumn col { 0 };
    const auto       model = UfeUiTest::createEmptyModel();
    const auto       a1 = UfeUiTest::createTreeItem(model.get(), UfeUiTest::TestHierarchy::A1);
    EXPECT_EQ(QString("A1"), col.data(a1.get(), Qt::DisplayRole));
    // Other roles, nothing.
    EXPECT_EQ(QVariant {}, col.data(a1.get(), Qt::CheckStateRole));
}

TEST_F(StandardColumnsTest, NameColumn_rootAlias)
{
    const NameColumn col { "foo", 0 };
    const auto       model = UfeUiTest::createEmptyModel();

    const auto dummRoot = UfeUiTest::createTreeItem(model.get(), {});
    const auto a1 = dummRoot->appendChild(Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::A1));
    const auto b1 = a1->appendChild(Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::B1));
    EXPECT_EQ(QString("foo"), col.data(a1, Qt::DisplayRole));
    EXPECT_EQ(QString("B1"), col.data(b1, Qt::DisplayRole));
}

TEST_F(StandardColumnsTest, NameColumn_setData)
{
    NameColumn col { 0 };
    const auto model = UfeUiTest::createEmptyModel();
    const auto a1 = UfeUiTest::createTreeItem(model.get(), UfeUiTest::TestHierarchy::A1);
    // Set data not doing anything.
    EXPECT_FALSE(col.setData(a1.get(), QString("foo"), Qt::DisplayRole));
    EXPECT_FALSE(col.setData(a1.get(), Qt::Checked, Qt::CheckStateRole));
}

TEST_F(StandardColumnsTest, NameColumn_flags)
{
    NameColumn    col { 0 };
    const auto    model = UfeUiTest::createEmptyModel();
    const auto    a1 = UfeUiTest::createTreeItem(model.get(), UfeUiTest::TestHierarchy::A1);
    Qt::ItemFlags flags;
    col.flags(a1.get(), flags);
    // No special flags.
    EXPECT_EQ(Qt::ItemFlags {}, flags);
}

// Type column tests.
TEST_F(StandardColumnsTest, TypeColumn_header)
{
    const TypeColumn col { 0 };
    ASSERT_EQ(QObject::tr("Type"), col.columnHeader(Qt::DisplayRole));
}

TEST_F(StandardColumnsTest, TypeColumn_data)
{
    const TypeColumn col { 0 };
    const auto       model = UfeUiTest::createEmptyModel();
    const auto       a1 = UfeUiTest::createTreeItem(model.get(), UfeUiTest::TestHierarchy::A1);
    EXPECT_EQ(QString("TestSceneItemType"), col.data(a1.get(), Qt::DisplayRole));
    // Other roles, nothing.
    EXPECT_EQ(QVariant {}, col.data(a1.get(), Qt::CheckStateRole));
}

TEST_F(StandardColumnsTest, TypeColumn_setData)
{
    TypeColumn col { 0 };
    const auto model = UfeUiTest::createEmptyModel();
    const auto a1 = UfeUiTest::createTreeItem(model.get(), UfeUiTest::TestHierarchy::A1);
    // Set data not doing anything.
    EXPECT_FALSE(col.setData(a1.get(), QString("foo"), Qt::DisplayRole));
    EXPECT_FALSE(col.setData(a1.get(), Qt::Checked, Qt::CheckStateRole));
}

TEST_F(StandardColumnsTest, VisColumn_clickEvents)
{
    VisColumn  col { 0 };
    const auto model = UfeUiTest::createEmptyModel();
    const auto a1 = UfeUiTest::createTreeItem(model.get(), UfeUiTest::TestHierarchy::A1);

    // Clicks.
    EXPECT_TRUE(Ufe::Object3d::object3d(a1->sceneItem())->visibility());
    col.clicked(a1.get());
    EXPECT_FALSE(Ufe::Object3d::object3d(a1->sceneItem())->visibility());
    col.clicked(a1.get());
    EXPECT_TRUE(Ufe::Object3d::object3d(a1->sceneItem())->visibility());

    // Double clicks act as click.
    EXPECT_TRUE(Ufe::Object3d::object3d(a1->sceneItem())->visibility());
    col.clicked(a1.get());
    EXPECT_FALSE(Ufe::Object3d::object3d(a1->sceneItem())->visibility());
    col.doubleClicked(a1.get()); // Double click will come after a click.
    EXPECT_TRUE(Ufe::Object3d::object3d(a1->sceneItem())->visibility());
}

TEST_F(StandardColumnsTest, TypeColumn_flags)
{
    TypeColumn    col { 0 };
    const auto    model = UfeUiTest::createEmptyModel();
    const auto    a1 = UfeUiTest::createTreeItem(model.get(), UfeUiTest::TestHierarchy::A1);
    Qt::ItemFlags flags;
    col.flags(a1.get(), flags);
    // No special flags.
    EXPECT_EQ(Qt::ItemFlags {}, flags);
}