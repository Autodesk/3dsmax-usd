//
// Copyright 2024 Autodesk
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

#include "MaxLayerEditorItemDelegate.h"

void MaxLayerEditorItemDelegate::paint(
    QPainter*                   painter,
    const QStyleOptionViewItem& option,
    const QModelIndex&          index) const
{
    // The Layer editor uses the Highlight role in ways not used in 3dsMax. Adjust
    // the palette to get the usual 3dsMax look.
    QStyleOptionViewItem opts = option;
    QPalette             maxPalette = opts.palette;
    maxPalette.setColor(
        QPalette::Highlight, QApplication::palette().color(QPalette::Normal, QPalette::Light));
    opts.palette = maxPalette;
    LayerTreeItemDelegate::paint(painter, opts, index);
}