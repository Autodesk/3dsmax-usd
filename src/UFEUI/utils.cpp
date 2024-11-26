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
#include "utils.h"

#include "treeItem.h"
#include "treeModel.h"

#include <QtCore/QSortFilterProxyModel.h>
#include <QtWidgets/QTreeView.h>

namespace UfeUi {
namespace Utils {

double dpiScaleFactor = 1.0;

double dpiScale() { return dpiScaleFactor; }

void setDpiScale(double dpiScale) { dpiScaleFactor = dpiScale; }

void findExpandedPaths(
    TreeModel*              model,
    QSortFilterProxyModel*  proxyModel,
    QTreeView*              treeView,
    const TreeItem*         subtreeRoot,
    std::vector<Ufe::Path>& expanded)
{
    auto isExpanded = [&](const TreeItem* item) {
        QModelIndex idx;
        const auto  sceneItem = item->sceneItem();
        if (!sceneItem) {
            // Pseudo-root item in the treeview.
            idx = model->index(0, 0, QModelIndex());
        } else {
            idx = model->getIndexFromPath(sceneItem->path());
            if (!idx.isValid()) {
                return false;
            }
        }

        const auto proxyIdx = proxyModel->mapFromSource(idx);
        if (!proxyIdx.isValid()) {
            return false;
        }
        return treeView->isExpanded(proxyIdx);
    };

    if (!isExpanded(subtreeRoot)) {
        return;
    }

    expanded.clear();
    const auto sceneItem = subtreeRoot->sceneItem();
    if (sceneItem) {
        expanded.push_back(sceneItem->path());
    }
    const auto expandedItems = subtreeRoot->findDescendants(isExpanded);
    for (const auto& item : expandedItems) {
        expanded.push_back(item->sceneItem()->path());
    }
}

void expandPaths(
    QTreeView*              treeView,
    TreeModel*              model,
    QSortFilterProxyModel*  proxyModel,
    std::vector<Ufe::Path>& expandedPaths)
{
    for (const auto& path : expandedPaths) {
        const auto proxyIdx = proxyModel->mapFromSource(model->getIndexFromPath(path));
        treeView->setExpanded(proxyIdx, true);
    }
}

ExpandStateGuard::ExpandStateGuard(
    QTreeView*             treeView,
    const TreeItem*        subtreeRoot,
    TreeModel*             model,
    QSortFilterProxyModel* proxyModel)
    : _model(model)
    , _proxyModel(proxyModel)
    , _treeView(treeView)
{
    findExpandedPaths(model, proxyModel, treeView, subtreeRoot, _expandedPaths);
}

ExpandStateGuard::~ExpandStateGuard()
{
    // If the items paths still exist, restore their expanded state.
    expandPaths(_treeView, _model, _proxyModel, _expandedPaths);
    _expandedPaths.clear();
}

bool filtersAreEqual(
    const Ufe::Hierarchy::ChildFilter& filter1,
    const Ufe::Hierarchy::ChildFilter& filter2)
{
    if (filter1.size() != filter2.size()) {
        return false;
    }
    auto it1 = filter1.begin();
    auto it2 = filter2.begin();
    while (it1 != filter1.end() && it2 != filter2.end()) {
        if (it1->name != it2->name || it1->value != it2->value) {
            return false;
        }
        ++it1;
        ++it2;
    }
    return true;
}

} // namespace Utils
} // namespace UfeUi
