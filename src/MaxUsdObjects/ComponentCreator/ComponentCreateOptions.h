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
#include <MaxUsdObjects/MaxUsdObjectsAPI.h>

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>

#include <AdskUsdComponentCreator/Infos/CreatorInfo.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <filesystem>
#include <memory>
#include <string>

namespace MaxUsd::ComponentCreator {

// Persisted inputs for the Create Component Options workflow: the selected template preset name
// and the export options used to build the component's source stage.
struct PersistedCreateOptions
{
    // templateName is used to remember the user's last-selected template
    // (i.e. the exportOptions are shared across all templates)
    std::string                    templateName;
    MaxUsd::USDSceneBuilderOptions exportOptions;
};

// Folder holding user component-option presets (templates); created lazily.
MaxUSDObjectsAPI std::filesystem::path GetComponentTemplatesFolder();

// Write the built-in templates to the templates folder, so they are visible on disk. Must run
// before any preset is loaded from there, or the user's own presets get rewritten over theirs.
MaxUSDObjectsAPI void WriteBuiltInComponentTemplates();

// The Options preset for the given template name; falls back to "ASWF Standard (default)" (baked
// into the component-creator library) and then to library defaults.
MaxUSDObjectsAPI AdskUsdComponentCreator::Options
                 ResolveTemplateOptions(const std::string& templateName);

// Load / save the persisted create options (template name + export options).
MaxUSDObjectsAPI PersistedCreateOptions LoadPersistedCreateOptions();
MaxUSDObjectsAPI void SavePersistedCreateOptions(const PersistedCreateOptions& opts);

// Build a CreatorInfo from a template's options for the dialog's read-only structure/file-structure
// previews. Seeds template tokens (<component name>, <component folder>, ...) so the previews show
// the template structure rather than resolved names.
MaxUSDObjectsAPI AdskUsdComponentCreator::CreatorInfo::Ptr
                 BuildPreviewCreatorInfo(const AdskUsdComponentCreator::Options& templateOptions);

// Open the Create Component Options dialog seeded from the persisted options; on OK, persist the
// selected template + export options. (The "Options..." create-menu entry calls this.)
MaxUSDObjectsAPI void ConfigureCreateComponentOptions();

} // namespace MaxUsd::ComponentCreator
