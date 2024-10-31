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
#pragma once
#include <MaxUsdObjects/MaxUsdObjectsAPI.h>

#include <MaxUsd/Widgets/QSpinnerOverlayWidget.h>
#include <MaxUsd/Widgets/QTreeModel.h>
#include <MaxUsd/Widgets/USDSearchThread.h>

#include <QtCore/QFileInfo.h>
#include <QtCore/QSortFilterProxyModel.h>
#include <QtWidgets/QDialog>

PXR_NAMESPACE_OPEN_SCOPE
// clang-format off
#define PXR_MAXUSD_PRIM_SELECTION_DIALOG_TOKENS \
	/* Dictionary keys */ \
	/* Whether or not to load payloads when opening the root layer. */ \
	(loadPayloads) \
	/* Whether or not to open the explorer after layer/prim selection. */ \
	(openInExplorer)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(
    MaxUsdPrimSelectionDialogTokens,
    MaxUSDObjectsAPI,
    PXR_MAXUSD_PRIM_SELECTION_DIALOG_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

namespace Ui {
class UsdStageNodePrimSelectionDialog;
}

class UsdStageNodePrimSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UsdStageNodePrimSelectionDialog(
        const QString&                              rootLayerPath = nullptr,
        const QString&                              maskPath = nullptr,
        MaxUsd::TreeModelFactory::TypeFilteringMode filterMode
        = MaxUsd::TreeModelFactory::TypeFilteringMode::NoFilter,
        const std::vector<std::string>& filteredTypeNames = {},
        const pxr::VtDictionary&        options = GetDefaultDictionary(),
        QWidget*                        parent = nullptr);

    virtual ~UsdStageNodePrimSelectionDialog();

    /**
     * \brief Function which sets up the UI of this Dialog widget, which includes the QTreeView
     * \param fileInfo QFileInfo ref which contains the RootFilename
     */
    void SetupUiFromRootLayerFilename(const QFileInfo& fileInfo);

    const QString& GetRootLayerPath() const;
    const QString& GetRootLayerFilename() const;
    const QString& GetMaskPath() const;
    const bool     GetPayloadsLoaded() const;
    const bool     GetOpenInUsdExplorer() const;

    /**
     * \brief This method will open a file selection window to select a USD layer to populate
     * the dialog from.
     * \param initialFilePath The path to which the file selection dialog will open, by default the APP_RENDER_ASSETS_DIR.
     * \return A QFileInfo object, setup with the selected file.
     */
    static QFileInfo SelectFile(const QString& initialFilePath = QString());

public Q_SLOTS:
    /// Qt callback functions based on named widgets in the associated .ui file
    void on_RootLayerPathButton_clicked();
    void on_RootLayerLineEdit_editingFinished();

protected:
    /**
     * \brief Callback function executed upon changing the text in the search box.
     * \param searchFilter The new content of the search box.
     */
    void OnSearchFilterChanged(const QString& searchFilter);

    /**
     * \brief Callback function executed upon selecting items in the QTreeView.
     * \param selectedItems The list of TreeView items which were selected.
     * \param deselectedItems The list of TreeView items which were deselected.
     */
    void OnTreeViewSelectionChanged(
        const QItemSelection& selectedItems,
        const QItemSelection& deselectedItems);

    /**
     * \brief This event handler can be reimplemented in a subclass to receive widget show events which are passed in the event parameter.
     * \param qEvent The QShowEvent pointer containing the event.
     */
    void showEvent(QShowEvent* qEvent) override;

    /**
     * \brief Looks for the given string path in the TreeView stage hierarchy and selects it.
     * \param pathToSelect The path for the prim to be selected in the TreeView hierarchy. Selects root prim if path can't be found.
     */
    void selectTreeViewPrimFromString(const QString& pathToSelect) const;

    /**
     * \brief Callback function that is called on Qt events.
     * \param ev Pointer to QEvent object that holds information on the event.
     */
    bool event(QEvent* ev) override;

private:
    /**
     * \brief The dictionary holding the default state of all the options.
     * \return The default options dictionary.
     */
    static const pxr::VtDictionary& GetDefaultDictionary();

    /// Member variable holding the root layer path, which is the file path
    QString rootLayerPath;
    /// Member variable holding only the file name portion of the path
    QString rootLayerFilename;
    /// Member variable holding stage mask, which is the path to the prim to be targeted
    QString maskPath;

    /// RefPtr to the stage
    pxr::UsdStageRefPtr stage;
    /// Reference to the Qt UI View of the dialog
    std::unique_ptr<Ui::UsdStageNodePrimSelectionDialog> ui {
        std::make_unique<Ui::UsdStageNodePrimSelectionDialog>()
    };

    /// Reference to the Model holding the structure of the USD file hierarchy
    std::unique_ptr<MaxUsd::QTreeModel> treeModel;
    /// Reference to the Proxy Model used to sort and filter the USD file hierarchy
    std::unique_ptr<QSortFilterProxyModel> proxyModel;
    /// TreeView overlay on which to display an animated Spinner or message to the User
    std::unique_ptr<MaxUsd::QSpinnerOverlayWidget> overlay;
    /// Reference to the thread used to perform Prim searches in the background
    std::unique_ptr<MaxUsd::USDSearchThread> searchThread;
    /// Reference to the timer used to display a Spinner overlay on top of the TreeView in case of
    /// lengthy search operations
    std::unique_ptr<QTimer> searchTimer;

    /// Variables used to filter the stage Prims based on their type.
    std::vector<std::string>                    filteredTypeNames;
    MaxUsd::TreeModelFactory::TypeFilteringMode filterMode;
};