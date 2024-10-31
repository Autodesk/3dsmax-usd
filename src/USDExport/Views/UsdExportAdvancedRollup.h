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

#include <MaxUsd.h>
#include <QtWidgets/QWidget>

namespace MAXUSD_NS_DEF {
class TooltipEventFilter;
}

namespace Ui {
class UsdExportAdvancedRollup;
}

class UsdExportAdvancedRollup : public QWidget
{
    Q_OBJECT
public:
    explicit UsdExportAdvancedRollup(
        MaxUsd::USDSceneBuilderOptions& buildOptions,
        QWidget*                        parent = nullptr);
    virtual ~UsdExportAdvancedRollup();

public Q_SLOTS:
    /// Qt callback functions based on named widgets in the associated .ui file
    void on_LogFileGroupBox_toggled(bool state);
    void on_LogOutputTypeComboBox_currentIndexChanged(int index);
    void on_LogFilePathToolButton_clicked();
    void on_AllowNestedGprimsCheckBox_stateChanged(int state);

private:
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdExportAdvancedRollup> ui {
        std::make_unique<Ui::UsdExportAdvancedRollup>()
    };

    std::unique_ptr<MaxUsd::TooltipEventFilter> tooltipFilter;

    /// member of USDExportDialog
    MaxUsd::USDSceneBuilderOptions& buildOptions;
};