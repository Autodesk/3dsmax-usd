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
#pragma once

#include "resource.h"

#include <MaxUsd/Interfaces/IUSDExportOptions.h>

#include <impexp.h>
#include <iparamb2.h>
#include <maxtypes.h>

/**
 * \brief Class_ID of the USD Exporter plugin.
 */
#define USDExporter_CLASS_ID Class_ID(0x9e90207a, 0x4caca4fe)

/**
 * \brief USD Exporter.
 */
class USDExporter : public SceneExport
{
public:
    /**
     * \brief Return the number of extensions supported.
     * \return The number of extensions supported.
     */
    int ExtCount() override;

    /**
     * \brief Return the representation of the supported extension at the given index (without leading dot).
     * \param index Index of the supported extension for which to return the representation.
     * \return Return the representation of the supported extension at the given index.
     */
    const MCHAR* Ext(int index) override;

    /**
     * \brief Return a long description for the plugin.
     * \return A long description for the plugin.
     */
    const MCHAR* LongDesc() override;

    /**
     * \brief Return a short description for the plugin.
     * \return A short description for the plugin.
     */
    const MCHAR* ShortDesc() override;

    /**
     * \brief Return the name of the author of the plugin.
     * \return The name of the author of the plugin.
     */
    const MCHAR* AuthorName() override;

    /**
     * \brief Return a copyright message for the plugin.
     * \return A copyright message for the plugin.
     */
    const MCHAR* CopyrightMessage() override;

    /**
     * \brief Return the first message to be displayed.
     * \return The first message to be displayed.
     */
    const MCHAR* OtherMessage1() override;

    /**
     * \brief Return the second message to be displayed.
     * \return The second message to be displayed.
     */
    const MCHAR* OtherMessage2() override;

    /**
     * \brief Return the version number of the plugin.
     * \remarks The format is the version number * 100 (i.e. "v3.01" is labeled as "301").
     * \return The version number of the plugin.
     */
    unsigned int Version() override;

    /**
     * \brief Display an "About..." box for the plugin.
     * \param hWnd Parent window handle of the dialog.
     */
    void ShowAbout(HWND hWnd) override;

    /**
     * \brief Inform 3ds Max about export options support for the given extension index.
     * \param index The index of the extension being queried for.
     * \param options Bitmask indicating which export option is being queried.
     * \return "TRUE" if the given extension supports the given export options, "FALSE" otherwise.
     */
    BOOL SupportsOptions(int index, DWORD options) override;

    /**
     * \brief Perform the file export.
     * \param filename Filename of the exported file.
     * \param ei Pointer which may be used to call methods to enumerate the scene.
     * \param i Interface pointer to be used to call 3ds Max methods.
     * \param suppressPrompts Flag indicating if UI input prompts should be suppressed.
     * \param options Bitmak indicating how the export should be performed. If the "SCENE_EXPORT_SELECTED" bit is set,
     * only the selected nodes should be exported.
     * \remarks The "options" bitmask is currently ignored by the USD export process.
     * \return A flag indicating the result of the export (either IMPEXP_FAIL, IMPEXP_SUCCESS, or IMPEXP_CANCEL).
     */
    int DoExport(
        const MCHAR*  filename,
        ExpInterface* ei,
        Interface*    i,
        BOOL          suppressPrompts = FALSE,
        DWORD         options = 0) override;

    /**
     * \brief Returns the UI options for the USD exporter.
     * These are available through Maxscript.
     * \return The export options.
     */
    static MaxUsd::IUSDExportOptions& GetUIOptions();

    /**
     * \brief Sets the UI options for the USD exporter.
     * \param newOptions The new export options.
     */
    static void SetUIOptions(const MaxUsd::USDSceneBuilderOptions& newOptions);

    /**
     * \brief Perform the file export with custom options.
     * \param filePath File path of the exported file.
     * \param exportOptions Export options to be used during the export.
     * \param suppressPrompts Flag indicating if UI input prompts should be suppressed.
     * \param defaultExt Default extension to be added to file name if no extension was provided.
     * \return A flag indicating the result of the export (either IMPEXP_FAIL, IMPEXP_SUCCESS, or IMPEXP_CANCEL).
     */
    static int ExportFile(
        const MCHAR*               filePath,
        MaxUsd::IUSDExportOptions& exportOptions,
        bool                       suppressPrompts = true,
        std::string                defaultExt = "usd");

private:
    // Global options for export.
    static MaxUsd::IUSDExportOptions uiExportOptions;
};

/**
 * \brief 3ds Max class description for the USD Exporter plugin.
 */
class USDExporterClassDesc : public ClassDesc2
{
public:
    /**
     * \brief Indicate if the plugin should be displayed in the list of plugins the User can choose from.
     * \return "TRUE" if the plugin can be picked and assigned by the user, "FALSE" otherwise.
     */
    int IsPublic() override { return TRUE; }

    /**
     * \brief Create an instance of the plugin.
     * \param loading Flag indicating if the class being created is going to be loaded from a disk file.
     * \return An instance of the plugin.
     */
    void* Create(BOOL /*loading = FALSE*/) override { return new USDExporter(); }

    /**
     * \brief Return the name of the plugin as it should appear in the 3ds Max UI.
     * \return The name of the plugin as it should appear in the 3ds Max UI.
     */
    const MCHAR* ClassName() override { return GetString(IDS_USDEXPORTER_CLASS_NAME); }

    /**
     * \brief Return the non-localized name of the class
     * \return The non-localized name of the plugin.
     */
    const MCHAR* NonLocalizedClassName() override
    {
        // NOTE: To maintain scripting compatibility with older max version (<2022),
        //  this value should be set to the en-US equivalent of ClassName()
        return _T("USDExporter");
    }

    /**
     * \brief Return a system-defined constant describing the class from which the plugin is derived.
     * \return The ID of the plugin's parent type.
     */
    SClass_ID SuperClassID() override { return SCENE_EXPORT_CLASS_ID; }

    /**
     * \brief Return a unique ID for the plugin.
     * \return A unique ID for the plugin.
     */
    Class_ID ClassID() override { return USDExporter_CLASS_ID; }

    /**
     * \brief Returns a localized string describing the category of the plugin.
     * \return A localized string describing the category of the plugin.
     */
    const MCHAR* Category() override { return GetString(IDS_CATEGORY); }

    /**
     * \brief Return the internal name of the plugin, which is used (among other things) to expose its features to
     * MaxScript.
     * \return The internal name of the plugin.
     */
    const MCHAR* InternalName() override { return _M("USDExporter"); }

    /**
     * \brief Only use the ClassDesc2's internal name when exposing names to MaxScript.
     * \return "true" if only the internal name should be used when exposing the ClassDesc2's feature to MaxScript,
     * "false" otherwise.
     */
    bool UseOnlyInternalNameForMAXScriptExposure() override { return true; }

    /**
     * \brief Return the owning module's handle.
     * \return The owning module's handle.
     */
    HINSTANCE HInstance() override { return hInstance; }
};

/**
 * \brief Return a reference to the ClassDesc2 definition of the USDExporter.
 * \return A reference to the ClassDesc2 definition of the USDExporter.
 */
ClassDesc2* GetUSDExporterDesc();