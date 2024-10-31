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
#include "explorer.h"

#include "ui_Explorer.h"

#include <UFEUI/ReplaceSelectionCommand.h>
#include <UFEUI/editCommand.h>
#include <UFEUI/explorerSearchThread.h>
#include <UFEUI/icon.h>
#include <UFEUI/qExplorerTreeViewContextMenu.h>
#include <UFEUI/treeModel.h>
#include <UFEUI/treeitem.h>
#include <UFEUI/utils.h>

#include <ufe/attributes.h>
#include <ufe/contextOps.h>
#include <ufe/globalSelection.h>
#include <ufe/hierarchy.h>
#include <ufe/object3d.h>
#include <ufe/observableSelection.h>
#include <ufe/scene.h>
#include <ufe/sceneItem.h>
#include <ufe/sceneNotification.h>
#include <ufe/selectionNotification.h>
#include <ufe/undoableCommandMgr.h>

#include <QtWidgets/QActionGroup.h>
#include <QtWidgets/QHeaderView.h>
#include <QtWidgets/QMenu.h>
#include <QtWidgets/QStyledItemDelegate.h>

namespace UfeUi {

Explorer::Explorer(
    const Ufe::SceneItem::Ptr&   rootItem,
    const TreeColumns&           columns,
    const TypeFilter&            typeFilter,
    Ufe::Hierarchy::ChildFilter& childFilter,
    bool                         autoExpandToSelection,
    const QString&               styleSheet,
    const ColorScheme&           colorScheme,
    QWidget*                     parent)
    : QWidget { parent }
    , _rootItem { rootItem }
    , _columns(columns)
    , _ui(new Ui::Explorer)
    , _typeFilter { typeFilter }
    , _childFilter { childFilter }
    , _autoExpandToSelection { autoExpandToSelection }
    , _sceneObserver { std::make_shared<Observer>(this) }
    , _colorScheme { colorScheme }
{
    _ui->setupUi(this);
    setParent(parent);

    setupUiFromRootItem(rootItem);
    const auto treeView = _ui->treeView;

    for (auto& c : _columns) {
        c->addExplorer(this);
    }

    treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    treeView->setSelectionBehavior(QAbstractItemView::SelectRows);

    if (!styleSheet.isEmpty()) {
        treeView->setStyleSheet(styleSheet);
    }

    // Get the explorer to observer scene events. The explorer itself is not an observer because of
    // UFE requires observers to be held in shared ptrs... but that is not a good idea for QT
    // objects. Therefor, the explorer just owns an explorer, which reports to it.
    Ufe::Scene::instance().addObserver(_sceneObserver);
    Ufe::Object3d::addObserver(_sceneObserver);
    Ufe::Attributes::addObserver(_sceneObserver);

    // Also observe the global selection.
    if (auto globalSelection = Ufe::GlobalSelection::get()) {
        globalSelection->addObserver(_sceneObserver);
    }

    // ctrl+click on expand arrow
    connect(treeView, &QTreeView::expanded, [this](const QModelIndex& index) {
        // Use the guard to avoid triggering costly updates for every expand, do it all at once.
        const auto expandGuard = ExpansionGuard { this };
        if (QApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
            _ui->treeView->expandRecursively(index);
        }
    });
    // ctrl+click on collapse arrow
    connect(treeView, &QTreeView::collapsed, [this](const QModelIndex& index) {
        // Use the guard to avoid triggering costly updates for every collapse, do it all at once.
        const auto expandGuard = ExpansionGuard { this };
        if (QApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
            int childCount = index.model()->rowCount(index);
            for (int i = 0; i < childCount; i++) {
                QModelIndex child = index.model()->index(i, 0, index);
                _ui->treeView->collapse(child);
            }
        }
    });

    // Columns may define their own item delegates.
    for (int i = 0; i < columns.size(); ++i) {
        const auto styleDelegate = columns[i]->createStyleDelegate(this);
        if (styleDelegate) {
            treeView->setItemDelegateForColumn(i, styleDelegate);
        }
    }

    QHeaderView* header = treeView->header();
    if (header != nullptr && !columns.empty()) {
        // Setup custom style for headers. If the header label is only an icon, this style makes
        // sure it is centered. Nothing else worked - the usual align flags only work on text, for
        // some reason.
        header->setStyle(new Icon::CenteredIconHeaderStyle(header->style()));

        header->setMinimumSectionSize(static_cast<int>(32 * Utils::dpiScale()));
        header->setSectionResizeMode(0, QHeaderView::ResizeMode::ResizeToContents);
        header->setSectionResizeMode(1, QHeaderView::ResizeMode::ResizeToContents);
        header->setSectionResizeMode(2, QHeaderView::ResizeMode::Interactive);
        header->setStretchLastSection(true);

        // Arbitrary width for the name column.
        static int primNameColumnWidth = static_cast<int>(190 * Utils::dpiScale());
        treeView->setColumnWidth(0, primNameColumnWidth);
    }

    // Initialize the selection in the tree from the UFE selection, which might already exist.
    updateTreeSelection();
}

void Explorer::setColumnState(int visualIdx, bool hidden)
{
    if (hidden) {
        _ui->treeView->hideColumn(visualIdx);
    } else {
        _ui->treeView->showColumn(visualIdx);
    }
}

Explorer::~Explorer()
{
    Ufe::Scene::instance().removeObserver(_sceneObserver);
    Ufe::Object3d::removeObserver(_sceneObserver);
    for (auto& c : _columns) {
        c->removeExplorer(this);
    }
}

void Explorer::setupUiFromRootItem(const Ufe::SceneItem::Ptr& rootItem)
{
    if (!rootItem) {
        return;
    }

    _treeModel = TreeModel::create(_columns, nullptr);
    _treeModel->buildTreeFrom(_treeModel->root(), rootItem, "", {}, _childFilter, true);

    // Configure the treeview.
    _proxyModel = std::make_unique<QSortFilterProxyModel>(this);
    _proxyModel->setSourceModel(_treeModel.get());
    _proxyModel->setDynamicSortFilter(false);
    _proxyModel->setFilterCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
    _ui->treeView->setModel(_proxyModel.get());
    _ui->treeView->expandToDepth(1);

    QHeaderView* treeHeader = _ui->treeView->header();

    // Move columns based on the specified visual indices in the column definitions.
    for (int logicalIdx = 0; logicalIdx < _columns.size(); ++logicalIdx) {
        const auto curVisualIdx = treeHeader->visualIndex(logicalIdx);
        treeHeader->moveSection(curVisualIdx, _columns[logicalIdx]->visualIndex());
    }

    connect(_ui->filterLineEdit, &QLineEdit::textChanged, this, &Explorer::onSearchFilterChanged);
    connect(
        _ui->treeView->selectionModel(),
        &QItemSelectionModel::selectionChanged,
        this,
        &Explorer::onTreeViewSelectionChanged);

    // Hook up context menu, for Ufe ContextOps.
    _ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        _ui->treeView,
        &QTreeView::customContextMenuRequested,
        this,
        &Explorer::onCustomContextMenuRequested);

