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

#ifndef UFEUI_EXPLORER_HOST
#define UFEUI_EXPLORER_HOST

class QMainWindow;

#include "Explorer.h"

#include <QtWidgets/qwidget.h>

namespace UfeUi {

class ExplorerHostPrivate;

//! Host widget for explorers. Explorers come in as tabs within this widget.
class UFEUIAPI ExplorerHost
    : public QWidget
    , public Ufe::Subject
{
    Q_OBJECT

public:
    /** Constructor.
     * \param parent The parent QMainWindow widget. The explorer host will be
     * added to this widget as the central widget. If there is no menu bar
     * assigned to the the main window yet, a new QMenuBar will be added. */
    ExplorerHost(QMainWindow* parent = nullptr, Qt::WindowFlags windowFlags = {});

    /**
     * \brief Adds a new explorer, in a new tab.
     * \param explorer The explorer to add.
     * \param name Name for the explorer (show's up in the tab)
     * \param setActive Whether or not to set the explorer as active (as the current tab).
     */
    void addExplorer(Explorer* explorer, const QString& name, bool setActive);

    /**
     * \brief Sets the tab of the explorer rooted at the given UFE path as the active/current one.
     * \param rootItemPath Root item path of the explorer to set active.
     * \return The explorer, for convenience.
     */
    Explorer* setActiveExplorer(const Ufe::Path& rootItemPath);

    Explorer* activeExplorer() const;

    /**
     * \brief Closes the explorer rooted at the given UFE path.
     * \param rootItemPath The root item path of the explorer to close.
     */
    void closeExplorer(const Ufe::Path& rootItemPath);

    /**
     * \brief Sets the placeholder text for when no explorer/tabs exist. The empty state.
     * \param placeHolder The placeholder text.
     */
    void setPlaceHolderText(const QString& placeHolder);

    /**
     * \brief returns the menu bar used in the explorer host.
     * \return The menu bar.
     */
    QMenuBar* menuBar() const;

    /**
     * \brief Returns all the explorers hosted by the host.
     * \return A vector of all explorers.
     */
    std::vector<Explorer*> explorers() const;

    class ExplorerClosedNotification : public Ufe::Notification
    {
    public:
        ExplorerClosedNotification(Explorer* explorer, bool fromUI)
            : _explorer(explorer)
            , _fromUI(fromUI)
        {
        }
        UfeUi::Explorer* explorer() const { return _explorer; }
        bool fromUI() const { return _fromUI; }

    private:
        Explorer* _explorer = nullptr;
        bool      _fromUI = false;
    };

private:
    Q_DECLARE_PRIVATE(ExplorerHost);
    ExplorerHostPrivate* d_ptr = nullptr;
};

} // namespace UfeUi

#endif // UFEUI_EXPLORER_HOST
