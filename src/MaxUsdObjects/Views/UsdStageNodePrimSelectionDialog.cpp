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
#include "UsdStageNodePrimSelectionDialog.h"

#include "ui_UsdStageNodePrimSelectionDialog.h"

#include <MaxUsd/Widgets/TreeModelFactory.h>

#include <Qt/QmaxMainWindow.h>

#include <IPathConfigMgr.h>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWhatsThis>
#include <helpsys.h>
#include <maxapi.h>

#define idh_usd_stageref _T("idh_usd_stageref")

PXR_NAMESPACE_OPEN_SCOPE
TF_DEFINE_PUBLIC_TOKENS(MaxUsdPrimSelectionDialogTokens, PXR_MAXUSD_PRIM_SELECTION_DIALOG_TOKENS);
PXR_NAMESPACE_CLOSE_SCOPE

UsdStageNodePrimSelectionDialog::UsdStageNodePrimSelectionDialog(
    const QString&                              rootLayerPath,
    const QString&                              maskPath,
    MaxUsd::TreeModelFactory::TypeFilteringMode filterMode,
    const std::vector<std::string>&             filteredTypeNames,
    const pxr::VtDictionary&                    options,
    QWidget*                                    parent)
    : rootLayerPath { rootLayerPath }
    , maskPath { maskPath }
    , QDialog { parent }
    , ui(new Ui::UsdStageNodePrimSelectionDialog)
    , filterMode { filterMode }
    , filteredTypeNames { filteredTypeNames }
{
    setWindowFlags(windowFlags() | Qt::WindowContextHelpButtonHint);
    ui->setupUi(this);
    setParent(GetCOREInterface()->GetQmaxMainWindow(), windowFlags());

    ui->Buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    if (!rootLayerPath.isEmpty()) {
        SetupUiFromRootLayerFilename(QFileInfo(rootLayerPath));
    }
    // Setup or hide, the "open in explorer" option.
    const auto openInExplorerIt
        = options.find(pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer);
    if (openInExplorerIt == options.end()) {
        ui->OpenInUsdExplorerCheckbox->setVisible(false);
        ui->OpenInUsdExplorerCheckbox->setChecked(false);
    } else {
        ui->OpenInUsdExplorerCheckbox->setChecked(openInExplorerIt->second.UncheckedGet<bool>());
    }

    // Setup or hide, the "load payloads" option.
    const auto payloadIt = options.find(pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads);
    if (payloadIt == options.end()) {
        ui->LoadPayloadsCheckbox->setChecked(false);
        ui->LoadPayloadsCheckbox->setVisible(false);
    } else {
        ui->LoadPayloadsCheckbox->setChecked(payloadIt->second.UncheckedGet<bool>());
    }

    const float dpiScale = MaxSDK::GetUIScaleFactor();
    this->resize(this->geometry().width() * dpiScale, this->geometry().height() * dpiScale);
}

UsdStageNodePrimSelectionDialog::~UsdStageNodePrimSelectionDialog() { }

const pxr::VtDictionary& UsdStageNodePrimSelectionDialog::GetDefaultDictionary()
{
    static pxr::VtDictionary defaultDict;
    static std::once_flag    once;
    std::call_once(once, []() {
        defaultDict[pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads] = true;
        defaultDict[pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer] = true;
    });

    return defaultDict;
}

void UsdStageNodePrimSelectionDialog::on_RootLayerLineEdit_editingFinished()
{
    QFileInfo file = QFileInfo { ui->RootLayerLineEdit->text() };
    if (file.isFile()) {
        SetupUiFromRootLayerFilename(file);
    } else {
        // These calls must come after the UI is initialized via "setupUi()":
        treeModel = MaxUsd::TreeModelFactory::CreateEmptyTreeModel();
        proxyModel = std::make_unique<QSortFilterProxyModel>(this);

        // Configure the TreeView of the dialog:
        proxyModel->setSourceModel(treeModel.get());
        ui->treeView->setModel(proxyModel.get());
    }
}

