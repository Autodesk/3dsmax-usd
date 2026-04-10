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
class UsdExportGeneralSettingsRollup;

namespace Ui {
class ExportDialog;
};

/**
 * \brief USD file export dialog.
 */
class MaxUSDAPI USDExportDialog
    : public QDialog
    , public IUSDExportView
{
public:
    /**
     * \brief Constructor.
     * \param buildOptions Initial build options to use to initialize the UI.
     */
    USDExportDialog(const MaxUsd::IUSDExportOptions& buildOptions);

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
    virtual void setupRollups() = 0;

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

    virtual const QString& GetRollupCategory() = 0;

    /**
     * \brief Get the state (open/close) of all the rollups.
     * \return A map of the rollup names and their state (open/close).
     */
    std::map<QString, bool> GetRollupState() const;

    /**
     * Create and adds a new rollup to the dialog.
     * @param w Widget contained by the rollup.
     * @param open True if the rollup initializes open.
     */
    void addRollup(QWidget* w, bool open = true);

    /**
     * Add a rollup containing UI to configure custom plugin contexts.
     */
    void addContextsRollup();

    /*
     * \brief The dialog's accept behavior
     */
    void accept() override;

    /// Reference to the Qt UI View of the dialog:
    std::unique_ptr<Ui::ExportDialog> ui;
    UsdExportAnimationRollup*         animationRollup;
    UsdExportGeneralSettingsRollup*   generalSettingsRollup;

    /// USD Scene build configuration options:
    MaxUsd::USDSceneBuilderOptions buildOptions;

    std::map<QString, bool> loadedRollupState;

    // Default size of the dialog, needs to be overriden in derived classes.
    int dialogHeight = 0;
    int dialogWidth = 0;

    /// Transform format last selected by the user in the export UI
    MaxUsd::TransformFormat transformFormat;
};

/**
 * \brief USD file export dialog.
 */
class MaxUSDAPI USDExportToFileDialog : public USDExportDialog
{
public:
    /**
     * \brief Constructor.
     * \param buildOptions Initial build options to use to initialize the UI.
     */
    USDExportToFileDialog(const fs::path& filePath, const MaxUsd::IUSDExportOptions& buildOptions);

protected:
    // From USDExportDialog
    void           accept() override;
    void           setupRollups() override;
    const QString& GetRollupCategory() override;

    /// The full path where the USD file will be exported:
    fs::path exportPath;
};

/**
 * \brief USD file export dialog when writing to an existing stage.
 */
class MaxUSDAPI USDExportToStageDialog : public USDExportDialog
{
public:
    /**
     * \brief Constructor.
     * \param buildOptions Initial build options to use to initialize the UI.
     */
    USDExportToStageDialog(
        const MaxUsd::IUSDExportOptions& buildOptions,
        const pxr::VtDictionary&         extraOptions);

    /**
     * Returns the extra options configured (those are options that only apply when exporting to
     * live stages, and are therefor not part of the regular export options).
     * @return
     */
    const pxr::VtDictionary& GetExtraOptions() const;

protected:
    // From USDExportDialog
    void           setupRollups() override;
    const QString& GetRollupCategory() override;
    void           accept() override;

    pxr::VtDictionary extraOptions;
};