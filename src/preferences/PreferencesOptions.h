//
// Copyright 2025 Autodesk
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

#include "PreferencesExport.h"
#include <MaxUsd/Utilities/DictionaryOptionProvider.h>

#include <pxr/pxr.h>
#include <pxr/base/tf/staticTokens.h>

#if _MSVC_LANG > 201402L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

class pxr::VtDictionary;

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
#define PXR_MAXUSD_PREFERENCE_OPTIONS_TOKENS \
    /* Dictionary keys */ \
    (useProjectTokens) \
    (mappingFile) \
    (userSearchPaths) \
    (userPathsFirst) \
    (includeEnvironmentSearchPaths) \
    /* not saving this option ; this is provided by the asset resolver directly */ \
    (environmentSearchPaths) 

// clang-format on

TF_DECLARE_PUBLIC_TOKENS(
    MaxUsdPreferenceOptionsTokens,
    MaxUsdPreferencesAPI,
    PXR_MAXUSD_PREFERENCE_OPTIONS_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

class MaxUsdPreferencesAPI UsdPreferenceOptions : public MaxUsd::DictionaryOptionProvider
{
public:
    UsdPreferenceOptions();
    ~UsdPreferenceOptions() = default;

    UsdPreferenceOptions& operator=(const UsdPreferenceOptions& other);

    void Load(pxr::VtDictionary& dict, const pxr::VtDictionary& guide);
    void Save() const;

    static UsdPreferenceOptions& GetInstance();

    bool IsUsingProjectTokens() const;
    void SetUsingProjectTokens(bool useProjectTokens);

    bool IsUsingUserSearchPathsFirst() const;
    void SetUsingUserSearchPathsFirst(bool userPathsFirst);

    const fs::path& GetMappingFile() const;
    void            SetMappingFile(const fs::path& mappingFile);

    const std::vector<std::string> GetUserSearchPaths() const;
    void SetUserSearchPaths(const std::vector<std::string>& userSearchPaths);

    bool IsIncludingEnvironmentSearchPaths() const;
    void SetIncludingEnvironmentSearchPaths(bool includeEnvironmentSearchPaths);

    const std::vector<std::string> GetEnvironmentSearchPaths() const;
	void SetEnvironmentSearchPaths(const std::vector<std::string>& environmentSearchPaths);

private:
    const pxr::VtDictionary& GetDefaultDictionary();

    static UsdPreferenceOptions* instance;
};
