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
#include "ComponentCreateOptionsDialog.h"

#include "ComponentCreateOptions.h"
#include "ComponentPreviewRollups.h"
#include "ComponentTemplateRollup.h"

#include <MaxUsd/Interfaces/IUSDExportOptions.h>

#include <QLayout>
#include <QMenu>
#include <QMenuBar>

ComponentCreateOptionsDialog::ComponentCreateOptionsDialog(
    const std::string&                    templateName,
    const MaxUsd::USDSceneBuilderOptions& exportOptions)
    : USDExportDialog(MaxUsd::IUSDExportOptions(exportOptions))
    , initialTemplateName(templateName)
{
    setWindowTitle(tr("Create USD Component Options"));

    dialogHeight = 650;
    dialogWidth = 460;

    // Edit menu: persist the current template + export options, or reset the export options.
    auto* menuBar = new QMenuBar(this);
    auto* editMenu = menuBar->addMenu(tr("Edit"));
    editMenu->addAction(tr("Save Settings"), this, [this] {
        MaxUsd::ComponentCreator::SavePersistedCreateOptions(
            { GetSelectedTemplateName(), GetBuildOptions() });
    });
    editMenu->addAction(tr("Reset Settings"), this, [this] { resetStandardExportRollups(); });
    layout()->setMenuBar(menuBar);
}

void ComponentCreateOptionsDialog::setupRollups()
{
    // Component-specific rollups on top: template picker + read-only previews.
    templateRollup = new ComponentTemplateRollup();
    templateRollup->setWindowTitle(tr("Template"));
    templateRollup->SetSelectedTemplateName(initialTemplateName);
    addRollup(templateRollup, true);

    primHierarchyRollup = new ComponentPrimHierarchyRollup();
    primHierarchyRollup->setWindowTitle(tr("Prim Hierarchy Preview"));
    addRollup(primHierarchyRollup, true);

    fileStructureRollup = new ComponentFileStructureRollup();
    fileStructureRollup->setWindowTitle(tr("File Structure Preview"));
    addRollup(fileStructureRollup, false);

    connect(
        templateRollup,
        &ComponentTemplateRollup::templateChanged,
        this,
        [this](const std::string& name) { refreshPreviews(name); });

    // Standard export-options rollups (constructed inside maxUsd, bound to buildOptions).
    addStandardExportRollups();

    // Hide the export-dialog widgets that don't apply to component creation
    for (const char* name : { "ExportPathGroupBox",
                              "openInUsdViewCheckbox",
                              "separateMaterialLayer",
                              "layerPathLabel",
                              "materialLayerPath",
                              "materialLayerPicker" }) {
        if (auto* w = findChild<QWidget*>(name)) {
            w->setHidden(true);
        }
    }

    // Seed the previews from the initial template selection.
    refreshPreviews(templateRollup->GetSelectedTemplateName());
}

void ComponentCreateOptionsDialog::refreshPreviews(const std::string& templateName)
{
    const auto opts = MaxUsd::ComponentCreator::ResolveTemplateOptions(templateName);
    const auto info = MaxUsd::ComponentCreator::BuildPreviewCreatorInfo(opts);
    primHierarchyRollup->SetFromInfo(info);
    fileStructureRollup->SetFromInfo(info);
}

const QString& ComponentCreateOptionsDialog::GetRollupCategory()
{
    static const QString category = "CreateComponentOptionsDialogRollups";
    return category;
}

std::string ComponentCreateOptionsDialog::GetSelectedTemplateName() const
{
    return templateRollup ? templateRollup->GetSelectedTemplateName() : initialTemplateName;
}
