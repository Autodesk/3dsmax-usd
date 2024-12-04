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
#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <QtWidgets/QWidget>

namespace Ui {
class UsdExportMaterialsRollup;
}

class QCheckBox;

class UsdExportMaterialsRollup : public QWidget
{
    Q_OBJECT
public:
    explicit UsdExportMaterialsRollup(
        MaxUsd::USDSceneBuilderOptions& buildOptions,
        QWidget*                        parent = nullptr);
    virtual ~UsdExportMaterialsRollup();

public Q_SLOTS:
    /// Qt callback functions based on named widgets in the associated .ui file
    void on_ExportMaterialsGroupBox_toggled(bool state);
#ifdef IS_MAX2024_OR_GREATER
    void OnMaterialSwitcherExportStyleComboBoxChanged(int index);
#endif

    /// Rebuild the material conversion set when toggling the material type options.
    void OnMaterialConversionStateChanged(bool state);

    /*
     * \brief Callback executed when the checkbox to separate materials into a separate layer is clicked.
     */
    void OnSeparateMaterialChanged(bool checked);

    /*
     * \brief Callback executed when the LineEdit to define the default Prim path for Materials has been committed.
     */
    void OnMaterialPrimPathChanged();

    /*
     * \brief Callback executed when the LineEdit to define the Layer file path has been edited.
     */
    void OnMaterialLayerPathChanged();
    /*
     * \brief Callback executed when the Tool Button to pick a file for the material layer has been clicked.
     */
    void OnMaterialLayerClicked();

private:
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdExportMaterialsRollup> ui {
        std::make_unique<Ui::UsdExportMaterialsRollup>()
    };

    /// Material Conversion options
    std::map<QCheckBox*, pxr::TfToken> materialConversionMap;

    /// member of USDExportDialog
    MaxUsd::USDSceneBuilderOptions& buildOptions;
};