    // Item clicked events, (just forwarded to columns implementations).
    connect(_ui->treeView, &QTreeView::clicked, this, &Explorer::onItemClicked);
    connect(_ui->treeView, &QTreeView::doubleClicked, this, &Explorer::onItemDoubleClicked);

    for (int i = 0; i < _columns.size(); ++i) {
        _ui->treeView->setColumnSelectable(i, _columns[i]->isSelectable());
    }

    // Create the Spinner overlay on top of the TreeView, once it is configured:
    _overlay = std::make_unique<QSpinnerOverlayWidget>(_ui->treeView);

    // Need to manually call the filter function if a filter had already been typed
    if (!_ui->filterLineEdit->text().isEmpty()) {
        onSearchFilterChanged(_ui->filterLineEdit->text());
        _overlay->resize(_ui->treeView->size());
    }
}

Ufe::SceneItem::Ptr Explorer::rootItem() const { return _rootItem; }

void Explorer::onItemClicked(const QModelIndex& index) const
{
    const auto& sourceIndex = _proxyModel->mapToSource(index);
    const auto  col = column(sourceIndex);
    if (col) {
        const auto treeItem = _treeModel->treeItem(sourceIndex);
        if (treeItem) {
            col->clicked(treeItem);
        }
    }
}

void Explorer::onItemDoubleClicked(const QModelIndex& index) const
{
    const auto& sourceIndex = _proxyModel->mapToSource(index);
    const auto  col = column(sourceIndex);
    if (col) {
        const auto treeItem = _treeModel->treeItem(sourceIndex);
        if (treeItem) {
            col->doubleClicked(treeItem);
        }
    }
}

