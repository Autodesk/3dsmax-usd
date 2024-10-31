//
// Copyright 2023 Autodesk
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
#include "USDSceneController.h"

#include "Builders/MaxSceneBuilder.h"
#include "USDCore.h"
#include "Utilities/DiagnosticDelegate.h"
#include "Utilities/Logging.h"
#include "Utilities/MaxProgressBar.h"
#include "Utilities/PluginUtils.h"
#include "Utilities/ScopeGuard.h"
#include "Utilities/TranslationUtils.h"
#include "Utilities/TypeUtils.h"
#include "Utilities/UsdToolsUtils.h"

#include <pxr/usd/sdf/copyUtils.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/usdFileFormat.h>
#include <pxr/usd/usd/usdaFileFormat.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdUtils/dependencies.h>

#include <IPathConfigMgr.h>
#include <impexp.h>
#include <string>

using MaxUsd::ToMax;
using MaxUsd::ToUsd;

namespace MAXUSD_NS_DEF {

USDSceneController::USDSceneController() { }

USDSceneController::~USDSceneController() { }

int USDSceneController::Import(
    const MaxUsd::UsdStageSource& stageSource,
    const MaxSceneBuilderOptions& buildOptions,
    const fs::path&               filename)
{
    MaxUsd::Log::Session importLog("USDImport", buildOptions.GetLogOptions());
    MaxUsd::Log::Info("Starting import of {} ", stageSource.ToString());

    const auto diagnosticDelegate
        = MaxUsd::Diagnostics::ScopedDelegate::Create<MaxUsd::Diagnostics::LogDelegate>();

    const auto stage = stageSource.LoadStage(buildOptions);
    if (!stage) {
        MaxUsd::Log::Error("Unable to load the USD stage from {} ", stageSource.ToString());
        return IMPEXP_FAIL;
    }
    // Disable a few things that may interfere with the import...
    const auto prevAutoKey = GetCOREInterface12()->GetAutoKeyDefaultKeyOn();
    const auto prevAutoTime = GetCOREInterface12()->GetAutoKeyDefaultKeyTime();
    const auto importScopeGuard = MaxUsd::MakeScopeGuard(
        []() {
            GetCOREInterface17()->DisableSceneRedraw();
            theHold.Suspend();
            GetCOREInterface12()->SetAutoKeyDefaultKeyOn(false);
            // Even with auto-key off, having the auto-key time non-zero is a source of trouble when
            // importing animations.
            GetCOREInterface12()->SetAutoKeyDefaultKeyTime(0);
        },
        [&prevAutoKey, &prevAutoTime]() {
            GetCOREInterface17()->EnableSceneRedraw();
            theHold.Resume();
            GetCOREInterface12()->SetAutoKeyDefaultKeyOn(prevAutoKey);
            GetCOREInterface12()->SetAutoKeyDefaultKeyTime(prevAutoTime);
        });

    // Builder to translate content from USD to 3ds Max:
    MaxSceneBuilder    maxSceneBuilder;
    auto               options = buildOptions.OptionsWithAppliedContexts();
    const pxr::UsdPrim prim = stage->GetPseudoRoot();
    int                importStatus
        = maxSceneBuilder.Build(GetCOREInterface()->GetRootNode(), prim, options, filename);

    if (importStatus == IMPEXP_SUCCESS) {
        MaxUsd::Log::Info(L"Import completed.");
    }

    return importStatus;
}

int USDSceneController::Export(const fs::path& filePath, const USDSceneBuilderOptions& buildOptions)
{
    // Starting new USD export
    MaxUsd::Log::Session exportLog("USDExport", buildOptions.GetLogOptions());
    MaxUsd::Log::Info("Starting export to {} ", filePath.u8string());

    auto oldPath = fs::current_path();
    auto exportFolderPath = filePath.parent_path();
    // Make sure to create this working directory. Otherwise current_path() will fail.
    fs::create_directories(exportFolderPath);
    // Suspend problematic activities (scene editing and scene redraw) while exporting. Also suspend
    // the hold (undo/redo system). Some modifiers may create temporary nodes while they are being
    // edited, these should not be considered.
    const auto exportScopeGuard = MaxUsd::MakeScopeGuard(
        [&exportFolderPath]() {
            GetCOREInterface17()->SuspendEditing();
            GetCOREInterface17()->DisableSceneRedraw();
            theHold.Suspend();
            fs::current_path(exportFolderPath);
        },
        [&oldPath]() {
            GetCOREInterface17()->ResumeEditing();
            GetCOREInterface17()->EnableSceneRedraw();
            theHold.Resume();
            fs::current_path(oldPath);
        });

    const auto diagnosticDelegate
        = MaxUsd::Diagnostics::ScopedDelegate::Create<MaxUsd::Diagnostics::LogDelegate>();
    USDSceneBuilder usdSceneBuilder;

    auto options = buildOptions.OptionsWithAppliedContexts();
    auto actualRootPath = options.GetRootPrimPath();

    // Default behavior, use the file stem as root prim path name.
    if (actualRootPath.IsEmpty()) {
        actualRootPath = pxr::SdfPath("/").AppendPath(
            pxr::SdfPath(pxr::TfMakeValidIdentifier(filePath.stem().string())));
    }
    options.SetRootPrimPath(actualRootPath);

    fs::path    exportStageFilePath = filePath;
    std::string stageExportExtension = filePath.extension().string();
    bool        isUSDZExport = false;

    // older max versions will provide upper case filename extensions
    // this causes issues with SdfFileFormat api, so we force lowercase instead
    std::transform(
        stageExportExtension.begin(),
        stageExportExtension.end(),
        stageExportExtension.begin(),
        [](unsigned char c) { return std::tolower(c); });

    if (stageExportExtension == ".usdz") {
        isUSDZExport = true;
    }

    if (isUSDZExport) {
        // first export stage to .usd in #temp folder, then convert to usdz.
        stageExportExtension = "usd";

        exportStageFilePath = fs::path(MaxUsd::MaxStringToUsdString(
            MaxSDKSupport::GetString(IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_TEMP_DIR))));
        exportStageFilePath /= MaxUsd::GenerateGUID();
        exportStageFilePath /= filePath.filename();
        exportStageFilePath.replace_extension(".usd");
    }

