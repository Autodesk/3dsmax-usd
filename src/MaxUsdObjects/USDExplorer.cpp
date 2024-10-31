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
#include "USDExplorer.h"

#include "LayerEditor/USDLayerEditor.h"
#include "MaxUsdUfe/StageObjectMap.h"
#include "MaxUsdUfe/UfeUtils.h"
#include "MaxUsdUfe/UsdTreeColumns.h"
#include "Objects/USDStageObject.h"
#include "UFEUI/Views/TabWidget.h"

#include <UFEUI/StandardTreeColumns.h>
#include <UFEUI/Views/Explorer.h>
#include <UFEUI/Views/ExplorerHost.h>
#include <UFEUI/editCommand.h>

#include <MaxUsd/Utilities/UiUtils.h>

#include <usdUfe/ufe/Global.h>
#include <usdUfe/ufe/StagesSubject.h>

#include <Qt/QMaxHelpers.h>
#include <Qt/QmaxDockWidget.h>
#include <ufe/hierarchy.h>
#include <ufe/runTimeMgr.h>

#include <QtWidgets/QApplication>
#include <max.h>
#include <qevent.h>
#include <qpointer.h>

std::unique_ptr<USDExplorer> USDExplorer::instance;

MaxSDK::QmaxDockWidget* getHostDockWidget();

namespace {
const std::string filterNames = "InactivePrims";

void PopulateCustomizeColumnMenu(QMenu* configureColumnsMenu)
{
    const auto toggleColumnVisibility = [](int visualIdx, bool checked) {
        auto explorer = USDExplorer::Instance();
        if (explorer->IsColumnHidden(visualIdx) == checked) {
            explorer->ToggleColumnHiddenState(visualIdx);
        }
    };
    auto explorer = USDExplorer::Instance();

    const auto visibilityColumnAction = configureColumnsMenu->addAction(
        QObject::tr("Visibility"), std::bind(toggleColumnVisibility, 1, std::placeholders::_1));
    visibilityColumnAction->setCheckable(true);
    const auto typeColumnAction = configureColumnsMenu->addAction(
        QObject::tr("Type"), std::bind(toggleColumnVisibility, 2, std::placeholders::_1));
    typeColumnAction->setCheckable(true);
    const auto kindColumnAction = configureColumnsMenu->addAction(
        QObject::tr("Kind"), std::bind(toggleColumnVisibility, 3, std::placeholders::_1));
    kindColumnAction->setCheckable(true);
    const auto purposeColumnAction = configureColumnsMenu->addAction(
        QObject::tr("Purpose"), std::bind(toggleColumnVisibility, 4, std::placeholders::_1));
    purposeColumnAction->setCheckable(true);

    QObject::connect(configureColumnsMenu, &QMenu::aboutToShow, [=]() {
        visibilityColumnAction->setChecked(!explorer->IsColumnHidden(1));
        typeColumnAction->setChecked(!explorer->IsColumnHidden(2));
        kindColumnAction->setChecked(!explorer->IsColumnHidden(3));
        purposeColumnAction->setChecked(!explorer->IsColumnHidden(4));
    });
}

class ContextMenuEventFilter : public QObject
{
public:
    ContextMenuEventFilter(QObject* parent)
        : QObject(parent)
    {
    }

    bool eventFilter(QObject* watched, QEvent* event)
    {
        if (event && event->type() == QEvent::Type::ContextMenu) {
            auto contextmenu_event = static_cast<QContextMenuEvent*>(event);
            if (auto treeView = dynamic_cast<QTreeView*>(watched)) {
                if (treeView->header()->geometry().contains(contextmenu_event->pos())) {
                    QMenu menu(treeView);
                    PopulateCustomizeColumnMenu(&menu);
                    menu.exec(contextmenu_event->globalPos());
                    contextmenu_event->accept();
                    return true;
                }
            }
        }
        return false;
    }

