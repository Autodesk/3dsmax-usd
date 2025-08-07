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
#include "UsdExportGeneralSettingsRollup.h"

#include "ui_UsdExportGeneralSettingsRollup.h"

#include <MaxUsd/Translators/ShadingModeRegistry.h>

UsdExportGeneralSettingsRollup::UsdExportGeneralSettingsRollup(
    MaxUsd::USDSceneBuilderOptions& buildOptions,
    QWidget*                        parent /* = nullptr */)
    : ui(new Ui::UsdExportGeneralSettingsRollup)
    , buildOptions(buildOptions)
{
    ui->setupUi(this);

    ui->UpAxisComboBox->setCurrentIndex(
        buildOptions.GetUpAxis() == MaxUsd::USDSceneBuilderOptions::UpAxis::Y ? 0 : 1);
}

UsdExportGeneralSettingsRollup::~UsdExportGeneralSettingsRollup() { }

void UsdExportGeneralSettingsRollup::on_UpAxisComboBox_currentIndexChanged(int index)
{
    switch (index) {
    case 0: buildOptions.SetUpAxis(MaxUsd::USDSceneBuilderOptions::UpAxis::Y); break;
    case 1: buildOptions.SetUpAxis(MaxUsd::USDSceneBuilderOptions::UpAxis::Z); break;
    default: DbgAssert(false && "Invalid USD Up Axis option - this should not be hit!"); break;
    }
}
