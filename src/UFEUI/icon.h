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
#ifndef UFEUI_ICON_H
#define UFEUI_ICON_H

#include <ufe/uiInfoHandler.h>

#include <QtGui/qicon.h>
#include <QtWidgets/QHeaderView.h>
#include <QtWidgets/QProxyStyle.h>
#include <QtWidgets/QStyledItemDelegate.h>

class QStyledItemDelegate;
class QStyle;

namespace UfeUi {
namespace Icon {

/**
 * \brief Builds and returns a QICon from the given ufe icon. Uses an internal cache.
 * \param ufeIcon The ufe icon.
 * \return The created/or previously cached QIcon.
 */
QIcon build(const Ufe::UIInfoHandler::Icon& ufeIcon);

/**
 * \brief Draws an Icon centered, within a rectangle. Meant for usage in the UFE explorer.
 * Uses a fixed icon height (DPI scaled).
 * \param painter Painter object to draw the icon with.
 * \param icon The icon to draw.
 * \param rect Rectangle we are drawing in, will draw at center.
 */
void drawCentered(QPainter& painter, const QIcon& icon, const QRect& rect);

/**
 * \brief QStyle to center icon labels in a QHeaderView.
 */
class CenteredIconHeaderStyle : public QProxyStyle
{
public:
    CenteredIconHeaderStyle(QStyle* baseStyle) { this->baseStyle = baseStyle; }
    void drawControl(
        ControlElement      element,
        const QStyleOption* styleOptions,
        QPainter*           painter,
        const QWidget*      widget) const;

private:
    QStyle* baseStyle;
};

/**
 * \brief Styled item delegate to center icons in item views.
 */
class CenterIconDelegate : public QStyledItemDelegate
{
public:
    CenterIconDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)
        const override;
};

} // namespace Icon
} // namespace UfeUi

#endif UFEUI_ICON_H