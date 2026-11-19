//
// Copyright 2026 Autodesk
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
#include "ComponentTemplateRollup.h"

#include "ComponentCreateOptions.h"

#include <AdskUsdComponentCreator/Options/KnownOptions.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

ComponentTemplateRollup::ComponentTemplateRollup(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);

    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(tr("Template:"), this));
    templateCombo = new QComboBox(this);
    row->addWidget(templateCombo, 1);
    // Button sits to the right of the dropdown, at its natural width (the combo takes the slack).
    auto* openBtn = new QPushButton(tr("Open Template Location"), this);
    row->addWidget(openBtn);
    layout->addLayout(row);

    // Register any user presets from the templates folder alongside the built-in ones, then list.
    AdskUsdComponentCreator::LoadAllKnownOptions(
        MaxUsd::ComponentCreator::GetComponentTemplatesFolder().string());
    for (const auto& [name, opts] : AdskUsdComponentCreator::GetAllKnownOptions()) {
        templateCombo->addItem(QString::fromStdString(name));
        templateCombo->setItemData(
            templateCombo->count() - 1,
            QString::fromStdString(opts.OptionsDescription()),
            Qt::ToolTipRole);
    }

    connect(templateCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        Q_EMIT templateChanged(text.toStdString());
    });
    connect(openBtn, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(
            MaxUsd::ComponentCreator::GetComponentTemplatesFolder().string())));
    });
}

std::string ComponentTemplateRollup::GetSelectedTemplateName() const
{
    return templateCombo->currentText().toStdString();
}

void ComponentTemplateRollup::SetSelectedTemplateName(const std::string& name)
{
    templateCombo->setCurrentText(QString::fromStdString(name));
}
