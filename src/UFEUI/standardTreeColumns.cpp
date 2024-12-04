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
#include "standardTreeColumns.h"

#include "Views/explorer.h"
#include "editCommand.h"
#include "highlightItemDelegate.h"
#include "icon.h"
#include "utils.h"

#include <ufe/globalSelection.h>
#include <ufe/object3d.h>
#include <ufe/observableSelection.h>
#include <ufe/uiInfoHandler.h>
#include <ufe/undoableCommandMgr.h>

#include <QtCore/qpointer.h>
#include <QtGui/qicon.h>
#include <QtWidgets/qapplication.h>

NameColumn::NameColumn(int visualIndex)
    : TreeColumn::TreeColumn(visualIndex)
{
}

NameColumn::NameColumn(const QString& rootItemAlias, int visualIndex)
    : TreeColumn::TreeColumn(visualIndex)
    , _rootAlias { rootItemAlias }
{
}

QVariant NameColumn::columnHeader(int role) const
{
    if (role == Qt::DisplayRole) {
        return QObject::tr("Prim Name");
    }
    return {};
}

QVariant NameColumn::data(const UfeUi::TreeItem* treeItem, int role) const
{
    const auto parent = treeItem->parentItem();
    const bool isRootItem = !parent || parent->sceneItem() == nullptr;
    const auto sceneItem = treeItem->sceneItem();

    // DecorationRole is for the item's icon.
    if (role == Qt::DecorationRole && !isRootItem) {
        auto uiInfoHdlr = Ufe::UIInfoHandler::uiInfoHandler(sceneItem->runTimeId());
        if (!uiInfoHdlr) {
            return QVariant {};
        }
        const auto ufeIcon = uiInfoHdlr->treeViewIcon(sceneItem);
        const auto qtIcon = UfeUi::Icon::build(ufeIcon);
        QVariant   ret;
        ret.setValue(qtIcon);
        return ret;
    }

    if (role == Qt::ToolTipRole) {
        auto uiInfoHdlr = Ufe::UIInfoHandler::uiInfoHandler(sceneItem->runTimeId());
        if (!uiInfoHdlr) {
            return QVariant {};
        }
        return QString::fromStdString(uiInfoHdlr->treeViewTooltip(sceneItem));
    }

    if (role == Qt::FontRole) {
        auto uiInfoHdlr = Ufe::UIInfoHandler::uiInfoHandler(sceneItem->runTimeId());
        if (!uiInfoHdlr) {
            return QVariant {};
        }

        Ufe::CellInfo cellInfo;
        if (uiInfoHdlr->treeViewCellInfo(sceneItem, cellInfo)) {
            QFont font;
            font.setStrikeOut(cellInfo.fontStrikeout);
            font.setBold(cellInfo.fontBold);
            font.setItalic(cellInfo.fontItalics);
            return font;
        }
    }

    if (role == Qt::ForegroundRole) {
        if (!treeItem->computedVisibility()) {
            return QApplication::palette().color(QPalette::Disabled, QPalette::WindowText);
        }

        auto uiInfoHdlr = Ufe::UIInfoHandler::uiInfoHandler(sceneItem->runTimeId());
        if (!uiInfoHdlr) {
            return QVariant {};
        }

        Ufe::CellInfo cellInfo;
        if (uiInfoHdlr->treeViewCellInfo(sceneItem, cellInfo)) {
            return QColor(
                int(cellInfo.textFgColor.r() * 255.0),
                int(cellInfo.textFgColor.g() * 255.0),
                int(cellInfo.textFgColor.b() * 255.0));
        }
    }

    if (role != Qt::DisplayRole) {
        return QVariant {};
    }

    if (isRootItem && !_rootAlias.isEmpty()) {
        return _rootAlias;
    }
    return QString::fromStdString(sceneItem->nodeName());
};

QStyledItemDelegate* NameColumn::createStyleDelegate(QObject* parent)
{
    const auto explorer = dynamic_cast<UfeUi::Explorer*>(parent);
    if (!explorer) {
        return nullptr;
    }
    const auto selectionColor = explorer->colorScheme().selected;

    // Mix the selection color with the background color.
    QColor bgColor = QApplication::palette().color(QPalette::Active, QPalette::Window);
    QColor highlightColor = UfeUi::Utils::mixColors(bgColor, selectionColor, 100);
    return new UfeUi::HighlightItemDelegate(
        &(explorer->selectionAncestors()), highlightColor, parent);
}

QVariant TypeColumn::columnHeader(int role) const
{
    if (role == Qt::DisplayRole) {
        return QObject::tr("Type");
    }
    return {};
}

QVariant TypeColumn::data(const UfeUi::TreeItem* treeItem, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant {};
    }
    return QString::fromStdString(treeItem->sceneItem()->nodeType());
}