TreeColumn* Explorer::column(const QModelIndex& index) const
{
    const auto col = index.column();
    if (!index.isValid() || col < 0 || col >= _columns.size()) {
        return nullptr;
    }
    return _columns[col].get();
}

void Explorer::updateSelectionAncestors()
{
    auto       selection = _ui->treeView->selectionModel()->selection().indexes();
    const auto previous = _selectionAncestors;

    _selectionAncestors.clear();

    for (const auto& item : _parentHighlightExtend) {
        auto parent = item.parent();
        if (!parent.isValid()) {
            continue;
        }
        if (_ui->treeView->isExpanded(parent)) {
            _selectionAncestors.push_back(item);
            continue;
        }
        // Act as if it was selected to compute what ancestor should be lit up.
        selection.push_back(item);
    }

    // For each item in the selection, we have to figure out if we need to highlight
    // an ancestor, in case it itself is not visible because one of its ancestor is
    // collapsed.
    // If we are processing a multi-selection, keep track of indices that we have already
    // visited to avoid duplicating work. For each selected item we recursively look at
    // all ancestors and highlight the top-most collapsed one that we find. When items
    // share ancestors, the answer is the same. So when we find that an item was already
    // visited, we can stop.
    std::unordered_set<quintptr> visited;

    for (const auto& idx : selection) {
        if (idx.column() != 0) {
            continue;
        }

        QModelIndex ancestorToHighlight;
        QModelIndex current = idx;
        do {
            current = current.parent();

            // Did we already visit this index when processing another selected item?
            const auto srcIdx = _proxyModel->mapToSource(current);
            // internalId of the tree model index is a unique hash of the ufe path.
            const auto key = srcIdx.internalId();
            if (visited.find(key) != visited.end()) {
                continue;
            }

            // If the index is collapsed, it's a candidate, but keep going up...
            if (!_ui->treeView->isExpanded(current)) {
                ancestorToHighlight = current;
            }
            visited.insert(key);
        } while (current.parent().isValid());

        if (ancestorToHighlight.isValid()) {
            _selectionAncestors.emplace_back(ancestorToHighlight);
        }
    }

    // Did anything actually change?
    if (std::equal(
            previous.begin(),
            previous.end(),
            _selectionAncestors.begin(),
            _selectionAncestors.end())) {
        return;
    }

    _ui->treeView->viewport()->repaint();
}

bool Explorer::isRelevantToExplorer(const Ufe::Path& path) const
{
    return path.startsWith(rootItem()->path());
}

