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
#include "USDZExport.h"

#include "DLLEntry.h"
#include "Views/USDExportDialog.h"

#include <MaxUsd/Interfaces/IUSDExportOptions.h>
#include <MaxUsd/USDSceneController.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <maxscript/maxscript.h>
#include <maxscript/util/listener.h>

#define ScriptPrint (the_listener->edit_stream->printf)

int USDZExporter::ExtCount() { return 1; }

const MCHAR* USDZExporter::Ext(int index) { return (index == 0) ? _M("usdz") : nullptr; }

const MCHAR* USDZExporter::LongDesc() { return GetString(IDS_USDZ_LONGDESCRIPTION); }

const MCHAR* USDZExporter::ShortDesc() { return GetString(IDS_USDZ_SHORTDESCRIPTION); }

int USDZExporter::DoExport(
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
    auto uiExportOptions = GetUIOptions();

    uiExportOptions.SetContentSource(contentSource);

    // Using the global options
    return USDExporter::ExportFile(filename, uiExportOptions, suppressPrompts, "usdz");
}

ClassDesc2* GetUSDZExporterDesc()
{
    static USDZExporterClassDesc USDZExporterDesc;
    return &USDZExporterDesc;
}
