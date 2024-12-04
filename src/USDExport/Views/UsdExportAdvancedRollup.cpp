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
#include "UsdExportAdvancedRollup.h"

#include "ui_UsdExportAdvancedRollup.h"

#include <MaxUsd/Widgets/TooltipEventFilter.h>

#include <Qt/QmaxToolClips.h>

#include <IPathConfigMgr.h>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QToolTip>

UsdExportAdvancedRollup::UsdExportAdvancedRollup(
    MaxUsd::USDSceneBuilderOptions& buildOptions,
    QWidget*                        parent /* = nullptr */)
    : ui(new Ui::UsdExportAdvancedRollup)
    , buildOptions(buildOptions)
{
    ui->setupUi(this);

    const auto& options = buildOptions.GetLogOptions();
    ui->LogFileGroupBox->setChecked(options.level != MaxUsd::Log::Level::Off);
    MaxSDK::QmaxToolClips::disableToolClip(ui->LogFileGroupBox);

    const auto logPath = QString::fromStdString(options.path.u8string());
    ui->LogFilePathLineEdit->setText(logPath);
    // Disable Max tooltips as they do not handle long strings well.
    MaxSDK::QmaxToolClips::disableToolClip(ui->LogFilePathLineEdit);
    ui->LogFilePathLineEdit->setToolTip(logPath);

    ui->AllowNestedGprimsCheckBox->setChecked(buildOptions.GetAllowNestedGprims());

    // filter tootips for the following ui elements
    auto tooltipFilterFunction = [this](QObject* o) { QToolTip::hideText(); };
    tooltipFilter = std::make_unique<MaxUsd::TooltipEventFilter>(tooltipFilterFunction);
    ui->LogFilePathToolButton->installEventFilter(tooltipFilter.get());
}

UsdExportAdvancedRollup::~UsdExportAdvancedRollup() { }

void UsdExportAdvancedRollup::on_LogFileGroupBox_toggled(bool state)
{
    if (state) {
        if (buildOptions.GetLogLevel() == MaxUsd::Log::Level::Off) {
            buildOptions.SetLogLevel(
                static_cast<MaxUsd::Log::Level>(ui->LogOutputTypeComboBox->currentIndex() + 1));
        } else {
            ui->LogOutputTypeComboBox->setCurrentIndex(
                static_cast<int>(buildOptions.GetLogOptions().level) - 1);
        }
    } else {
        buildOptions.SetLogLevel(MaxUsd::Log::Level::Off);
    }

    // disable/enable groupbox content
    ui->LogOutputTypeLabel->setEnabled(state);
    ui->LogOutputTypeComboBox->setEnabled(state);
    ui->LogFilePathLabel->setEnabled(state);
    ui->LogFilePathLineEdit->setEnabled(state);
    ui->LogFilePathToolButton->setEnabled(state);
}

void UsdExportAdvancedRollup::on_LogOutputTypeComboBox_currentIndexChanged(int index)
{
    switch (index) {
    case 0: buildOptions.SetLogLevel(MaxUsd::Log::Level::Error); break;
    case 1: buildOptions.SetLogLevel(MaxUsd::Log::Level::Warn); break;
    case 2: buildOptions.SetLogLevel(MaxUsd::Log::Level::Info); break;
    default:
        DbgAssert(false && "Invalid USD export log output type - this should not be hit!");
        break;
    }
}

void UsdExportAdvancedRollup::on_LogFilePathToolButton_clicked()
{
    const TCHAR* exportDir
        = MaxSDKSupport::GetString(IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_EXPORT_DIR));
    QString qDir = QString::fromStdString(MaxUsd::MaxStringToUsdString(exportDir));
    QString logfile = QFileDialog::getSaveFileName(
        this, tr("Select file to save logs"), qDir, tr("Log (*.txt *.log)"));
    if (!logfile.isEmpty()) {
        ui->LogFilePathLineEdit->setText(logfile);
        ui->LogFilePathLineEdit->setToolTip(logfile);
        buildOptions.SetLogPath(fs::path(logfile.toStdString()));
    }
}

void UsdExportAdvancedRollup::on_AllowNestedGprimsCheckBox_stateChanged(int state)
{
    buildOptions.SetAllowNestedGprims(state == Qt::Checked);
}