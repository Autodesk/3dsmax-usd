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

#include "USDImportDialog.h"

#include <MaxUsd/Builders/JobContextRegistry.h>
#include <MaxUsd/Utilities/DiagnosticDelegate.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/PluginUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/UiUtils.h>
#include <MaxUsd/Widgets/DiagnosticMessagesModelFactory.h>
#include <MaxUsd/Widgets/TreeModelFactory.h>

#include <Qt/QmaxMainWindow.h>
#include <Qt/QmaxToolClips.h>

#include <IPathConfigMgr.h>
#include <QtGui/qevent.h>
#include <helpsys.h>
#include <maxapi.h>
#include <qfiledialog.h>
#include <qpushbutton.h>
#include <qwhatsthis.h>

#define idh_usd_import _T("idh_usd_import")

USDImportDialog::USDImportDialog(
    const fs::path&                       filename,
    const MaxUsd::MaxSceneBuilderOptions& buildOptions,
    QWidget*                              parent)
    : QDialog { parent }
    , buildOptions(buildOptions)
{
    setWindowFlags(windowFlags() | Qt::WindowContextHelpButtonHint);
    ui->setupUi(this);
    setParent(GetCOREInterface()->GetQmaxMainWindow(), windowFlags());

    QPixmap headerIcon = style()->standardPixmap(QStyle::SP_MessageBoxInformation);
    ui->left_label->setPixmap(headerIcon);

    ui->version_label->setText(QString::fromStdString(MaxUsd::GetPluginDisplayVersion()));

    const std::string stageFilepath = filename.u8string();

    const auto filePath = QString::fromStdString(stageFilepath);
    ui->path->setText(filePath);
    ui->path->setToolTip(filePath);

    {
        // Attach the Diagnostic Message delegate to USD only in the context of the initial call to
        // "Open()", since the goal is only to inform the User about contextual information that may
        // be relevant at that moment:
        const auto diagnosticDelegate
            = MaxUsd::Diagnostics::ScopedDelegate::Create<MaxUsd::Diagnostics::LogDelegate>(
                true /* buffered */);

        stage = CreateUSDStage(stageFilepath);
        // If any Diagnostic Messages where emitted by USD, display them in a scrollable list for
        // the User to review:
        if (diagnosticDelegate.HasMessages()) {
            QDiagnosticMessagesModel
                = MaxUsd::DiagnosticMessagesModelFactory::CreateFromMessageList(
                    diagnosticDelegate.GetDiagnosticMessages(), this);
            ui->diagnosticMessagesView->setModel(QDiagnosticMessagesModel.get());
            ui->diagnosticMessagesView->horizontalHeader()->setSectionResizeMode(
                MaxUsd::QDiagnosticMessagesModel::TYPE, QHeaderView::ResizeMode::ResizeToContents);
        } else {
            ui->usdDiagnosticsWidget->setVisible(false);
        }
    }

    // Animation block
    SetAnimationConfiguration();

    connect(
        ui->startFrameSpinBox,
        static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
        this,
        &USDImportDialog::OnStartTimeCodeValueChanged);
    connect(
        ui->endFrameSpinBox,
        static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
        this,
        &USDImportDialog::OnEndTimeCodeValueChanged);
    connect(
        ui->endFrameCheckBox,
        &QCheckBox::stateChanged,
        this,
        &USDImportDialog::OnEndFrameCheckBoxStateChanged);

    // scene content block
    ui->importMaterialsCheckbox->setChecked(buildOptions.GetTranslateMaterials());
    QObject::connect(
        ui->importMaterialsCheckbox,
        &QCheckBox::stateChanged,
        this,
        &USDImportDialog::OnTranslateMaterialsStateChanged);

    // These calls must come after the UI is initialized via "setupUi()":
    treeModel = MaxUsd::TreeModelFactory::CreateFromStage(stage, this);
    proxyModel = std::make_unique<QSortFilterProxyModel>(this);

    // Configure the TreeView of the dialog:
    proxyModel->setSourceModel(treeModel.get());

    proxyModel->setDynamicSortFilter(false);
    proxyModel->setFilterCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
    ui->treeView->setModel(proxyModel.get());
    ui->treeView->expandToDepth(3);

    QHeaderView* treeHeader = ui->treeView->header();

    // Use the same width for the first column of the TreeView as the width of the "filter" text box
    // above it:
    treeHeader->resizeSection(0, ui->filterLineEdit->size().width());

    // Configure the "Path" column to be the one that stretches to accommodate sufficient space for
    // content:
    treeHeader->setStretchLastSection(false);
    treeHeader->setSectionResizeMode(
        MaxUsd::QTreeModel::TREE_COLUMNS::PATH, QHeaderView::ResizeMode::Stretch);

    treeHeader->setToolTip(QApplication::translate(
        "ImportDialog",
        "Select a prim for import. All prims descending from the selected prim are imported into "
        "your scene.",
        nullptr));

    connect(
        ui->filterLineEdit, &QLineEdit::textChanged, this, &USDImportDialog::OnSearchFilterChanged);
    connect(
        ui->treeView->selectionModel(),
        &QItemSelectionModel::selectionChanged,
        this,
        &USDImportDialog::OnTreeViewSelectionChanged);

    // Select the first row by default:
    ui->treeView->setCurrentIndex(proxyModel->index(0, 0));

    // Create the Spinner overlay on top of the TreeView, once it is configured:
    overlay = std::make_unique<MaxUsd::QSpinnerOverlayWidget>(ui->treeView);

    const MaxUsd::Log::Options& logOptions = buildOptions.GetLogOptions();
    QObject::connect(
        ui->logDataLevelCombo,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this,
        &USDImportDialog::OnLogDataLevelStateChanged);
    QObject::connect(
        ui->logPathToolButton,
        &QToolButton::clicked,
        this,
        &USDImportDialog::OnLogPathBrowseClicked);
    ui->logDataLevelCombo->setCurrentIndex(static_cast<int>(logOptions.level));

    const auto logPath = QString::fromStdString(logOptions.path.u8string());
    ui->logPathLineEdit->setText(logPath);
    ui->logPathLineEdit->setToolTip(logPath);

    ToggleLogUI();

    const float dpiScale = MaxSDK::GetUIScaleFactor();
    this->resize(
        static_cast<int>(this->geometry().width() * dpiScale),
        static_cast<int>(this->geometry().height() * dpiScale));

    // contexts / chasers
    const auto& currentContexts = buildOptions.GetContextNames();
    auto        contexts = pxr::MaxUsdJobContextRegistry::ListJobContexts();

    // filters out the export contexts
    contexts.erase(
        std::remove_if(
            contexts.begin(),
            contexts.end(),
            [](pxr::TfToken& c) {
                return !pxr::MaxUsdJobContextRegistry::GetJobContextInfo(c).importEnablerCallback;
            }),
        contexts.end());

    if (!contexts.empty()) {
        // sort the registered import chasers alphabetically
        std::sort(
            contexts.begin(), contexts.end(), [](const pxr::TfToken& a, const pxr::TfToken& b) {
                return pxr::MaxUsdJobContextRegistry::GetJobContextInfo(a).niceName
                    < pxr::MaxUsdJobContextRegistry::GetJobContextInfo(b).niceName;
            });

        auto layout = new QVBoxLayout(ui->pluginConfigurationGroupBox);

        int offset = MaxSDK::UIScaled(3);
        int largeOffset = MaxSDK::UIScaled(6);
        layout->setContentsMargins(largeOffset, offset, offset, MaxSDK::UIScaled(4));
        layout->setSpacing(MaxSDK::UIScaled(2));

        bool first_context = true;

        for (const auto& context : contexts) {
            const auto& contextInfo = pxr::MaxUsdJobContextRegistry::GetJobContextInfo(context);

            auto        jobContext = contextInfo.jobContext;
            std::string contextName = jobContext.GetString();
            std::string contextNiceName = contextInfo.niceName.GetString();

            auto contextLayout = new QHBoxLayout();
            contextLayout->setContentsMargins(0, 0, 0, 0);

            auto contextCheckBox = new QCheckBox(QString::fromStdString(contextNiceName));
            contextCheckBox->setObjectName(
                QString("Enable_Context_%1").arg(QString::fromStdString(contextName)));
            contextCheckBox->setToolTip(contextInfo.exportDescription.GetString().c_str());
            contextCheckBox->setChecked(currentContexts.find(context) != currentContexts.end());
            contextLayout->addWidget(contextCheckBox, 1);

            if (contextInfo.importOptionsCallback != nullptr) {
                auto contextOptionsBtn = new QPushButton(tr("Options"));
                contextOptionsBtn->setObjectName(
                    QString("Options_Context_%1").arg(QString::fromStdString(contextName)));
                contextLayout->addWidget(contextOptionsBtn, 0);

                contextOptionsBtn->setEnabled(contextCheckBox->isChecked());
                connect(
                    contextCheckBox,
                    &QAbstractButton::clicked,
                    contextOptionsBtn,
                    &QWidget::setEnabled);

                auto optionsCallback = contextInfo.importOptionsCallback;
                connect(
                    contextOptionsBtn,
                    &QPushButton::clicked,
                    this,
                    [this, jobContext, contextName, optionsCallback]() {
                        pxr::VtDictionary contextOptions = optionsCallback(
                            contextName, this, this->buildOptions.GetJobContextOptions(jobContext));
                        this->buildOptions.SetJobContextOptions(jobContext, contextOptions);
                    });
            }

            connect(
                contextCheckBox,
                &QAbstractButton::clicked,
                this,
                [this, contextName](bool checked) {
                    std::set<std::string> contextNames = this->buildOptions.GetContextNames();
                    if (checked) {
                        auto result = contextNames.insert(contextName);
                        DbgAssert(result.second);
                    } else {
                        auto result = contextNames.erase(contextName);
                        DbgAssert(result == 1u);
                    }
                    this->buildOptions.SetContextNames(contextNames);
                });

            if (first_context) {
                first_context = false;
            } else {
                auto separator = new QFrame();
                separator->setFrameStyle(QFrame::HLine | QFrame::Sunken);
                layout->addWidget(separator);
            }

            layout->addLayout(contextLayout);
        }
    } else {
        ui->pluginConfigurationGroupBox->hide();
    }

    // 3dsMax toolclips do not behave so well (linger and do not disappear or move with the dialog).
    // Disable until these issues are fixed.
    MaxUsd::Ui::DisableMaxToolClipsRecursively(this);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool USDImportDialog::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
#else
bool USDImportDialog::nativeEvent(const QByteArray& eventType, void* message, long* result)
#endif
{
    MSG* msg = static_cast<MSG*>(message); // safe to do because we are on windows only
    if (msg->message == WM_HELP) {
        MaxSDK::IHelpSystem::GetInstance()->ShowProductHelpForTopic(idh_usd_import);
        *result = TRUE;
        return true;
    }
    return false;
}

bool USDImportDialog::event(QEvent* ev)
{
    if (ev->type() == QEvent::EnterWhatsThisMode) {
        // We need to leave immediately the "What's this" mode, otherwise
        // system is waiting for a click on particular widget
        QWhatsThis::leaveWhatsThisMode();
        // Open a new web page containing help about usd component import
        MaxSDK::IHelpSystem::GetInstance()->ShowProductHelpForTopic(idh_usd_import);
        return true;
    }
    return __super::event(ev);
}

void USDImportDialog::showEvent(QShowEvent* ev)
{
    // the import button is  the one with focus on show dialog
    // if the focus has not changed when pressing 'enter' the import is launched
    ui->buttons->button(QDialogButtonBox::StandardButton::Ok)->setFocus(Qt::NoFocusReason);
}

void USDImportDialog::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        // ONLY if the import button is still the one with focus
        // when pressing 'enter' the import is launched
        QWidget* w = focusWidget();
        if (w != ui->buttons->button(QDialogButtonBox::StandardButton::Ok))
            return;
    } break;

    default: QDialog::keyPressEvent(e);
    }
}

