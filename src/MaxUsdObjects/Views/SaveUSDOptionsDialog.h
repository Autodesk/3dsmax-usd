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
#pragma once

#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>

namespace Ui {
class SaveUSDOptionsDialog;
}

class SaveUSDOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    enum class SaveMode
    {
        SaveAll,
        Save3dsMaxOnly
    };

    explicit SaveUSDOptionsDialog(QWidget* parent = nullptr);

    ~SaveUSDOptionsDialog() = default;

    SaveMode GetSaveMode() const { return saveMode; }

private:
    /// Reference to the Qt UI View of the dialog
    std::unique_ptr<Ui::SaveUSDOptionsDialog> ui { std::make_unique<Ui::SaveUSDOptionsDialog>() };

    QButtonGroup* buttonGroup = nullptr;

    SaveMode saveMode = SaveMode::SaveAll;
};