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

#include "editCommand.h"
#include "treeitem.h"
#include "utils.h"

#include <ufe/hierarchy.h>
#include <ufe/pathString.h>
#include <ufe/undoableCommandMgr.h>

#include <QIODevice>
#include <QMimeData>
#include <QtGui/QPalette>
#include <QtWidgets/QApplication>
#include <functional>

namespace {
const QString UFE_PATHS_MIME_TYPE = "application/x-ufe-paths";
}

namespace UfeUi {

TreeModel::TreeModel(const TreeColumns& columns, QObject* parent)
    : QAbstractItemModel(parent)
    , _rootItem(new TreeItem(this, nullptr))
    , _columns(columns)
    , _useSearchItems(false)
{
}

TreeModel::~TreeModel() { delete _rootItem; }

int TreeModel::columnCount(const QModelIndex&) const { return static_cast<int>(_columns.size()); }

bool TreeModel::hasChildren(const QModelIndex& parent) const
{
    auto ti = treeItem(parent);
    if (!ti || ti->sceneItem() == nullptr) {
        return true;
    }

    const auto hierarchy = Ufe::Hierarchy::hierarchy(ti->sceneItem());
    return !hierarchy->filteredChildren(_childFilter).empty();
}

bool TreeModel::canFetchMore(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return false;
    }

    auto item = treeItem(parent);
    if (!item || !item->sceneItem()) {
        return false;
    }

    const auto hierarchy = Ufe::Hierarchy::hierarchy(item->sceneItem());
    auto       children = hierarchy->filteredChildren(_childFilter);

    bool canFetchMore = !children.empty();
    // If we are using a search filter, check if the children are in the search items paths.
    if (_useSearchItems) {
        canFetchMore = false;

        for (const auto& child : children) {
            // if there is at least one child in the search items paths, we can fetch more
            if (_searchItemsPaths.find(child->path()) != _searchItemsPaths.end()) {
                children.push_back(child);
                canFetchMore = true;
                break;
            }
        }
    }
    return canFetchMore;
}

void TreeModel::fetchMore(const QModelIndex& parent)
{
    if (!parent.isValid()) {
        return;
    }

    auto item = treeItem(parent);
    if (!item || !item->sceneItem()) {
        return;
    }

    auto hierarchy = Ufe::Hierarchy::hierarchy(item->sceneItem());
    auto children = hierarchy->filteredChildren(_childFilter);

    // If we are using a search filter, check if the children are in the search items paths.
    if (_useSearchItems) {
        children.erase(
            std::remove_if(
                children.begin(),
                children.end(),
                [this](const Ufe::SceneItem::Ptr& child) {
                    return _searchItemsPaths.find(child->path()) == _searchItemsPaths.end();
                }),
            children.end());
    }

    // items have already been fetched; can leave
    if (item->childCount() == children.size()) {
        return;
    }

    std::vector<Ufe::SceneItemPtr> itemsToAdd;
    for (const auto& child : children) {
        bool alreadyAdded = false;
        for (int i = 0; i < item->childCount(); ++i) {
            if (child->path() == item->child(i)->sceneItem()->path()) {
                alreadyAdded = true;
                break;
            }
        }
        if (alreadyAdded) {
            continue;
        }
        itemsToAdd.push_back(child);
    }

    if (itemsToAdd.empty()) {
        return;
    }

    int startRow = static_cast<int>(item->childCount());
    int lastRow = startRow + static_cast<int>(itemsToAdd.size()) - 1;
    beginInsertRows(parent, startRow, lastRow);
    for (const auto& sceneItem : itemsToAdd) {
        item->appendChild(sceneItem);
    }
    endInsertRows();
}

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