void Explorer::updateTreeSelection()
{

    auto currentHighlightExtend = _parentHighlightExtend;
    _parentHighlightExtend.clear();

    const auto& globalSelection = Ufe::GlobalSelection::get();
    if (globalSelection && !globalSelection->empty()) {
        std::unordered_set<Ufe::Path> newPaths;
        for (const auto& item : *globalSelection) {
            if (item == nullptr) {
                continue;
            }

            const auto& path = item->path();
            if (isRelevantToExplorer(path)) {
                newPaths.insert(path);
            }
        }

        // Querying and assigning the QT selection state is very slow. Make sure we really need to.
        // selectedIndexes() is the fastest way to get the selection. selectedRows() is dead slow.
        std::unordered_set<Ufe::Path> currentSelection;
        const auto&                   indexes = _ui->treeView->selectionModel()->selectedIndexes();
        for (auto& idx : indexes) {
            if (idx.column() != 0) {
                continue;
            }
            const auto item = treeModel()->treeItem(_proxyModel->mapToSource(idx));
            if (!item) {
                continue;
            }
            currentSelection.insert(item->sceneItem()->path());
        }

        if (newPaths == currentSelection) {
            return;
        }

        // Selecting each index individually in QT is dead slow, under the hood QT insist on doing
        // absolutely everything O(N^2). What we need to do is select "ranges" of indices at the
        // same time and QT is able to deal with that a bit better.
        std::set<QModelIndex> toSelect;
        for (const auto& path : newPaths) {
            const auto modelIdx = treeModel()->getIndexFromPath(path);
            if (!modelIdx.isValid()) {
                // "Parent-Highlight" parent if it is in the tree.
                const auto parentPath = path.pop();
                const auto parentModelIdx = treeModel()->getIndexFromPath(parentPath);
                if (parentModelIdx.isValid()) {
                    _parentHighlightExtend.insert(_proxyModel->mapFromSource(parentModelIdx));
                }
                continue;
            }
            toSelect.insert(_proxyModel->mapFromSource(modelIdx));
        }

        // From each index, build a new range if the index was not already dealt with.
        QItemSelection selection;
        // Keep track of processed items, i.e. already part of a selection range. Use the
        // internal id of the source index - which is the hash of the ufe path. The proxy
        // index's id is not guaranteed to be unique.
        std::unordered_set<quintptr> processed;
        for (const auto& idx : toSelect) {
            if (processed.find(_proxyModel->mapToSource(idx).internalId()) != processed.end()) {
                continue;
            }
            const auto parent = idx.parent();

            // Create a new range to select.
            QModelIndex top;
            QModelIndex bottom;

            // Lambda to expand the range in a direction, will go up or down, and expand
            // the range if the next index is also selected.
            auto expandRange
                = [this, &idx, &parent, &toSelect, &processed](bool up, QModelIndex& bound) {
                      bound = idx;
                      QModelIndex next;
                      while (true) {
                          // Figure out the next index, from the expand direction..
                          if (up) {
                              next = treeView()->indexAbove(bound);
                          } else {
                              next = treeView()->indexBelow(bound);
                          }

                          // If the next item is not a sibling, or is not selected, we are done.
                          if (next.parent() != parent || toSelect.find(next) == toSelect.end()) {
                              break;
                          }

                          bound = next;
                          processed.insert(_proxyModel->mapToSource(next).internalId());
                      }
                  };

            // Find top/bottom range boundaries.
            expandRange(true, top);
            expandRange(false, bottom);

            selection.select(top, bottom.siblingAtColumn(static_cast<int>(_columns.size()) - 1));
        }

        _ui->treeView->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
        if (isAutoExpandedToSelection()) {
            expandToSelection(selection);
        }
    } else {
        _ui->treeView->selectionModel()->clearSelection();
    }

    // When the tree selection changes, updateSelectionAncestors() is called, but
    // we also need to force it here, if selected items that are not in the tree should
    // impact parent highlighting.
    if (currentHighlightExtend != _parentHighlightExtend) {
        updateSelectionAncestors();
    }
}

Explorer::ColorScheme Explorer::colorScheme() { return _colorScheme; }

void Explorer::setColorScheme(const ColorScheme& colorScheme) { _colorScheme = colorScheme; }

const std::vector<QPersistentModelIndex>& Explorer::selectionAncestors()
{
    return _selectionAncestors;
}

bool Explorer::isAutoExpandedToSelection() const { return _autoExpandToSelection; }

void Explorer::setAutoExpandedToSelection(bool autoExpandToSelection)
{
    // If we are turning on the option, expand to the current selection.
    if (autoExpandToSelection && !_autoExpandToSelection) {
        expandToSelection(_ui->treeView->selectionModel()->selection());
    }
    _autoExpandToSelection = autoExpandToSelection;
}

void Explorer::setChildFilter(const Ufe::Hierarchy::ChildFilter& childFilter)
{
    if (!Utils::filtersAreEqual(_childFilter, childFilter)) {
        _childFilter = childFilter;

        rebuildSubtree(treeModel()->root()->child(0));
    }
}

const Ufe::Hierarchy::ChildFilter& Explorer::childFilter() { return _childFilter; }

void Explorer::updateItem(const Ufe::Path& itemPath) const
{
    treeView()->setUpdatesEnabled(false);
    _treeModel->update(itemPath);
    treeView()->setUpdatesEnabled(true);
}

Ufe::Observer::Ptr Explorer::observer() { return _sceneObserver; }

TreeModel* Explorer::treeModel() const { return _treeModel.get(); }

QTreeView* Explorer::treeView() const { return _ui->treeView; }

QString Explorer::searchFilter() const { return _ui->filterLineEdit->text(); }