void UsdStageNodePrimSelectionDialog::SetupUiFromRootLayerFilename(const QFileInfo& fileInfo)
{
    if (!fileInfo.isFile()) {
        return;
    }

    rootLayerPath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    ui->RootLayerLineEdit->setText(rootLayerPath);

    stage
        = pxr::UsdStage::Open(rootLayerPath.toStdString(), pxr::UsdStage::InitialLoadSet::LoadNone);

    if (!stage) {
        return;
    }

    // These calls must come after the UI is initialized via "setupUi()":
    treeModel
        = MaxUsd::TreeModelFactory::CreateFromSearch(stage, "", filterMode, filteredTypeNames);
    proxyModel = std::make_unique<QSortFilterProxyModel>(this);

    // Configure the TreeView of the dialog:
    proxyModel->setSourceModel(treeModel.get());

    proxyModel->setDynamicSortFilter(false);
    proxyModel->setFilterCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
    ui->treeView->setModel(proxyModel.get());
    ui->treeView->expandToDepth(3);

    QHeaderView* treeHeader = ui->treeView->header();
    treeHeader->resetDefaultSectionSize();

    // Set the width for the first column of the TreeView as the width divided by 3 of the "filter"
    // text box above it:
    treeHeader->resizeSection(0, ui->filterLineEdit->size().width() / 3);

    // Configure the "Path" column to be the one that stretches to accomodate sufficient space for
    // content:
    treeHeader->setStretchLastSection(false);
    treeHeader->setSectionResizeMode(
        MaxUsd::QTreeModel::TREE_COLUMNS::PATH, QHeaderView::ResizeMode::Stretch);

    treeHeader->setToolTip(QApplication::translate(
        "UsdStageNodePrimSelectionDialog",
        "Select a prim for stage reference creation. All prims descending from the selected prim "
        "are added into your referenced scene.",
        nullptr));

    connect(
        ui->filterLineEdit,
        &QLineEdit::textChanged,
        this,
        &UsdStageNodePrimSelectionDialog::OnSearchFilterChanged);
    connect(
        ui->treeView->selectionModel(),
        &QItemSelectionModel::selectionChanged,
        this,
        &UsdStageNodePrimSelectionDialog::OnTreeViewSelectionChanged);

    connect(
        ui->treeView->selectionModel(),
        &QItemSelectionModel::selectionChanged,
        [=]() { // with lambda
            if (ui->treeView->selectionModel()->selectedIndexes().size() > 2) {
                QList<QModelIndex> lst = ui->treeView->selectionModel()->selectedIndexes();
                ui->treeView->selectionModel()->select(lst.first(), QItemSelectionModel::Deselect);
            }
        });

    // Select row based on maskPath
    selectTreeViewPrimFromString(maskPath);

    // Create the Spinner overlay on top of the TreeView, once it is configured:
    overlay = std::make_unique<MaxUsd::QSpinnerOverlayWidget>(ui->treeView);

    // need to manually call the filter function if a filter had already been typed
    if (!ui->filterLineEdit->text().isEmpty()) {
        OnSearchFilterChanged(ui->filterLineEdit->text());
        overlay->resize(ui->treeView->size());
    }
}

void UsdStageNodePrimSelectionDialog::showEvent(QShowEvent* qEvent)
{
    QHeaderView* treeHeader = ui->treeView->header();
    // Need to set the section width here again because the size of the filterLineEdit is calculated
    // after rendering and in some cases, having this code only in the other function won't resize
    // to the correct amount
    treeHeader->resizeSection(0, ui->filterLineEdit->size().width() / 3);
}

void UsdStageNodePrimSelectionDialog::selectTreeViewPrimFromString(
    const QString& pathToSelect) const
{
    const QModelIndexList nextMatches = ui->treeView->selectionModel()->model()->match(
        ui->treeView->model()->index(0, 1),
        Qt::DisplayRole,
        pathToSelect,
        1,
        Qt::MatchRecursive | Qt::MatchExactly);
    if (!nextMatches.empty()) {
        const QModelIndex itemIndex = nextMatches.at(0);
        ui->treeView->setCurrentIndex(itemIndex);
        ui->treeView->selectionModel()->select(itemIndex, QItemSelectionModel::Select);
    } else {
        ui->treeView->setCurrentIndex(ui->treeView->model()->index(0, 0));
    }
}

QFileInfo UsdStageNodePrimSelectionDialog::SelectFile(const QString& initialFilePath)
{
    QString initialDir = initialFilePath;
    if (initialFilePath.isEmpty() || !QFile::exists(initialFilePath)) {
        initialDir = MSTR(IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_RENDER_ASSETS_DIR));
    }

    QFileInfo file { QFileDialog::getOpenFileName(
        nullptr,
        tr("Select Universal Scene Description (USD) File"),
        initialDir,
        tr("USD (*.usd;*.usda;*.usdc)")) };
    if (file.isFile()) {
        return file;
    }
    return QFileInfo();
}

void UsdStageNodePrimSelectionDialog::on_RootLayerPathButton_clicked()
{
    QFileInfo file = SelectFile(rootLayerPath);
    SetupUiFromRootLayerFilename(file);
}

const QString& UsdStageNodePrimSelectionDialog::GetRootLayerPath() const { return rootLayerPath; }

const QString& UsdStageNodePrimSelectionDialog::GetRootLayerFilename() const
{
    return rootLayerFilename;
}

const QString& UsdStageNodePrimSelectionDialog::GetMaskPath() const { return maskPath; }

const bool UsdStageNodePrimSelectionDialog::GetPayloadsLoaded() const
{
    return ui->LoadPayloadsCheckbox->isChecked();
}

