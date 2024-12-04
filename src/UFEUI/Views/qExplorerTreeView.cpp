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
#include "qExplorerTreeView.h"

#include <QMouseEvent>
#include <qguiapplication.h>

void qExplorerTreeView::setColumnSelectable(int column, bool selectable)
{
    _columnSelectability[column] = selectable;
}

void qExplorerTreeView::setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command)
{
    const QModelIndex item = indexAt(rect.topLeft());
    if (QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier)) {
        QTreeView::setSelection(rect, command);
        return;
    }
    if (!item.isValid()) {
        return;
    }
    QTreeView::setSelection(QRect(rect.topLeft(), QSize(1, 1)), command);
}

QItemSelectionModel::SelectionFlags
qExplorerTreeView::selectionCommand(const QModelIndex& index, const QEvent* event) const
{
    if (index.isValid()) {
        const auto it = _columnSelectability.find(index.column());
        if (it == _columnSelectability.end() || !it->second) {
            return QItemSelectionModel::NoUpdate;
        }
    }
    return QTreeView::selectionCommand(index, event);
}