const TypeFilter& Explorer::typeFilter() { return _typeFilter; }

void Explorer::onSearchFilterChanged(const QString& searchFilter)
{
    // Stop any search that was already ongoing but that has not yet completed:
    if (_searchThread != nullptr && !_searchThread->isFinished()) {
        _searchThread->quit();
        _searchThread->wait();
    }

    // Create a timer that will display a Spinner if the search has been ongoing for a (small)
    // amount of time, to let the User know that a background task is ongoing and that the widget is
    // not frozen:
    _searchTimer = std::make_unique<QTimer>(this);
    _searchTimer->setSingleShot(true);
    connect(_searchTimer.get(), &QTimer::timeout, _searchTimer.get(), [this]() {
        _ui->treeView->setEnabled(false);
        _overlay->startSpinning();
    });
    _searchTimer->start(std::chrono::milliseconds(125).count());

    // Create a thread to perform a search for the given criteria in the background in order to
    // maintain a responsive UI that continues accepting input from the User:
    _searchThread = std::make_unique<ExplorerSearchThread>(
        _rootItem, _columns, searchFilter.toStdString(), _typeFilter, _childFilter);
    connect(
        _searchThread.get(),
        &ExplorerSearchThread::finished,
        _searchThread.get(),
        [&, searchFilter, this]() {
            // Since results have been received, discard the timer that was waiting for results so
            // that the Spinner Widget is not displayed:
            _searchTimer->stop();

            // Starting a new search. Store the expanded paths, so that we get back to this state
            // once we are done searching.
            if (_previousSearchFilter.isEmpty() && !searchFilter.isEmpty()) {
                Utils::findExpandedPaths(
                    _treeModel.get(),
                    _proxyModel.get(),
                    _ui->treeView,
                    _treeModel->root(),
                    _preSearchExpandedPaths);
            }

            // Set the search results as the new effective data:
            _treeModel = std::move(_searchThread->consumeResults());
            _proxyModel->setSourceModel(_treeModel.get());

            // Set the View to a sensible state to reflect the new data:
            const bool searchYieledResults = _proxyModel->hasChildren();

            // If we just cleared the search filter, expand paths as we had them before we started
            // searching.
            if (!_previousSearchFilter.isEmpty() && searchFilter.isEmpty()) {
                if (!_preSearchExpandedPaths.empty()) {
                    // Expand the pseudo-root, not tied to a path. We know we need to do this, as we
                    // have at least one UFE path expanded.
                    const auto rootIdxInProxy
                        = _proxyModel->mapFromSource(_treeModel->index(0, 0, QModelIndex {}));
                    _ui->treeView->setExpanded(rootIdxInProxy, true);

                    Utils::expandPaths(
                        _ui->treeView,
                        _treeModel.get(),
                        _proxyModel.get(),
                        _preSearchExpandedPaths);
                    _preSearchExpandedPaths.clear();
                }
            } else {
                // While searching, expand all.
                _ui->treeView->expandAll();
            }

            _previousSearchFilter = searchFilter;

            _ui->treeView->selectionModel()->clearSelection();
            _ui->treeView->setEnabled(searchYieledResults);

            if (searchYieledResults) {
                _overlay->hide();
            } else {
                _overlay->showInformationMessage(QObject::tr("No results found."));
            }
        });

    _searchThread->start(QThread::Priority::TimeCriticalPriority);
}

