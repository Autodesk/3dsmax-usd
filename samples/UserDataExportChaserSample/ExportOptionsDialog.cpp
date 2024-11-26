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

#include "ExportOptionsDialog.h"

#include "UI_ExportOptionsDialog.h"

#include <string.h>
using namespace std::string_literals;

#include <pxr/base/vt/value.h>

ExportOptionsDialog::ExportOptionsDialog(QWidget* parent /* = nullptr */)
    : QDialog(parent)
    , ui(new Ui::ExportOptionsDialog())
{
    ui->setupUi(this);
}

ExportOptionsDialog::~ExportOptionsDialog() = default;

pxr::VtDictionary ExportOptionsDialog::ShowOptionsDialog(
    const std::string&       jobContext,
    QWidget*                 parent,
    const pxr::VtDictionary& options)
{
    auto dlg = new ExportOptionsDialog(parent);
    if (!dlg) {
        return options;
    }

    if (auto v = options.GetValueAtPath({ "Option A"s })) {
        if (v->CanCast<bool>()) {
            dlg->ui->radioButton_Option_A->setChecked(v->Get<bool>());
        }
    }

    if (auto v = options.GetValueAtPath({ "Option B"s })) {
        if (v->CanCast<bool>()) {
            dlg->ui->radioButton_Option_B->setChecked(v->Get<bool>());
        }
    }

    if (auto v = options.GetValueAtPath({ "Greeting"s, "Greet User"s })) {
        if (v->CanCast<bool>()) {
            dlg->ui->groupBox_Greeting->setChecked(v->Get<bool>());
        }
    }

    if (auto v = options.GetValueAtPath({ "Greeting"s, "User Name"s })) {
        if (v->CanCast<std::string>()) {
            dlg->ui->lineEdit_UserName->setText(QString::fromStdString(v->Get<std::string>()));
        }
    }

    if (auto v = options.GetValueAtPath({ "Greeting"s, "Formal"s })) {
        if (v->CanCast<bool>()) {
            dlg->ui->checkBox_FormalGreeting->setChecked(v->Get<bool>());
        }
    }

    if (dlg->exec() != QDialog::Accepted) {
        return options;
    }

    pxr::VtDictionary modified_options = options;
    modified_options.SetValueAtPath(
        { "Option A"s }, pxr::VtValue(dlg->ui->radioButton_Option_A->isChecked()));
    modified_options.SetValueAtPath(
        { "Option B"s }, pxr::VtValue(dlg->ui->radioButton_Option_B->isChecked()));

    modified_options.SetValueAtPath(
        { "Greeting"s, "Greet User"s }, pxr::VtValue(dlg->ui->groupBox_Greeting->isChecked()));
    modified_options.SetValueAtPath(
        { "Greeting"s, "User Name"s },
        pxr::VtValue(dlg->ui->lineEdit_UserName->text().toStdString()));
    modified_options.SetValueAtPath(
        { "Greeting"s, "Formal"s }, pxr::VtValue(dlg->ui->checkBox_FormalGreeting->isChecked()));

    return modified_options;
}