bool USDImportDialog::Execute() { return exec() == QDialog::Accepted; }

MaxUsd::MaxSceneBuilderOptions USDImportDialog::GetBuildOptions() const { return buildOptions; }

pxr::UsdStageRefPtr USDImportDialog::CreateUSDStage(const fs::path& filename)
{
    pxr::UsdStageRefPtr stage
        = pxr::UsdStage::Open(filename.u8string(), pxr::UsdStage::InitialLoadSet::LoadNone);
    if (stage == nullptr) {
        stage = pxr::UsdStage::CreateInMemory();
    }
    return stage;
}

void USDImportDialog::OnSearchFilterChanged(const QString& searchFilter)
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
    searchTimer->start(static_cast<int>(std::chrono::milliseconds(125).count()));

    // Create a thread to perform a search for the given criterias in the background in order to
    // maintain a responsive UI that continues accepting input from the User:
    searchThread = std::make_unique<MaxUsd::USDSearchThread>(stage, searchFilter.toStdString());
    connect(
        searchThread.get(), &MaxUsd::USDSearchThread::finished, searchThread.get(), [&, this]() {
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
            ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
            if (searchYieledResults) {
                overlay->Hide();
            } else {
                overlay->ShowInformationMessage(tr("Your search did not match any Prim."));
            }
        });
    searchThread->start(QThread::Priority::TimeCriticalPriority);
}

