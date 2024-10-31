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
#include "USDImport.h"

#include "DLLEntry.h"
#include "Views/USDImportDialog.h"

#include <MaxUsd/USDCore.h>
#include <MaxUsd/USDSceneController.h>
#include <MaxUsd/Utilities/OptionUtils.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/UiUtils.h>

#include <maxscript/maxscript.h>
#include <maxscript/util/listener.h>

#include <maxapi.h>

#define ScriptPrint (the_listener->edit_stream->printf)

MaxUsd::IUSDImportOptions USDImporter::uiImportOptions;
bool                      USDImporter::optionDefaultsApplied { false };

int USDImporter::ExtCount() { return 3; }

const MCHAR* USDImporter::Ext(int index)
{
    switch (index) {
    case 0: return _M("usd");
    case 1: return _M("usdc");
    case 2: return _M("usda");
    default: return nullptr;
    }
}

const MCHAR* USDImporter::LongDesc()
{
    static MSTR str = GetMString(IDS_LONGDESCRIPTION);
    return str;
}

const MCHAR* USDImporter::ShortDesc()
{
    static MSTR str = GetMString(IDS_SHORTDESCRIPTION);
    return str;
}

const MCHAR* USDImporter::AuthorName()
{
    static MSTR str = GetMString(IDS_AUTHOR);
    return str;
}

const MCHAR* USDImporter::CopyrightMessage()
{
    static MSTR str = GetMString(IDS_COPYRIGHT);
    return str;
}

unsigned int USDImporter::Version() { return 100; }

void USDImporter::ShowAbout(HWND /*hWnd*/)
{
    // Optional: nothing to do.
}

const MCHAR* USDImporter::OtherMessage1() { return _M(""); }

const MCHAR* USDImporter::OtherMessage2() { return _M(""); }

int USDImporter::DoImport(
    const MCHAR* filePath,
    ImpInterface* /*ei*/,
    Interface* /*ip*/,
    BOOL suppressPrompts)
{
    // make sure to clear cancel flag before/after export
    const auto resetCancelFlag = MaxUsd::MakeScopeGuard(
        []() { GetCOREInterface()->SetCancel(false); },
        []() { GetCOREInterface()->SetCancel(false); });
    // Using the UI (global) options
    if (!optionDefaultsApplied) {
        uiImportOptions.SetOptions(MaxUsd::OptionUtils::LoadImportOptions());
        optionDefaultsApplied = true;
    }
    return ImportFile(filePath, uiImportOptions, suppressPrompts);
}

ClassDesc2* GetUSDImporterDesc()
{
    static USDImporterClassDesc USDImporterDesc;
    return &USDImporterDesc;
}

MaxUsd::IUSDImportOptions& USDImporter::GetUIOptions() { return uiImportOptions; }

void USDImporter::SetUIOptions(const MaxUsd::MaxSceneBuilderOptions& newOptions)
{
    if (!optionDefaultsApplied) {
        optionDefaultsApplied = true;
    }
    uiImportOptions.SetOptions(newOptions);
}

int USDImporter::ImportFile(
    const MCHAR*               filePath,
    MaxUsd::IUSDImportOptions& importOptions,
    bool                       suppressPrompts)
{
    if (filePath == nullptr) {
        return IMPEXP_FAIL;
    }

    auto reportExportConfigError = [](const std::wstring& message) {
        // Report to both in the maxscript listener and the max log.
        ScriptPrint(message.c_str());
        GetCOREInterface()->Log()->LogEntry(
            SYSLOG_ERROR, NO_DIALOG, L"USDImporter Configuration Error", message.c_str());
    };
    auto importFile = USDCore::sanitizedFilename(filePath);
    if (MaxUsd::HasUnicodeCharacter(importFile.string())) {
        const MCHAR* errorMsg = L"USD does not support unicode characters in filepath, please "
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
    if (suppressPrompts || GetCOREInterface()->GetQuietMode()) {
        if (!ValidateImportOptions(importOptions)) {
            return IMPEXP_FAIL;
        }

        int result = MaxUsd::GetUSDSceneController()->Import(
            MaxUsd::UsdStageSource { filePath }, importOptions, filePath);

        return result;
    }

    // pre-build material conversion list
    // possible conflicting Python plugin loader when called from the Qt dialog
    // pxr::MaxUsdShadingModeRegistry::ListMaterialConversions();
    // For now, when importing through UI, we do not populate using the global import options
    // accessible through maxscript.
    std::unique_ptr<IUSDImportView> usdImportDialog
        = std::make_unique<USDImportDialog>(filePath, importOptions);
    if (usdImportDialog->Execute()) {
        const auto optionFromUI = usdImportDialog->GetBuildOptions();
        SetUIOptions(optionFromUI);
        const int result = MaxUsd::GetUSDSceneController()->Import(
            MaxUsd::UsdStageSource { filePath }, optionFromUI, filePath);
        MaxUsd::OptionUtils::SaveImportOptions(uiImportOptions);
        return result;
    }
    return IMPEXP_CANCEL;
}

bool USDImporter::ValidateImportOptions(const MaxUsd::IUSDImportOptions& options)
{
    const auto endTime = options.GetEndTimeCode();
    const auto startTime = options.GetStartTimeCode();

    if (endTime < startTime) {
        const WStr message = L"UsdImporter Error : The end time code can't be smaller than the "
                             L"start time code! \n";
        ScriptPrint(message);
        GetCOREInterface()->Log()->LogEntry(
            SYSLOG_ERROR, NO_DIALOG, L"UsdImporter Configuration Error", message.data());
        return false;
    }
    return true;
}
