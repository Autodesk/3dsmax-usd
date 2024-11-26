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
#include "QTreeModel.h"

namespace MAXUSD_NS_DEF {

QTreeModel::QTreeModel(QObject* parent) noexcept
    : QStandardItemModel { parent }
{
    // Nothing to do.
}

QTreeItem* QTreeModel::GetItemAtIndex(const QModelIndex& index) const
{
    if (index.isValid()) {
        return static_cast<QTreeItem*>(index.internalPointer());
    }
    return nullptr;
}

} // namespace MAXUSD_NS_DEF
