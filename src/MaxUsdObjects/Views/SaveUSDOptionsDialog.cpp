//
// Copyright 2024 Autodesk
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
#include "SaveUSDOptionsDialog.h"

#include "ui_SaveUSDOptionsDialog.h"

#include <MaxUsdObjects/LayerEditor/USDLayerManager.h>

#include <max.h>

SaveUSDOptionsDialog::SaveUSDOptionsDialog(QWidget* parent)
{
    ui->setupUi(this);
    setParent(parent, windowFlags());

    buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(ui->saveAllButton);
    buttonGroup->addButton(ui->saveAllEditsMaxFileButton);
    buttonGroup->addButton(ui->saveMaxOnlyButton);

    SaveMode currentSaveMode = USDLayerManager::Instance()->GetSaveMode();
    if (currentSaveMode == SaveMode::SaveAll) {
        ui->saveAllButton->setChecked(true);
    } else if (currentSaveMode == SaveMode::SaveAllEditsMax) {
        ui->saveAllEditsMaxFileButton->setChecked(true);
        ui->importantlabel->setHidden(false);
    } else if (currentSaveMode == SaveMode::Save3dsMaxOnly) {
        ui->saveMaxOnlyButton->setChecked(true);
    }

    connect(ui->saveAllButton, &QRadioButton::toggled, [this](bool checked) {
        if (checked) {
            USDLayerManager::Instance()->SetSaveMode(SaveMode::SaveAll);
        }
    });
    connect(ui->saveAllEditsMaxFileButton, &QRadioButton::toggled, [this](bool checked) {
        if (checked) {
            USDLayerManager::Instance()->SetSaveMode(SaveMode::SaveAllEditsMax);
        }
        ui->importantlabel->setHidden(!checked);
    });
    connect(ui->saveMaxOnlyButton, &QRadioButton::toggled, [this](bool checked) {
        if (checked) {
            USDLayerManager::Instance()->SetSaveMode(SaveMode::Save3dsMaxOnly);
        }
    });

    connect(ui->saveCancel, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->saveCancel, &QDialogButtonBox::rejected, this, &QDialog::reject);

    this->adjustSize();
}