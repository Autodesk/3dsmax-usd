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

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/Views/USDExportDialog.h>

#include <string>

class ComponentTemplateRollup;
class ComponentPrimHierarchyRollup;
class ComponentFileStructureRollup;

// The Create Component Options dialog. Reuses the export dialog scaffolding and the standard export
// rollups (via USDExportDialog::addStandardExportRollups) and adds a template picker plus read-only
// prim-hierarchy / file-structure previews. Configures options; the controller persists them.
class ComponentCreateOptionsDialog : public USDExportDialog
{
public:
    ComponentCreateOptionsDialog(
        const std::string&                    templateName,
        const MaxUsd::USDSceneBuilderOptions& exportOptions);

    // The template preset name currently selected in the dialog.
    std::string GetSelectedTemplateName() const;

protected:
    void           setupRollups() override;
    const QString& GetRollupCategory() override;

private:
    void refreshPreviews(const std::string& templateName);

    std::string                   initialTemplateName;
    ComponentTemplateRollup*      templateRollup = nullptr;
    ComponentPrimHierarchyRollup* primHierarchyRollup = nullptr;
    ComponentFileStructureRollup* fileStructureRollup = nullptr;
};