void USDImportDialog::OnTreeViewSelectionChanged(
    const QItemSelection& selectedItems,
    const QItemSelection& deselectedItems)
{
    // Ensure that items cannot be deselected from the TreeView, to avoid being in a state where no
    // item of the hierarchy from which to import is selected.
    //
    // Note that Qt does not trigger "selectionChanged" signals when changing selection from within
    // the propagation chain, so this will not cause an infinite callback loop.
    QItemSelectionModel* selectionModel = ui->treeView->selectionModel();
    if (selectionModel != nullptr) {
        bool selectionIsEmpty = selectionModel->selection().isEmpty();
        if (selectionIsEmpty) {
            selectionModel->select(deselectedItems, QItemSelectionModel::Select);
        } else {
            pxr::UsdStagePopulationMask stagePopulationMask;
            std::vector<pxr::SdfPath>   maskPaths;

            for (const auto& selectedPathIndex :
                 selectionModel->selectedRows(MaxUsd::QTreeModel::TREE_COLUMNS::PATH)) {
                QStandardItem* item
                    = treeModel->itemFromIndex(proxyModel->mapToSource(selectedPathIndex));
                if (item != nullptr) {
                    QVariant pathData = item->data(Qt::DisplayRole);
                    if (pathData.isValid() && pathData.canConvert<QString>()) {
                        maskPaths.push_back(pxr::SdfPath(pathData.toString().toStdString()));
                    }
                }
            }
            buildOptions.SetStageMaskPaths(maskPaths);
        }

        // Make sure the "Import" button is disabled if no item of the Tree is selected:
        ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(!selectionIsEmpty);
    }
}