    static ContextMenuEventFilter* Instance();
};

ContextMenuEventFilter* ContextMenuEventFilter::Instance()
{
    static QPointer<ContextMenuEventFilter> instance
        = new ContextMenuEventFilter(getHostDockWidget());
    return instance;
}

class USDExplorerQMaxMainWindow : public MaxSDK::QmaxMainWindow
{
public:
    USDExplorerQMaxMainWindow(QWidget* parent, Qt::WindowFlags flags)
        : QmaxMainWindow(parent, flags)
    {
    }

protected:
    QMenu* createPopupMenu() override
    {
        auto menu = QmaxMainWindow::createPopupMenu();
        // remove the customize... context menu from the menu.
        auto actions = menu->actions();
        if (!actions.isEmpty()) {
            if (auto first_action = menu->actions().first()) {
                if (!first_action->isCheckable()) // some weak check
                {
                    menu->removeAction(first_action);
                }
            }
        }
        return menu;
    }
};

/**
 * Simple observer to react to tabs being closed from the Explorer Host.
 * When a tab is closed from the UI, we need to update the pb param to properly
 * persist that new state.
 */
class ExplorerHostObserver : public Ufe::Observer
{
public:
    void operator()(const Ufe::Notification& notification) override
    {
        if (const auto ec
            = dynamic_cast<const UfeUi::ExplorerHost::ExplorerClosedNotification*>(&notification)) {
            if (!ec->fromUI()) {
                return;
            }
            const auto explorer = ec->explorer();
            auto       stageObjectPath = explorer->rootItem()->path().head(1);
            auto       stageObject = StageObjectMap::GetInstance()->Get(stageObjectPath);
            stageObject->GetParamBlock(0)->SetValue(IsOpenInExplorer, false, 0);
        }
    }
};

} // namespace

MaxSDK::QmaxDockWidget* getHostDockWidget()
{
    // Only ever need a single instance.

    static MaxSDK::QmaxDockWidget* dockWidget = [] {
        auto max_main_window = GetCOREInterface()->GetQmaxMainWindow();

        auto dockWidget = new MaxSDK::QmaxDockWidget(
            "USD Explorer", QObject::tr("USD Explorer"), max_main_window);
        dockWidget->setProperty("QmaxDockMinMaximizable", true);

        auto explorer_main_window = new USDExplorerQMaxMainWindow(dockWidget, Qt::Widget);
        auto explorerHost = new UfeUi::ExplorerHost(explorer_main_window);

        static auto observer = std::make_shared<ExplorerHostObserver>();
        explorerHost->addObserver(observer);
        explorerHost->setPlaceHolderText(
            QObject::tr("No stage data currently displayed.\nSelect a USD Stage Object "
                        "and open it in the explorer, from the Parameters rollup."));
        dockWidget->setWidget(explorer_main_window);

        dockWidget->setFocusProxy(explorerHost);
        dockWidget->setFocusPolicy(Qt::StrongFocus);

        // Workaround to trick 3dsmax into properly docking this widget.
        max_main_window->addDockWidget(Qt::RightDockWidgetArea, dockWidget);
        // We added it as a dock widget and that would show the widget, but at this point we just
        // want to set it up, not necessarily show it.
        dockWidget->hide();

        // We want our dock-widget to float with native window behavior
        dockWidget->setFloating(true);
        // Arbitrary default size, similar to the Scene Explorer.
        dockWidget->resize(MaxSDK::UIScaled(280), MaxSDK::UIScaled(440));

        // Set back default size when un-docking.
        QSize floatingSize = dockWidget->size();
        QObject::connect(
            dockWidget,
            &MaxSDK::QmaxDockWidget::topLevelChanged,
            [floatingSize, dockWidget](bool topLevel) {
                if (topLevel) {
                    dockWidget->resize(floatingSize);
                }
            });

        // USD Specific menus.
        const auto menuBar = explorerHost->menuBar();
        const auto displayMenu = menuBar->addMenu(QObject::tr("Display"));

        // Show inactive prims option.
        const auto inactivePrimAction = displayMenu->addAction("Inactive Prims", []() {
            // Toggle display of inactive prims.
            const auto explorer = USDExplorer::Instance();
            explorer->SetShowInactivePrims(!explorer->ShowInactivePrims());
        });
        inactivePrimAction->setCheckable(true);
        inactivePrimAction->setChecked(USDExplorer::Instance()->ShowInactivePrims());

        // Auto-expand to selection option.
        const auto autoExpandAction = displayMenu->addAction("Auto-Expand to Selection", []() {
            const auto explorer = USDExplorer::Instance();
            explorer->SetAutoExpandedToSelection(!explorer->IsAutoExpandedToSelection());
        });
        autoExpandAction->setCheckable(true);
        autoExpandAction->setChecked(USDExplorer::Instance()->IsAutoExpandedToSelection());

        // customize menu
        const auto customizeMenu = menuBar->addMenu(QObject::tr("Customize"));
        const auto configureColumnsMenu = customizeMenu->addMenu(QObject::tr("Configure Columns"));
        PopulateCustomizeColumnMenu(configureColumnsMenu);

        // tools menu
        const auto toolsMenu = menuBar->addMenu(QObject::tr("Tools"));
        toolsMenu->addAction("USD Layer Editor...", []() {
            const auto explorer = USDExplorer::Instance();
            auto       activeExplorer = explorer->ActiveStageExplorer();
            if (!activeExplorer) {
                USDLayerEditor::Instance()->Open();
                return;
            }

            auto stageObject
                = MaxUsd::ufe::getUsdStageObjectFromPath(activeExplorer->rootItem()->path());
            USDLayerEditor::Instance()->OpenStage(stageObject);
        });
        return dockWidget;
    }();

    return dockWidget;
}

