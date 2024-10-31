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
#include "UsdStageSource.h"

#include "Utilities/TranslationUtils.h"

namespace MAXUSD_NS_DEF {

UsdStageSource::UsdStageSource(const pxr::UsdStageCache::Id& cacheId)
{
    type = CACHE;
    this->cacheId = cacheId;
}

UsdStageSource::UsdStageSource(const fs::path& filePath)
{
    type = FILE;
    this->filePath = filePath;
}

std::string UsdStageSource::ToString() const
{
    switch (type) {
    case CACHE: return std::string("Cached Stage Id : ") + cacheId.ToString();
    case FILE: return filePath.u8string();
    default: DbgAssert(_T("Unhandled stage source type.")); return "";
    }
}

pxr::UsdStageRefPtr UsdStageSource::LoadStage(const MaxSceneBuilderOptions& buildOptions) const
{
    switch (type) {
    case FILE: {
        pxr::UsdStagePopulationMask stageMask;
        const auto&                 maskPaths = buildOptions.GetStageMaskPaths();
        for (const auto& path : maskPaths) {
            stageMask.Add(path);
        }
        const std::string usdfile = filePath.u8string();
        return pxr::UsdStage::OpenMasked(usdfile, stageMask, buildOptions.GetStageInitialLoadSet());
    }
    case CACHE: {
        MaxSceneBuilderOptions defaultOptions;
        defaultOptions.SetDefaults();
        const auto& defaultStageMask = defaultOptions.GetStageMaskPaths();
        const auto& maskPaths = buildOptions.GetStageMaskPaths();
        if (!std::equal(
                defaultStageMask.begin(),
                defaultStageMask.end(),
                maskPaths.begin(),
                maskPaths.end())) {
            MaxUsd::Log::Warn("A stage population mask is specified, but this option is ignored "
                              "when importing from the stage cache.");
        }
        if (defaultOptions.GetStageInitialLoadSet() != buildOptions.GetStageInitialLoadSet()) {
            MaxUsd::Log::Warn("An initial loading set is specified, but this option is ignored "
                              "when importing from the stage cache.");
        }
        return pxr::UsdUtilsStageCache::Get().Find(cacheId);
    }
    default: DbgAssert(_T("Unhandled stage souce type.")); return pxr::TfNullPtr;
    }
}
} // namespace MAXUSD_NS_DEF