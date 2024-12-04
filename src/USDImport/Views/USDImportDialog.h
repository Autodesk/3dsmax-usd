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

#include "IUSDImportView.h"
#include "ui_USDImportDialog.h"

#include <MaxUsd/Widgets/QDiagnosticMessagesModel.h>
#include <MaxUsd/Widgets/QSpinnerOverlayWidget.h>
#include <MaxUsd/Widgets/QTreeModel.h>
#include <MaxUsd/Widgets/USDSearchThread.h>

#include <QtCore/QSortFilterProxyModel.h>
#include <memory>

class QKeyEvent;

/**
 * \brief USD file import dialog.
 */
class USDImportDialog
    : public QDialog
    , public IUSDImportView
{
public:
    /**
     * \brief Constructor.
     * \param filename Absolute file path of a USD file to import.
     * \param buildOptions The currently set import options.
     * \param parent A reference to the parent widget of the dialog.
     */
    explicit USDImportDialog(
        const fs::path&                       filename,
        const MaxUsd::MaxSceneBuilderOptions& buildOptions,
        QWidget*                              parent = nullptr);

    /**
     * \brief Display the View.
     * \return A flag indicating whether the User chose to import the USD file.
     */
    bool Execute() override;

    /**
     * \brief Return the build options for the 3ds Max scene to build.
     * \return The build options for the 3ds Max scene to build.
     */
    MaxUsd::MaxSceneBuilderOptions GetBuildOptions() const override;

protected:
    /**
     * \brief Callback function that is called on platform/OS native events.
     *	See https://doc.qt.io/qt-6/qwidget.html#nativeEvent for more information.
     * \param eventType QByteArray ref that holds the type of platform/OS that generated the event.
     * \param message void pointer holding the message (the actual event).
     * \param result long pointer holding the result of the operation to be set by the function.
     */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    virtual bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#else
    virtual bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
#endif

    /**
     * \brief Callback function that is called on Qt events.
     * \param ev Pointer to QEvent object that holds information on the event.
     */
    bool event(QEvent* ev) override;

    /**
     * \brief Callback function that is called on Qt show events.
     * \param ev Pointer to QShowEvent object that holds information on the show event.
     */
    void showEvent(QShowEvent* ev) override;

    /**
     * \brief Callback function that is called on Qt key press events.
     * \param e Pointer to QKeyEvent object that holds information on the key press event.
     */
    void keyPressEvent(QKeyEvent* e) override;

    /**
     * \brief Create a USD Stage from the given source file so its content can be displayed in the dialog.
     * \remarks In case of error, a valid (albeit empty) USD Stage will be returned. The "diagnosticDelegate" can be
     * consulted to infer information about USD Diagnostic information emitted by USD when
     * attempting to open the given file.
     * \param filename Absolute file path of a USD file to import.
     * \return The USD Stage created from the given source file.
     */
    pxr::UsdStageRefPtr CreateUSDStage(const fs::path& filename);

    /**
     * \brief Callback function executed upon changing the text in the search box.
     * \param searchFilter The new content of the search box.
     */
    void OnSearchFilterChanged(const QString& searchFilter);

    /**
     * \brief Callback function executed upon selecting items in the TreeView.
     * \param selectedItems The list of TreeView items which were selected.
     * \param deselectedItems The list of TreeView items which were deselected.
     */
    void OnTreeViewSelectionChanged(
        const QItemSelection& selectedItems,
        const QItemSelection& deselectedItems);

    /**
     * \brief Callback function executed upon changing the value of the "start time code value" spinbox.
     * \param value The new value of the timecode spinbox value.
     */
    void OnStartTimeCodeValueChanged(double value);

    /**
     * \brief Callback function executed upon changing the value of the "end time code value" spinbox.
     * \param value The new value of the timecode spinbox value.
     */
    void OnEndTimeCodeValueChanged(double value);

    /**
     * \brief Callback function executed upon changing the value of the "end time code enabled" checkbox.
     * \param checked The new value of the timecode checkbox value.
     */
    void OnEndFrameCheckBoxStateChanged(bool checked);

    /**
     * \brief Callback function executed upon changing the value of the "Log Level" dropdown
     * \param currentIndex index of the selection option for "Log Level"
     */
    void OnLogDataLevelStateChanged(int currentIndex);

    /**
     * \brief Callback executed upon clicking the "Materials" checkbox of the UI.
     * \param checked Flag indicating if the checkbox was just checked by the User.
     */
    void OnTranslateMaterialsStateChanged(bool checked);

    /**
     * \brief Callback for file browsing log path
     */
    void OnLogPathBrowseClicked();

    /**
     * \brief Toggles logPath selection UI based on log level selection.
     */
    void ToggleLogUI();

    /**
     * \brief Toggles import button based on options.
     */
    void ToggleImportButton();

protected:
    /// Sets the initial configuration for the animation session of the UI
    void SetAnimationConfiguration();

    /// Reference to the Qt UI View of the dialog:
    std::unique_ptr<Ui::ImportDialog> ui { std::make_unique<Ui::ImportDialog>() };

    /// Reference to the Model holding the structure of the USD file hierarchy:
    std::unique_ptr<MaxUsd::QTreeModel> treeModel;
    /// Reference to the Proxy Model used to sort and filter the USD file hierarchy:
    std::unique_ptr<QSortFilterProxyModel> proxyModel;

    /// Reference to the thread used to perform Prim searches in the background:
    std::unique_ptr<MaxUsd::USDSearchThread> searchThread;
    /// Reference to the timer used to display a Spinner overlay on top of the TreeView in case of
    /// lengthy search operations:
    std::unique_ptr<QTimer> searchTimer;

    /// Reference to the USD Stage holding the list of Prims which could be imported:
    pxr::UsdStageRefPtr stage;

    /// Build options to use to convert the USD content into 3ds Max content:
    MaxUsd::MaxSceneBuilderOptions buildOptions;

    /// TreeView overlay on which to display an animated Spinner or message to the User:
    std::unique_ptr<MaxUsd::QSpinnerOverlayWidget> overlay;

    /// Reference to the Model holding the structure of Diagnostic Messags emitted by USD:
    std::unique_ptr<MaxUsd::QDiagnosticMessagesModel> QDiagnosticMessagesModel;

    /// Used to keep the old user's input so that it can be restored afterwards
    double oldEndFrameSpinnerValue { 0 };
};
