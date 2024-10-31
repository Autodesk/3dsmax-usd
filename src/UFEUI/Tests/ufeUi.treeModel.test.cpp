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

#include <UFEUI/treeitem.h>

#include <QtGui/qpalette.h>
#include <QtWidgets/qapplication.h>
#include <gtest/gtest.h>

class TreeModelTest : public UfeUiBaseTest
{
};

TEST_F(TreeModelTest, TreeModel_createFromRoot)
{
    UfeUi::TreeColumns columns;
    columns.push_back(std::make_shared<UfeUiTest::TestColumn>(0));

    UfeUi::TypeFilter           typeFilter;
    Ufe::Hierarchy::ChildFilter childFilter;

    bool       includeRoot = true;
    const auto modelWithRoot = UfeUi::TreeModel::create(columns, nullptr);
    modelWithRoot->buildTreeFrom(
        modelWithRoot->root(),
        Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::root),
        "",
        typeFilter,
        childFilter,
        includeRoot);

    auto treeItemRoot = modelWithRoot->root();

    EXPECT_EQ(treeItemRoot->sceneItem(), nullptr);
    EXPECT_EQ(treeItemRoot->childCount(), 1);
    EXPECT_EQ(treeItemRoot->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::root);

    EXPECT_EQ(treeItemRoot->child(0)->childCount(), 3);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::A1);
    EXPECT_EQ(treeItemRoot->child(0)->child(1)->sceneItem()->path(), UfeUiTest::TestHierarchy::A2);
    EXPECT_EQ(treeItemRoot->child(0)->child(2)->sceneItem()->path(), UfeUiTest::TestHierarchy::A3);

    EXPECT_EQ(treeItemRoot->child(0)->child(0)->childCount(), 2);
    EXPECT_EQ(
        treeItemRoot->child(0)->child(0)->child(0)->sceneItem()->path(),
        UfeUiTest::TestHierarchy::B1);
    EXPECT_EQ(
        treeItemRoot->child(0)->child(0)->child(1)->sceneItem()->path(),
        UfeUiTest::TestHierarchy::B2);
    EXPECT_EQ(treeItemRoot->child(0)->child(1)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(0)->child(2)->childCount(), 0);

    EXPECT_EQ(treeItemRoot->child(0)->child(0)->child(0)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->child(1)->childCount(), 2);
    EXPECT_EQ(
        treeItemRoot->child(0)->child(0)->child(1)->child(0)->sceneItem()->path(),
        UfeUiTest::TestHierarchy::C1);
    EXPECT_EQ(
        treeItemRoot->child(0)->child(0)->child(1)->child(1)->sceneItem()->path(),
        UfeUiTest::TestHierarchy::C2);

    includeRoot = false;
    const auto modelWithoutRoot = UfeUi::TreeModel::create(columns, nullptr);
    modelWithoutRoot->buildTreeFrom(
        modelWithoutRoot->root(),
        Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::root),
        "",
        typeFilter,
        childFilter,
        includeRoot);
    treeItemRoot = modelWithoutRoot->root();

    EXPECT_EQ(modelWithoutRoot->root()->sceneItem(), nullptr);
    EXPECT_EQ(modelWithoutRoot->root()->childCount(), 3);

    EXPECT_EQ(treeItemRoot->childCount(), 3);
    EXPECT_EQ(treeItemRoot->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::A1);
    EXPECT_EQ(treeItemRoot->child(1)->sceneItem()->path(), UfeUiTest::TestHierarchy::A2);
    EXPECT_EQ(treeItemRoot->child(2)->sceneItem()->path(), UfeUiTest::TestHierarchy::A3);

    EXPECT_EQ(treeItemRoot->child(0)->childCount(), 2);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::B1);
    EXPECT_EQ(treeItemRoot->child(0)->child(1)->sceneItem()->path(), UfeUiTest::TestHierarchy::B2);
    EXPECT_EQ(treeItemRoot->child(1)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(2)->childCount(), 0);

    EXPECT_EQ(treeItemRoot->child(0)->child(0)->childCount(), 0);
    EXPECT_EQ(treeItemRoot->child(0)->child(1)->childCount(), 2);
    EXPECT_EQ(
        treeItemRoot->child(0)->child(1)->child(0)->sceneItem()->path(),
        UfeUiTest::TestHierarchy::C1);
    EXPECT_EQ(
        treeItemRoot->child(0)->child(1)->child(1)->sceneItem()->path(),
        UfeUiTest::TestHierarchy::C2);
}
TEST_F(TreeModelTest, TreeModel_createFromSearch)
{
    UfeUi::TreeColumns columns;
    columns.push_back(std::make_shared<UfeUiTest::TestColumn>(0));
    UfeUi::TypeFilter           typeFilter;
    Ufe::Hierarchy::ChildFilter childFilter;
    // Search "1".

    // Should get A1, B1, C1 from search.
    // We should also get B2, as it is the parent of C1.
    auto search = "1";

    auto model = UfeUi::TreeModel::create(columns, nullptr);

    model->buildTreeFrom(
        model->root(),
        Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::root),
        search,
        typeFilter,
        childFilter,
        false);
    auto rootItem = model->root();

    EXPECT_EQ(rootItem->childCount(), 1);
    EXPECT_EQ(rootItem->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::A1);
    EXPECT_EQ(rootItem->child(0)->childCount(), 2);
    EXPECT_EQ(rootItem->child(0)->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::B1);
    EXPECT_EQ(rootItem->child(0)->child(1)->sceneItem()->path(), UfeUiTest::TestHierarchy::B2);
    EXPECT_EQ(rootItem->child(0)->child(0)->childCount(), 0);
    EXPECT_EQ(rootItem->child(0)->child(1)->childCount(), 1);
    EXPECT_EQ(
        rootItem->child(0)->child(1)->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::C1);

    // Search "C2"
    // Should C2 and its hierarchy, A1 & B2
    search = "C2";
    model = UfeUi::TreeModel::create(columns, nullptr);
    model->buildTreeFrom(
        model->root(),
        Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::root),
        search,
        typeFilter,
        childFilter,
        false);
    rootItem = model->root();

    EXPECT_EQ(rootItem->childCount(), 1);
    EXPECT_EQ(rootItem->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::A1);
    EXPECT_EQ(rootItem->child(0)->childCount(), 1);
    EXPECT_EQ(rootItem->child(0)->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::B2);
    EXPECT_EQ(rootItem->child(0)->child(0)->childCount(), 1);
    EXPECT_EQ(
        rootItem->child(0)->child(0)->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::C2);

    // Search "a1"
    // Should only get the top level item.
    search = "a1";
    model = UfeUi::TreeModel::create(columns, nullptr);
    model->buildTreeFrom(
        model->root(),
        Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::root),
        search,
        typeFilter,
        childFilter,
        false);
    rootItem = model->root();
    EXPECT_EQ(rootItem->childCount(), 1);
    EXPECT_EQ(rootItem->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::A1);
    EXPECT_EQ(rootItem->child(0)->childCount(), 0);
}

