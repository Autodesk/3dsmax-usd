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
#ifndef UFEUI_EXPLORER_H
#define UFEUI_EXPLORER_H

#include <UFEUI/Widgets/qSpinnerOverlayWidget.h>
#include <UFEUI/itemSearch.h>
#include <UFEUI/treeColumn.h>

#include <ufe/attributesNotification.h>
#include <ufe/contextOps.h>
#include <ufe/object3dNotification.h>
#include <ufe/observer.h>
#include <ufe/subject.h>

#include <QtCore/qsortfilterproxymodel.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qtreeview.h>

namespace Ui {
class Explorer;
}

namespace UfeUi {

class ExplorerSearchThread;
class TreeModel;

/**
 * \brief A UFE TreeView explorer widget, with search/filtering/edit capabilities.
 */
class UFEUIAPI Explorer : public QWidget
{
    Q_OBJECT

public:
    struct ColorScheme
    {
        QColor hover;
        QColor selected;
        QColor selectedHover;
    };

    /**
     * \brief Constructor.
     * \param rootItem Root UFE item to build the tree from.
     * \param columns Column definitions.
     * \param typeFilter Type filtering configuration - used to filter out items by type.
     * \param childFilter Ufe Hierarchy child filter, filters item when traversing the
     * hierarchy. Used by the runtime hierarchy implementation.
     * \param autoExpandToSelection Whether the explorer should auto-expand when the selection changes.
     * \param styleSheet QT style sheet for the treeview. Can be empty.
     * \param colorScheme Color scheme to use in the explorer.
     * \param parent QT parent object.
     */
    explicit Explorer(
        const Ufe::SceneItem::Ptr&   rootItem,
        const TreeColumns&           columns,
        const TypeFilter&            typeFilter,
        Ufe::Hierarchy::ChildFilter& childFilter,
        bool                         autoExpandToSelection,
        const QString&               styleSheet,
        const ColorScheme&           colorScheme,
        QWidget*                     parent = nullptr);

    /**
     * \brief Destructor.
     */
    ~Explorer() override;

    /**
     * \brief Returns the root UFE item in the explorer.
     * \return The root item.
     */
    Ufe::SceneItem::Ptr rootItem() const;

    /**
     * \brief Updates the treeview item associated with the given UFE path.
     * \param itemPath The item's path.
     */
    void updateItem(const Ufe::Path& itemPath) const;

    /**
     * \brief The explorer's observer for the UFE scene.
     * \return The observer.
     */
    Ufe::Observer::Ptr observer();

    /**
     * \brief The TreeModel backing the TreeView in the explorer.
     * \return The tree model.
     */
    TreeModel* treeModel() const;

    QTreeView* treeView() const;

    /**
     * \brief The current search filter.
     * \return The search filter string.
     */
    QString searchFilter() const;

    /**
     * \brief The current type filter.
     * \return The type filter.
     */
    const TypeFilter& typeFilter();

    /**
     * \brief Sets the hierarchy ChildFilter. If the filter set differs from the
     * previously set filter, the tree is updated.
     * \param childFilter The childFilter to set.
     */
    void setChildFilter(const Ufe::Hierarchy::ChildFilter& childFilter);

    /**
     * \brief Returns the current hierarchy child filter.
     * \return The child filter.
     */
    const Ufe::Hierarchy::ChildFilter& childFilter();

    /**
     * \brief Gets whether the explorer will auto-expand and scroll to the current
     * selection when it changes.
     * \return True if the explorer will auto-expand.
     */
    bool isAutoExpandedToSelection() const;

    /**
     * \brief Sets whether the explorer will auto-expand and scroll to the current
     * selection when it changes.
     * \param autoExpandToSelection True if the explorer should auto-expand.
     */
    void setAutoExpandedToSelection(bool autoExpandToSelection);

    /**
     * \brief Returns the color scheme used by the explorer.
     * \return The color scheme.
     */
    ColorScheme colorScheme();

    /**
     * \brief Sets the color scheme that should be used by the explorer.
     * \param colorScheme The color scheme.
     */
    void setColorScheme(const ColorScheme& colorScheme);

    /**
     * \brief Returns the first visible (meaning the parent is expanded, but not
     * necessarily currently visible on screen) ancestors of the current selection,
     * only ancestors of currently invisible selected items are returned. This can
     * be used to highlight ancestors of hidden selected items, for example.
     * \return The selection ancestors.
     */
    const std::vector<QPersistentModelIndex>& selectionAncestors();

    /**
     * \brief Sets the hidden state of a column of the tree view
     * \param visualIdx The visual index of the column of the tree view
     * \param hidden The hidden state of the column
     */
    void setColumnState(int visualIdx, bool hidden);

    void setIgnoreUfeNotifications(bool ignore) { _ignoreUfeNotifications = ignore; }
    bool isIgnoringUfeNotifications() const { return _ignoreUfeNotifications; }

protected:
    /**
     * \brief Setup the UI for the explorer.
     * \param rootItem The root UFE item to build the treeview with.
     */
    void setupUiFromRootItem(const Ufe::SceneItem::Ptr& rootItem);

    /**
     * \brief Callback function executed upon changing the text in the search box.
     * \param searchFilter The new content of the search box.
     */
    void onSearchFilterChanged(const QString& searchFilter);

    /**
     * \brief Callback function executed upon selecting items in the TreeView.
     * \param selectedItems The list of TreeView items which were selected.
     * \param deselectedItems The list of TreeView items which were deselected.
     */
    void onTreeViewSelectionChanged(
        const QItemSelection& selectedItems,
        const QItemSelection& deselectedItems);