const bool UsdStageNodePrimSelectionDialog::GetOpenInUsdExplorer() const
{
    return ui->OpenInUsdExplorerCheckbox->isChecked();
}

void UsdStageNodePrimSelectionDialog::OnSearchFilterChanged(const QString& searchFilter)
{
    // Stop any search that was already ongoing but that has not yet completed:
    if (searchThread != nullptr && !searchThread->isFinished()) {
        searchThread->quit();
        searchThread->wait();
    }

    // Create a timer that will display a Spinner if the search has been ongoing for a (small)
    // amount of time, to let the User know that a background task is ongoing and that 3ds Max is
    // not frozen:
    searchTimer = std::make_unique<QTimer>(this);
    searchTimer->setSingleShot(true);
    connect(searchTimer.get(), &QTimer::timeout, searchTimer.get(), [this]() {
        ui->treeView->setEnabled(false);
        overlay->StartSpinning();
    });
    searchTimer->start(std::chrono::milliseconds(125).count());

    // Create a thread to perform a search for the given criteria in the background in order to
    // maintain a responsive UI that continues accepting input from the User:
    searchThread = std::make_unique<MaxUsd::USDSearchThread>(
        stage, searchFilter.toStdString(), filterMode, filteredTypeNames);
    connect(
        searchThread.get(),
        &MaxUsd::USDSearchThread::finished,
        searchThread.get(),
        [&, searchFilter, this]() {
            // Since results have been received, discard the timer that was waiting for results so
            // that the Spinner Widget is not displayed:
            searchTimer->stop();

            // Set the search results as the new effective data:
            treeModel = std::move(searchThread->ConsumeResults());
            proxyModel->setSourceModel(treeModel.get());

            // Set the View to a sensible state to reflect the new data:
            bool searchYieledResults = proxyModel->hasChildren();
            ui->treeView->expandAll();
            ui->treeView->selectionModel()->clearSelection();
            ui->treeView->setEnabled(searchYieledResults);
            ui->Buttons->button(QDialogButtonBox::Ok)->setEnabled(false);

            // Select the root prim row when the searchFilter is empty so that when the user inputs
            // a filter and eventually removes it, there will be a row selected in prim browser.
            if (searchFilter.toStdString().empty()) {
                // Our selection UI spans over 2 columns starting at column 1
                ui->treeView->selectionModel()->select(
                    ui->treeView->model()->index(0, 1), QItemSelectionModel::Select);
                ui->treeView->selectionModel()->select(
                    ui->treeView->model()->index(0, 2), QItemSelectionModel::Select);
                ui->Buttons->button(QDialogButtonBox::Ok)->setEnabled(true);
            }

            if (searchYieledResults) {
                overlay->Hide();
            } else {
                overlay->ShowInformationMessage(tr("Your search did not match any Prim."));
            }
        });

    searchThread->start(QThread::Priority::TimeCriticalPriority);
}

void UsdStageNodePrimSelectionDialog::OnTreeViewSelectionChanged(
    const QItemSelection& selectedItems,
    const QItemSelection& deselectedItems)
{
    // Note that Qt does not trigger "selectionChanged" signals when changing selection from within
    // the propagation chain, so this will not cause an infinite callback loop.
    QItemSelectionModel* selectionModel = ui->treeView->selectionModel();
    if (selectionModel != nullptr) {
        bool selectionIsEmpty = selectionModel->selection().isEmpty();
        if (selectionIsEmpty) {
            selectionModel->select(deselectedItems, QItemSelectionModel::Deselect);
        } else {
            const auto& selectedRows
                = selectionModel->selectedRows(MaxUsd::QTreeModel::TREE_COLUMNS::PATH);
            if (!selectedRows.isEmpty()) {
                const auto& selectedPathIndex = selectedRows[0];

                auto           sourceIndex = proxyModel->mapToSource(selectedPathIndex);
                QStandardItem* item = treeModel->itemFromIndex(sourceIndex);
                if (item != nullptr) {
                    QVariant pathData = item->data(Qt::DisplayRole);
                    if (pathData.isValid() && pathData.canConvert<QString>()) {
                        maskPath = pathData.toString();
                    }
                }
            }
        }

        // Make sure the "Ok" button is disabled if no item of the Tree is selected:
        ui->Buttons->button(QDialogButtonBox::Ok)->setEnabled(!selectionIsEmpty);
    }
}

bool UsdStageNodePrimSelectionDialog::event(QEvent* ev)
{
    if (ev->type() == QEvent::EnterWhatsThisMode) {
        // We need to leave immediately the "What's this" mode, otherwise
        // system is waiting for a click on particular widget
        QWhatsThis::leaveWhatsThisMode();
        // Open a new web page containing help about usd component export
        MaxSDK::IHelpSystem::GetInstance()->ShowProductHelpForTopic(idh_usd_stageref);
        return true;
    }
    return __super::event(ev);
}
