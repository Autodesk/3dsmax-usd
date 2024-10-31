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
#include "TreeModel.h"

#include "treeitem.h"

#include <ufe/hierarchy.h>

#include <QtGui/QPalette>
#include <QtWidgets/QApplication>
#include <functional>

namespace UfeUi {

TreeModel::TreeModel(const TreeColumns& columns, QObject* parent)
    : QAbstractItemModel(parent)
    , _rootItem(new TreeItem(this, nullptr))
    , _columns(columns)
{
}

TreeModel::~TreeModel() { delete _rootItem; }

int TreeModel::columnCount(const QModelIndex&) const { return static_cast<int>(_columns.size()); }

TreeItem* TreeModel::root() const { return _rootItem; }

void TreeModel::update(const Ufe::Path& path)
{
    const auto idx = getIndexFromPath(path);
    if (idx.isValid()) {
        if (const auto item = treeItem(idx)) {
            item->clearStateCache();

            std::function<QModelIndex(const QModelIndex&)> getLastSubtreeItem
                = [&](const QModelIndex& idx) {
                      int childCount = rowCount(idx);
                      if (!childCount) {
                          return idx;
                      }
                      return getLastSubtreeItem(index(childCount - 1, 0, idx));
                  };
            // Emit dataChanged for this path and its subtree items
            Q_EMIT dataChanged(idx, getLastSubtreeItem(idx).siblingAtColumn(columnCount() - 1));
        }
    }
}

QModelIndex TreeModel::getIndexFromPath(const Ufe::Path& path) const
{
    const auto it = _treeItemMap.find(path.hash());
    if (it != _treeItemMap.end()) {
        return it->first;
    }
    return QModelIndex {};
}

TreeItem* TreeModel::treeItem(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return nullptr;
    }

    const auto it = _treeItemMap.find(index.internalId());
    if (it == _treeItemMap.end()) {
        return nullptr;
    }

    return it->second;
}

QVariant TreeModel::data(const QModelIndex& index, int role) const
{
    const auto& columnIdx = index.column();
    if (!index.isValid() || columnIdx < 0 || columnIdx >= static_cast<int>(_columns.size())) {
        return QVariant {};
    }

    const auto item = treeItem(index);
    if (!item) {
        return QVariant {};
    }

    return _columns[columnIdx]->data(item, role);
}

bool TreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    if (index.column() < 0 || index.column() >= _columns.size()) {
        return false;
    }

    const auto item = treeItem(index);
    if (!item) {
        return false;
    }

    const auto res = _columns[index.column()]->setData(item, value, role);
    return res;
}

Qt::ItemFlags TreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    if (index.column() < 0 || index.column() >= _columns.size()) {
        return Qt::NoItemFlags;
    }

    const auto item = treeItem(index);
    if (!item) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = QAbstractItemModel::flags(index);
    _columns[index.column()]->flags(item, flags);

    return flags;
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || section < 0
        || section >= static_cast<int>(_columns.size())) {
        return QVariant {};
    }
    return _columns[section]->columnHeader(role);
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex {};
    }

    TreeItem* parentItem = nullptr;
    if (!parent.isValid()) {
        parentItem = _rootItem;
    } else {
        parentItem = treeItem(parent);
        if (!parentItem) {
            return QModelIndex {};
        }
    }

    if (const auto childItem = parentItem->child(row)) {
        return createIndex(row, column, childItem->uniqueId());
    }
    return QModelIndex {};
}

QModelIndex TreeModel::parent(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QModelIndex {};
    }

    const auto childItem = treeItem(index);
    if (!childItem) {
        return QModelIndex {};
    }

    TreeItem* parentItem = childItem->parentItem();

    if (parentItem == _rootItem) {
        return QModelIndex {};
    }
    return createIndex(parentItem->row(), 0, parentItem->uniqueId());
}

