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

#include "USDExport.h"

#include <iparamb2.h>
#include <maxtypes.h>

/**
 * \brief Class_ID of the USDZ Exporter plugin.
 */
#define USDZExporter_CLASS_ID Class_ID(0x1bc20574, 0x187249ab)

/**
 * \brief USDZ Exporter.
 */
class USDZExporter : public USDExporter
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
};

/**
 * \brief 3ds Max class description for the USDZ Exporter plugin.
 */
class USDZExporterClassDesc : public ClassDesc2
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
    void* Create(BOOL /*loading = FALSE*/) override { return new USDZExporter(); }

    /**
     * \brief Return the name of the plugin as it should appear in the 3ds Max UI.
     * \return The name of the plugin as it should appear in the 3ds Max UI.
     */
    const MCHAR* ClassName() override { return GetString(IDS_USDZEXPORTER_CLASS_NAME); }

    /**
     * \brief Return the non-localized name of the class
     * \return The non-localized name of the plugin.
     */
    const MCHAR* NonLocalizedClassName() override
    {
        // NOTE: To maintain scripting compatibility with older max version (<2022),
        // this value should be set to the en-US equivalent of ClassName()
        return _T("USDZExporter");
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
    Class_ID ClassID() override { return USDZExporter_CLASS_ID; }

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
    const MCHAR* InternalName() override { return _M("USDZExporter"); }

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
 * \brief Return a reference to the ClassDesc2 definition of the USDZExporter.
 * \return A reference to the ClassDesc2 definition of the USDZExporter.
 */
ClassDesc2* GetUSDZExporterDesc();