void Explorer::onTreeViewSelectionChanged(
    const QItemSelection& selectedItems,
    const QItemSelection& deselectedItems)
{
    const auto&    globalSelection = *Ufe::GlobalSelection::get();
    Ufe::Selection newSelection { globalSelection };

    auto processItems = [this, &newSelection](const QItemSelection& items, bool select) {
        for (const auto& index : _proxyModel->mapSelectionToSource(items).indexes()) {
            if (index.column() != 0) {
                continue;
            }

            const auto treeItem = _treeModel->treeItem(index);
            if (!treeItem) {
                continue;
            }
            const auto ufeSceneItem = treeItem->sceneItem();
            if (!ufeSceneItem) {
                continue;
            }
            if (select) {
                newSelection.append(ufeSceneItem);
                continue;
            }
            newSelection.remove(ufeSceneItem);
        }
    };

    // Reflect deselected and selected items in the UFE selection.
    processItems(deselectedItems, false);
    processItems(selectedItems, true);

    // If the new selection is equivalent the current selection, it means the selection
    // was changed from outside of the explorer, only need to update the selection
    // ancestor highlighting.
    if (Utils::selectionsAreEquivalent(newSelection, globalSelection)) {
        updateSelectionAncestors();
        return;
    }

    // Selection was changed from explorer, remove any item that are not displayed in the explorer.
    std::vector<Ufe::SceneItemPtr> toRemove;
    for (const auto& si : newSelection) {
        if (!isRelevantToExplorer(si->path())) {
            continue;
        }
        if (!treeModel()->getIndexFromPath(si->path()).isValid()) {
            toRemove.push_back(si);
        }
    }
    for (const auto& si : toRemove) {
        newSelection.remove(si);
    }

    _parentHighlightExtend.clear();
    updateSelectionAncestors();

    Ufe::UndoableCommandMgr::instance().executeCmd(
        std::make_shared<ReplaceSelectionCommand>(newSelection));
}

void Explorer::rebuildSubtree(const TreeItem* item)
{
    // Save and restore the tree expand state as much as possible.
    auto expandGuard
        = Utils::ExpandStateGuard { _ui->treeView, item, _treeModel.get(), _proxyModel.get() };

    // Rebuild the tree from that item.
    const auto model = treeModel();
    const auto treeItem = model->treeItem(model->getIndexFromPath(item->sceneItem()->path()));
    if (!treeItem) {
        return;
    }
    model->buildTreeFrom(
        treeItem,
        item->sceneItem(),
        searchFilter().toStdString(),
        typeFilter(),
        _childFilter,
        false);
    return;
}

void Explorer::buildContextMenu(
    const Ufe::ContextOps::Ptr&      contextOps,
    QMenu*                           menu,
    const Ufe::ContextOps::ItemPath& parentPath)
{
    const auto items = contextOps->getItems(parentPath);

    QActionGroup* actionGroup = nullptr;
    // If all the items are checkable and exclusive, use an exclusive QActionGroup for the menu
    // items. In principle, QT should then use radio buttons. Seems like in 3dsMax, something is
    // forcing checkboxes...
    if (std::all_of(items.begin(), items.end(), [](const Ufe::ContextItem& item) {
            return item.checkable && item.exclusive;
        })) {
        actionGroup = new QActionGroup(menu);
        actionGroup->setExclusive(true);
    }

    for (const auto& item : items) {
        const auto& name = item.item;
        if (name.empty()) {
            menu->addSeparator();
            continue;
        }

        auto fullItemPath = parentPath;
        fullItemPath.push_back(item.item);

        // If the item has children, we need to create a submenu for it.
        if (item.hasChildren) {
            // Recurse.
            const auto subMenu
                = new QExplorerTreeViewContextMenu(QString::fromStdString(item.label), this);
            menu->addMenu(subMenu);
            buildContextMenu(contextOps, subMenu, fullItemPath);
        }
        // Otherwise, add an a menu action, and hook up the context ops cmd.
        else {
            Ufe::Value isHeader = item.getMetaData("isMenuHeader");
            if (!isHeader.empty() && isHeader.get<bool>()) {
                menu->addSection(QString::fromStdString(item.label));
            } else {
                const auto action = menu->addAction(QString::fromStdString(item.label));
                action->setCheckable(item.checkable);
                action->setChecked(item.checked);
                action->setActionGroup(actionGroup);
                if (!item.image.empty()) {
                    action->setIcon(UfeUi::Icon::build(item.image));
                }
                // Connect to command.
                connect(action, &QAction::triggered, [this, contextOps, fullItemPath]() {
                    // Wrap the context op command in an "edit command". Edit commands can add
                    // pre/post execution behaviors, for execute/undo/redo
                    try {
                        const auto cmd = contextOps->doOpCmd(fullItemPath);
                        if (cmd) {
                            const auto editCmd = UfeUi::EditCommand::create(
                                contextOps->sceneItem()->path(), cmd, "USD Stage Edit");
                            // Execute via the UndoableCommandManager - this way, execution can be
                            // extended by the DCC via a derived UndoableCommandMgr.
                            Ufe::UndoableCommandMgr::instance().executeCmd(editCmd);
                        }
                    } catch (std::exception&){
                        // UsdExpiredPrimAccessError exception thrown (from pxr/usd/usd/errors.h)
                    }

                    // Hack / Workaround :
                    // There is an issue where the view and model expand states get out of sync
                    // internally on the QT side. It may be related to
                    // https://bugreports.qt.io/browse/QTBUG-22546 The "view" and "model" sides get
                    // out of sync internally in qtreeview.cpp. Here
                    // https://code.qt.io/cgit/qt/qtbase.git/tree/src/widgets/itemviews/qtreeview.cpp?h=dev#n3099
                    // viewItems.at(item).expanded is true, while there is no matching entry in
                    // "expandedIndexes". I only ever got this for the top level TreeItem. When the
                    // issue happens, the symptom is that the root item suddenly appears collapsed
                    // (no collapse signal sent). The following code block works around this by
                    // re-expanding the root, if its found to not be expanded and we are in a
                    // context op on an item below it (situation which doesnt make sense, how can we
                    // right click a hidden item to open the context ops in the first place?). The
                    // issue is hard to reproduce consistently.
                    const auto topLevelItem = treeModel()->root()->child(0)->sceneItem();
                    if (contextOps->sceneItem()->path() != topLevelItem->path()) {
                        const auto idx = treeModel()->getIndexFromPath(topLevelItem->path());
                        const auto proxyIdx = _proxyModel->mapFromSource(idx);
                        const bool topLevelIsExpanded = _ui->treeView->isExpanded(proxyIdx);
                        if (!topLevelIsExpanded) {
                            _ui->treeView->setExpanded(proxyIdx, true);
                        }
                    }
                });
            }
        }
    }
}

