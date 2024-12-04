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

#include "highlightItemDelegate.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/qstyleditemdelegate.h>
#include <QtWidgets/qstyleoption.h>

namespace UfeUi {

void HighlightItemDelegate::paint(
    QPainter*                   painter,
    const QStyleOptionViewItem& option,
    const QModelIndex&          index) const
{
    QStyleOptionViewItem itemOption(option);
    initStyleOption(&itemOption, index);

    // Display selection if actually selected, or if ancestor of an collapsed selected item.
    const bool ancestorHighlight
        = std::find(_selectionAncestors->begin(), _selectionAncestors->end(), index)
        != _selectionAncestors->end();
    if (itemOption.state & QStyle::State_Selected || ancestorHighlight) {
        // Disable focus flag to avoid ugly dotted lines on selected items.
        itemOption.state.setFlag(QStyle::State_HasFocus, false);
        if (ancestorHighlight) {
            itemOption.backgroundBrush = QBrush { _ancestorHighlightColor };
        }
        // On selected items, the disabled color is barely visible. Make a special
        // case to replace it with black.
        const auto textColor = itemOption.palette.color(QPalette::Text);
        const auto disabledColor
            = QApplication::palette().color(QPalette::Disabled, QPalette::WindowText);
        if (textColor == disabledColor) {
            if (ancestorHighlight) {
                // When hovering an item that is not truely selected, the background color changes
                // and with that color as background, we dont want to override.
                if (!itemOption.state.testFlag(QStyle::State_MouseOver)) {
                    itemOption.palette.setColor(QPalette::Text, Qt::black);
                }
            } else {
                itemOption.palette.setColor(QPalette::HighlightedText, Qt::black);
            }
        } else {
            // For some reason, if we draw the control ourselves, the highlight color is not
            // initialized, and would render black. We want it to be the same color as the
            // regular text color.
            itemOption.palette.setColor(QPalette::HighlightedText, textColor);
        }
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter, nullptr);
        return;
    }
    QStyledItemDelegate::paint(painter, itemOption, index);
}

} // namespace UfeUi