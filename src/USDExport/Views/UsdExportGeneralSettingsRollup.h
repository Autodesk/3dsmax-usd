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

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>

#include <QtWidgets/QWidget>

namespace Ui {
class UsdExportGeneralSettingsRollup;
}

class QCheckBox;

class UsdExportGeneralSettingsRollup : public QWidget
{
    Q_OBJECT
public:
    explicit UsdExportGeneralSettingsRollup(
        MaxUsd::USDSceneBuilderOptions& buildOptions,
        QWidget*                        parent = nullptr);
    virtual ~UsdExportGeneralSettingsRollup();

public Q_SLOTS:
    /// Qt callback functions based on named widgets in the associated .ui file
    void on_UpAxisComboBox_currentIndexChanged(int index);

private:
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdExportGeneralSettingsRollup> ui {
        std::make_unique<Ui::UsdExportGeneralSettingsRollup>()
    };

    /// member of USDExportDialog
    MaxUsd::USDSceneBuilderOptions& buildOptions;
};