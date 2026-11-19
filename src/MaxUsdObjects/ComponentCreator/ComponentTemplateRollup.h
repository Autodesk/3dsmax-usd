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
#pragma once

#include <QWidget>
#include <string>

class QComboBox;

// Rollup for the Create Component Options dialog: picks the component template (Options preset)
// and opens the templates folder. Populated from the component-creator known-options presets.
class ComponentTemplateRollup : public QWidget
{
    Q_OBJECT
public:
    explicit ComponentTemplateRollup(QWidget* parent = nullptr);

    std::string GetSelectedTemplateName() const;
    void        SetSelectedTemplateName(const std::string& name);

Q_SIGNALS:
    void templateChanged(const std::string& name);

private:
    QComboBox* templateCombo = nullptr;
};