std::unique_ptr<UfeUi::TreeModel> buildSimpleModel()
{
    UfeUiTest::TestHierarchy::resetTestHierarchy();
    UfeUi::TreeColumns columns;
    columns.push_back(std::make_shared<UfeUiTest::TestColumn>(0));
    columns.push_back(std::make_shared<UfeUiTest::TestColumn>(1));
    const UfeUi::TypeFilter     typeFilter;
    Ufe::Hierarchy::ChildFilter childFilter;
    auto                        model = UfeUi::TreeModel::create(columns, nullptr);
    model->buildTreeFrom(
        model->root(),
        Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::root),
        "",
        typeFilter,
        childFilter,
        false);
    return model;
}
TEST_F(TreeModelTest, TreeModel_getIndex)
{
    auto model = buildSimpleModel();

    // Root Not included in the model, query should return an invalid index.
    const auto idxRoot = model->getIndexFromPath(UfeUiTest::TestHierarchy::root);
    EXPECT_FALSE(idxRoot.parent().isValid());

    // Non-sensical path
    const auto badIdx = model->getIndexFromPath(UfeUiTest::getUfePath("/foo/bar"));
    EXPECT_FALSE(badIdx.parent().isValid());

    const auto idxA1 = model->getIndexFromPath(UfeUiTest::TestHierarchy::A1);
    EXPECT_EQ(idxA1.row(), 0);
    EXPECT_EQ(idxA1.column(), 0);
    EXPECT_FALSE(idxA1.parent().isValid());

    const auto idxA2 = model->getIndexFromPath(UfeUiTest::TestHierarchy::A2);
    EXPECT_EQ(idxA2.row(), 1);
    EXPECT_EQ(idxA2.column(), 0);
    EXPECT_FALSE(idxA2.parent().isValid());

    const auto idxA3 = model->getIndexFromPath(UfeUiTest::TestHierarchy::A3);
    EXPECT_EQ(idxA3.row(), 2);
    EXPECT_EQ(idxA3.column(), 0);
    EXPECT_FALSE(idxA3.parent().isValid());

    const auto idxB1 = model->getIndexFromPath(UfeUiTest::TestHierarchy::B1);
    EXPECT_EQ(idxB1.row(), 0);
    EXPECT_EQ(idxB1.column(), 0);
    EXPECT_EQ(idxB1.parent(), idxA1);

    const auto idxB2 = model->getIndexFromPath(UfeUiTest::TestHierarchy::B2);
    EXPECT_EQ(idxB2.row(), 1);
    EXPECT_EQ(idxB2.column(), 0);
    EXPECT_EQ(idxB2.parent(), idxA1);

    const auto idxC1 = model->getIndexFromPath(UfeUiTest::TestHierarchy::C1);
    EXPECT_EQ(idxC1.row(), 0);
    EXPECT_EQ(idxC1.column(), 0);
    EXPECT_EQ(idxC1.parent(), idxB2);

    const auto idxC2 = model->getIndexFromPath(UfeUiTest::TestHierarchy::C2);
    EXPECT_EQ(idxC2.row(), 1);
    EXPECT_EQ(idxC2.column(), 0);
    EXPECT_EQ(idxC2.parent(), idxB2);
}

