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

#include "PreferenceApplicationHost.h"
#include "PreferencesDialog.h"

#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/OptionUtils.h>
#include <MaxUsd/Utilities/VtDictionaryUtils.h>

#ifdef IS_MAX2026_OR_GREATER
#include <AssetResolverPreferences/AssetResolverSettings.h>
#include <AssetResolverPreferences/AssetResolverSettingsManagement.h>
#endif

#include <pxr/base/vt/dictionary.h>

#include <Qt/QmaxMainWindow.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <maxapi.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace PreferencesManagement {

MaxSDK::Util::Path GetPathToUsdPreferences()
{
    auto pathToUsdSettings = MaxUsd::OptionUtils::GetPathToUSDSettings();
    pathToUsdSettings.Append(_T("\\usdPreferences.json"));
    return pathToUsdSettings;
}

#ifdef IS_MAX2026_OR_GREATER
VtDictionary LoadUsdPreferences()
{
    QFile       file(GetPathToUsdPreferences().GetString());
    QJsonObject json;

    VtDictionary guide = Adsk::AssetResolverSettings::GetDefaultSettings();

    if (file.exists()) {
        if (!MaxUsd::OptionUtils::ReadJsonFile(json, file, GetPathToUsdPreferences().GetCStr())) {
            return guide;
        }

        VtDictionary  dict;
        QJsonDocument doc(json);
        QString       strJson(doc.toJson());
        auto          stdStr = strJson.toStdString();
        MaxUsd::DictUtils::VtDictFromString(stdStr, dict);
        MaxUsd::DictUtils::CoerceDictToGuideType(dict, guide);
        return VtDictionaryOver(dict, guide);
    }
    return guide;
}

void SaveUsdPreferences(const Adsk::AssetResolverSettings& options)
{
    // update the options instance
    // the copy clears env search paths as they are not saved
    // those are only used to display the paths in the dialog
    Adsk::AssetResolverSettings::GetInstance() = options;

    // save options to disk
    QJsonObject json;
    MaxUsd::DictUtils::VtDictToJson(Adsk::AssetResolverSettings::GetInstance().GetSettings(), json);

    QFile file(GetPathToUsdPreferences().GetString());
    MaxUsd::OptionUtils::WriteJsonFile(
        file, QJsonDocument(json).toJson().toStdString(), GetPathToUsdPreferences().GetCStr());
}

void InitializeUsdPreferences()
{
    // Add ApplicationHost for the USD Preferences dialog
    PreferenceApplicationHost::CreateInstance(GetCOREInterface()->GetQmaxMainWindow());
    // Load USD Preference options to ensure the Adsk Asset Resolver works as configured
    Adsk::AssetResolverSettings::GetInstance().SetSettings(LoadUsdPreferences());
    Adsk::AssetResolverSettingsManagement::ApplySettings(
        Adsk::AssetResolverSettings(), Adsk::AssetResolverSettings::GetInstance());
}
#endif
void ShowPreferencesDialog()
{
    std::unique_ptr<UsdPreferencesDialog> usdPreferencesDialog
        = std::make_unique<UsdPreferencesDialog>(GetCOREInterface()->GetQmaxMainWindow());

    QRect savedGeometry = QRect(-1, -1, -1, -1);
    {
        VtDictionary dict;
        MaxUsd::OptionUtils::LoadUiOptions("USD Preferences", dict);
        auto           it = dict.find("Dialog Geometry");
        VtArray<float> val = { -1.0, -1.0, -1.0, -1.0 };
        if (it != dict.end()) {
            if (it->second.IsHolding<VtArray<float>>()) {
                val = it->second.GetWithDefault<VtArray<float>>(val);
            } else if (it->second.CanCast<VtArray<float>>()) {
                val = it->second.Cast<VtArray<float>>().GetWithDefault<VtArray<float>>(val);
            }
        }
        if (val.size() == 4 && val[2] >= 0.0f) {
            savedGeometry = QRect(
                MaxSDK::UIScaled(val[0]),
                MaxSDK::UIScaled(val[1]),
                MaxSDK::UIScaled(val[2]),
                MaxSDK::UIScaled(val[3]));
            usdPreferencesDialog->setGeometry(savedGeometry);
        }
    }

    QRect dialogGeometry = savedGeometry;
    QObject::connect(
        usdPreferencesDialog.get(),
        &UsdPreferencesDialog::geometryChanged,
        [&dialogGeometry](const QRect& geometry) { dialogGeometry = geometry; });

    QObject::connect(
        usdPreferencesDialog.get(), &QDialog::finished, [&dialogGeometry, &savedGeometry]() {
            if (dialogGeometry != savedGeometry) {
                VtDictionary   dict;
                VtArray<float> val
                    = { MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.left())),
                        MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.top())),
                        MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.width())),
                        MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.height())) };
                dict["Dialog Geometry"] = val;
                MaxUsd::OptionUtils::SaveUiOptions("USD Preferences", dict);
            }
        });

#ifdef IS_MAX2026_OR_GREATER
    if (usdPreferencesDialog->exec() == QDialog::Accepted) {
        auto newOptions = usdPreferencesDialog->getOptions();
        Adsk::AssetResolverSettingsManagement::ApplySettings(
            Adsk::AssetResolverSettings::GetInstance(), newOptions);
        SaveUsdPreferences(newOptions);
    }
#endif
}

} // namespace PreferencesManagement
