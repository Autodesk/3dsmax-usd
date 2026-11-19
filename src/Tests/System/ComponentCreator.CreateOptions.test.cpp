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

#include "ComponentCreatorTestHelpers.h"
#include "TestHelpers.h"

#include <MaxUsdObjects/ComponentCreator/ComponentCreateOptions.h>
#include <MaxUsdObjects/ComponentCreator/UtilComponentCreator.h>

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/Utilities/OptionUtils.h>
#include <MaxUsd/Utilities/ScopeGuard.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <AdskUsdComponentCreator/Infos/CreatorInfo.h>
#include <AdskUsdComponentCreator/Reports.h>
#include <filesystem>
#include <fstream>

using namespace CCTest;
namespace ccutil = MaxUsd::ComponentCreator;

// The source-stage export must always use the NodeList content source rooted at the pseudo-root,
// regardless of the user's export options, while preserving unrelated user settings.
TEST(ComponentCreatorCreateOptions, SourceStageOptionsForceNodeListRoot)
{
    MaxUsd::USDSceneBuilderOptions base;
    base.SetContentSource(MaxUsd::USDSceneBuilderOptions::ContentSource::Selection);
    base.SetUpAxis(MaxUsd::USDSceneBuilderOptions::UpAxis::Z);

    const MaxUsd::USDSceneBuilderOptions merged = ccutil::BuildSourceStageExportOptions(base);

    EXPECT_EQ(merged.GetContentSource(), MaxUsd::USDSceneBuilderOptions::ContentSource::NodeList);
    EXPECT_EQ(merged.GetRootPrimPath(), pxr::SdfPath::AbsoluteRootPath());
    EXPECT_EQ(merged.GetUpAxis(), MaxUsd::USDSceneBuilderOptions::UpAxis::Z)
        << "unrelated user export settings must be preserved";
}

// The export options passed to the create flow must actually reach the node export: a non-default
// up-axis chosen in the options shows up as the source stage's up-axis metadata.
TEST(ComponentCreatorCreateOptions, ExportOptionsReachTheNodeExport)
{
    const auto resetGuard = MaxUsd::MakeScopeGuard([]() {}, []() { QuickReset(); });

    INode* cube = MakeCubeNode(L"OptCube");
    ASSERT_NE(cube, nullptr);

    MaxUsd::USDSceneBuilderOptions exportOpts;
    exportOpts.SetUpAxis(
        MaxUsd::USDSceneBuilderOptions::UpAxis::Y); // non-default (Max default is Z)

    const pxr::UsdStageRefPtr src = ccutil::ExportINodesToSourceStage({ cube }, exportOpts);
    ASSERT_TRUE(src);
    EXPECT_EQ(pxr::UsdGeomGetStageUpAxis(src), pxr::UsdGeomTokens->y)
        << "the up-axis from the passed export options must reach the exported source stage";
}

// An unknown template name resolves to a usable preset (the built-in ASWF Standard fallback).
TEST(ComponentCreatorCreateOptions, ResolveTemplateFallsBackToAswf)
{
    const auto unknown = ccutil::ResolveTemplateOptions("Nope Not A Real Template");
    EXPECT_FALSE(unknown.OptionsName().empty())
        << "an unknown template must fall back to a usable preset (ASWF Standard)";
}

// The persisted create options (template name + export options) survive a save/load round-trip.
TEST(ComponentCreatorCreateOptions, PersistenceRoundTrip)
{
    // The persistence goes through the user's real usdUiSettings.json, so snapshot the file:
    // left behind, the non-default up-axis below becomes the user's component-create default.
    const std::filesystem::path uiSettings
        = MaxUsd::OptionUtils::GetPathToUsdUiSettings().GetCStr();
    std::error_code ec;
    const bool      hadSettings = std::filesystem::exists(uiSettings, ec);
    std::string     previousSettings;
    if (hadSettings) {
        std::ifstream file(uiSettings, std::ios::binary);
        previousSettings.assign(
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    const auto settingsGuard = MaxUsd::MakeScopeGuard([]() {}, [&]() {
        if (hadSettings) {
            std::ofstream file(uiSettings, std::ios::binary | std::ios::trunc);
            file << previousSettings;
        } else {
            std::error_code removeError;
            std::filesystem::remove(uiSettings, removeError);
        }
    });

    ccutil::PersistedCreateOptions in;
    in.templateName = "ASWF Standard (default)";
    in.exportOptions.SetUpAxis(
        MaxUsd::USDSceneBuilderOptions::UpAxis::Y); // non-default (Max default is Z)
    ccutil::SavePersistedCreateOptions(in);

    const auto out = ccutil::LoadPersistedCreateOptions();
    EXPECT_EQ(out.templateName, in.templateName);
    EXPECT_EQ(out.exportOptions.GetUpAxis(), MaxUsd::USDSceneBuilderOptions::UpAxis::Y)
        << "persisted export options must round-trip";
}

// The dialog's read-only previews are driven by the component structure / file-structure reports;
// a CreatorInfo built from a template must yield non-empty reports.
TEST(ComponentCreatorCreateOptions, PreviewReportsNonEmpty)
{
    const auto opts = ccutil::ResolveTemplateOptions("ASWF Standard (default)");
    const auto info = ccutil::BuildPreviewCreatorInfo(opts);
    ASSERT_TRUE(info);

    const auto structReport = AdskUsdComponentCreator::GetComponentStructureReport(info);
    const auto fileReport = AdskUsdComponentCreator::GetComponentFileStructureReport(info);
    ASSERT_TRUE(structReport);
    ASSERT_TRUE(fileReport);
    EXPECT_FALSE(structReport->GetItems().empty())
        << "prim-hierarchy preview report should have rows";
    EXPECT_FALSE(fileReport->GetItems().empty())
        << "file-structure preview report should have rows";
}

// The built-in template must be visible as a file in the templates folder.
TEST(ComponentCreatorCreateOptions, BuiltInTemplateIsWrittenToDisk)
{
    const std::filesystem::path templateFile
        = ccutil::GetComponentTemplatesFolder() / "ASWF Standard (default).json";

    // The built-in is left read-only on disk, which blocks removal until the write bit is back.
    const auto removeTemplateFile = [&templateFile]() {
        std::error_code ec;
        std::filesystem::permissions(
            templateFile,
            std::filesystem::perms::owner_write,
            std::filesystem::perm_options::add,
            ec);
        std::filesystem::remove(templateFile, ec);
    };

    removeTemplateFile();
    ASSERT_FALSE(std::filesystem::exists(templateFile))
        << "could not clear " << templateFile.string() << " before the test";

    const auto cleanupGuard = MaxUsd::MakeScopeGuard([]() {}, removeTemplateFile);

    ccutil::WriteBuiltInComponentTemplates();

    ASSERT_TRUE(std::filesystem::exists(templateFile))
        << "the built-in template must be written to " << templateFile.string();

    std::ifstream     in(templateFile);
    const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(json.find("\"options_name\""), std::string::npos)
        << "the written template must be a serialized options file. Content:\n"
        << json;
}
