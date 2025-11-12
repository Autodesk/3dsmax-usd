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

#include "PreferencesManagement.h"
#include "PreferencesOptions.h"
#include "PreferenceApplicationHost.h"

#include <MaxUsd/Utilities/MaxSupportUtils.h>

#ifdef IS_MAX2026_OR_GREATER
#include <AdskAssetResolver/AssetResolverContextDataRegistry.h>
#endif

#include <Qt/QmaxMainWindow.h>
#include <maxapi.h>

#include <pxr/usd/ar/resolver.h>

namespace PreferencesManagement {

#ifdef IS_MAX2026_OR_GREATER
// Names of the asset resolver context data sets
static const std::string PREFERENCE_MAPPING_FILE_DATA_SET_NAME = "3ds Max Preference Mapping File";
static const std::string SESSION_USER_PATHS_DATA_SET_NAME = "3ds Max Session User Paths";
static const std::string PROJECT_TOKENS_DATA_SET_NAME = "3ds Max Project Tokens";
#endif

void InitializeUsdPreferences()
{
    // Add ApplicationHost for the USD Preferences dialog
    PreferenceApplicationHost::CreateInstance(GetCOREInterface()->GetQmaxMainWindow());
    // Load USD Preference options to ensure the Adsk Asset Resolver works as configured
    ApplyUsdPreferences(UsdPreferenceOptions(), UsdPreferenceOptions::GetInstance());
}

const UsdPreferenceOptions GetUsdPreferences()
{
    UsdPreferenceOptions options = UsdPreferenceOptions::GetInstance();

#ifdef IS_MAX2026_OR_GREATER
    // fill in the env search paths from the context data manager
    const auto envContextData = Adsk::AssetResolverContextDataRegistry::GetContextData(
        Adsk::AssetResolverContextDataRegistry::GetEnvironmentMappingContextDataName());
    if (envContextData.has_value()) {
        options.SetEnvironmentSearchPaths(envContextData.value().get().searchPaths);
    }
#endif
    return options;
}
    
void ApplyUsdPreferences(
    const UsdPreferenceOptions& options,
    const UsdPreferenceOptions& newOptions)
{
#ifdef IS_MAX2026_OR_GREATER
    auto allContextData
        = Adsk::AssetResolverContextDataRegistry::GetAvailableContextData();
    // helper to set the state of a context data, adding it if it does not exist
    auto setContextDataState = [&allContextData](const std::string& name, bool active) {
        for (auto& contextData : allContextData) {
            if (contextData.first == name) {
                contextData.second = active;
                return;
            }
        }
        allContextData.insert(allContextData.begin(), { name, active });
    };
    if (options.GetMappingFile() != newOptions.GetMappingFile()) {
        auto mappingFileContent
            = Adsk::GetContextDataFromFile(newOptions.GetMappingFile().string());
        if (mappingFileContent.has_value()) {
            auto preferenceMappingFileContextData
                = Adsk::AssetResolverContextDataRegistry::GetContextData(
                    PREFERENCE_MAPPING_FILE_DATA_SET_NAME, true);
            if (preferenceMappingFileContextData.has_value()) {
                preferenceMappingFileContextData.value().get() = mappingFileContent.value();
                setContextDataState(PREFERENCE_MAPPING_FILE_DATA_SET_NAME, true);
            }
        } else {
            Adsk::AssetResolverContextDataRegistry::RemoveContextData(
                PREFERENCE_MAPPING_FILE_DATA_SET_NAME);
            setContextDataState(PREFERENCE_MAPPING_FILE_DATA_SET_NAME, false);
        }
    }

    if (options.GetUserSearchPaths() != newOptions.GetUserSearchPaths()) {
        auto userSearchPathsContextData
            = Adsk::AssetResolverContextDataRegistry::GetContextData(
                SESSION_USER_PATHS_DATA_SET_NAME, true);
        if (userSearchPathsContextData.has_value()) {
            userSearchPathsContextData.value().get().searchPaths = newOptions.GetUserSearchPaths();
            setContextDataState(SESSION_USER_PATHS_DATA_SET_NAME, true);
        } else {
            setContextDataState(SESSION_USER_PATHS_DATA_SET_NAME, false);
        }
    }

    setContextDataState(PROJECT_TOKENS_DATA_SET_NAME, newOptions.IsUsingProjectTokens());
    setContextDataState(
        Adsk::AssetResolverContextDataRegistry::GetEnvironmentMappingContextDataName(),
        newOptions.IsIncludingEnvironmentSearchPaths());

    // now that we have processed options, we can make a list of the selected context data
    std::vector<std::string> selectedContextData;
    if (!newOptions.IsUsingUserSearchPathsFirst()) {
        selectedContextData.push_back(
            Adsk::AssetResolverContextDataRegistry::GetEnvironmentMappingContextDataName());
    }
    for (const auto& contextData : allContextData) {
        if (contextData.second) {
            selectedContextData.push_back(contextData.first);
        }
    }
    // ordering user search paths first if the option is set
    if (newOptions.IsIncludingEnvironmentSearchPaths()) {
        auto userIt = std::find(
            selectedContextData.begin(),
            selectedContextData.end(),
            SESSION_USER_PATHS_DATA_SET_NAME);
        auto envIt = std::find(
            selectedContextData.begin(),
            selectedContextData.end(),
            Adsk::AssetResolverContextDataRegistry::GetEnvironmentMappingContextDataName());
        if (newOptions.IsUsingUserSearchPathsFirst() && userIt != selectedContextData.end()
            && envIt != selectedContextData.end() && envIt > userIt) {
            // move user paths before environment paths
            std::swap(*envIt, *userIt);
        }
    }
    Adsk::AssetResolverContextDataRegistry::SetActiveContextData(selectedContextData);
#endif
}

void SaveUsdPreferences(const UsdPreferenceOptions& options)
{
    // update the options instance
    // the copy clears env search paths as they are not saved
    // those are only used to display the paths in the dialog 
    UsdPreferenceOptions::GetInstance() = options;
    // save options to disk
    UsdPreferenceOptions::GetInstance().Save();
}

} // namespace PreferencesManagement