int TreeModel::rowCount(const QModelIndex& parent) const
{
    TreeItem* parentItem;
    if (parent.column() > 0) {
        return 0;
    }
    if (!parent.isValid()) {
        parentItem = _rootItem;
    } else {
        parentItem = treeItem(parent);
        if (!parentItem) {
            return 0;
        }
    }
    return static_cast<int>(parentItem->childCount());
}

std::unique_ptr<TreeModel> TreeModel::create(const TreeColumns& columns, QObject* parent)
{
    std::unique_ptr<TreeModel> treeModel = std::make_unique<TreeModel>(columns, parent);
    if (const auto qApplication = QApplication::instance()) {
        treeModel->moveToThread(qApplication->thread());
    }
    return treeModel;
}

void TreeModel::buildTreeFrom(
    TreeItem*                          buildRoot,
    const Ufe::SceneItem::Ptr&         sceneItem,
    const std::string&                 searchFilter,
    const TypeFilter&                  typeFilter,
    const Ufe::Hierarchy::ChildFilter& childFilter,
    bool                               includeRoot)
{
    Q_EMIT layoutAboutToBeChanged();

    buildRoot->clearChildren();

    ItemIncludes itemIncludes;
    itemIncludes.root = includeRoot;

    // Optimization: If the provided search filter is empty, fallback to loading the entire
    // hierarchy under the given item. This can can happen in cases where the User already typed
    // characters in the search box before pressing backspace up until all characters were removed.
    if (searchFilter.empty()
        && (typeFilter.names.empty() || typeFilter.mode == TypeFilter::Mode::NoFilter)) {
        buildTree(sceneItem, buildRoot, itemIncludes, childFilter);
        Q_EMIT layoutChanged();
        return;
    }

    itemIncludes.useItemList = true;

    const auto items
        = ItemSearch::findMatchingPaths(sceneItem, searchFilter, typeFilter, childFilter);
    for (const auto& item : items) {
        // When walking up the ancestry chain, the root item will end up being considered once and
        // its parent (an invalid Prim) will be selected. Since there is no point iterating up the
        // hierarchy at this point, stop processing the current Prim an move on to the next one
        // matching the search filter.
        Ufe::Path currentPath = item->path();
        while (currentPath.size()) {
            if (const bool pathAlreadyIncluded
                = itemIncludes.itemPaths.find(currentPath) != itemIncludes.itemPaths.end()) {
                // If the path is already part of the set of search results to be displayed, it is
                // unnecessary to walk up the ancestry chain in an attempt to process further item,
                // as it means they have already been added to the list up to the root item.
                break;
            }
            itemIncludes.itemPaths.insert(currentPath);
            currentPath = currentPath.pop();
        }
    }

    // Optimization: Count the number of itemns expected to be inserted in the TreeModel, so that
    // the search process can stop early if all items have already been found. While additional
    // "narrowing" techniques can be used in the future to further enhance the performance, this may
    // provide sufficient performance in most cases to remain as-is for early User feedback.
    buildTree(sceneItem, buildRoot, itemIncludes, childFilter);
    Q_EMIT layoutChanged();
}

void TreeModel::buildTree(
    const Ufe::SceneItem::Ptr&         sceneItem,
    TreeItem*                          parentItem,
    ItemIncludes&                      includes,
    const Ufe::Hierarchy::ChildFilter& childFilter)
{
    if (!includes.useItemList
        || includes.itemPaths.find(sceneItem->path()) != includes.itemPaths.end()) {
        auto parentItemForChildren = parentItem;
        if (includes.root) {
            parentItemForChildren = parentItem->appendChild(sceneItem);
        }

        includes.root = true;

        // Only continue processing additional items if all expected results have not already been
        // found:
        if (--includes.insertionsRemaining > 0) {
            const auto item = Ufe::Hierarchy::createItem(sceneItem->path());
            if (!item) {
                return;
            }

            const auto               hierarchy = Ufe::Hierarchy::hierarchy(item);
            const Ufe::SceneItemList children = hierarchy->filteredChildren(childFilter);
            for (const auto& childItem : children) {
                buildTree(childItem, parentItemForChildren, includes, childFilter);
            }
        }
    }
}
} // namespace UfeUi
