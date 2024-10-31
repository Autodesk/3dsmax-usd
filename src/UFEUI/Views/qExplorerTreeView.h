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
#ifndef UFEUI_QEXPLORER_TREEVIEW_H
#define UFEUI_QEXPLORER_TREEVIEW_H

#include <QtWidgets/QTreeView>
#include <unordered_map>

class qExplorerTreeView : public QTreeView
{
public:
    using QTreeView::QTreeView;

    /**
     * \brief Sets a column as selectable or not. Clicking on an unselectable column of a row does not affect the
     * TreeView selection.
     * \param column The column to configure.
     * \param selectable True if the column is selectable, false otherwise.
     */
    void setColumnSelectable(int column, bool selectable);

protected:
    /**
     * \brief Override the setSelection by rect method to prevent drag selection, except when shift
     * is pressed, to make sure the shift selection behavior still works.
     * \param rect The rectangle where the selection should happen.
     * \param command The selection command.
     */
    void setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command) override;

    /**
     * \brief Override the selectionCommand by ModelIndex method to prevent selection happening when clicking on an
     * unselectable column.
     * \param index The index from where selection happens.
     * \param event The event
     * \return Selection flags to use.
     */
    QItemSelectionModel::SelectionFlags
    selectionCommand(const QModelIndex& index, const QEvent* event) const override;

private:
    std::unordered_map<int, bool> _columnSelectability;
};

#endif // UFEUI_QEXPLORER_TREEVIEW_H
