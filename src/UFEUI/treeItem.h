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
#ifndef UFEUI_TREEITEM_H
#define UFEUI_TREEITEM_H

#include "TreeModel.h"

#include <ufe/sceneItem.h>

namespace UfeUi {

class TreeItem
{

public:
    typedef std::unique_ptr<TreeItem> Ptr;
    typedef std::vector<TreeItem*>    TreeItems;
    /**
     * \brief Constructor.
     * \param model TreeModel to which the item belong.
     * \param sceneItem The UFE scene item associated with the tree item.
     */
    explicit UFEUIAPI TreeItem(TreeModel* model, const Ufe::SceneItem::Ptr& sceneItem);

    /**
     * \brief Destructor.
     */
    virtual UFEUIAPI ~TreeItem();

    /**
     * \brief Append a child to the TreeItem, from a UFE scene item.
     * \param child The UFE item to append.
     * \return The corresponding TreeItem that was created.
     */
    UFEUIAPI TreeItem* appendChild(const Ufe::SceneItem::Ptr& sceneItem);

    /**
     * \brief Gets the child at the given row (index).
     * \param row The row of the child to fetch.
     * \return The child item.
     */
    UFEUIAPI TreeItem* child(int row) const;

    /**
     * \brief Returns the number of children of the item.
     * \return The number of children.
     */
    UFEUIAPI size_t childCount() const;

    /**
     * \brief Returns the row of the item, from the parent.
     * \return Return the index from the parent.
     */
    UFEUIAPI int row() const;

    /**
     * \brief Return the parent item.
     * \return The parent.
     */
    UFEUIAPI TreeItem* parentItem() const;

    /**
     * \brief A unique identifier for the TreeItem. Essentially the hash of the corresponding UFE path.
     * \return The unique identifier.
     */
    UFEUIAPI size_t uniqueId() const;

    /**
     * \brief Returns the resolved visible state of the item. Uses some caching.
     * \return True if the item is visible.
     */
    virtual UFEUIAPI bool computedVisibility() const;

    /**
     * \brief Return the scene item associated with the treeview item.
     * \return The UFE scene item.
     */
    UFEUIAPI Ufe::SceneItem::Ptr sceneItem() const { return _item; }

    /**
     * \brief Clear all child items.
     */
    UFEUIAPI void clearChildren();

    /**
     * \brief Removes the given child item.
     * \param item The child item to remove.
     */
    UFEUIAPI void removeChild(TreeItem* item);
    ;

    /**
     * \brief Finds descendant, using a predicate.
     * NOTE : If an item does not satisfy the predicate, the search is stopped
     * in that subtree.
     * \param predicate The predicate to search with.
     * \return Found items.
     */
    UFEUIAPI TreeItems findDescendants(const std::function<bool(const TreeItem*)>& predicate) const;

    /**
     * \brief Clears any cached state.
     */
    UFEUIAPI void clearStateCache();

private:
    /**
     * \brief Sets the parent of the item. Usually called when the item is added as a new child
     * of another item.
     * \param parent The new parent.
     */
    void setParent(TreeItem* parent);

    /**
     * \brief Returns the hash of the UFE path associated with the item, or 0.
     * \param item The tree item.
     * \return The hash.
     */
    static size_t hash(const TreeItem* item);

    TreeItem* _parent;
    // Children of the TreeItem. Owns the memory.
    std::vector<TreeItem::Ptr> _children;
    Ufe::SceneItem::Ptr        _item;
    TreeModel*                 _model;
    size_t                     _uniqueId;

    enum class VisStateCache
    {
        Visible,
        Hidden,
        None // Cache not set.
    };

    mutable VisStateCache _visCache = VisStateCache::None;
};

} // namespace UfeUi

#endif UFEUI_TREEITEM_H