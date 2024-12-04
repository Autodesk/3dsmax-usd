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
// 3ds Max SDK
#include "USDExport.h"

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/Interfaces/IUSDExportOptions.h>
#include <MaxUsd/Utilities/OptionUtils.h>

#include <maxscript/maxwrapper/mxsobjects.h>

#include <max.h>

class USDExportInterface : public FPStaticInterface
{
public:
    DECLARE_DESCRIPTOR(USDExportInterface)

    void SetUIOptions(FPInterface* options)
    {
        if (options && options->GetID() == IUSDExportOptions_INTERFACE_ID) {
            MaxUsd::IUSDExportOptions* exportOptions
                = dynamic_cast<MaxUsd::IUSDExportOptions*>(options);
            if (!exportOptions) {
                throw MAXException(L"Invalid export options object.");
            }
            USDExporter::SetUIOptions((*exportOptions));
        }
    }

    FPInterface* GetUIOptions() { return &USDExporter::GetUIOptions(); }

    int ExportFile(
        const MCHAR*       filePath,
        FPInterface*       usdExportOptions,
        int                contentSource,
        const Tab<INode*>* nodesToExport)
    {
        // No filePath, can't export so return export failure
        if (!filePath) {
            return IMPEXP_FAIL;
        }

        MaxUsd::IUSDExportOptions* exportOptions = nullptr;
        int                        res = IMPEXP_FAIL;
        bool                       toDelete = false;

        // Get export options from argument, if no options were passed, then create a new temporary
        // export options config
        if (usdExportOptions && usdExportOptions->GetID() == IUSDExportOptions_INTERFACE_ID) {
            exportOptions = dynamic_cast<MaxUsd::IUSDExportOptions*>(usdExportOptions);
        } else if (!usdExportOptions) {
            exportOptions = new MaxUsd::IUSDExportOptions;
            toDelete = true;
        }

        // Export USD file
        if (exportOptions) {
            exportOptions->SetContentSource(
                MaxUsd::USDSceneBuilderOptions::ContentSource(contentSource));

            // if the nodes list wasn't passed in the argument, nodesToExport can reference a null
            // value
            if (exportOptions->GetContentSource()
                    == MaxUsd::USDSceneBuilderOptions::ContentSource::NodeList
                || (nodesToExport && nodesToExport->Count() > 0)) {
                exportOptions->SetNodesToExport(*nodesToExport);
            }
            res = USDExporter::ExportFile(filePath, (*exportOptions), true, "usd");
        }

        // Clean up if we used a new temporary default export options config
        if (toDelete) {
            delete exportOptions;
            exportOptions = nullptr;
        }

        return res;
    }

    FPInterface* CreateOptions() { return new MaxUsd::IUSDExportOptions; }

    void Log(int messageType, const std::wstring& message)
    {
        MaxUsd::Log::Message(MaxUsd::Log::Level(messageType), message);
    }

    FPInterface* CreateOptionsFromJsonString(const MCHAR* jsonString)
    {
        if (jsonString) {
            MaxUsd::USDSceneBuilderOptions options(MaxUsd::OptionUtils::DeserializeOptionsFromJson(
                QString::fromWCharArray(jsonString).toUtf8()));
            return new MaxUsd::IUSDExportOptions(options);
        }
        throw MAXException(L"Invalid JSON string");
    }

    enum
    {
        eid_ContentSource,
        eid_LogLevel
    };

    enum
    {
        fid_SetUIOptions,
        fid_GetUIOptions,
        fid_ExportFile,
        fid_CreateOptions,
        fid_Log,
        fid_CreateOptionsFromJsonString
    };

    // clang-format off
    BEGIN_FUNCTION_MAP
        PROP_FNS(fid_GetUIOptions, GetUIOptions, fid_SetUIOptions, SetUIOptions, TYPE_INTERFACE);
        FN_4(fid_ExportFile, TYPE_INT, ExportFile, TYPE_STRING, TYPE_INTERFACE, TYPE_ENUM, TYPE_INODE_TAB);
        FN_0(fid_CreateOptions, TYPE_INTERFACE, CreateOptions);
        FN_1(fid_CreateOptionsFromJsonString, TYPE_INTERFACE, CreateOptionsFromJsonString, TYPE_STRING);
        VFN_2(fid_Log, Log, TYPE_ENUM, TYPE_STRING);
    END_FUNCTION_MAP
// clang-format on    
};

#define USDEXPORT_INTERFACE Interface_ID(0x56ae003c, 0x6d122605)

// clang-format off
static USDExportInterface usdExportInterface(
    USDEXPORT_INTERFACE, _T("USDExport"), 0, GetUSDExporterDesc(), 0,
    // Functions
    USDExportInterface::fid_ExportFile, _T("ExportFile"), "Export USD file with custom options.", TYPE_INT, FP_NO_REDRAW, 4,
        _T("filePath"), 0, TYPE_STRING,
        _T("exportOptions"), 0, TYPE_INTERFACE, f_keyArgDefault, NULL,
        _T("contentSource"), 0, TYPE_ENUM, USDExportInterface::eid_ContentSource, f_keyArgDefault, MaxUsd::USDSceneBuilderOptions::ContentSource::RootNode,
        _T("nodeList"), 0, TYPE_INODE_TAB, f_keyArgDefault, NULL,
    USDExportInterface::fid_CreateOptions, _T("CreateOptions"), "Create a new set of export options filled with default values", TYPE_INTERFACE, FP_NO_REDRAW, 0,
    USDExportInterface::fid_CreateOptionsFromJsonString, _T("CreateOptionsFromJson"), "Creates export options from a JSON formatted string.", TYPE_INTERFACE, FP_NO_REDRAW, 1,
        _T("jsonString"), 0, TYPE_STRING,
    USDExportInterface::fid_Log, _T("Log"), "Log info, warning, and error messages to USD export logs from USD export callbacks.", TYPE_VOID, FP_NO_REDRAW, 2,
        _T("logLevel"), 0, TYPE_ENUM, USDExportInterface::eid_LogLevel,
        _T("message"), 0, TYPE_STRING, properties,
    properties,
    USDExportInterface::fid_GetUIOptions, USDExportInterface::fid_SetUIOptions, _T("UIOptions"), 0, TYPE_INTERFACE,
    enums,
    USDExportInterface::eid_ContentSource, 3,
        _T("all"), MaxUsd::USDSceneBuilderOptions::ContentSource::RootNode,
        _T("selected"), MaxUsd::USDSceneBuilderOptions::ContentSource::Selection,
        _T("nodeList"), MaxUsd::USDSceneBuilderOptions::ContentSource::NodeList,
    USDExportInterface::eid_LogLevel, 3,
        _T("info"), MaxUsd::Log::Level::Info,
        _T("warn"), MaxUsd::Log::Level::Warn,
        _T("error"), MaxUsd::Log::Level::Error,
    p_end
);
// clang-format on