QIcon VisColumn::_iconVisible;
QIcon VisColumn::_iconVisibleInherit;
QIcon VisColumn::_iconHidden;
QIcon VisColumn::_iconHiddenInherit;

VisColumn::VisColumn(int visualIndex)
    : TreeColumn(visualIndex)
{
    if (_iconVisible.isNull()) {
        // the first shall initialize those!
        _iconVisible = QIcon { ":/ufe/Icons/visible.png" };
        _iconVisibleInherit = QIcon { ":/ufe/Icons/visibleInherit.png" };
        _iconHidden = QIcon { ":/ufe/Icons/hidden.png" };
        _iconHiddenInherit = QIcon { ":/ufe/Icons/hiddenInherit.png" };
    }
}

VisColumn::~VisColumn() = default;

QVariant VisColumn::columnHeader(int role) const
{
    if (role == Qt::DecorationRole) {
        return _iconVisible;
    }
    if (role == Qt::ToolTipRole) {
        return QObject::tr("Toggle the visibility property of a prim between invisible and "
                           "inherit. Note: Ancestor "
                           "visibility affects the resolved visibility of its descendants.");
    }
    return {};
}

QVariant VisColumn::data(const UfeUi::TreeItem* treeItem, int role) const
{
    const auto sceneItem = treeItem->sceneItem();
    // DecorationRole is for the item's icon.
    if (role == Qt::DecorationRole) {
        const auto object3d = Ufe::Object3d::object3d(sceneItem);
        if (!object3d) {
            return QVariant {};
        }

        const bool authoredVis = object3d->visibility();
        const auto computedVis = treeItem->computedVisibility();
        if (!computedVis && authoredVis) {
            return _iconHiddenInherit;
        }
        if (!object3d->visibility()) {
            return _iconHidden;
        }
        return _iconVisibleInherit;
    }
    // No text to display, only the checkbox.
    return QVariant {};
}

struct _UfeRootPathsHelper
{
    std::map<std::int64_t, _UfeRootPathsHelper> subComponentMap;
    const Ufe::Path*                            path = nullptr;
};

