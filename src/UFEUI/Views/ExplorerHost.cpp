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

#include "explorerHost.h"

#include "ExplorerStyle.h"
#include "tabWidget.h"

#include <UFEUI/utils.h>

#include <qboxlayout.h>
#include <qmainwindow.h>
#include <qmenubar.h>
#include <qpointer.h>
#include <qtabwidget.h>

namespace UfeUi {

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
class ExplorerHostPrivate
{
public:
    ExplorerHostPrivate(ExplorerHost* explorerHost, QMainWindow* mainWindow)
        : m_MainWindow(mainWindow)
        , q_ptr(explorerHost)
    {
    }

    QPointer<QMainWindow> m_MainWindow;
    QPointer<QMenuBar>    m_MenuBar;
    QPointer<TabWidget>   m_TabWidget;

    // True if we are in the middle of a programmatic tab close.
    // Used to differentiate from closing tabs from the X button in the UI.
    bool m_InProgrammaticTabClose = false;

    Q_DECLARE_PUBLIC(ExplorerHost);
    ExplorerHost* q_ptr;

    /** Returns the tab index for the explorer rooted at the given path.
     * \param rootItemPath The root item path.
     * \param result Receives the explorer widget when found.
     * \return The tab index. */
    int tabIndex(const Ufe::Path& rootItemPath, Explorer** result = nullptr) const
    {
        if (!m_TabWidget) {
            return -1;
        }
        int tabIdx = 0;
        for (int i = 0; i < m_TabWidget->count(); ++i) {
            if (auto explorer = dynamic_cast<Explorer*>(m_TabWidget->widget(i))) {
                if (explorer->rootItem()->path() == rootItemPath) {
                    if (result) {
                        *result = explorer;
                    }
                    return tabIdx;
                }
            }
            tabIdx++;
        }
        return -1;
    }
};

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
ExplorerHost::ExplorerHost(QMainWindow* parent, Qt::WindowFlags windowFlags)
    : QWidget(parent, windowFlags)
    , d_ptr(new ExplorerHostPrivate(this, parent))
{
    setObjectName("ExplorerHost");

    Q_D(ExplorerHost);

    d->m_TabWidget = new TabWidget(this);
    auto l = new QVBoxLayout(this);
    l->addWidget(d->m_TabWidget);
    l->setContentsMargins(0, 0, 0, 0);
    d->m_TabWidget->setDocumentMode(true);
    d->m_TabWidget->setTabsClosable(true);

    if (parent) {
        parent->setCentralWidget(this);
        if (!parent->menuBar()) {
            parent->setMenuBar(new QMenuBar(parent));
        }
        d->m_MenuBar = parent->menuBar();

        auto style = parent->style();
        if (!dynamic_cast<ExplorerStyle*>(style)) {
            parent->setStyle(new ExplorerStyle(style));
        }
    }

    // Connect the close (X) button on tabs.
    connect(d->m_TabWidget, &QTabWidget::tabCloseRequested, [this](int index) {
        Q_D(ExplorerHost);
        if (d->m_TabWidget) {
            const auto widget = d->m_TabWidget->widget(index);
            d->m_TabWidget->removeTab(index);

            ExplorerClosedNotification closeNotif(static_cast<Explorer*>(widget), !d->m_InProgrammaticTabClose);
            notify(closeNotif);
            
            widget->deleteLater();
        }
    });
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
void ExplorerHost::addExplorer(Explorer* explorer, const QString& name, bool setActive)
{
    // Adopt the widget.
    explorer->setParent(this);
    Q_D(ExplorerHost);
    if (d->m_TabWidget) {
        const auto tab = d->m_TabWidget->addTab(explorer, name);
        if (setActive) {
            d->m_TabWidget->setCurrentIndex(tab);
        }
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
Explorer* ExplorerHost::setActiveExplorer(const Ufe::Path& rootItemPath)
{
    Explorer* explorer = nullptr;
    Q_D(ExplorerHost);
    const auto tabIdx = d->tabIndex(rootItemPath, &explorer);
    if (tabIdx < 0) {
        return nullptr;
    }
    d->m_TabWidget->setCurrentIndex(tabIdx);
    return explorer;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
Explorer* ExplorerHost::activeExplorer() const
{
    Q_D(const ExplorerHost);
    if (!d->m_TabWidget) {
        return nullptr;
    }
    return dynamic_cast<Explorer*>(d->m_TabWidget->currentWidget());
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
void ExplorerHost::closeExplorer(const Ufe::Path& rootItemPath)
{
    Q_D(ExplorerHost);
    if (!d->m_TabWidget) {
        return;
    }
    const auto tabIdx = d->tabIndex(rootItemPath);
    if (tabIdx < 0) {
        return;
    }

    d->m_InProgrammaticTabClose = true;
    d->m_TabWidget->tabCloseRequested(tabIdx);
    d->m_InProgrammaticTabClose = false;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
void ExplorerHost::setPlaceHolderText(const QString& placeHolder)
{
    Q_D(ExplorerHost);
    if (d->m_TabWidget) {
        d->m_TabWidget->setPlaceHolderText(placeHolder);
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
QMenuBar* ExplorerHost::menuBar() const
{
    Q_D(const ExplorerHost);
    return d->m_MenuBar;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
std::vector<Explorer*> ExplorerHost::explorers() const
{
    Q_D(const ExplorerHost);
    std::vector<Explorer*> explorers;
    if (d->m_TabWidget) {
        for (int i = 0; i < d->m_TabWidget->count(); ++i) {
            if (auto explorer = dynamic_cast<Explorer*>(d->m_TabWidget->widget(i))) {
                explorers.push_back(explorer);
            }
        }
    }
    return explorers;
}

} // namespace UfeUi