TEST_F(TreeModelTest, TreeModel_dataGetSet)
{
    const auto model = buildSimpleModel();

    const auto B1FirstColumn = model->getIndexFromPath(UfeUiTest::TestHierarchy::B1);
    EXPECT_EQ(
        UfeUiTest::TestColumn::DefaultCheckStateData,
        model->data(B1FirstColumn, static_cast<int>(Qt::CheckStateRole)));
    EXPECT_EQ(
        UfeUiTest::TestColumn::DefaultDisplayData,
        model->data(B1FirstColumn, static_cast<int>(Qt::DisplayRole)));
    const auto newDisplay1 = QString("Test1");
    const auto newCheckState1 = Qt::CheckState::Unchecked;
    EXPECT_TRUE(model->setData(B1FirstColumn, newDisplay1, Qt::DisplayRole));
    EXPECT_TRUE(model->setData(B1FirstColumn, newCheckState1, Qt::CheckStateRole));
    EXPECT_EQ(newCheckState1, model->data(B1FirstColumn, static_cast<int>(Qt::CheckStateRole)));
    EXPECT_EQ(newDisplay1, model->data(B1FirstColumn, static_cast<int>(Qt::DisplayRole)));

    const auto B1SecondColumn
        = model->index(B1FirstColumn.row(), B1FirstColumn.column() + 1, B1FirstColumn.parent());
    EXPECT_EQ(
        UfeUiTest::TestColumn::DefaultCheckStateData,
        model->data(B1SecondColumn, static_cast<int>(Qt::CheckStateRole)));
    EXPECT_EQ(
        UfeUiTest::TestColumn::DefaultDisplayData,
        model->data(B1SecondColumn, static_cast<int>(Qt::DisplayRole)));
    const auto newDisplay2 = QString("Test2");
    const auto newCheckState2 = Qt::CheckState::Unchecked;
    EXPECT_TRUE(model->setData(B1SecondColumn, newDisplay2, Qt::DisplayRole));
    EXPECT_TRUE(model->setData(B1SecondColumn, newCheckState2, Qt::CheckStateRole));
    EXPECT_EQ(newCheckState2, model->data(B1SecondColumn, static_cast<int>(Qt::CheckStateRole)));
    EXPECT_EQ(newDisplay2, model->data(B1SecondColumn, static_cast<int>(Qt::DisplayRole)));

    // Get with invalid index should an empty QVariant.

    // Get with our of range column should an empty QVariant.
    EXPECT_EQ(QVariant {}, model->data(QModelIndex {}, Qt::CheckStateRole));
    EXPECT_EQ(QVariant {}, model->data(QModelIndex {}, Qt::DisplayRole));

    const auto oorCol = model->index(0, -10, B1FirstColumn.parent());
    const auto oorCol2 = model->index(0, 10, B1FirstColumn.parent());
    EXPECT_EQ(QVariant {}, model->data(oorCol, Qt::CheckStateRole));
    EXPECT_EQ(QVariant {}, model->data(oorCol2, Qt::DisplayRole));

    // Set with invalid index should return false
    EXPECT_FALSE(model->setData(QModelIndex {}, newDisplay1, Qt::DisplayRole));
    EXPECT_FALSE(model->setData(QModelIndex {}, newCheckState1, Qt::CheckStateRole));
    // Set with out of range column should false
    EXPECT_FALSE(model->setData(oorCol, newDisplay1, Qt::DisplayRole));
    EXPECT_FALSE(model->setData(oorCol2, newCheckState1, Qt::CheckStateRole));
}
TEST_F(TreeModelTest, TreeModel_dataDisabledForeground)
{
    const auto model = buildSimpleModel();

    const auto idxA1 = model->getIndexFromPath(UfeUiTest::TestHierarchy::A1);
    const auto idxB1 = model->getIndexFromPath(UfeUiTest::TestHierarchy::B1);

    const auto item = model->treeItem(idxB1);
    Ufe::Object3d::object3d(item->sceneItem())->setVisibility(false);

    EXPECT_EQ(QVariant {}, model->data(idxA1, Qt::ForegroundRole));

    const auto disabledStyle
        = QVariant { QApplication::palette().color(QPalette::Disabled, QPalette::WindowText) };
    EXPECT_EQ(disabledStyle, model->data(idxB1, Qt::ForegroundRole));
}
TEST_F(TreeModelTest, TreeModel_flags)
{
    const auto model = buildSimpleModel();

    // Happy path
    const auto C1IdxCol1 = model->getIndexFromPath(UfeUiTest::TestHierarchy::C1);

    const auto flags1 = model->flags(C1IdxCol1);
    EXPECT_TRUE(flags1.testFlag(Qt::ItemIsUserCheckable));
    EXPECT_FALSE(flags1.testFlag(Qt::ItemIsEnabled));

    const auto C1IdxCol2
        = model->index(C1IdxCol1.row(), C1IdxCol1.column() + 1, C1IdxCol1.parent());
    const auto flags2 = model->flags(C1IdxCol2);
    EXPECT_FALSE(flags2.testFlag(Qt::ItemIsUserCheckable));
    EXPECT_TRUE(flags2.testFlag(Qt::ItemIsEnabled));

    // No flags on invalid index.
    EXPECT_EQ(Qt::NoItemFlags, model->flags(QModelIndex {}));

    // No flags on out-of-range column
    const auto oorCol1 = model->index(C1IdxCol1.row(), -10, C1IdxCol1.parent());
    const auto oorCol2 = model->index(C1IdxCol1.row(), 10, C1IdxCol1.parent());
    EXPECT_EQ(Qt::NoItemFlags, model->flags(oorCol1));
    EXPECT_EQ(Qt::NoItemFlags, model->flags(oorCol2));
}

