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
#pragma once
#include <MaxUsd.h>
#include <QtGui/QStandardItemModel.h>

namespace MAXUSD_NS_DEF {

class QTreeItem;

/**
 * \brief Qt Model to explore the hierarchy of a USD file.
 * \remarks Populating the Model with the content of a USD file is done through the APIs exposted by the
 * TreeModelFactory.
 */
class QTreeModel : public QStandardItemModel
{
public:
    /**
     * \brief Constructor.
     * \param parent A reference to the parent of the QTreeModel.
     */
    explicit QTreeModel(QObject* parent = nullptr) noexcept;

    /**
     * \brief Return the item at the given index, or "nullptr" if the given index is invalid.
     * \param index The index for which to return the item.
     * \return The item at the given index, or "nullptr" if it is invalid.
     */
    QTreeItem* GetItemAtIndex(const QModelIndex& index) const;

    /**
     * \brief Order of the columns as they appear in the Tree.
     * \remarks The order of the enumeration is important.
     */
    enum TREE_COLUMNS
    {
        NAME, /// Name of the item as it appears in the TreeView.
        PATH, /// Path of the item relative to the root.
        TYPE, /// Type of the primitive.
        LAST  /// Last element of the enum.
    };
};

} // namespace MAXUSD_NS_DEF
