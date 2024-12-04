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

#include <MaxUsd/UsdStageSource.h>

#include <pxr/usd/usd/stageCacheContext.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief Handle a Usd stage within a 3ds Max scene.
 */
class MaxUSDAPI USDSceneController
{
public:
    USDSceneController();
    ~USDSceneController();

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
     * \brief Export root layer stage content to \a filename.
     * \param filePath The absolute path to export to.
     * \param buildOptions Build configuration options to use during the translation of 3ds Max content to USD content.
     * \return IMPEXP_FAIL if failed, IMPEXP_SUCCESS if success and IMPEXP_CANCEL if cancelled by user
     */
    int Export(const fs::path& filePath, const USDSceneBuilderOptions& buildOptions);

private:
    /// Avoid caching USD files opened in 3ds Max based on their filenames, as successive openings
    /// within the same 3ds Max session after editing content in an external application may cause
    /// changes not to be picked up:
    pxr::UsdStageCacheContext stageCacheContext {
        pxr::UsdStageCacheContextBlockType::UsdBlockStageCachePopulation
    };
};

MaxUSDAPI USDSceneController* GetUSDSceneController();

} // namespace MAXUSD_NS_DEF