TEST_F(TreeModelTest, TreeModel_parent)
{
    const auto model = buildSimpleModel();

    // Happy path
    const auto C1Idx = model->getIndexFromPath(UfeUiTest::TestHierarchy::C1);
    const auto parentC1 = model->parent(C1Idx);
    EXPECT_TRUE(parentC1.isValid());
    EXPECT_EQ(1, parentC1.row());
    EXPECT_EQ(0, parentC1.column());

    // Parent of invalid
    const auto parentOfInvalid = model->parent(QModelIndex {});
    EXPECT_FALSE(parentOfInvalid.isValid());

    // A1 is at the root, no parent
    const auto A1Idx = model->getIndexFromPath(UfeUiTest::TestHierarchy::A1);
    const auto rootParentIdx = model->parent(A1Idx);
    EXPECT_FALSE(rootParentIdx.isValid());
}

TEST_F(TreeModelTest, TreeModel_roRowColCount)
{
    const auto model = buildSimpleModel();

    // Happy path
    const auto C1Idx = model->getIndexFromPath(UfeUiTest::TestHierarchy::C1);
    EXPECT_EQ(2, model->rowCount(C1Idx.parent()));
    EXPECT_EQ(2, model->columnCount(C1Idx.parent())); // Constant

    // Invalid parent passed (at the root)
    EXPECT_EQ(3, model->rowCount(QModelIndex {}));
    EXPECT_EQ(2, model->columnCount(QModelIndex {})); // Constant

    // Non-zero column...invalid request.
    const auto noneZeroCol = model->index(C1Idx.row(), 1, C1Idx.parent());
    EXPECT_EQ(0, model->rowCount(noneZeroCol));
    EXPECT_EQ(2, model->columnCount(noneZeroCol)); // Constant
}