void Explorer::onCustomContextMenuRequested(const QPoint& pos)
{
    // Figure out the treeModel index.
    const auto proxyIndex = _ui->treeView->indexAt(pos);
    if (!proxyIndex.isValid()) {
        return;
    }
    const auto src = _proxyModel->mapToSource(proxyIndex);
    const auto item = _treeModel->treeItem(src);
    if (!item) {
        return;
    }
    const auto sceneItem = item->sceneItem();
    if (!sceneItem) {
        return;
    }

    const auto contextOps = Ufe::ContextOps::contextOps(sceneItem);
    if (!contextOps) {
        return;
    }

    // Build the menu, and show it where it was requested.
    const auto menu = new QExplorerTreeViewContextMenu(nullptr, this);
    buildContextMenu(contextOps, menu, {});
    menu->exec(_ui->treeView->viewport()->mapToGlobal(pos));
}

void Explorer::expandToSelection(const QItemSelection& selection)
{
    if (selection.empty()) {
        return;
    }

    {
        // Use the guard to avoid triggering an update of the ancestors for every expand() in the
        // loop. Do it once at the end instead.
        const auto expandGuard = ExpansionGuard { this };
        for (const auto& idx : selection) {
            //  Expand the parent items to make the selected item visible
            auto parent = idx.parent();
            while (parent.isValid()) {
                if (!_ui->treeView->isExpanded(parent)) {
                    _ui->treeView->expand(parent);
                }
                parent = parent.parent();
            }
        }
    }

    // Ensure that the first selected item is visible. But only scroll if not already visible.
    const auto& firstSelectedIdx = selection.indexes().first();
    const auto  indexRect = _ui->treeView->visualRect(firstSelectedIdx);
    const auto  treeviewRect = _ui->treeView->viewport()->rect();

    const auto indexBottom = indexRect.y() + indexRect.height();
    const auto treeviewRectHeight = treeviewRect.height();
    // Check if the selected item is visible in the explorer, if not scroll to it.
    if (treeviewRectHeight <= indexBottom || indexBottom < 0) {
        _ui->treeView->scrollTo(firstSelectedIdx, QTreeView::PositionAtCenter);
    }
}