static UfeUi::ExplorerHost* getExplorerHost()
{
    // If we "only ever need a single instance" of the dock, we only ever will
    // get the same single instance of the host as well.
    static UfeUi::ExplorerHost* host
        = getHostDockWidget()->findChild<UfeUi::ExplorerHost*>("ExplorerHost");
    return host;
}

USDExplorer* USDExplorer::Instance()
{
    if (!instance) {
        instance = std::unique_ptr<USDExplorer>(new USDExplorer);
    }
    return instance.get();
}

void USDExplorer::Open()
{
    const auto dock = getHostDockWidget();
    dock->setWindowState(dock->windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
    dock->show();
    dock->raise();
}

void USDExplorer::Close()
{
    const auto host = getHostDockWidget();
    host->hide();
}

void USDExplorer::OpenStage(USDStageObject* stageObject)
{
    if (!stageObject) {
        return;
    }

    const auto stage = stageObject->GetUSDStage();
    if (!stage) {
        return;
    }

    const auto stagePath = MaxUsd::ufe::getUsdStageObjectPath(stageObject);
    if (stagePath.empty()) {
        return;
    }

    const auto sceneItem = Ufe::Hierarchy::createItem(stagePath);
    if (!sceneItem) {
        return;
    }

    const auto dock = getHostDockWidget();
    const auto host = dock->findChild<UfeUi::ExplorerHost*>("ExplorerHost");

    UfeUi::Explorer* explorer = host->setActiveExplorer(sceneItem->path());
    if (!explorer) {
        UfeUi::TreeColumns columns;

        columns.push_back(std::make_shared<NameColumn>(QString("root"), 0));
        columns.push_back(std::make_shared<VisColumn>(1));
        columns.push_back(std::make_shared<TypeColumn>(2));
        columns.push_back(std::make_shared<KindColumn>(3));
        columns.push_back(std::make_shared<PurposeColumn>(4));

        UfeUi::TypeFilter typeFilter;

        const UfeUi::Explorer::ColorScheme colors = {
            // match item color in 3dsMax.
            QApplication::palette().color(QPalette::Inactive, QPalette::Button).name(),
            // match item selected color in 3dsMax.
            QApplication::palette().color(QPalette::Normal, QPalette::Light).name(),
            // match item selected/hovered color in 3dsMax
            QApplication::palette().color(QPalette::Inactive, QPalette::Light).name(),
        };

        const QString treeViewBranchAdjustStyle
            = QString { "	QTreeView, QTreeWidget{"
                        "	    show-decoration-selected: 1;"
                        "	}"
                        "	QTreeView:branch:hover {"
                        "		background-color: %1;"
                        "	}"
                        "	QTreeView:branch:selected {"
                        "		background-color: %2;"
                        "	}"
                        "	QTreeView:branch:selected:hover {"
                        "		background-color: %3;"
                        "	}"
                        "	QTreeView::branch:open{"
                        "		padding: 0.35em;"
                        "	}"
                        "	QTreeView::branch:closed{"
                        "		padding: 0.35em;"
                        "	}"
                        "	QTreeView::branch:open:has-children{"
                        "		image: url(:/ufe/Icons/branch_opened.png);"
                        "	}"
                        "	QTreeView::branch:closed:has-children {"
                        "		image: url(:/ufe/Icons/branch_closed.png);"
                        "	}" }
                  .arg(colors.hover.name(), colors.selected.name(), colors.selectedHover.name());

        explorer = new UfeUi::Explorer(
            sceneItem,
            columns,
            typeFilter,
            childFilter,
            autoExpandToSelection,
            treeViewBranchAdjustStyle,
            colors);

        const auto        layerNameWithExt = stage->GetRootLayer()->GetDisplayName();
        const size_t      lastIndex = layerNameWithExt.find_last_of(".");
        const std::string layerName = layerNameWithExt.substr(0, lastIndex);

        explorer->setColumnState(1 /*VisColumn*/, IsColumnHidden(1));
        explorer->setColumnState(2 /*TypeColumn*/, IsColumnHidden(2));
        explorer->setColumnState(3 /*KindColumn*/, IsColumnHidden(3));
        explorer->setColumnState(4 /*PurposeColumn*/, IsColumnHidden(4));

        for (int i = 1; i < 4; ++i) {
            int w = GetManualColumnWidth(i);
            if (w != -1) {
                explorer->treeView()->header()->resizeSection(i, w);
            } else {
                explorer->treeView()->resizeColumnToContents(i);
            }
        }

        host->addExplorer(explorer, layerName.c_str(), true);

        explorer->treeView()->installEventFilter(ContextMenuEventFilter::Instance());
    }
    // Make sure the host is showed.
    dock->show();

    // We want the 3dsMax hotkeys to work while we are focused on the treeView.
    // For some keys this is a bit clunky, as both the treeView and 3dsMax will
    // react to the same key (for example the up-arrow). However, this is also
    // the behavior in the Scene Explorer, and it is quite hard to selectively
    // decide, as we are in a mix of QT and Win32. Probably to be improved later.
    // The timing of the call to enable max accelerators is important, we do it
    // here as it is after the "show", yet before the control gets focus. If we
    // do it too early, max will apply its own rules vs what widget types should
    // disable accelerators, and undo what we do. In general, QTreeViews have
    // accelerator disabled.
    auto treeViews = host->findChildren<QTreeView*>();
    for (const auto& treeView : treeViews) {
        MaxUsd::Ui::DisableMaxAcceleratorsOnFocus(treeView, false);
    }
    dock->setWindowState(dock->windowState() & ~Qt::WindowMinimized | Qt::WindowActive);
    dock->raise();
}

void USDExplorer::CloseStage(USDStageObject* stageObject)
{
    if (!stageObject) {
        return;
    }

    const auto stagePath = MaxUsd::ufe::getUsdStageObjectPath(stageObject);
    if (stagePath.empty()) {
        return;
    }

    const auto sceneItem = Ufe::Hierarchy::createItem(stagePath);
    if (!sceneItem) {
        return;
    }

    if (const auto host = getExplorerHost()) {
        // Close the stage's tab if opened.
        host->closeExplorer(sceneItem->path());
    }
}

void USDExplorer::SetShowInactivePrims(bool showInactive)
{
    const auto it = std::find_if(
        childFilter.begin(), childFilter.end(), [](const Ufe::ChildFilterFlag& filter) {
            return filter.name == filterNames;
        });
    if (it == childFilter.end()) {
        DbgAssert(0 && _T("Usd Ufe inactive child filter is not initalized."));
        return;
    }

    it->value = showInactive;

    for (const auto& explorer : AllStageExplorers()) {
        explorer->setChildFilter(childFilter);
    }
}

bool USDExplorer::ShowInactivePrims() const
{
    const auto it = std::find_if(
        childFilter.begin(), childFilter.end(), [](const Ufe::ChildFilterFlag& filter) {
            return filter.name == filterNames;
        });
    if (it == childFilter.end()) {
        DbgAssert(0 && _T("Usd Ufe inactive child filter is not initalized."));
        return false;
    }
    return it->value;
}

bool USDExplorer::IsAutoExpandedToSelection() { return autoExpandToSelection; }

void USDExplorer::SetAutoExpandedToSelection(bool autoExpandToSelection)
{
    for (const auto& explorer : AllStageExplorers()) {
        explorer->setAutoExpandedToSelection(autoExpandToSelection);
    }

    this->autoExpandToSelection = autoExpandToSelection;
}

USDExplorer::USDExplorer()
{
    // Initialize the child filter to show inactive prims by default.
    const auto handler = Ufe::RunTimeMgr::instance().hierarchyHandler(UsdUfe::getUsdRunTimeId());
    childFilter = handler->childFilter();

    treeViewColumnsState.push_back({ 1 /* VisColumn */, false });
    treeViewColumnsState.push_back({ 2 /* TypeColumn */, false });
    treeViewColumnsState.push_back({ 3 /* KindColumn */, true });
    treeViewColumnsState.push_back({ 4 /* PurposeColumn */, true });
}

std::vector<UfeUi::Explorer*> USDExplorer::AllStageExplorers()
{
    std::vector<UfeUi::Explorer*> results;
    if (const auto host = getExplorerHost()) {
        results = host->explorers();
    }
    return results;
}

UfeUi::Explorer* USDExplorer::ActiveStageExplorer()
{
    if (const auto host = getExplorerHost()) {
        return host->activeExplorer();
    }
    return nullptr;
}

bool USDExplorer::IsColumnHidden(int visualIdx)
{
    const TreeViewColumsState::iterator foundItem = std::find_if(
        treeViewColumnsState.begin(),
        treeViewColumnsState.end(),
        [visualIdx](const TreeViewColumn& item) { return item.visualIdx == visualIdx; });
    if (foundItem == treeViewColumnsState.end()) {
        return false;
    }
    return foundItem->hidden;
}

bool USDExplorer::ToggleColumnHiddenState(int visualIdx)
{
    const auto state = std::find_if(
        treeViewColumnsState.begin(),
        treeViewColumnsState.end(),
        [visualIdx](const TreeViewColumn& item) { return item.visualIdx == visualIdx; });

    if (state == treeViewColumnsState.end())
        return false;

    state->hidden = !state->hidden; // toggle the state of the column

    if (state->hidden) // just about to be hidden - so save the width...
    {
        if (auto explorer = ActiveStageExplorer()) {
            auto header = explorer->treeView()->header();
            bool was_last_section = false;
            if (header->stretchLastSection()) {
                was_last_section = true;
                for (int i = visualIdx + 1; i < header->count(); ++i) {
                    if (!header->isSectionHidden(i)) {
                        was_last_section = false;
                        break;
                    }
                }
            }
            if (!was_last_section) {
                state->manualColumnWidth = header->sectionSize(visualIdx);
            }
        }
    }

    for (const auto& explorer : AllStageExplorers()) {
        explorer->setColumnState(visualIdx, state->hidden);

        if (!state->hidden) // has been shown again - so restore the width...
        {
            auto tree_view = explorer->treeView();
            if (state->manualColumnWidth == -1) {
                tree_view->resizeColumnToContents(visualIdx);
            }
        }
    }
    return state->hidden;
}

int USDExplorer::GetManualColumnWidth(int visualIdx) const
{
    for (const auto& state : treeViewColumnsState) {
        if (state.visualIdx == visualIdx) {
            return state.manualColumnWidth;
        }
    }
    return -1;
}
