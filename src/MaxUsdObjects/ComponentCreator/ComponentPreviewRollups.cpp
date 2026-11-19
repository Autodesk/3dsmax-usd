//
// Copyright 2026 Autodesk
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
#include "ComponentPreviewRollups.h"

#include <AdskUsdComponentCreator/Infos/CreatorInfo.h>
#include <AdskUsdComponentCreator/Reports.h>
#include <QAbstractItemView>
#include <QEvent>
#include <QHeaderView>
#include <QObject>
#include <QScrollBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>

namespace {

// Scrolls the preview tree when the mouse wheel is used over it, instead of letting the enclosing
// dialog scroll area steal the event. Only consumes the wheel when the tree can actually scroll, so
// a short preview still lets the dialog scroll.
class TreeWheelFilter : public QObject
{
public:
    explicit TreeWheelFilter(QTreeWidget* tree)
        : QObject(tree)
        , tree(tree)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Wheel) {
            QScrollBar* bar = tree->verticalScrollBar();
            if (bar && bar->minimum() != bar->maximum()) {
                const auto* wheel = static_cast<QWheelEvent*>(event);
                const int   notches = wheel->angleDelta().y() / 120;
                bar->setValue(bar->value() - notches * 48); // ~3 rows per wheel notch
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QTreeWidget* tree;
};

QTreeWidget* makeTree(QWidget* owner)
{
    auto* layout = new QVBoxLayout(owner);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* tree = new QTreeWidget(owner);
    tree->setSelectionMode(QAbstractItemView::NoSelection);
    tree->setRootIsDecorated(true);
    tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // Route the wheel to the tree (not the dialog) when hovering a scrollable preview.
    tree->viewport()->installEventFilter(new TreeWheelFilter(tree));
    layout->addWidget(tree);
    return tree;
}

void addItems(
    QTreeWidget*                                                             tree,
    QTreeWidgetItem*                                                         parent,
    const std::vector<std::shared_ptr<AdskUsdComponentCreator::ReportItem>>& items)
{
    for (const auto& item : items) {
        if (!item) {
            continue;
        }
        QStringList cols;
        for (const auto& value : item->GetValues()) {
            cols << QString::fromStdString(value);
        }
        auto* node = parent ? new QTreeWidgetItem(parent, cols) : new QTreeWidgetItem(tree, cols);
        addItems(tree, node, item->GetChildren());
    }
}

void fillTree(QTreeWidget* tree, const std::shared_ptr<AdskUsdComponentCreator::Report>& report)
{
    tree->clear();
    if (!report) {
        return;
    }
    const auto& columns = report->GetColumns();
    tree->setColumnCount(static_cast<int>(std::max<size_t>(1, columns.size())));
    if (columns.empty()) {
        tree->setHeaderHidden(true);
    } else {
        QStringList headers;
        for (const auto& column : columns) {
            headers << QString::fromStdString(column);
        }
        tree->setHeaderLabels(headers);
    }
    addItems(tree, nullptr, report->GetItems());
    tree->expandAll();

    // Give the first (name/hierarchy) column the slack so it's wider than the value columns.
    if (tree->columnCount() > 0) {
        tree->header()->setStretchLastSection(false);
        tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int column = 1; column < tree->columnCount(); ++column) {
            tree->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
        }
    }
}

} // namespace

ComponentPrimHierarchyRollup::ComponentPrimHierarchyRollup(QWidget* parent)
    : QWidget(parent)
{
    tree = makeTree(this);
}

void ComponentPrimHierarchyRollup::SetFromInfo(
    const AdskUsdComponentCreator::CreatorInfo::Ptr& info)
{
    fillTree(tree, info ? AdskUsdComponentCreator::GetComponentStructureReport(info) : nullptr);
}

ComponentFileStructureRollup::ComponentFileStructureRollup(QWidget* parent)
    : QWidget(parent)
{
    tree = makeTree(this);
}

void ComponentFileStructureRollup::SetFromInfo(
    const AdskUsdComponentCreator::CreatorInfo::Ptr& info)
{
    fillTree(tree, info ? AdskUsdComponentCreator::GetComponentFileStructureReport(info) : nullptr);
}
