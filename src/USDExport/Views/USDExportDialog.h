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

#include "IUSDExportView.h"

#include <MaxUsd/Interfaces/IUSDExportOptions.h>

#include <QtWidgets/qdialog.h>
#include <memory>

class QStandardItem;
class QKeyEvent;
class UsdExportAnimationRollup;

namespace Ui {
class ExportDialog;
};

/**
 * \brief USD file export dialog.
 */
class USDExportDialog
    : public QDialog
    , public IUSDExportView
{
public:
    /**
     * \brief Constructor.
     * \param buildOptions Initial build options to use to initialize the UI.
     */
    USDExportDialog(const fs::path& filePath, const MaxUsd::IUSDExportOptions& buildOptions);

    ~USDExportDialog() override;

    /**
     * \brief Display the View.
     * \return A flag indicating whether the User chose to export the USD file.
     */
    bool Execute() override;

    /**
     * \brief Get the build configuration options for the file to export.
     * \return The build configuration options for the file to export.
     */
    const MaxUsd::USDSceneBuilderOptions& GetBuildOptions() const override;

protected:
    /**
     * \brief Callback function that is called on platform/OS native events.
     *	See https://doc.qt.io/qt-5/qwidget.html#nativeEvent4 for more information.
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
     * \brief Filter the mouse wheel event on spin and combo boxes if not in focus to avoid them "stealing"
     * the focus of the scroll when scrolling in the dialog.
     * \param object The object to filter on.
     * \param event The event to be filtered.
     */
    bool eventFilter(QObject* object, QEvent* event) override;

    /*
     * \brief The dialog's accept behavior
     */
    void accept() override;

    /**
     * \brief Get the state (open/close) of all the rollups.
     * \return A map of the rollup names and their state (open/close).
     */
    std::map<QString, bool> GetRollupState() const;

    /// Reference to the Qt UI View of the dialog:
    std::unique_ptr<Ui::ExportDialog> ui;
    UsdExportAnimationRollup*         animationRollup;

    /// USD Scene build configuration options:
    MaxUsd::USDSceneBuilderOptions buildOptions;
    /// The full path where the USD file will be exported:
    fs::path exportPath;

    const QString rollupCategory = "ExportDialogRollups";
};