void VisColumn::clicked(const UfeUi::TreeItem* treeItem)
{
    const auto sceneItem = treeItem->sceneItem();
    const auto object3d = Ufe::Object3d::object3d(sceneItem);
    if (!object3d) {
        return;
    }

    std::vector<const Ufe::Path*> affectedPaths;

    std::vector<Ufe::Object3d::Ptr> objectsToToggle;
    objectsToToggle.push_back(object3d);
    affectedPaths.push_back(&(sceneItem->path()));

    // Check the global selection. If the item we clicked is selected, also
    // toggle the visibility of the other selected items.
    const Ufe::GlobalSelection::Ptr& selection = Ufe::GlobalSelection::get();
    const auto                       numSegments = sceneItem->path().nbSegments();
    const auto                       rootPath = sceneItem->path().popSegment();
    if (selection && selection->contains(treeItem->sceneItem()->path())) {
        for (const auto item : *selection) {
            // Only consider items in the same subtree (in USD, that means in the same stage)
            if (numSegments != item->path().nbSegments()) {
                continue;
            }
            const auto itemRootPath = item->path().popSegment();
            if (rootPath != itemRootPath) {
                continue;
            }

            // Clicked item, already added.
            if (item->path() == treeItem->sceneItem()->path()) {
                continue;
            }

            const auto obj3d = Ufe::Object3d::object3d(item);
            if (!obj3d) {
                continue;
            }
            objectsToToggle.emplace_back(obj3d);
            affectedPaths.emplace_back(&(item->path()));
        }
    }

    const auto compositeCommand = Ufe::CompositeUndoableCommand::create({});
    const auto newValue = !object3d->visibility();
    for (const auto& object : objectsToToggle) {
        compositeCommand->append(object->setVisibleCmd(newValue));
    }

    const auto editCmd = UfeUi::EditCommand::create(
        treeItem->sceneItem()->path(), compositeCommand, "USD Stage Edit");

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    // Tell the explorers affected by this operation to ignore UFE notifications
    // to avoid them reacting to every single UFE change notification down the
    // road...
    _pausedExplorers.clear();
    if (affectedPaths.size() > 1) {
        const auto& first_item_path = affectedPaths.front();
        for (const auto& explorer : _affectedExplorers) {
            if (explorer && !explorer->isIgnoringUfeNotifications()
                && first_item_path->startsWith(explorer->rootItem()->path())) {
                explorer->setIgnoreUfeNotifications(true);
                _pausedExplorers.push_back(explorer);
            }
        }
    }

    Ufe::UndoableCommandMgr::instance().executeCmd(editCmd);

    // As those "paused" explorers did miss out on all the individual UFE change
    // notifications, it is necessary to inform them manually that things have
    // changed.
    for (const auto& explorer : _pausedExplorers) {
        if (explorer) {
            explorer->setIgnoreUfeNotifications(false);

            // -----------------------------------------------------------------
            // As the model->update(path) will do trigger a recursive update of
            // all the children of this path in the treeview, it is crucial to
            // performance to only update the topmost (independent) selection
            // items only to not do too much work down the road!
            //
            // -----------------------------------------------------------------
            // The algorithm works like this:
            // -----------------------------------------------------------------
            //
            // First entry would be "1/2/3/4/5": (p1)
            //
            // It creates a chain of IDs and adds itself to the last entry:
            //
            // +---+   +---+   +---+   +---+   +--------+
            // | 1 |-->| 2 |-->| 3 |-->| 4 |-->| 5 (p1) |
            // +---+   +---+   +---+   +---+   +--------+
            //
            // Second entry would be "1/2/3/4/6": (p2)
            //
            // It walks through the chain of IDs, extends it and adds itself to
            // the last entry:
            //                                   +--------+
            // +---+   +---+   +---+   +---+  +->| 5 (p1) |
            // | 1 |-->| 2 |-->| 3 |-->| 4 |--+  +--------+
            // +---+   +---+   +---+   +---+  |  +--------+
            //                                +->| 6 (p2) |
            //                                   +--------+
            //
            // Third entry would be "1/2/3/4/6/7": (p3)
            //
            // It walks through the chain of IDs, but when it passed the 6, it
            // sees that there is already a path there (p2) - so this would be
            // higher than (p3), so it stops -> p3 will be skipped!
            //
            //                                   +--------+
            // +---+   +---+   +---+   +---+  +->| 5 (p1) |
            // | 1 |-->| 2 |-->| 3 |-->| 4 |--+  +--------+
            // +---+   +---+   +---+   +---+  |  +----------+
            //                                +->| * 6 (p2) | * stops here !
            //                                   +----------+
            //
            // Third entry would be "1/2/3/": (p4)
            //
            // It creates a chain of IDs and adds itself to the last entry, and
            // it also removes everything below it:
            //
            //                            +-----------------------+
            //                            |            +--------+ |
            // +---+   +---+   +--------+ |  +---+  +->| 5 (p1) | |
            // | 1 |-->| 2 |-->| 3 (p4) | x->| 4 |--+  +--------+ |
            // +---+   +---+   +--------+ |  +---+  |  +--------+ |
            //                            |         +->| 6 (p2) | |
            //                            |            +--------+ |
            //                            +-- this gets removed --+
            // results in:
            // +---+   +---+   +--------+
            // | 1 |-->| 2 |-->| 3 (p4) |
            // +---+   +---+   +--------+
            //
            // So, as a result, the topmost (independent) paths are the only
            // ones existing in the struct, so the only thing to do is to
            // collect those.

            _UfeRootPathsHelper helper;
            for (const Ufe::Path* path : affectedPaths) {
                bool                 nextOnePlease = false;
                _UfeRootPathsHelper* current = &helper;
                for (const auto& component : *path) {
                    current = &current->subComponentMap[component.id()];
                    if (current
                            ->path) // there is already a selected node above this one, outta here!
                    {
                        nextOnePlease = true;
                        break;
                    }
                }
                if (!nextOnePlease) {
                    current->path = path;
                    current->subComponentMap
                        .clear(); // remove all the subcomponents, as those are not needed anymore.
                }
            }
            affectedPaths.clear();

            std::function<void(const _UfeRootPathsHelper&)> collectPaths
                = [&affectedPaths, &collectPaths](const _UfeRootPathsHelper& helper) {
                      if (helper.path) {
                          affectedPaths.push_back(helper.path);
                      }
                      for (const auto& subComponent : helper.subComponentMap) {
                          collectPaths(subComponent.second);
                      }
                  };
            collectPaths(helper);

            // Now the affectedPaths vector contains just the topmost paths.

            for (const auto& path : affectedPaths) {
                explorer->treeModel()->update(*path);
            }
            explorer->treeView()->viewport()->update();
        }
    }
    _pausedExplorers.clear();

    QApplication::restoreOverrideCursor();
}

void VisColumn::doubleClicked(const UfeUi::TreeItem* treeItem)
{
    // Consider the double click as just another click.
    clicked(treeItem);
}

QStyledItemDelegate* VisColumn::createStyleDelegate(QObject* parent)
{
    return new UfeUi::Icon::CenterIconDelegate(parent);
}

bool VisColumn::isSelectable() const { return false; }