void Explorer::Observer::operator()(const Ufe::Notification& notification)
{
    auto getTreeItem = [this](const Ufe::Path& path) -> TreeItem* {
        const auto model = _explorer->treeModel();
        const auto idx = model->getIndexFromPath(path);
        return model->treeItem(idx);
    };

    auto updateExplorerItem = [this](const Ufe::Path& path) {
        if (path.getSegments()[0] == _explorer->rootItem()->path().getSegments()[0]) {
            _explorer->updateItem(path);
        }
    };

    if (const auto oa = dynamic_cast<const Ufe::ObjectAdd*>(&notification)) {
        if (!_explorer->isRelevantToExplorer(oa->changedPath())) {
            return;
        }

        const auto model = _explorer->treeModel();
        const auto addedPath = oa->changedPath();

        // It's possible to already have the tree item. For example if an item is inactive,
        // and so technically not in the scene, but still potentially displayed in the tree.
        TreeItem*           treeItem = getTreeItem(addedPath);
        Ufe::SceneItem::Ptr sceneItem = nullptr;
        if (treeItem) {
            sceneItem = treeItem->sceneItem();
            updateExplorerItem(sceneItem->path());
        } else {
            auto parentItem = getTreeItem(oa->changedPath().pop());
            if (!parentItem) {
                parentItem = model->root();
            }
            sceneItem = Ufe::Hierarchy::createItem(addedPath);
            model->layoutAboutToBeChanged();
            treeItem = parentItem->appendChild(sceneItem);
            model->layoutChanged();
        }
        // There could be items below the added one, that we dont get a notification for...
        model->buildTreeFrom(
            treeItem,
            sceneItem,
            _explorer->searchFilter().toStdString(),
            _explorer->typeFilter(),
            _explorer->_childFilter,
            false);
        return;
    }
    if (const auto od = dynamic_cast<const Ufe::ObjectDelete*>(&notification)) {
        if (!_explorer->isRelevantToExplorer(od->changedPath())) {
            return;
        }
        const auto model = _explorer->treeModel();
        const auto item = getTreeItem(od->changedPath());
        if (!item) {
            return;
        }

        // There are cases (for example inactivation of a prim in the USD ufe runtime,
        // without filtering of inactive prims), where object delete notifications are
        // sent out, but we still need to display the prim in question, probably in a
        // different style. To achieve this, when an object delete is received, look at
        // whether the item is actually still present given the hierarchy child filter
        // in use.
        const auto ufeParentSceneItem = item->parentItem()->sceneItem();
        if (ufeParentSceneItem) {
            const auto ufeHier = Ufe::Hierarchy::hierarchy(ufeParentSceneItem);
            const auto children = ufeHier->filteredChildren(_explorer->_childFilter);
            for (const auto& child : children) {
                if (child->path() == item->sceneItem()->path()) {
                    // Keep the item, but display/subtree may have changed.
                    _explorer->rebuildSubtree(item);
                    return;
                }
            }
        }

        model->layoutAboutToBeChanged();
        item->parentItem()->removeChild(item);
        model->layoutChanged();
        return;
    }
    if (const auto si = dynamic_cast<const Ufe::SubtreeInvalidate*>(&notification)) {
        if (!_explorer->isRelevantToExplorer(si->changedPath())) {
            return;
        }
        const auto item = getTreeItem(si->changedPath());
        if (!item) {
            return;
        }

        _explorer->rebuildSubtree(item);
    }

    if (const auto vc = dynamic_cast<const Ufe::VisibilityChanged*>(&notification)) {
        if (!_explorer->_ignoreUfeNotifications) {
            updateExplorerItem(vc->path());
        }
        return;
    }
    if (const auto ac = dynamic_cast<const Ufe::AttributeChanged*>(&notification)) {
        if (!_explorer->_ignoreUfeNotifications) {
            updateExplorerItem(ac->path());
        }
        return;
    }

    // React to the global UFE selection being changed, updating the tree's selection.
    if (const auto sc = dynamic_cast<const Ufe::SelectionChanged*>(&notification)) {
        _explorer->updateTreeSelection();
    }
}

Explorer::ExpansionGuard::ExpansionGuard(Explorer* explorer)
    : _explorer { explorer }
{
    // If we are already within a guard, do nothing.
    if (_explorer->_inSelectionExpansion) {
        _isNested = true;
        return;
    }
    _explorer->_inSelectionExpansion = true;
}

Explorer::ExpansionGuard::~ExpansionGuard()
{
    if (_isNested) {
        return;
    }
    _explorer->_inSelectionExpansion = false;
    _explorer->updateSelectionAncestors();
}

} // namespace UfeUi
