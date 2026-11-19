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
#include "ComponentCreateOptions.h"

#include "ComponentCreateOptionsDialog.h"

#include <MaxUsdObjects/ComponentCreator/Commands/CreateComponentFromNodesCmd.h>

#include <MaxUsd/Utilities/OptionUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/base/vt/dictionary.h>

#include <AdskUsdComponentCreator/Infos/CreatorInfo.h>
#include <AdskUsdComponentCreator/Options/KnownOptions.h>
#include <IPathConfigMgr.h>
#include <system_error>

namespace {
const std::string kCategory = "ComponentCreateOptions";
const std::string kTemplateKey = "templateName";
const std::string kExportKey = "exportOptions";
const char* const kDefaultTemplate = "ASWF Standard (default)";
} // namespace

namespace MaxUsd::ComponentCreator {

std::filesystem::path GetComponentTemplatesFolder()
{
    const MSTR            base = IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_MAXDATA_DIR);
    std::filesystem::path folder = std::filesystem::path(base.data()) / "USD" / "Templates";
    std::error_code       ec;
    std::filesystem::create_directories(folder, ec);
    return folder;
}

void WriteBuiltInComponentTemplates()
{
    AdskUsdComponentCreator::SaveAllKnownOptions(GetComponentTemplatesFolder().string());
}

AdskUsdComponentCreator::Options ResolveTemplateOptions(const std::string& templateName)
{
    namespace cc = AdskUsdComponentCreator;
    // Register any user presets from the templates folder alongside the built-in ones.
    cc::LoadAllKnownOptions(GetComponentTemplatesFolder().string());

    if (auto opts = cc::GetKnownOptions(templateName)) {
        return *opts;
    }
    if (auto def = cc::GetKnownOptions(kDefaultTemplate)) {
        return *def;
    }
    cc::Options fallback;
    fallback.LoadDefaults();
    return fallback;
}

void SavePersistedCreateOptions(const PersistedCreateOptions& opts)
{
    pxr::VtDictionary dict;
    dict[kTemplateKey] = opts.templateName;
    dict[kExportKey] = opts.exportOptions.GetOptions();
    MaxUsd::OptionUtils::SaveUiOptions(kCategory, dict);
}

PersistedCreateOptions LoadPersistedCreateOptions()
{
    PersistedCreateOptions result;
    result.templateName = kDefaultTemplate;

    pxr::VtDictionary dict;
    MaxUsd::OptionUtils::LoadUiOptions(kCategory, dict);

    if (auto it = dict.find(kTemplateKey);
        it != dict.end() && it->second.IsHolding<std::string>()) {
        result.templateName = it->second.Get<std::string>();
    }
    if (auto it = dict.find(kExportKey);
        it != dict.end() && it->second.IsHolding<pxr::VtDictionary>()) {
        result.exportOptions = MaxUsd::USDSceneBuilderOptions(it->second.Get<pxr::VtDictionary>());
    }
    return result;
}

AdskUsdComponentCreator::CreatorInfo::Ptr
BuildPreviewCreatorInfo(const AdskUsdComponentCreator::Options& templateOptions)
{
    AdskUsdComponentCreator::Options opts = templateOptions;
    // Seed template tokens (not resolved values) so the previews show the template structure
    // (e.g. "<component folder>/<component name>") rather than concrete names.
    opts.SetComponentFilename("<component filename>");
    opts.SetComponentFolder("<component folder>");
    opts.SetComponentName("<component name>");
    opts.SetComponentVariants({ { "<variant-set>", "<variant-name>" } });
    return std::make_shared<AdskUsdComponentCreator::CreatorInfo>(opts);
}

void ConfigureCreateComponentOptions()
{
    const PersistedCreateOptions current = LoadPersistedCreateOptions();

    // The base dialog reparents itself to the Max main window, so no explicit parent is needed.
    ComponentCreateOptionsDialog dialog(current.templateName, current.exportOptions);
    if (!dialog.Execute()) {
        return;
    }

    PersistedCreateOptions updated;
    updated.templateName = dialog.GetSelectedTemplateName();
    updated.exportOptions = dialog.GetBuildOptions();
    SavePersistedCreateOptions(updated);

    // Create from the current selection using the just-configured options. Every selected node
    // goes into a single variant; the multi-variant flow is its own menu entry.
    std::vector<INode*> nodes = MaxUsd::RemoveDescendantINodes(MaxUsd::GetSelectedNodes());
    if (nodes.empty()) {
        return;
    }
    const AdskUsdComponentCreator::Options options = ResolveTemplateOptions(updated.templateName);
    CreateComponentFromNodes(nodes, options, updated.exportOptions);
}

} // namespace MaxUsd::ComponentCreator