void USDImportDialog::OnStartTimeCodeValueChanged(double value)
{
    buildOptions.SetStartTimeCode(value);

    ui->endFrameSpinBox->setMinimum(value);
}

void USDImportDialog::OnEndTimeCodeValueChanged(double value)
{
    buildOptions.SetEndTimeCode(value);
    if (!ui->endFrameSpinBox->isEnabled()) {
        ui->endFrameSpinBox->clear();
    }
}

void USDImportDialog::OnEndFrameCheckBoxStateChanged(bool checked)
{
    ui->endFrameSpinBox->setEnabled(checked);

    auto       timeConfig = buildOptions.GetResolvedTimeConfig(stage);
    const auto startTime = timeConfig.GetStartTimeCode();
    if (checked) {
        if (oldEndFrameSpinnerValue < startTime) {
            oldEndFrameSpinnerValue = startTime;
        }
        ui->endFrameSpinBox->setValue(oldEndFrameSpinnerValue);
    } else {
        oldEndFrameSpinnerValue = ui->endFrameSpinBox->value();
        ui->endFrameSpinBox->clear();
        buildOptions.SetEndTimeCode(startTime);
    }
}

void USDImportDialog::OnLogDataLevelStateChanged(int currentIndex)
{
    buildOptions.SetLogLevel(static_cast<MaxUsd::Log::Level>(currentIndex));
    ToggleLogUI();
    ToggleImportButton();
}