    bool                                       isCancelled = false;
    std::map<std::string, pxr::SdfLayerRefPtr> editedLayers;
    pxr::UsdStageRefPtr                        exportStage = usdSceneBuilder.Build(
        options, isCancelled, exportStageFilePath, editedLayers, isUSDZExport);
    if (isCancelled) {
        return IMPEXP_CANCEL;
    } else if (!exportStage) {
        return IMPEXP_FAIL;
    }

    // Only use the format specified in the options if it is not explicit from the extension.
    // When exporting via scripting, if there is a mismatch between the option and the extension,
    // an exception is raised. When exporting via the UI, it is only possible to specify the
    // format if not already inferred from the extension.
    auto sdfFileFormat = pxr::SdfFileFormat::FindByExtension(stageExportExtension);
    if (sdfFileFormat == nullptr) {
        MaxUsd::Log::Error("Failed to find SdfFileFormat for extension {}", stageExportExtension);
        return IMPEXP_FAIL;
    }
    auto formatId = sdfFileFormat->GetFormatId();
    if (formatId == pxr::UsdUsdFileFormatTokens->Id) {
        if (options.GetFileFormat() == USDSceneBuilderOptions::FileFormat::ASCII && !isUSDZExport) {
            formatId = pxr::UsdUsdaFileFormatTokens->Id;
        } else {
            formatId = pxr::UsdUsdcFileFormatTokens->Id;
        }
    }

    pxr::SdfLayer::FileFormatArguments fileFormatArguments {
        { pxr::UsdUsdFileFormatTokens->FormatArg, formatId }
    };
    std::string exportStageFilePathStr = exportStageFilePath.u8string();
    if (MaxUsd::HasUnicodeCharacter(exportStageFilePathStr)) {
        MaxUsd::Log::Error(
            "Failed to export to usdz as max's `getDir #temp` has unicode characters in its file "
            "path: {0}",
            exportStageFilePathStr);
        return IMPEXP_FAIL;
    }

    auto rootLayer = exportStage->GetRootLayer();
    auto subLayerPaths = rootLayer->GetSubLayerPaths();

    pxr::VtDictionary customLayerData;
    customLayerData[pxr::MaxUsdMetadataTokens->creator]
        = "USD for Autodesk 3ds Max: " + MaxUsd::GetPluginDisplayVersion();

    for (const auto& editedLayer : editedLayers) {
        auto layer = editedLayer.second; // pxr::SdfLayer::FindOrOpen(editedLayer);
        // Add custom layer metadata
        layer->SetCustomLayerData(customLayerData);
        if (!layer->Export(editedLayer.first)) {
            MaxUsd::Log::Error("Failed to export layer to {}", layer->GetIdentifier());
            return IMPEXP_FAIL;
        }
        // Replace the sublayer path with a relative path to the exported stage file
        if (subLayerPaths.Find(layer->GetIdentifier()) != static_cast<size_t>(-1)) {
#if MAX_VERSION_MAJOR < 26
            auto resultStr
                = USDCore::relativePath(editedLayer.first, exportStageFilePath.parent_path());
#else
            auto resultStr
                = relative(editedLayer.first, exportStageFilePath.parent_path()).string();
#endif
            std::replace(resultStr.begin(), resultStr.end(), '\\', '/');
            subLayerPaths.Replace(layer->GetIdentifier(), resultStr);
        }
    }

    rootLayer->SetCustomLayerData(customLayerData);

    if (!rootLayer->Export(exportStageFilePathStr, "", fileFormatArguments)) {
        MaxUsd::Log::Error("Failed to export stage to {}", exportStageFilePathStr);
        return IMPEXP_FAIL;
    }

    if (isUSDZExport) {
        MaxUsd::Log::Info(L"Converting exported stage to USDZ");

        if (!MaxUsd::UsdToolsUtils::RunUsdZip(filePath, exportStageFilePath)) {
            MaxUsd::Log::Error("Failed to write usdz file {}", filePath.string());
            return IMPEXP_FAIL;
        }
        MaxUsd::Log::Info("Added {} to {}", exportStageFilePathStr, filePath.string());

        // remove the temp folder along with the exported stage file
        if (!fs::remove_all(exportStageFilePath.parent_path())) {
            MaxUsd::Log::Error(
                "Failed to remove temp exported stage path {}",
                exportStageFilePath.parent_path().u8string());
            return IMPEXP_FAIL;
        }
    }

    MaxUsd::Log::Info("Export completed.");

    // If requested, open the file in Usdview.
    if (options.GetOpenInUsdview()) {
        if (!MaxUsd::UsdToolsUtils::OpenInUsdView(filePath)) {
            MaxUsd::Log::Error("Failed to open {} in Usdview.", filePath.string());
        }
    }
    return IMPEXP_SUCCESS;
}

MaxUSDAPI USDSceneController* GetUSDSceneController()
{
    static USDSceneController controller;
    return &controller;
}

} // namespace MAXUSD_NS_DEF