QModelIndex TreeModel::getIndexFromPath(const Ufe::Path& path, bool forceFetchMore)
{
    if (path.empty()) {
        return QModelIndex {};
    }

    auto it = _treeItemMap.find(path.hash());
    if (it != _treeItemMap.end()) {
        return it->first;
    }

    if (!forceFetchMore) {
        return QModelIndex {};
    }

    // It could be the case the item is being lazy loaded and not in the tree yet.
    // In this case, we need to expand the parents up to the chosen path.
    for (size_t i = 1; i < path.size(); ++i) {
        auto currentPath = path.head(static_cast<int>(i));
        auto idx = getIndexFromPath(currentPath, true);
        if (canFetchMore(idx)) {
            fetchMore(idx);
        }
    }

    // After all elements have been fetch up to the path, try to find the index again.
    it = _treeItemMap.find(path.hash());
    return it != _treeItemMap.end() ? it->first : QModelIndex {};
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

    if (index.column() < 0 || static_cast<size_t>(index.column()) >= _columns.size()) {
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
    flags |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    _columns[index.column()]->flags(item, flags);
    return flags;
}

Qt::DropActions TreeModel::supportedDropActions() const { return Qt::MoveAction; }

Qt::DropActions TreeModel::supportedDragActions() const { return Qt::MoveAction; }

QMimeData* TreeModel::mimeData(const QModelIndexList& indexes) const
{
    // Write out the dragged UFE paths into the mime data.

    std::vector<Ufe::Path> ufePaths;

    for (const QModelIndex& index : indexes) {
        // Only care about rows, not individual cells. So only process the first column
        // for each row.
        if (!index.isValid() || index.column() != 0) {
            continue;
        }
        TreeItem* item = treeItem(index);
        if (!item) {
            continue;
        }
        const auto sceneItem = item->sceneItem();
        if (!sceneItem) {
            continue;
        }

        ufePaths.push_back(item->sceneItem()->path());
    }

    // Only add the root-most paths in the mime data. If parent/child are drag & dropped,
    // only need to care about the parent, as all of its children will move along with it.

    // Sort paths by ascending size, to make sure ancestors come before descendants.
    std::sort(ufePaths.begin(), ufePaths.end(), [](const Ufe::Path& a, const Ufe::Path& b) {
        return a.size() < b.size();
    });

    std::vector<Ufe::Path> rootMostPaths;
    for (const auto& path : ufePaths) {
        bool ancestorExists = false;
        for (const auto& rootPath : rootMostPaths) {
            if (path.startsWith(rootPath)) {
                ancestorExists = true;
                break;
            }
        }
        if (!ancestorExists) {
            rootMostPaths.push_back(path);
        }
    }

    QMimeData*  mimeData = new QMimeData();
    QByteArray  itemData;
    QDataStream dataStream(&itemData, QIODevice::WriteOnly);

    for (const auto& path : rootMostPaths) {
        // Get the UFE path as a string using PathString, not toString(). This allows us to
        // rebuild
        // the paths easily on the other side, on the drop, in TreeModel::dropMimeData().
        const std::string pathString = Ufe::PathString::string(path);
        dataStream << QString::fromStdString(pathString);
    }

    mimeData->setData(UFE_PATHS_MIME_TYPE, itemData);
    return mimeData;
}

bool TreeModel::dropMimeData(
    const QMimeData*   data,
    Qt::DropAction     action,
    int                row,
    int                column,
    const QModelIndex& parent)
{
    if (action == Qt::IgnoreAction || column > 0 || !data->hasFormat(UFE_PATHS_MIME_TYPE)) {
        return true;
    }

    // Get the parent item where we're dropping into.
    TreeItem* parentItem = nullptr;
    if (parent.isValid()) {
        parentItem = treeItem(parent);
    } else {
        parentItem = _rootItem;
    }

    if (!parentItem) {
        return false;
    }

    // Dropped items will be reparented here in a single composite command.
    const auto compositeCommand = Ufe::CompositeUndoableCommand::create({});

    // Extract the dropped UFE paths.
    QByteArray  encodedData = data->data(UFE_PATHS_MIME_TYPE);
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    QString     pathString;
    while (!stream.atEnd()) {
        stream >> pathString;
        // Convert the string back to a UFE path using PathString
        const std::string stdPathString = pathString.toStdString();
        Ufe::Path         path = Ufe::PathString::path(stdPathString);

        QModelIndex sourceIndex = getIndexFromPath(path);
        if (!sourceIndex.isValid()) {
            continue;
        }
        if (TreeItem* sourceItem = treeItem(sourceIndex)) {

            Ufe::InsertChildCommand::Ptr cmd = nullptr;
            try {
                auto hier = Ufe::Hierarchy::hierarchy(parentItem->sceneItem());
                cmd = hier->insertChildCmd(sourceItem->sceneItem(), nullptr);
            } catch (const std::exception& ex) {
                Utils::ReportError(ex.what());
                continue;
            }
            if (cmd) {
                compositeCommand->append(cmd);
            }
        }
    }

    if (!compositeCommand->cmdsList().empty()) {
        const auto editCmd = UfeUi::EditCommand::create(
            parentItem->sceneItem()->path(), compositeCommand, "Reparent");
        Ufe::UndoableCommandMgr::instance().executeCmd(editCmd);
    }
    return true;
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
        return 1;
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
    this->_childFilter = childFilter;

    Q_EMIT layoutAboutToBeChanged();

    QModelIndex parentIdx = {};
    if (auto parentSceneItem = buildRoot->sceneItem()) {
        parentIdx = getIndexFromPath(parentSceneItem->path());
    }

    beginRemoveRows(parentIdx, 0, static_cast<int>(buildRoot->childCount()) - 1);
    buildRoot->clearChildren();
    endRemoveRows();

    ItemIncludes itemIncludes {};
    itemIncludes.root = includeRoot;

    // Optimization: If the provided search filter is empty, fallback to loading the entire
    // hierarchy under the given item. This can can happen in cases where the User already typed
    // characters in the search box before pressing backspace up until all characters were removed.
    if (searchFilter.empty()
        && (typeFilter.names.empty() || typeFilter.mode == TypeFilter::Mode::NoFilter)) {
        buildTree(sceneItem, buildRoot, itemIncludes);
        Q_EMIT layoutChanged();
        return;
    }

    itemIncludes.useItemList = true;

    std::vector<Ufe::SceneItem::Ptr> items;
    ItemSearch::findMatchingPaths(sceneItem, searchFilter, typeFilter, childFilter, items);
    for (const auto& item : items) {
        // When walking up the ancestry chain, the root item will end up being considered once and
        // its parent (an invalid Prim) will be selected. Since there is no point iterating up the
        // hierarchy at this point, stop processing the current Prim and move on to the next one
        // matching the search filter.
        Ufe::Path currentPath = item->path();
        while (!currentPath.empty()) {
            if (itemIncludes.itemPaths.find(currentPath) != itemIncludes.itemPaths.end()) {
                // If the path is already part of the set of search results to be displayed, it is
                // unnecessary to walk up the ancestry chain in an attempt to process further item,
                // as it means they have already been added to the list up to the root item.
                break;
            }
            itemIncludes.itemPaths.insert(currentPath);
            currentPath = currentPath.pop();
        }
    }

    buildTree(sceneItem, buildRoot, itemIncludes);
    Q_EMIT layoutChanged();
}

void TreeModel::buildTree(
    const Ufe::SceneItem::Ptr& sceneItem,
    TreeItem*                  parentItem,
    ItemIncludes&              includes)
{
    _useSearchItems = includes.useItemList;
    if (!includes.useItemList
        || includes.itemPaths.find(sceneItem->path()) != includes.itemPaths.end()) {
        if (includes.root) {

            int startRow = static_cast<int>(parentItem->childCount());
            int lastRow = startRow + 1;

            QModelIndex parentIdx = {};
            if (auto parentSceneItem = parentItem->sceneItem()) {
                parentIdx = getIndexFromPath(parentSceneItem->path());
            }

            beginInsertRows(parentIdx, startRow, lastRow);
            parentItem->appendChild(sceneItem);
            endInsertRows();
        }

        includes.root = true;
        // If using the item list (most likely from a search), also cache those items.
        // The cached items will be used by the `fetchMore` method to determine which
        // children should be added to the view.
        if (_useSearchItems) {
            _searchItemsPaths = includes.itemPaths;
        }
    }
}

bool TreeModel::canDropMimeData(
    const QMimeData*   data,
    Qt::DropAction     action,
    int                row,
    int                column,
    const QModelIndex& parent) const
{
    if (!data->hasFormat(UFE_PATHS_MIME_TYPE)) {
        return false;
    }
    return true;
}
} // namespace UfeUi
