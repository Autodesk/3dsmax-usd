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
#ifndef UFEUI_TREE_COLUMN_H
#define UFEUI_TREE_COLUMN_H

#include "UFEUIAPI.h"

#include <QtCore/qpointer.h>
#include <QtCore/qvariant.h>

class QStyledItemDelegate;

namespace UfeUi {

class TreeItem;
class Explorer;

/**
 * \brief Represents the definition of a column in an explorer's tree. Abstract class.
 */
class TreeColumn
{
public:
    /**
     * \brief Constructor.
     * \param visualIndex Visual index of the column (as opposed to the QT logical index)
     */
    UFEUIAPI TreeColumn(int visualIndex);

    /**
     * \brief Destructor.
     */
    virtual UFEUIAPI ~TreeColumn();

    /**
     * \brief Returns the header for the column.
     * \param role The QT role (display, decoration, etc.)
     * \return The header.
     */
    virtual UFEUIAPI QVariant columnHeader(int role) const = 0;

    /**
     * \brief Returns the data for this column, for the given role and item.
     * \param treeItem The treeItem to get the data for.
     * \param role The QT role of the data we want.
     * \return The data!
     */
    virtual UFEUIAPI QVariant data(const TreeItem* treeItem, int role) const = 0;

    /**
     * \brief Returns the flags for the cell of the given scene item in this column.
     * \param treeItem The treeItem to get flags for.
     * \param flags The flags.
     */
    virtual UFEUIAPI void flags(const TreeItem* treeItem, Qt::ItemFlags& flags) {};

    /**
     * \brief Sets the data in the column.
     * \param treeItem The treeItem to set the data for.
     * \param value The new value.
     * \param role The QT role of what is being set on the cell.
     * \return True is successful.
     */
    virtual UFEUIAPI bool setData(const TreeItem* treeItem, const QVariant& value, int role)
    {
        return false;
    }

    /**
     * \brief React to the tree item being clicked.
     * \param treeItem The clicked treeItem.
     */
    virtual UFEUIAPI void clicked(const TreeItem* treeItem) { }

    /**
     * \brief React to the tree item being doubleClicked.
     * \param treeItem The double clicked treeItem.
     */
    virtual UFEUIAPI void doubleClicked(const TreeItem* treeItem) { }

    /**
     * \brief Returns an optional QStyledItemDelegate to customize the draw code
     * for items in the column.
     * \param parent Object parent for the delegate.
     * \return The created styled item delegate.
     */
    virtual UFEUIAPI QStyledItemDelegate* createStyleDelegate(QObject* parent) { return nullptr; }

    /**
     * \brief Whether the column is selectable.
     * \return True if the column is selectable, i.e. an item's row can be selected
     * from clicking on this column.
     */
    virtual UFEUIAPI bool isSelectable() const { return true; }

    /**
     * \brief Returns the visual index of the column.
     * \return The index.
     */
    UFEUIAPI int visualIndex() const { return this->_visualIdx; }

    UFEUIAPI void addExplorer(Explorer* explorer);
    UFEUIAPI void removeExplorer(Explorer* explorer);

protected:
    std::vector<QPointer<Explorer>> _affectedExplorers;

private:
    int _visualIdx = 0;
};

typedef std::vector<std::shared_ptr<TreeColumn>> TreeColumns;
} // namespace UfeUi

#endif // UFEUI_TREE_COLUMN_H
