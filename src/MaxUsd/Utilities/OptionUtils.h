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
#pragma once
#include <MaxUsd/MaxUSDAPI.h>

#include <MaxUsd.h>

class QByteArray;
class pxr::VtDictionary;

namespace MAXUSD_NS_DEF {

class DictionaryOptionProvider;
class USDSceneBuilderOptions;
class MaxSceneBuilderOptions;

namespace OptionUtils {

// Get path to the MaxUsd settings folder
MaxUSDAPI MaxSDK::Util::Path GetPathToUSDSettings();
// Get path to the MaxUsd export settings file
MaxUSDAPI MaxSDK::Util::Path GetPathToUsdExportSettings();
// Get path to the MaxUsd import settings file
MaxUSDAPI MaxSDK::Util::Path GetPathToUsdImportSettings();
// Get path to the MaxUsd general settings file
MaxUSDAPI MaxSDK::Util::Path GetPathToUsdUiSettings();

/**
 * \brief Save the rollup states to the settings file, under the given category name.
 * @param category The name of the json object to save the rollups state under.
 * @param rollupStates A map of the rollup names and their state (open/close).
 */
MaxUSDAPI void
SaveRollupStates(const QString& category, const std::map<QString, bool>& rollupStates);
/**
 * \brief Get the state (open/close) of all the rollups saved under this category name.
 * \param category The name of the json object the rollups state were saved under.
 * \return A map of the rollup names and their state (open/close).
 */
MaxUSDAPI std::map<QString, bool> LoadRollupStates(const QString& category);

/**
 * \brief Serialize the options to a json string.
 * \param options The options to serialize.
 * \return A json string representing the options.
 */
MaxUSDAPI std::string SerializeOptionsToJson(const DictionaryOptionProvider& options);

/**
 * \brief Deserialize the options from a json string to a VtDictionary.
 * \param data The json string to deserialize.
 * \return The deserialized options as a VtDictionary.
 */
MaxUSDAPI pxr::VtDictionary DeserializeOptionsFromJson(const QByteArray& data);

/**
 * \brief Serialize the options to a json string and save it to the UI settings file.
 * \param category The name of the json object to save the options under.
 * \param dict The options to save.
 */
MaxUSDAPI void SaveUiOptions(const std::string& category, const pxr::VtDictionary& dict);

/**
 * \brief Load the options from the UI settings file under the given category name.
 * \param category The name of the json object the options were saved under.
 * \param dict The VtDictionary to load the options into.
 * \param guide The guide to enforce the type of the values in the dictionary,
 * if provided will also ensure that the guide entries are present.
 */
MaxUSDAPI void LoadUiOptions(
    const std::string&       category,
    pxr::VtDictionary&       dict,
    const pxr::VtDictionary& guide = pxr::VtDictionary());

/**
 * \brief Serialize and save the export options to disc.
 * \param options The USDSceneBuilderOptions object to save.
 */
MaxUSDAPI void SaveExportOptions(const USDSceneBuilderOptions& options);

/**
 * \brief Load and deserialize the export options from disc.
 * \return A USDSceneBuilderOptions object with the loaded options.
 */
MaxUSDAPI USDSceneBuilderOptions LoadExportOptions();

/**
 * \brief Serialize and save the import options to disc.
 * \param options The MaxSceneBuilderOptions object to save.
 */
MaxUSDAPI void SaveImportOptions(const MaxSceneBuilderOptions& options);

/**
 * \brief Load and deserialize the import options from disc.
 * @return A MaxSceneBuilderOptions object with the loaded options.
 */
MaxUSDAPI MaxSceneBuilderOptions LoadImportOptions();

} // namespace OptionUtils
} // namespace MAXUSD_NS_DEF