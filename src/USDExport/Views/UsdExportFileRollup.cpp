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
#include "UsdExportFileRollup.h"

#include "ui_UsdExportFileRollup.h"

#include <Qt/QmaxToolClips.h>

UsdExportFileRollup::UsdExportFileRollup(
    const fs::path&                 filePath,
    MaxUsd::USDSceneBuilderOptions& buildOptions,
    QWidget*                        parent /* = nullptr */)
    : ui(new Ui::UsdExportFileRollup)
    , buildOptions(buildOptions)
{
    ui->setupUi(this);

    // file name line edit setup
    const auto filenameQstr = QString::fromStdString(filePath.filename().u8string());
    ui->FileNameLineEdit->setText(filenameQstr);
    // Disable Max tooltips as they do not handle long strings well.
    MaxSDK::QmaxToolClips::disableToolClip(ui->FileNameLineEdit);
    ui->FileNameLineEdit->setToolTip(filenameQstr);

    auto HideFileFormatComboBox = [this]() {
        ui->FileFormatComboBox->setDisabled(true);
        ui->FileFormatComboBox->hide();
        ui->horizontalLayout->setStretch(0, 1);
        ui->horizontalLayout->setStretch(1, 2);
        ui->horizontalLayout->setStretch(2, 0);
    };
    // file format initial setup
    const auto fileExtension = filePath.extension();
    if (fileExtension == ".usda") {
        ui->FileFormatComboBox->setCurrentIndex(0);
        HideFileFormatComboBox();
    } else if (fileExtension == ".usdc") {
        ui->FileFormatComboBox->setCurrentIndex(1);
        HideFileFormatComboBox();
    } else if (fileExtension == ".usdz") {
        HideFileFormatComboBox();
    } else {
        // handle .usd extension
        if (buildOptions.GetFileFormat() == MaxUsd::USDSceneBuilderOptions::FileFormat::ASCII) {
            ui->FileFormatComboBox->setCurrentIndex(0);
        } else {
            ui->FileFormatComboBox->setCurrentIndex(1);
        }
    }
}

UsdExportFileRollup::~UsdExportFileRollup() { }

void UsdExportFileRollup::on_FileFormatComboBox_currentIndexChanged(int index)
{
    switch (index) {
    case 0: buildOptions.SetFileFormat(MaxUsd::USDSceneBuilderOptions::FileFormat::ASCII); break;
    case 1: buildOptions.SetFileFormat(MaxUsd::USDSceneBuilderOptions::FileFormat::Binary); break;
    default: DbgAssert(false && "Invalid USD export file format - this should not be hit!"); break;
    }
}
