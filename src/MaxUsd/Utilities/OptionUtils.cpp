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
#include <MaxUsd/MaxTokens.h>
#include <MaxUsd/Translators/ShadingModeRegistry.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

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

MaxSDK::Util::Path GetPathToUsdExportSettings(const USDSceneBuilderOptions::Type& type)
{
    auto pathToUsdSettings = GetPathToUSDSettings();
    switch (type) {
    case USDSceneBuilderOptions::Type::ToFile:
        pathToUsdSettings.Append(_T("\\usdExportSettings.json"));
        break;
    case USDSceneBuilderOptions::Type::ToStage:
        pathToUsdSettings.Append(_T("\\usdExportToStageSettings.json"));
        break;
    default: DbgAssert("Unknown USD export type"); return {};
    }
    return pathToUsdSettings.GetCStr();
}

MaxSDK::Util::Path GetPathToUsdExportToStageExtraSettings()
{
    auto pathToUsdSettings = GetPathToUSDSettings();
    pathToUsdSettings.Append(_T("\\usdExportToStageExtraSettings.json"));
    return pathToUsdSettings;
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

void SaveExportOptions(
    const USDSceneBuilderOptions&       options,
    const USDSceneBuilderOptions::Type& type)
{
    SaveToFile(options, GetPathToUsdExportSettings(type));
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
    auto       opts = LoadOptions<MaxSceneBuilderOptions>(GetPathToUsdImportSettings());
    const auto version
        = VtDictionaryGet<int>(opts.GetOptions(), MaxUsdMaxSceneBuilderOptionsTokens->version);
    // From version 1 to 2, the default ShadingModes were changed.
    // If the version is 1, and the ShadingMode is the previous default, update the options to the
    // current default.
    if (version == 1) {
        const auto& shadingModes = opts.GetShadingModes();
        if (shadingModes.size() == 1) {
            const auto& shadingMode = shadingModes.front();
            if (VtDictionaryIsHolding<TfToken>(shadingMode, MaxUsdShadingModesTokens->mode)) {
                const auto mode
                    = VtDictionaryGet<TfToken>(shadingMode, MaxUsdShadingModesTokens->mode);
                const auto materialConversion = VtDictionaryGet<TfToken>(
                    shadingMode, MaxUsdShadingModesTokens->materialConversion);
                if (mode == MaxUsdShadingModeTokens->useRegistry
                    && materialConversion == UsdImagingTokens->UsdPreviewSurface) {
                    // This is a legacy import settings file, with the default shading mode.
                    // Update the options to the current default.
                    opts.SetDefaultShadingModes();
                }
            }
        }
    }

    auto optionsDict = opts.GetOptions();
    // Update the version to the current one.
    optionsDict[MaxUsdMaxSceneBuilderOptionsTokens->version] = VtDictionaryGet<VtValue>(
        MaxSceneBuilderOptions::GetDefaultDictionary(),
        MaxUsdMaxSceneBuilderOptionsTokens->version);

    opts.SetOptions(MaxSceneBuilderOptions { optionsDict });
    return opts;
}

USDSceneBuilderOptions LoadExportOptions(const USDSceneBuilderOptions::Type& type)
{
    // Export to stage options should initialize to the current "regular" export options, with
    // only a different value for UseWorldSpaceRoot and root prim path.
    if (type == USDSceneBuilderOptions::Type::ToStage) {
        const auto exportSettingsPath = GetPathToUsdExportSettings(type);
        QFile      file(exportSettingsPath.GetString());
        if (!file.exists()) { // No saved options.
            USDSceneBuilderOptions opts = LoadExportOptions(USDSceneBuilderOptions::Type::ToFile);
            opts.SetUseWorldspaceRoot(true);
            opts.SetRootPrimPath(SdfPath { MaxUsdExportTokens->DEFAULT_PRIM });
            return opts;
        }
    }

    auto opts = LoadOptions<USDSceneBuilderOptions>(GetPathToUsdExportSettings(type));
    auto optionsDict = opts.GetOptions();

    // Previously, the default value for RootPrimPath was /root, but that was changed when exporting
    // to existing stages, to instead target the default prim. If we had options saved with "/root",
    // assume this was simply the default, and update it.
    if (type == USDSceneBuilderOptions::Type::ToStage) {
        if (VtDictionaryIsHolding<int>(optionsDict, MaxUsdMaxSceneBuilderOptionsTokens->version)) {
            const auto version
                = VtDictionaryGet<int>(optionsDict, MaxUsdUsdSceneBuilderOptionsTokens->version);
            if (version == 1) {
                optionsDict[MaxUsdUsdSceneBuilderOptionsTokens->rootPrimPath]
                    = SdfPath { MaxUsdExportTokens->DEFAULT_PRIM };
            }
        }
    }

    // Update the version to the current one.
    optionsDict[MaxUsdUsdSceneBuilderOptionsTokens->version] = VtDictionaryGet<VtValue>(
        USDSceneBuilderOptions::GetDefaultDictionary(),
        MaxUsdUsdSceneBuilderOptionsTokens->version);

    opts.SetOptions(USDSceneBuilderOptions { optionsDict });
    return opts;
}

void SaveExportToStageExtraOptions(VtDictionary& options)
{
    QJsonObject json;
    DictUtils::VtDictToJson(options, json);

    QJsonDocument doc(json);
    const auto    optsStr = doc.toJson().toStdString();
    const auto    filePath = GetPathToUsdExportToStageExtraSettings().GetString();
    QFile         file(filePath);
    WriteJsonFile(file, doc.toJson().toStdString(), filePath);
}

pxr::VtDictionary LoadExportToStageExtraOptions()
{
    const auto filePath = GetPathToUsdExportToStageExtraSettings().GetString();
    QFile      file(filePath);
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray data = file.readAll();
            file.close();

            return DeserializeOptionsFromJson(data);
        } else {
            TF_WARN("Failed to load options from : %s", MaxStringToUsdString(filePath));
        }
    }

    VtDictionary extra;
    extra.SetValueAtPath(MaxUsdExportTokens->allowPrimOverwrite, VtValue { true });
    extra.SetValueAtPath(MaxUsdExportTokens->inheritStageObjectTransform, VtValue { true });
    return extra;
}

} // namespace OptionUtils
} // namespace MAXUSD_NS_DEF