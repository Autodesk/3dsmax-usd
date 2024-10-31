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
#ifndef UFEUI_QEXPLORER_TREEVIEW_CONTEXTMENU_H
#define UFEUI_QEXPLORER_TREEVIEW_CONTEXTMENU_H

#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTreeView>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtGui/QAction>
#else
#include <QtWidgets/QAction>
#endif

namespace UfeUi {
class QExplorerTreeViewContextMenu : public QMenu
{
public:
    using QMenu::QMenu;

protected:
    /*
     * Overridden mouseReleaseEvent function to handle mouse release events of this class. In
     * particular this function is used to handle the Ctrl key press + mouse release events
     * distinctly from standard mouse release events, in order to keep the context menu open when
     * the Ctrl key is pressed
     * \param event The QMouseEvent event.
     */
    void mouseReleaseEvent(QMouseEvent* event) override;
};
} // namespace UfeUi
#endif // UFEUI_QEXPLORER_TREEVIEW_CONTEXTMENU_H
