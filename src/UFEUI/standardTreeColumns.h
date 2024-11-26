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
#ifndef UFEUI_STANDARD_TREE_COLUMNS
#define UFEUI_STANDARD_TREE_COLUMNS

#include "TreeColumn.h"
#include "UFEUIAPI.h"
#include "treeitem.h"

#include <QtCore/qpointer.h>

namespace UfeUi {
class Explorer;
};

class QStyledItemDelegate;
/**
 * \brief Column for a UFE scene item's name.
 */
class UFEUIAPI NameColumn : public UfeUi::TreeColumn
{
public:
    NameColumn(int visualIndex);

    NameColumn(const QString& rootAlias, int visualIndex);

    QVariant columnHeader(int role) const override;

    QVariant data(const UfeUi::TreeItem*, int role) const override;

    QStyledItemDelegate* createStyleDelegate(QObject* parent) override;

private:
    // An alias for the root item in the hierarchy.
    QString _rootAlias;
};

/**
 * \brief Column for a UFE scene item's type.
 */
class UFEUIAPI TypeColumn : public UfeUi::TreeColumn
{
public:
    using TreeColumn::TreeColumn;

    QVariant columnHeader(int role) const override;

    QVariant data(const UfeUi::TreeItem* treeItem, int role) const override;
};

/**
 * \brief Column for a UFE scene item's visibility (it's own visibility, not including any
 * inherited visibility state).
 */
class UFEUIAPI VisColumn : public UfeUi::TreeColumn
{
public:
    using TreeColumn::TreeColumn;

    VisColumn(int visualIndex);
    ~VisColumn() override;

    QVariant columnHeader(int role) const override;

    QVariant data(const UfeUi::TreeItem* treeItem, int role) const override;

    void clicked(const UfeUi::TreeItem* treeItem) override;

    void doubleClicked(const UfeUi::TreeItem* treeItem) override;

    QStyledItemDelegate* createStyleDelegate(QObject* parent) override;

    bool isSelectable() const override;

private:
    static QIcon _iconHidden;
    static QIcon _iconHiddenInherit;
    static QIcon _iconVisible;
    static QIcon _iconVisibleInherit;

    std::vector<QPointer<UfeUi::Explorer>> _pausedExplorers;
};

#endif // UFEUI_STANDARD_TREE_COLUMNS