void USDImportDialog::OnTranslateMaterialsStateChanged(bool checked)
{
    if (!checked) {
        buildOptions.SetShadingModes(MaxUsd::MaxSceneBuilderOptions::ShadingModes());
    } else {
        buildOptions.SetDefaultShadingModes();
    }
}

void USDImportDialog::OnLogPathBrowseClicked()
{
    const TCHAR* importDir
        = MaxSDKSupport::GetString(IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_EXPORT_DIR));
    QString qDir = QString::fromStdString(MaxUsd::MaxStringToUsdString(importDir));
    QString logfile = QFileDialog::getSaveFileName(
        this, USDImportDialog::tr("Select file to save logs"), qDir, tr("Log (*.txt *.log)"));
    if (!logfile.isEmpty()) {
        ui->logPathLineEdit->setText(logfile);
        ui->logPathLineEdit->setToolTip(logfile);
        buildOptions.SetLogPath(logfile.toStdString());
    }
    ToggleImportButton();
}

void USDImportDialog::ToggleLogUI()
{
    if (buildOptions.GetLogOptions().level == MaxUsd::Log::Level::Off) {
        ui->logPathWidget->setEnabled(false);
    } else {
        ui->logPathWidget->setEnabled(true);
    }
}

void USDImportDialog::ToggleImportButton()
{
    bool enableImportButton = true;
    if (buildOptions.GetLogOptions().level != MaxUsd::Log::Level::Off
        && buildOptions.GetLogOptions().path.empty()) {
        enableImportButton = false;
        ui->logPathLineEdit->setFocus();
    }
    ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(enableImportButton);
}

void USDImportDialog::SetAnimationConfiguration()
{
    ui->startFrameSpinBox->setMinimum(-DBL_MAX);
    ui->startFrameSpinBox->setMaximum(DBL_MAX);
    ui->endFrameSpinBox->setMinimum(-DBL_MAX);
    ui->endFrameSpinBox->setMaximum(DBL_MAX);

    MaxUsd::MaxSceneBuilderOptions::ImportTimeMode initialTimeMode = buildOptions.GetTimeMode();
    buildOptions.SetTimeMode(MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::AllRange);
    const auto timeConfig = buildOptions.GetResolvedTimeConfig(stage);
    if (initialTimeMode == MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::StartTime
        || initialTimeMode == MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::EndTime) {
        buildOptions.SetTimeMode(initialTimeMode);
    }

    const double startTimeCode = timeConfig.GetStartTimeCode();
    double       endTimeCode = timeConfig.GetEndTimeCode();
    // the user could have change the time config from script before opening the ui
    // this switch is to try to maintain their configuration when displaying the window
    switch (buildOptions.GetTimeMode()) {
    case MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::StartTime:
    case MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::EndTime: {
        oldEndFrameSpinnerValue = endTimeCode;
        ui->endFrameCheckBox->setChecked(false);
        break;
    }
    default: {
        break;
    }
    }

    // this shouldn't happen, but case that end time code was not properly set
    if (endTimeCode < startTimeCode) {
        endTimeCode = startTimeCode;
    }

    ui->startFrameSpinBox->setResetValue(startTimeCode);
    if (buildOptions.GetTimeMode() == MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::EndTime) {
        ui->startFrameSpinBox->setValue(endTimeCode);
    } else {
        ui->startFrameSpinBox->setValue(startTimeCode);
    }
    buildOptions.SetStartTimeCode(startTimeCode);

    if (buildOptions.GetTimeMode() == MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::EndTime
        || buildOptions.GetTimeMode()
            == MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::StartTime) {
        ui->endFrameSpinBox->clear();
    } else {
        ui->endFrameSpinBox->setResetValue(endTimeCode);
        ui->endFrameSpinBox->setMinimum(startTimeCode);
        ui->endFrameSpinBox->setValue(endTimeCode);
    }
    buildOptions.SetEndTimeCode(endTimeCode);

    // set the time mode to custom because the user may change things around
    buildOptions.SetTimeMode(MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::CustomRange);
}
