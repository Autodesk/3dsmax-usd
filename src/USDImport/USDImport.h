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

#include "DLLEntry.h"
#include "resource.h"

#include <MaxUsd/Interfaces/IUSDImportOptions.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <impexp.h>
#include <iparamb2.h>

/**
 * \brief Class_ID of the USD Importer plugin.
 */
#define USDImporter_CLASS_ID Class_ID(0x9e90207a, 0x4cacb4fe)

/**
 * \brief USD Importer.
 */
class USDImporter : public SceneImport
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
     * \brief Perform the file import.
     * \param filePath The filepath of the imported file.
     * \param ei Pointer which may be used to call methods to enumerate the scene.
     * \param i Interface pointer to be used to call 3ds Max methods.
     * \param suppressPrompts Flag indicating if UI input prompts should be suppressed.
     * \return A flag indicating the result of the import (either IMPEXP_FAIL, IMPEXP_SUCCESS, or IMPEXP_CANCEL).
     */
    int
    DoImport(const MCHAR* filePath, ImpInterface* ei, Interface* i, BOOL suppressPrompts = FALSE)
        override;

    /**
     * \brief Returns the UI options for the USD importer.
     * These are available through MaxScript.
     * \return The import options.
     */
    static MaxUsd::IUSDImportOptions& GetUIOptions();

    /**
     * \brief Sets the UI options for the USD importer.
     * \param newOptions The new import options.
     */
    static void SetUIOptions(const MaxUsd::MaxSceneBuilderOptions& newOptions);

    /**
     * \brief Perform the file import.
     * \param filePath The filePath of the imported file.
     * \param suppressPrompts Flag indicating if UI input prompts should be suppressed.
     * \return A flag indicating the result of the import (either IMPEXP_FAIL, IMPEXP_SUCCESS, or IMPEXP_CANCEL).
     */
    static int ImportFile(
        const MCHAR*               filePath,
        MaxUsd::IUSDImportOptions& importOptions,
        bool                       suppressPrompts);

    /**
     * \brief Validates that import options are correct. The function will log any issues with the options
     * in the listener as well as the Max log.
     * \param options The options to validate.
     * \return True if options are valid, false otherwise.
     */
    static bool ValidateImportOptions(const MaxUsd::IUSDImportOptions& options);

private:
    // Global options for import.
    static MaxUsd::IUSDImportOptions uiImportOptions;
    static bool                      optionDefaultsApplied;
};

/**
 * \brief 3ds Max class description for the USD Importer plugin.
 */
class USDImporterClassDesc : public ClassDesc2
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
    void* Create(BOOL /*loading = FALSE*/) override { return new USDImporter(); }

    /**
     * \brief Return the name of the plugin as it should appear in the 3ds Max UI.
     * \return The name of the plugin as it should appear in the 3ds Max UI.
     */
    const MCHAR* ClassName() override
    {
        static MSTR str = GetMString(IDS_USDIMPORTER_CLASS_NAME);
        return str;
    }

    /**
     * \brief Return the non-localized name of the class
     * \return The non-localized name of the plugin.
     */
    const MCHAR* NonLocalizedClassName() override
    {
        // NOTE: To maintain scripting compatibility with older max version (<2022),
        // this value should be set to the en-US equivalent of ClassName()
        return _T("USDImporter");
    }

    /**
     * \brief Return a system-defined constant describing the class from which the plugin is derived.
     * \return The ID of the plugin's parent type.
     */
    SClass_ID SuperClassID() override { return SCENE_IMPORT_CLASS_ID; }

    /**
     * \brief Return a unique ID for the plugin.
     * \return A unique ID for the plugin.
     */
    Class_ID ClassID() override { return USDImporter_CLASS_ID; }

    /**
     * \brief Returns a localized string describing the category of the plugin.
     * \return A localized string describing the category of the plugin.
     */
    const MCHAR* Category() override
    {
        static MSTR str = GetMString(IDS_CATEGORY);
        return str;
    }

    /**
     * \brief Return the internal name of the plugin, which is used (among other things) to expose its features to
     * MaxScript.
     * \return The internal name of the plugin.
     */
    const MCHAR* InternalName() override { return _M("USDImporter"); }

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
 * \brief Return a reference to the ClassDesc2 definition of the USDImporter.
 * \return A reference to the ClassDesc2 definition of the USDImporter.
 */
ClassDesc2* GetUSDImporterDesc();