TEST_F(TreeModelTest, TreeModel_headerData)
{
    // Happy path
    const auto model = buildSimpleModel();
    EXPECT_EQ(QString("TestColumnHeader0"), model->headerData(0, Qt::Horizontal, Qt::DisplayRole));
    EXPECT_EQ(QString("TestColumnHeader1"), model->headerData(1, Qt::Horizontal, Qt::DisplayRole));

    // Model has 2 columns, only secion 0-1 should work.
    EXPECT_EQ(QVariant {}, model->headerData(-1, Qt::Horizontal, Qt::DisplayRole));
    EXPECT_EQ(QVariant {}, model->headerData(2, Qt::Horizontal, Qt::DisplayRole));

    // Only Horizontal / DisplayRole is implemented.
    EXPECT_EQ(QVariant {}, model->headerData(0, Qt::Vertical, Qt::DisplayRole));
    EXPECT_EQ(QVariant {}, model->headerData(0, Qt::Vertical, Qt::DisplayRole));
    EXPECT_EQ(QVariant {}, model->headerData(0, Qt::Horizontal, Qt::CheckStateRole));
    EXPECT_EQ(QVariant {}, model->headerData(0, Qt::Horizontal, Qt::CheckStateRole));
}

TEST_F(TreeModelTest, TreeModel_treeItem)
{
    const auto model = buildSimpleModel();
    const auto a1Idx = model->getIndexFromPath(UfeUiTest::TestHierarchy::A1);
    const auto a1Item = model->treeItem(a1Idx);
    ASSERT_NE(nullptr, a1Item);
    EXPECT_EQ(UfeUiTest::TestHierarchy::A1, a1Item->sceneItem()->path());

    const auto notAnItem = model->treeItem(QModelIndex());
    EXPECT_EQ(nullptr, notAnItem);
}

TEST_F(TreeModelTest, TreeModel_update)
{
    const auto model = buildSimpleModel();

    const auto idx = model->getIndexFromPath(UfeUiTest::TestHierarchy::A1);

    const auto item = model->treeItem(idx);

    EXPECT_TRUE(item->computedVisibility());
    Ufe::Object3d::object3d(item->sceneItem())->setVisibility(false);

    EXPECT_TRUE(item->computedVisibility());
    model->update(item->sceneItem()->path());
    EXPECT_FALSE(item->computedVisibility());

    // Noop.
    model->update(Ufe::Path {});
}

TEST_F(TreeModelTest, TreeModel_childFilter)
{
    UfeUi::TreeColumns columns;
    columns.push_back(std::make_shared<UfeUiTest::TestColumn>(0));

    UfeUi::TypeFilter typeFilter;

    const auto childFilter = UfeUiTest::TestHierarchy::childFilter();

    bool       includeRoot = true;
    const auto model = UfeUi::TreeModel::create(columns, nullptr);
    model->buildTreeFrom(
        model->root(),
        Ufe::Hierarchy::createItem(UfeUiTest::TestHierarchy::root),
        "",
        typeFilter,
        childFilter,
        includeRoot);

    auto treeItemRoot = model->root();

    EXPECT_EQ(treeItemRoot->sceneItem(), nullptr);
    EXPECT_EQ(treeItemRoot->childCount(), 1);
    EXPECT_EQ(treeItemRoot->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::root);
    EXPECT_EQ(treeItemRoot->child(0)->childCount(), 2);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->sceneItem()->path(), UfeUiTest::TestHierarchy::A1);
    EXPECT_EQ(treeItemRoot->child(0)->child(1)->sceneItem()->path(), UfeUiTest::TestHierarchy::A3);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->childCount(), 1);
    EXPECT_EQ(
        treeItemRoot->child(0)->child(0)->child(0)->sceneItem()->path(),
        UfeUiTest::TestHierarchy::B1);
    EXPECT_EQ(treeItemRoot->child(0)->child(0)->child(0)->childCount(), 0);
}
