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
#include "icon.h"

#include "utils.h"

#include <QtGui/qpainter.h>
#include <unordered_map>

namespace UfeUi {
namespace Icon {

QIcon build(const Ufe::UIInfoHandler::Icon& ufeIcon)
{
    const auto resourceString = [](const QString& name) {
        static const QString iconResBase = ":/ufe/Icons/UfeRt/";
        return iconResBase + name;
    };

    // Build a key for the icon, used for caching.
    size_t hash;
    Utils::hashCombine(hash, ufeIcon.baseIcon);
    Utils::hashCombine(hash, ufeIcon.badgeIcon);
    Utils::hashCombine(hash, ufeIcon.pos);
    Utils::hashCombine(hash, ufeIcon.mode);

    static std::unordered_map<size_t, QPixmap> pixmapCache;

    const auto it = pixmapCache.find(hash);
    if (it != pixmapCache.end()) {
        return QIcon { it->second };
    }

    const auto baseIconRes = resourceString(QString::fromStdString(ufeIcon.baseIcon));
    QPixmap    pixmap(baseIconRes);

    // If there is a badge - draw it on top of the base icon.
    if (!ufeIcon.badgeIcon.empty()) {
        const auto badgeIconRes = resourceString(QString::fromStdString(ufeIcon.badgeIcon));
        QPixmap    badgePixmap(badgeIconRes);
        QPainter   painter(&pixmap);

        // Badges can be in any of the four quadrants of the base icon.
        // Figure out the draw position (top left corner) of the base.
        int x, y;
        switch (ufeIcon.pos) {
        case Ufe::UIInfoHandler::UpperLeft:
            x = 0;
            y = 0;
            break;
        case Ufe::UIInfoHandler::UpperRight:
            x = pixmap.width() / 2;
            y = 0;
            break;
        case Ufe::UIInfoHandler::LowerLeft:
            x = 0;
            y = pixmap.height() / 2;
            break;
        case Ufe::UIInfoHandler::LowerRight:
        default: // Default to lower right.
            x = pixmap.width() / 2;
            y = pixmap.height() / 2;
            break;
        }
        painter.drawPixmap(x, y, badgePixmap);
    }

    pixmapCache.insert({ hash, pixmap });
    return QIcon { pixmap };
}

void drawCentered(QPainter& painter, const QIcon& icon, const QRect& rect)
{
    const static int fixedHeight = 16;
    const auto       iconHeight = fixedHeight * Utils::dpiScale();
    // Figure out the possible size for the icon, given the fixed height.
    const auto rectSize = rect.size();
    auto       size = icon.actualSize(QSize(rectSize.width(), iconHeight));
    if (size.width() == 0) {
        return;
    }
    // If the width doesn't fit, adjust.
    if (rectSize.width() < size.width()) {
        size = size * (static_cast<double>(rectSize.width()) / static_cast<double>(size.width()));
    }
    // Draw at the center.
    const QPixmap pixmap = icon.pixmap(size);
    const QPoint  offset
        = QPoint((rect.width() - pixmap.width()) / 2, (rect.height() - pixmap.height()) / 2);
    painter.drawPixmap(rect.topLeft() + offset, pixmap);
}

void CenteredIconHeaderStyle::drawControl(
    ControlElement      element,
    const QStyleOption* styleOptions,
    QPainter*           painter,
    const QWidget*      widget) const
{
    // The style only changes how the header label (icon) is drawn.
    if (element == CE_HeaderLabel) {
        const auto headerStyleOptions = static_cast<const QStyleOptionHeader*>(styleOptions);
        // If not only an icon, use the base style.
        if (headerStyleOptions->icon.isNull() || !headerStyleOptions->text.isEmpty()) {
            baseStyle->drawControl(element, styleOptions, painter, widget);
            return;
        }
        drawCentered(*painter, headerStyleOptions->icon, headerStyleOptions->rect);
        return;
    }
    baseStyle->drawControl(element, styleOptions, painter, widget);
}

void CenterIconDelegate::paint(
    QPainter*                   painter,
    const QStyleOptionViewItem& option,
    const QModelIndex&          index) const
{
    auto controlStyle = QStyleOptionViewItem(option);
    initStyleOption(&controlStyle, index);
    option.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &controlStyle, painter);

    const auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    drawCentered(*painter, icon, controlStyle.rect);
}

} // namespace Icon
} // namespace UfeUi