    /**
     * \brief Completely rebuild the subtree below the given item.
     * \param item The root of the subtree to rebuild.
     */
    void rebuildSubtree(const TreeItem* item);

    /**
     * \brief React to a context menu being requested at the given point.
     * \param pos Position on screen where the context menu is requested.
     */
    void onCustomContextMenuRequested(const QPoint& pos);

    /**
     * \brief Makes sure the given indices are visible, by expanding all of their ancestors.
     * \param selectedIndices The selected indices we want to expand to. The function will
     * also scroll the view so that the first selected index is visible.
     */
    void expandToSelection(const QItemSelection& selectedIndices);

    /**
     * \brief Build a QMenu context menu from a ContextOps interface, and parent context ops item path. It is called
     * recursively to build submenus.
     * \param contextOps Reference to the context ops object.
     * \param menu The QT menu to add items to.
     * \param parentPath The parent context ops item path. Its children items will be added to the menu.
     */
    void buildContextMenu(
        const Ufe::ContextOps::Ptr&      contextOps,
        QMenu*                           menu,
        const Ufe::ContextOps::ItemPath& parentPath);

    /**
     * \brief React to click events on items.
     * \param index The clicked index (proxy model index).
     */
    void onItemClicked(const QModelIndex& index) const;

    /**
     * \brief React to doubleClick events on items.
     * \param index The clicked index (proxy model index).
     */
    void onItemDoubleClicked(const QModelIndex& index) const;

    /**
     * \brief Returns the column associated with the given index (tree model index)
     * \param index The index to get the column for.
     * \return The tree column, or nullptr if the index is out of range.
     */
    TreeColumn* column(const QModelIndex& index) const;

    /**
     * \brief If selected items are not visible because some ancestor is collapsed,
     * we highlight the ancestor to show it. This method will update this state.
     */
    void updateSelectionAncestors();

    /**
     * \brief Checks whether a path is relevant to the explorer, in that it is a descendant
     * of the root of the explorer.
     * \param path The path to check.
     * \return True if the path is a descendant of the root of the explorer.
     */
    bool isRelevantToExplorer(const Ufe::Path& path) const;

    /**
     * \brief Updates the treeview selection from the global UFE selection.
     */
    void updateTreeSelection();

private:
    /**
     * \brief Observes a UFE subject and update the explorer accordingly.
     */
    class Observer : public Ufe::Observer
    {
    public:
        Observer(Explorer* explorer)
            : _explorer(explorer)
        {
        }

        void operator()(const Ufe::Notification& notification) override;

    private:
        Explorer* _explorer = nullptr;
    };

    /// The root UFE item displayed in the explorer.
    Ufe::SceneItem::Ptr _rootItem;
    /// Column definitions for the explorer.
    TreeColumns _columns;
    /// Reference to the Qt UI View of the dialog
    std::unique_ptr<Ui::Explorer> _ui { std::make_unique<Ui::Explorer>() };
    /// Reference to the Model holding the structure of the UFE hierarchy,
    std::unique_ptr<TreeModel> _treeModel;
    /// Reference to the Proxy Model used to filter the UFE hierarchy.
    std::unique_ptr<QSortFilterProxyModel> _proxyModel;
    /// Overlay on which to display an animated Spinner or message to the User.
    std::unique_ptr<QSpinnerOverlayWidget> _overlay;
    /// Reference to the thread used to perform UFE scene item searches in the background.
    std::unique_ptr<ExplorerSearchThread> _searchThread;
    /// Keep track of the previous search, this is to control the saving/restoring of
    /// the expanded items after clearing the search filter.
    QString _previousSearchFilter;
    /// The currently expanded items, when a search is initiated. To be restored after the
    /// search filter is cleared.
    std::vector<Ufe::Path> _preSearchExpandedPaths;
    /// Reference to the timer used to display a Spinner overlay on top of the TreeView in case of
    /// lengthy search operations
    std::unique_ptr<QTimer> _searchTimer;
    /// Filtering of UFE items based on scene item type.
    TypeFilter _typeFilter;
    /// Child filter (For example, used to optionally filter inactive prims in the USD runtime.)
    Ufe::Hierarchy::ChildFilter _childFilter;
    /// The explorer's own observer of changes in the scene.
    Ufe::Observer::Ptr _sceneObserver = nullptr;
    /// Observers of scene edition notification, sent from the explorer.
    std::vector<Ufe::Observer::Ptr> _treeEditObservers;
    /// Auto-expand to selection.
    bool _autoExpandToSelection = false;
    /// Color scheme for the explorer
    ColorScheme _colorScheme;
    /// Current ancestor of selected items that need highlighting.
    std::vector<QPersistentModelIndex> _selectionAncestors;
    /// Some UFE items may not be displayed in the explorer (for example USD point instances)
    /// But we still want to highlight their parent. This set contains such parents, that should
    /// be lit, even though their children are not actually in the tree.
    std::set<QPersistentModelIndex> _parentHighlightExtend;

    class ExpansionGuard
    {
    public:
        ExpansionGuard(Explorer* explorer);
        ~ExpansionGuard();

    private:
        Explorer* _explorer;
        bool      _isNested = false;
    };
    /// Flag telling us that we are in the process of programmatically changing the
    /// expansion state of items in the view, used to avoid triggering
    /// unnecessary work from the expand and collapse signals.
    bool _inSelectionExpansion = false;
    bool _ignoreUfeNotifications = false;
};

} // namespace UfeUi

#endif // UFEUI_EXPLORER_H
