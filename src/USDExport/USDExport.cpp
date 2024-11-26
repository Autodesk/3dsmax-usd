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

#include "USDExport.h"

#include "DLLEntry.h"
#include "Views/USDExportDialog.h"

#include <MaxUsd/Interfaces/IUSDExportOptions.h>
#include <MaxUsd/USDCore.h>
#include <MaxUsd/USDSceneController.h>
#include <MaxUsd/Utilities/OptionUtils.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/UiUtils.h>

#include <maxscript/maxscript.h>
#include <maxscript/util/listener.h>

#include <memory>

#define ScriptPrint (the_listener->edit_stream->printf)

MaxUsd::IUSDExportOptions USDExporter::uiExportOptions(MaxUsd::OptionUtils::LoadExportOptions());

int USDExporter::ExtCount() { return 3; }

const MCHAR* USDExporter::Ext(int index)
{
    switch (index) {
    case 0: return _M("usd");
    case 1: return _M("usdc");
    case 2: return _M("usda");
    default: return nullptr;
    }
}

const MCHAR* USDExporter::LongDesc() { return GetString(IDS_LONGDESCRIPTION); }

const MCHAR* USDExporter::ShortDesc() { return GetString(IDS_SHORTDESCRIPTION); }

const MCHAR* USDExporter::AuthorName() { return GetString(IDS_AUTHOR); }

const MCHAR* USDExporter::CopyrightMessage() { return GetString(IDS_COPYRIGHT); }

unsigned int USDExporter::Version() { return 100; }

const MCHAR* USDExporter::OtherMessage1() { return _M(""); }

const MCHAR* USDExporter::OtherMessage2() { return _M(""); }

void USDExporter::ShowAbout(HWND /*hWnd*/)
{
    // Optional: nothing to do.
}

BOOL USDExporter::SupportsOptions(int index, DWORD options)
{
    if (index >= 0 && index < ExtCount()) {
        return (options & SCENE_EXPORT_SELECTED) == SCENE_EXPORT_SELECTED;
    }
    return FALSE;
}

int USDExporter::DoExport(
    const MCHAR* filename,
    ExpInterface* /*ei*/,
    Interface* /*ip*/,
    BOOL  suppressPrompts,
    DWORD optionFlags)
{
    MaxUsd::USDSceneBuilderOptions::ContentSource contentSource
        = (optionFlags & SCENE_EXPORT_SELECTED) == SCENE_EXPORT_SELECTED
        ? MaxUsd::USDSceneBuilderOptions::ContentSource::Selection
        : MaxUsd::USDSceneBuilderOptions::ContentSource::RootNode;
    uiExportOptions.SetContentSource(contentSource);

    // Using the global options
    return ExportFile(filename, uiExportOptions, suppressPrompts);
}

MaxUsd::IUSDExportOptions& USDExporter::GetUIOptions() { return uiExportOptions; }

void USDExporter::SetUIOptions(const MaxUsd::USDSceneBuilderOptions& newOptions)
{
    uiExportOptions.SetOptions(newOptions);
}

int USDExporter::ExportFile(
    const MCHAR*               filePath,
    MaxUsd::IUSDExportOptions& exportOptions,
    bool                       suppressPrompts,
    std::string                defaultExt)
{
    // make sure to clear cancel flag before/after export
    const auto resetCancelFlag = MaxUsd::MakeScopeGuard(
        []() { GetCOREInterface()->SetCancel(false); },
        []() { GetCOREInterface()->SetCancel(false); });

    if (filePath != nullptr) {
        auto reportExportConfigError = [](const std::wstring& message) {
            // Report to both in the maxscript listener and the max log.
            ScriptPrint(message.c_str());
            GetCOREInterface()->Log()->LogEntry(
                SYSLOG_ERROR, NO_DIALOG, L"UsdExporter Configuration Error", message.c_str());
        };
        auto exportFile = USDCore::sanitizedFilename(filePath);

        // on UI, max's set the default extension if it's empty
        // when importing via maxscript, it is possible to have a filepath with no extension
        if (exportFile.extension().empty()) {
            // replicate same behavior as when importing in UI
            exportFile.replace_extension(defaultExt);
        }

        if (MaxUsd::HasUnicodeCharacter(exportFile.string())) {
            const MCHAR* errorMsg = L"USD does not support unicode characters in file path, please "
                                    L"remove these characters.";
            if (suppressPrompts) {
                reportExportConfigError(errorMsg);
            } else {
                MaxSDK::MaxMessageBox(
                    GetCOREInterface()->GetMAXHWnd(), errorMsg, _T("Unicode Error"), MB_OK);
            }

            return IMPEXP_FAIL;
        }

        // Avoid displaying blocking UI Dialogs when 3ds Max is running in Quiet Mode:
        if (suppressPrompts) {
            // When running from a script, make sure there is no mismatch between the extension
            // and the file format used.
            const auto ext = exportFile.extension();

            if (exportOptions.GetFileFormat() != MaxUsd::USDSceneBuilderOptions::FileFormat::Binary
                && ext == ".usdc") {
                reportExportConfigError(
                    L"UsdExporter error : #ascii is not a valid file format for the \".usdc\" "
                    L"extension, consider using #binary instead.\n");
                return IMPEXP_FAIL;
            }
            if (exportOptions.GetFileFormat() != MaxUsd::USDSceneBuilderOptions::FileFormat::ASCII
                && ext == ".usda") {
                reportExportConfigError(
                    L"UsdExporter error : #binary is not a valid file format for the \".usda\" "
                    L"extension, consider using #ascii instead.\n");
                return IMPEXP_FAIL;
            }
            if (exportOptions.GetContentSource()
                    != MaxUsd::USDSceneBuilderOptions::ContentSource::NodeList
                && exportOptions.GetNodesToExport().Count() != 0) {
                reportExportConfigError(
                    L"UsdExporter error : argument \"contentSource\" needs to be set to "
                    L"\"#nodeList\" when a \"nodeList\" has been provided.\n");
                return IMPEXP_FAIL;
            }
            if (exportOptions.GetContentSource()
                    == MaxUsd::USDSceneBuilderOptions::ContentSource::NodeList
                && exportOptions.GetNodesToExport().Count() == 0) {
                reportExportConfigError(L"UsdExporter error : argument \"contentSource:#nodeList\" "
                                        L"requires \"nodeList\" to be passed as argument.\n");
                return IMPEXP_FAIL;
            }

            int result = MaxUsd::GetUSDSceneController()->Export(exportFile, exportOptions);

            return result;
        } else {
            std::unique_ptr<IUSDExportView> usdExportDialog
                = std::make_unique<USDExportDialog>(exportFile, exportOptions);
            if (usdExportDialog->Execute()) {
                const auto options = usdExportDialog->GetBuildOptions();
                SetUIOptions(options);

                int result = MaxUsd::GetUSDSceneController()->Export(exportFile, options);
                MaxUsd::OptionUtils::SaveExportOptions(uiExportOptions);
                return result;
            }
            return IMPEXP_CANCEL;
        }
    }
    return IMPEXP_FAIL;
}

ClassDesc2* GetUSDExporterDesc()
{
    static USDExporterClassDesc USDExporterDesc;
    return &USDExporterDesc;
}
