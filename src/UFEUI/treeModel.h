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
#ifndef UFEUI_TREE_MODEL_H
#define UFEUI_TREE_MODEL_H

#include "ItemSearch.h"
#include "TreeColumn.h"
#include "UFEUIAPI.h"

#include <ufe/notification.h>
#include <ufe/path.h>
#include <ufe/sceneItem.h>
#include <ufe/subject.h>

#include <QtCore/QModelIndex>
#include <QtCore/QVariant>
#include <unordered_set>

namespace UfeUi {

class TreeItem;

/**
 * \brief Data model for the UFE treeview.
 */
class TreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    friend class TreeItem;

    explicit UFEUIAPI TreeModel(const TreeColumns& columns, QObject* parent);
    UFEUIAPI ~TreeModel() override;

    UFEUIAPI QVariant data(const QModelIndex& index, int role) const override;
    UFEUIAPI bool     setData(const QModelIndex& index, const QVariant& value, int role) override;
    UFEUIAPI Qt::ItemFlags flags(const QModelIndex& index) const override;
    UFEUIAPI QVariant
    headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    UFEUIAPI QModelIndex
    index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    UFEUIAPI QModelIndex parent(const QModelIndex& index) const override;
    UFEUIAPI int         rowCount(const QModelIndex& parent = QModelIndex()) const override;
    UFEUIAPI int         columnCount(const QModelIndex& parent = QModelIndex()) const override;

    /**
     * \brief Returns the root TreeItem of this model.
     * \return The root item.
     */
    UFEUIAPI TreeItem* root() const;

    /**
     * \brief Get the model index, in this model, of a given UFE path. If it doesnt exist in the model,
     * return an invalid index.
     * \param path The ufe path to get the index for.
     * \return The index.
     */
    UFEUIAPI QModelIndex getIndexFromPath(const Ufe::Path& path) const;

    /**
     * \brief Retrieve the TreeItem stored in the data of the given index.
     * \param index The index to get the item from.
     * \return The tree item.
     */
    UFEUIAPI TreeItem* treeItem(const QModelIndex& index) const;

    /**
     * \brief Updates the row associated with a single UFE path. This will clear any cached UI data
     * on the associated TreeItem (and descendants) and signal QT that the data may have changed.
     * \param path The ufe path of the item to update.
     */
    UFEUIAPI void update(const Ufe::Path& path);

    /**
     * \brief Builds an emptry TreeModel.
     * \param columns Definitions of columns that should appear in the tree.
     * we want executed when some data is set on the model.
     * \param parent Parent QT object.
     * \return The created TreeModel.
     */
    static UFEUIAPI std::unique_ptr<TreeModel> create(const TreeColumns& columns, QObject* parent);

    /**
     * \brief Builds a TreeModel from hierarchy search.
     * \param root The root UFE item to build the hierarchy from.
     * we want executed when some data is set on the model.
     * \param searchFilter The search filter (can user regex).
     * \param typeFilter Type filtering configuration (to include or exclude certain types)
     * \param childFilter Ufe Hierarchy child filter, filters item when traversing the
     * hierarchy. Used by the runtime hierarchy implementation.
     * \param includeRoot Whether or not the root item should be included in the model.
     * \return The created TreeModel.
     */
    UFEUIAPI void buildTreeFrom(
        TreeItem*                          buildRoot,
        const Ufe::SceneItem::Ptr&         root,
        const std::string&                 searchFilter,
        const TypeFilter&                  typeFilter,
        const Ufe::Hierarchy::ChildFilter& childFilter,
        bool                               includeRoot = true);

    // Utility class to keep track of what items need to / have been included as we
    // are building the tree structure
    struct UFEUIAPI ItemIncludes
    {
        /// Whether or not the root item should be included.
        bool root = true;
        /// Are we using an explicit of ufe paths that should be included? (search scenario)
        bool useItemList = false;
        // Allowed UFE paths.
        std::unordered_set<Ufe::Path> itemPaths;
        /// How many items do we still have to insert in the tree, vs what we expect.
        size_t insertionsRemaining = 0;
    };

private:
    /**
     * \brief Builds a subtree. Called recursively. Generally speaking, the given item is added,
     * and buildTree() called on its children.
     * \param model The tree model to build into
     * \param sceneItem The top level item of the subtree.
     * \param parentItem The parent item.
     * \param includes Items includes config. Can be used to exclude some items from the tree.
     */
    void buildTree(
        const Ufe::SceneItem::Ptr&         sceneItem,
        TreeItem*                          parentItem,
        ItemIncludes&                      includes,
        const Ufe::Hierarchy::ChildFilter& childFilter);

    TreeItem*                                    _rootItem;
    TreeColumns                                  _columns;
    QHash<size_t, QPair<QModelIndex, TreeItem*>> _treeItemMap;
};

} // namespace UfeUi

#endif // UFEUI_TREE_MODEL_H
