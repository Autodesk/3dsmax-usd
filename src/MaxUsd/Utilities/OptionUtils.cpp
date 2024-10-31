//
// Copyright 2024 Autodesk
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

// Max includes
#include <IPathConfigMgr.h>
#include <MaxDirectories.h>

// Qt includes
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

// MaxUsd includes
#include "ListenerUtils.h"
#include "OptionUtils.h"
#include "VtDictionaryUtils.h"

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>
#include <MaxUsd/Builders/UsdSceneBuilderOptions.h>

// Pxr includes
#include <pxr/base/tf/diagnostic.h>

namespace MAXUSD_NS_DEF {
namespace OptionUtils {
PXR_NAMESPACE_USING_DIRECTIVE

MaxSDK::Util::Path GetPathToUSDSettings()
{
    MaxSDK::Util::Path maxUsdSettingsPath;

    IPathConfigMgr* pathMgr = IPathConfigMgr::GetPathConfigMgr();
    if (pathMgr) {
        maxUsdSettingsPath.SetPath(pathMgr->GetDir(APP_USER_SETTINGS_DIR));
        maxUsdSettingsPath.Append(_T("MaxUsd"));
        // Make sure the folder exists
        bool res = pathMgr->CreateDirectoryHierarchy(maxUsdSettingsPath);
        DbgAssert(res);
    }
    return maxUsdSettingsPath;
}

MaxSDK::Util::Path GetPathToUsdExportSettings()
{
    auto pathToUsdSettings = GetPathToUSDSettings();
    pathToUsdSettings.Append(_T("\\usdExportSettings.json"));
    return pathToUsdSettings.GetCStr();
}

MaxSDK::Util::Path GetPathToUsdImportSettings()
{
    auto pathToUsdSettings = GetPathToUSDSettings();
    pathToUsdSettings.Append(_T("\\usdImportSettings.json"));
    return pathToUsdSettings;
}

MaxSDK::Util::Path GetPathToUsdUiSettings()
{
    auto pathToUsdSettings = GetPathToUSDSettings();
    pathToUsdSettings.Append(_T("\\usdUiSettings.json"));
    return pathToUsdSettings;
}

bool ReadJsonFile(QJsonObject& json, QFile& file, const WStr& path)
{
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        file.close();
        QJsonParseError error {};
        auto            doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError) {
            TF_WARN(
                "Failed to parse : %s - %s",
                MaxStringToUsdString(path.data()),
                error.errorString().toStdString());
            return false;
        }
        if (doc.isObject()) {
            json = doc.object();
            return true;
        }
        TF_WARN("Failed to parse : %s - Not a JSON object", MaxStringToUsdString(path.data()));
    } else {
        TF_WARN("Failed to read : %s", MaxStringToUsdString(path.data()));
    }
    return false;
}

void WriteJsonFile(QFile& file, const std::string& jsonString, const WStr& path)
{
    auto writeToFile = [&]() -> bool {
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            file.write(jsonString.c_str());
            file.close();
            return true;
        }
        return false;
    };

    // Try to write to file, if it existed and failed, remove the file and try again
    if (!writeToFile()) {
        if (file.exists() && file.remove()) {
            if (!writeToFile()) {
                TF_WARN("Failed to write : %s", MaxStringToUsdString(path.data()));
            }
        } else {
            TF_WARN("Failed to write/overwrite : %s", MaxStringToUsdString(path.data()));
        }
    }
}

void SaveRollupStates(const QString& category, const std::map<QString, bool>& rollupStates)
{
    QJsonObject json;
    QFile       file(GetPathToUsdUiSettings().GetString());

    if (file.exists()) {
        if (!ReadJsonFile(json, file, GetPathToUsdUiSettings().GetString())) {
            return;
        }
    }
    QJsonObject categoryJson;
    for (const auto& item : rollupStates) {
        categoryJson[item.first] = item.second;
    }
    json[category] = categoryJson;

    QJsonDocument doc(json);
    WriteJsonFile(file, doc.toJson().toStdString(), GetPathToUsdUiSettings().GetString());
}

