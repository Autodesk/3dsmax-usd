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
#include "PreferencesOptions.h"

#include <MaxUsd/Utilities/VtDictionaryUtils.h>
#include <MaxUsd/Utilities/OptionUtils.h>

#include <pxr/base/vt/dictionary.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>


PXR_NAMESPACE_OPEN_SCOPE
TF_DEFINE_PUBLIC_TOKENS(MaxUsdPreferenceOptionsTokens, PXR_MAXUSD_PREFERENCE_OPTIONS_TOKENS);
PXR_NAMESPACE_CLOSE_SCOPE

PXR_NAMESPACE_USING_DIRECTIVE

MaxSDK::Util::Path GetPathToUsdPreferences()
{
    auto pathToUsdSettings = MaxUsd::OptionUtils::GetPathToUSDSettings();
    pathToUsdSettings.Append(_T("\\usdPreferences.json"));
    return pathToUsdSettings;
}

UsdPreferenceOptions* UsdPreferenceOptions::instance = nullptr;

UsdPreferenceOptions::UsdPreferenceOptions()
    : MaxUsd::DictionaryOptionProvider()
{ 
    options = GetDefaultDictionary();
}

UsdPreferenceOptions& UsdPreferenceOptions::operator=(const UsdPreferenceOptions& other)
{
    options[MaxUsdPreferenceOptionsTokens->useProjectTokens]
        = VtDictionaryGet<bool>(other.options, MaxUsdPreferenceOptionsTokens->useProjectTokens);
    options[MaxUsdPreferenceOptionsTokens->mappingFile]
        = VtDictionaryGet<fs::path>(other.options, MaxUsdPreferenceOptionsTokens->mappingFile);
    options[MaxUsdPreferenceOptionsTokens->userSearchPaths]
        = MaxUsd::DictUtils::ExtractVector<std::string>(
            other.options, MaxUsdPreferenceOptionsTokens->userSearchPaths);
    options[MaxUsdPreferenceOptionsTokens->userPathsFirst]
        = VtDictionaryGet<bool>(other.options, MaxUsdPreferenceOptionsTokens->userPathsFirst);
    options[MaxUsdPreferenceOptionsTokens->includeEnvironmentSearchPaths] = VtDictionaryGet<bool>(
        other.options, MaxUsdPreferenceOptionsTokens->includeEnvironmentSearchPaths);
    // not copying MaxUsdPreferenceOptionsTokens->environmentSearchPaths
    return *this;
}

const VtDictionary& UsdPreferenceOptions::GetDefaultDictionary()
{
    static VtDictionary   d;
    static std::once_flag once;
    std::call_once(once, []() {
        d[MaxUsdPreferenceOptionsTokens->useProjectTokens] = true;
        d[MaxUsdPreferenceOptionsTokens->mappingFile] = fs::path();
        d[MaxUsdPreferenceOptionsTokens->userSearchPaths] = std::vector<std::string>();
        d[MaxUsdPreferenceOptionsTokens->userPathsFirst] = true;
        d[MaxUsdPreferenceOptionsTokens->includeEnvironmentSearchPaths] = true;
        d[MaxUsdPreferenceOptionsTokens->environmentSearchPaths] = std::vector<std::string>();
    });
    return d;
}

UsdPreferenceOptions& UsdPreferenceOptions::GetInstance()
{
    if (!instance) {
        instance = new UsdPreferenceOptions();
        instance->Load(instance->options, instance->GetDefaultDictionary());    
    }
    return *instance;
}

void UsdPreferenceOptions::Load(VtDictionary& dict, const VtDictionary& guide)
{
    QFile       file(GetPathToUsdPreferences().GetString());
    QJsonObject json;

    if (file.exists()) {
        if (!MaxUsd::OptionUtils::ReadJsonFile(json, file, GetPathToUsdPreferences().GetCStr())) {
            dict = guide;
            return;
        }

        QJsonDocument doc(json);
        QString       strJson(doc.toJson());
        auto          stdStr = strJson.toStdString();
        MaxUsd::DictUtils::VtDictFromString(stdStr, dict);
        if (!guide.empty()) {
            MaxUsd::DictUtils::CoerceDictToGuideType(dict, guide);
            dict = VtDictionaryOver(dict, guide);
        }
    } else if (!guide.empty()) {
        dict = guide;
    }
}

void UsdPreferenceOptions::Save() const
{
    QJsonObject json;
    MaxUsd::DictUtils::VtDictToJson(options, json);

    QFile file(GetPathToUsdPreferences().GetString());
    MaxUsd::OptionUtils::WriteJsonFile(
        file, QJsonDocument(json).toJson().toStdString(), GetPathToUsdPreferences().GetCStr());
}

bool UsdPreferenceOptions::IsUsingProjectTokens() const
{
    return VtDictionaryGet<bool>(options, MaxUsdPreferenceOptionsTokens->useProjectTokens);
}

void UsdPreferenceOptions::SetUsingProjectTokens(bool useProjectTokens)
{
    options[MaxUsdPreferenceOptionsTokens->useProjectTokens] = useProjectTokens;
}

const fs::path& UsdPreferenceOptions::GetMappingFile() const
{
    return VtDictionaryGet<fs::path>(options, MaxUsdPreferenceOptionsTokens->mappingFile);
}

void UsdPreferenceOptions::SetMappingFile(const fs::path& mappingFile)
{
	options[MaxUsdPreferenceOptionsTokens->mappingFile] = mappingFile;
}

const std::vector<std::string> UsdPreferenceOptions::GetUserSearchPaths() const
{
    return MaxUsd::DictUtils::ExtractVector<std::string>(options, MaxUsdPreferenceOptionsTokens->userSearchPaths);
}

void UsdPreferenceOptions::SetUserSearchPaths(const std::vector<std::string>& userSearchPaths)
{
	options[MaxUsdPreferenceOptionsTokens->userSearchPaths] = userSearchPaths;
}

bool UsdPreferenceOptions::IsUsingUserSearchPathsFirst() const
{
	return VtDictionaryGet<bool>(options, MaxUsdPreferenceOptionsTokens->userPathsFirst);
}

void UsdPreferenceOptions::SetUsingUserSearchPathsFirst(bool userPathsFirst)
{
	options[MaxUsdPreferenceOptionsTokens->userPathsFirst] = userPathsFirst;
}

bool UsdPreferenceOptions::IsIncludingEnvironmentSearchPaths() const
{
	return VtDictionaryGet<bool>(options, MaxUsdPreferenceOptionsTokens->includeEnvironmentSearchPaths);
}

void UsdPreferenceOptions::SetIncludingEnvironmentSearchPaths(bool includeEnvironmentSearchPaths)
{
	options[MaxUsdPreferenceOptionsTokens->includeEnvironmentSearchPaths] = includeEnvironmentSearchPaths;
}

const std::vector<std::string> UsdPreferenceOptions::GetEnvironmentSearchPaths() const
{
    return MaxUsd::DictUtils::ExtractVector<std::string>(
        options, MaxUsdPreferenceOptionsTokens->environmentSearchPaths);
}

void UsdPreferenceOptions::SetEnvironmentSearchPaths(const std::vector<std::string>& environmentSearchPaths)
{
	options[MaxUsdPreferenceOptionsTokens->environmentSearchPaths] = environmentSearchPaths;
}

