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

namespace MAXUSD_NS_DEF {

class MaxUSDAPI UsdStageSource
{
public:
    enum Type
    {
        FILE,
        CACHE
    };

    /**
     * \brief Creates a USD stage source from the stage cache.
     * \param cacheId The Stage's cache ID.
     */
    explicit UsdStageSource(const pxr::UsdStageCache::Id& cacheId);

    /**
     * \brief Creates a USD stage source from a file on disk.
     * \param filePath The USD stage's file path.
     */
    explicit UsdStageSource(const fs::path& filePath);

    /**
     * \brief Returns a string representation of the USD source.
     * \return The string representation.
     */
    std::string ToString() const;

    /**
     * \brief Loads the stage from the source.
     * \param buildOptions Scene builder options to consider.
     * \return The loaded USD stage.
     */
    pxr::UsdStageRefPtr LoadStage(const MaxSceneBuilderOptions& buildOptions) const;

private:
    Type                   type;
    fs::path               filePath;
    pxr::UsdStageCache::Id cacheId;
};
} // namespace MAXUSD_NS_DEF