std::map<QString, bool> LoadRollupStates(const QString& category)
{
    static std::map<QString, bool> rollupStates;
    QFile                          file(GetPathToUsdUiSettings().GetString());
    QJsonObject                    json;

    if (file.exists()) {
        if (!ReadJsonFile(json, file, GetPathToUsdUiSettings().GetString())) {
            return rollupStates;
        }

        QJsonObject categoryJson = json[category].toObject();
        for (auto it = categoryJson.begin(); it != categoryJson.end(); ++it) {
            rollupStates[it.key()] = it.value().toBool();
        }
    }
    return rollupStates;
}

std::string SerializeOptionsToJson(const DictionaryOptionProvider& options)
{
    QJsonObject json;
    DictUtils::VtDictToJson(options.GetOptions(), json);

    QJsonDocument doc(json);
    return doc.toJson().toStdString();
}

pxr::VtDictionary DeserializeOptionsFromJson(const QByteArray& data)
{
    QJsonObject   json = QJsonDocument::fromJson(data).object();
    QJsonDocument doc(json);
    QString       strJson(doc.toJson());
    auto          stdStr = strJson.toStdString();

    pxr::VtDictionary dict;
    DictUtils::VtDictFromString(stdStr, dict);

    return dict;
}

void SaveUiOptions(const std::string& category, const pxr::VtDictionary& dict)
{
    QJsonObject json;

    QFile file(GetPathToUsdUiSettings().GetString());

    if (file.exists()) {
        if (!ReadJsonFile(json, file, GetPathToUsdUiSettings().GetString())) {
            return;
        }
    }

    QJsonObject jsonObj;
    DictUtils::VtDictToJson(dict, jsonObj);
    json[QString::fromStdString(category)] = jsonObj;

    QJsonDocument doc(json);
    WriteJsonFile(file, doc.toJson().toStdString(), GetPathToUsdUiSettings().GetString());
}

void LoadUiOptions(
    const std::string&       category,
    pxr::VtDictionary&       dict,
    const pxr::VtDictionary& guide)
{
    QFile       file(GetPathToUsdUiSettings().GetString());
    QJsonObject json;

    if (file.exists()) {
        if (!ReadJsonFile(json, file, GetPathToUsdUiSettings().GetString())) {
            dict = guide;
            return;
        }

        QJsonObject   categoryJson = json[QString::fromStdString(category)].toObject();
        QJsonDocument doc(categoryJson);
        QString       strJson(doc.toJson());
        auto          stdStr = strJson.toStdString();
        DictUtils::VtDictFromString(stdStr, dict);
        if (!guide.empty()) {
            DictUtils::CoerceDictToGuideType(dict, guide);
            dict = VtDictionaryOver(dict, guide);
        }
    } else if (!guide.empty()) {
        dict = guide;
    }
}

void SaveToFile(const DictionaryOptionProvider& optionsProvider, const MaxSDK::Util::Path& filePath)
{
    const auto json = SerializeOptionsToJson(optionsProvider);
    QFile      file(filePath.GetString());

    WriteJsonFile(file, json, filePath.GetString());
}

void SaveExportOptions(const USDSceneBuilderOptions& options)
{
    SaveToFile(options, GetPathToUsdExportSettings());
}

void SaveImportOptions(const MaxSceneBuilderOptions& options)
{
    SaveToFile(options, GetPathToUsdImportSettings());
}

template <typename OptionsType> OptionsType LoadOptions(const MaxSDK::Util::Path& filePath)
{
    OptionsType options; // Assumes default constructible

    QFile file(filePath.GetString());
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray data = file.readAll();
            file.close();

            return OptionsType(DeserializeOptionsFromJson(data));
        } else {
            TF_WARN("Failed to load options from : %s", MaxStringToUsdString(filePath.GetCStr()));
        }
    }
    options.SetDefaults();
    return options; // Return default options if file doesn't exist or failed to open
}

MaxSceneBuilderOptions LoadImportOptions()
{
    return LoadOptions<MaxSceneBuilderOptions>(GetPathToUsdImportSettings());
}

USDSceneBuilderOptions LoadExportOptions()
{
    return LoadOptions<USDSceneBuilderOptions>(GetPathToUsdExportSettings());
}

} // namespace OptionUtils
} // namespace MAXUSD_NS_DEF