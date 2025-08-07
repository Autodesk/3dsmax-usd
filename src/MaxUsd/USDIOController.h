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

#include "Builders/MaxSceneBuilderOptions.h"
#include "Builders/USDSceneBuilder.h"
#include "Builders/USDSceneBuilderOptions.h"
#include "MaxUSDAPI.h"
#include "Utilities/OptionUtils.h"

#include <MaxUsd/Interfaces/IUSDExportOptions.h>
#include <MaxUsd/UsdStageSource.h>

#include <pxr/usd/usd/stageCacheContext.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief Handles import/export to/from Usd stages within a 3ds Max scene.
 */
class MaxUSDAPI USDIOController
{
public:
    USDIOController();
    ~USDIOController();

    /**
     * \brief Import a USD stage, as a set of 3ds Max primitives.
     * \param stageSource The USD stage's source. Can be a file, or the cache.
     * \param buildOptions Build options for the translation of the USD content into 3ds Max content.
     * \param filename The USD filename/path. Can be an empty string in the case of importing from cache.
     * \return IMPEXP_FAIL if failed, IMPEXP_SUCCESS if success and IMPEXP_CANCEL if cancelled by user
     */
    int Import(
        const MaxUsd::UsdStageSource& stageSource,
        const MaxSceneBuilderOptions& buildOptions,
        const fs::path&               filename = "");

    /**
     * \brief Exports 3ds Max scene data to a new stage with the given root layer.
     * \param filePath The absolute path to export to.
     * \param buildOptions Build configuration options to use during the translation of 3ds Max content to USD content.
     * \return IMPEXP_FAIL if failed, IMPEXP_SUCCESS if success and IMPEXP_CANCEL if cancelled by user
     */
    int Export(const fs::path& filePath, const USDSceneBuilderOptions& buildOptions);

    /**
     * Export 3ds Max scene data to a given stage.
     * \param stage The stage being exported to.
     * \param buildOptions Build configuration options to use during the translation of 3ds Max content to USD content.
     * \param allowOverwrite Whether to allow prim overwrites, or to rename prims on conflicts.
     * \param rootTransform An additional transform to apply onto prims (if exporting to a USDStageObject's stage,
     * this would be the node's transform)
     * \return IMPEXP_FAIL if failed, IMPEXP_SUCCESS if success and IMPEXP_CANCEL if cancelled by user.
     */
    int Export(
        const pxr::UsdStageRefPtr&    stage,
        const USDSceneBuilderOptions& buildOptions,
        bool                          allowOverwrite,
        const Matrix3&                rootTransform);

    /**
     * Opens a dialog to configure global export options to live stage objects.
     */
    void ConfigureExportToStageOptions();

    /**
     * \brief Returns the UI options for the USD exporter.
     * These are available through Maxscript.
     * \return The export options.
     */
    MaxUsd::IUSDExportOptions& GetExportUIOptions(const USDSceneBuilderOptions::Type& type);

    /**
     * \brief Returns the UI options for the USD exporter when exporting to live stages.
     * \return The export options.
     */
    const pxr::VtDictionary& GetUIExportToStageExtraOptions();

    /**
     * Convenience overload to get usable extra options directly.
     * @param node The node holding a stage object we are exporting to.
     * @param overwritePrims output, whether to allow overwriting prims.
     * @param rootTransform output, the root transform to use in the export.
     */
    const void
    GetUIExportToStageExtraOptions(INode* node, bool& overwritePrims, Matrix3& rootTransform);

    /**
     * Set the global extra export options when exporting to live stages. These are the options
     * used when going through the UI.
     * @param options The extra options.
     */
    void SetUIExportToStageExtraOptions(const pxr::VtDictionary& options);

    /**
     * \brief Sets the UI options for the USD exporter.
     * \param newOptions The new export options.
     */
    void SetExportUIOptions(
        const MaxUsd::USDSceneBuilderOptions& newOptions,
        const USDSceneBuilderOptions::Type&   type);

private:
    /// Avoid caching USD files opened in 3ds Max based on their filenames, as successive openings
    /// within the same 3ds Max session after editing content in an external application may cause
    /// changes not to be picked up:
    pxr::UsdStageCacheContext stageCacheContext {
        pxr::UsdStageCacheContextBlockType::UsdBlockStageCachePopulation
    };

    // Global options for export
    static IUSDExportOptions uiExportToFileOptions;
    static IUSDExportOptions uiExportToStageOptions;
    static pxr::VtDictionary uiExportToStageExtraOptions;
};

MaxUSDAPI USDIOController* GetUSDIOController();

} // namespace MAXUSD_NS_DEF