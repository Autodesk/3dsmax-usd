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
#include "wrapMaxSceneBuilderOptions.h"

#include <MaxUsd/Utilities/OptionUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/base/tf/pyEnum.h>
#include <pxr/base/tf/pyResultConversions.h>
#include <pxr/base/tf/pyStaticTokens.h>
#include <pxr/base/tf/stringUtils.h>

#include <QByteArray>
#include <boost/python/class.hpp>
#include <boost/python/return_value_policy.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

MaxSceneBuilderOptionsWrapper::MaxSceneBuilderOptionsWrapper() { SetDefaults(); }

MaxSceneBuilderOptionsWrapper::MaxSceneBuilderOptionsWrapper(
    const MaxUsd::MaxSceneBuilderOptions& importArgs)
{
    SetOptions(importArgs);
}

MaxSceneBuilderOptionsWrapper::MaxSceneBuilderOptionsWrapper(const std::string& json)
{
    const QByteArray jsonBytes(json.c_str());
    SetOptions(MaxSceneBuilderOptions(MaxUsd::OptionUtils::DeserializeOptionsFromJson(jsonBytes)));
}

void MaxSceneBuilderOptionsWrapper::SetStageMaskPathsList(const pyboost::list& paths)
{
    std::vector<SdfPath> pathArray;
    try {
        for (int i = 0; i < len(paths); ++i) {
            pathArray.push_back(SdfPath(pyboost::extract<std::string>(paths[i])));
        }
    } catch (const pyboost::error_already_set&) {
        // rethrow error to Python
        throw;
    }
    SetStageMaskPaths(pathArray);
}

std::string MaxSceneBuilderOptionsWrapper::GetLogPath() const
{
    return __super::GetLogPath().u8string();
}

void MaxSceneBuilderOptionsWrapper::SetLogPath(const std::string& logPath)
{
    __super::SetLogPath(logPath);
}

void MaxSceneBuilderOptionsWrapper::SetPrimvarChannelMappingDefaults()
{
    auto primvarMapping = GetPrimvarMappingOptions();
    primvarMapping.SetDefaultPrimvarChannelMappings();
    SetPrimvarMappingOptions(primvarMapping);
}

bool MaxSceneBuilderOptionsWrapper::GetImportUnmappedPrimvars() const
{
    return GetPrimvarMappingOptions().GetImportUnmappedPrimvars();
}

void MaxSceneBuilderOptionsWrapper::SetImportUnmappedPrimvars(bool importUnmappedPrimvars)
{
    auto primvarMapping = GetPrimvarMappingOptions();
    primvarMapping.SetImportUnmappedPrimvars(importUnmappedPrimvars);
    SetPrimvarMappingOptions(primvarMapping);
}

void MaxSceneBuilderOptionsWrapper::SetPrimvarChannel(const std::string& primvarName, int channel)
{
    // Will throw on unmapped channels.
    if (!pxr::TfIsValidIdentifier(primvarName)) {
        const auto errorMsg = primvarName
            + std::string(" is not a valid primvar name. The name must start with a letter or "
                          "underscore, and must "
                          "contain only letters, underscores, and numerals..");
        throw std::runtime_error(errorMsg);
    }

    if (!MaxUsd::IsValidChannel(channel)) {
        const auto errorMsg
            = std::to_string(channel)
            + std::string(
                  " is not a valid map channel. Valid channels are from -2 to 99 inclusively.");
        throw std::runtime_error(errorMsg);
    }
    auto primvarMapping = GetPrimvarMappingOptions();
    primvarMapping.SetPrimvarChannelMapping(primvarName, channel);
    SetPrimvarMappingOptions(primvarMapping);
}

int MaxSceneBuilderOptionsWrapper::GetPrimvarChannel(const std::string& primvarName) const
{
    // Will throw on unmapped channels.
    if (!pxr::TfIsValidIdentifier(primvarName)) {
        const auto errorMsg = primvarName
            + std::string(" is not a valid primvar name. The name must start with a letter or "
                          "underscore, and must "
                          "contain only letters, underscores, and numerals..");
        throw std::runtime_error(errorMsg);
    }

    auto options = GetPrimvarMappingOptions();
    if (!options.IsMappedPrimvar(primvarName)) {
        return -1; // undefined
    }

    int channel = options.GetPrimvarChannelMapping(primvarName);
    if (channel == MaxUsd::PrimvarMappingOptions::invalidChannel) {
        return -1; // undefined
    }
    return channel;
}

std::vector<std::wstring> MaxSceneBuilderOptionsWrapper::GetMappedPrimvars() const
{
    std::vector<std::wstring> primvars;
    GetPrimvarMappingOptions().GetMappedPrimvars(primvars);
    return primvars;
}

bool MaxSceneBuilderOptionsWrapper::IsMappedPrimvar(const std::string& primvarName) const
{
    if (!pxr::TfIsValidIdentifier(primvarName)) {
        const auto errorMsg = primvarName
            + std::string(" is not a valid primvar name. The name must start with a letter or "
                          "underscore, and must "
                          "contain only letters, underscores, and numerals..");
        throw std::runtime_error(errorMsg);
    }
    return GetPrimvarMappingOptions().IsMappedPrimvar(primvarName);
}

void MaxSceneBuilderOptionsWrapper::ClearMappedPrimvars()
{
    auto primvarMapping = GetPrimvarMappingOptions();
    primvarMapping.ClearMappedPrimvars();
    SetPrimvarMappingOptions(primvarMapping);
}

pyboost::dict MaxSceneBuilderOptionsWrapper::GetAllChaserArgs() const
{
    pyboost::dict allChaserArgs;
    for (auto&& perChaser : MaxUsd::MaxSceneBuilderOptions::GetAllChaserArgs()) {
        auto perChaserDict = pyboost::dict();
        for (auto&& perItem : perChaser.second) {
            perChaserDict[perItem.first] = perItem.second;
        }
        allChaserArgs[perChaser.first] = perChaserDict;
    }
    return allChaserArgs;
}

void MaxSceneBuilderOptionsWrapper::SetAllChaserArgsFromDict(pyboost::dict args)
{
    std::map<std::string, ChaserArgs> allArgs;
    try {
        auto items = args.items();
        for (pyboost::ssize_t i = 0; i < pyboost::len(items); ++i) {
            std::string chaserKey = pyboost::extract<std::string>(items[i][0]);

            ChaserArgs chaserArgs;

            auto paramDict = pyboost::dict { items[i][1] };

            auto params = paramDict.items();
            for (pyboost::ssize_t i = 0; i < pyboost::len(params); ++i) {
                std::string name = pyboost::extract<std::string>(params[i][0]);
                std::string value = pyboost::extract<std::string>(params[i][1]);
                chaserArgs.insert({ name, value });
            }
            allArgs.insert({ chaserKey, chaserArgs });
        }
    } catch (...) {
        throw std::invalid_argument(
            std::string("Badly formed dictionary. Expecting the form : {'chaser' : {'param' : "
                        "'val', 'param1' : 'val2'}, 'chaser2' : {'param2' : 'val3'}}."));
    }
    MaxSceneBuilderOptions::SetAllChaserArgs(allArgs);
}

void MaxSceneBuilderOptionsWrapper::SetAllChaserArgsFromList(pyboost::list args)
{
    static const std::string badArgMsg(
        "Badly formed list. Expecting 3 elements per argument entry (<chaser>, <key>, <value>).");

    if (pyboost::len(args) % 3) {
        throw std::invalid_argument(badArgMsg);
    }

    std::map<std::string, ChaserArgs> allArgs;
    try {

        for (int i = 0; i < len(args); i = i + 3) {
            const std::string chaser = pyboost::extract<std::string>(args[i]);
            const std::string param = pyboost::extract<std::string>(args[i + 1]);
            const std::string value = pyboost::extract<std::string>(args[i + 2]);
            ChaserArgs&       chaserArgs = allArgs[chaser];
            chaserArgs[param] = value;
        }
    } catch (...) {
        throw std::invalid_argument(badArgMsg);
    }
    MaxSceneBuilderOptions::SetAllChaserArgs(allArgs);
}

void MaxSceneBuilderOptionsWrapper::SetShadingModes(pyboost::list args)
{
    static const std::string badArgMsg("Badly formed list. Expecting a vector of dictionaries, "
                                       "each dictionary containing two entries, "
                                       "'materialConversion' and 'mode'.");

    ShadingModes shadingModes;
    try {
        for (int i = 0; i < len(args); ++i) {
            pyboost::dict dict = pyboost::extract<pyboost::dict>(args[i]);
            std::string   materialConversion
                = pyboost::extract<std::string>(dict["materialConversion"]);
            std::string  mode = pyboost::extract<std::string>(dict["mode"]);
            VtDictionary shadingMode({ { "materialConversion", VtValue(materialConversion) },
                                       { "mode", VtValue(mode) } });
            shadingModes.push_back(shadingMode);
        }
    } catch (...) {
        throw std::invalid_argument(badArgMsg);
    }
    __super::SetShadingModes(shadingModes);
}

std::string MaxSceneBuilderOptionsWrapper::Serialize()
{
    return MaxUsd::OptionUtils::SerializeOptionsToJson(*this);
}

TF_REGISTRY_FUNCTION(TfEnum)
{
    TF_ADD_ENUM_NAME(MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::AllRange);
    TF_ADD_ENUM_NAME(MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::CustomRange);
    TF_ADD_ENUM_NAME(MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::EndTime);
    TF_ADD_ENUM_NAME(MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::StartTime);
    
    TF_ADD_ENUM_NAME(MaxUsd::MaxSceneBuilderOptions::SlateMaterialHandling::Off);
    TF_ADD_ENUM_NAME(MaxUsd::MaxSceneBuilderOptions::SlateMaterialHandling::UnboundMaterials);
    TF_ADD_ENUM_NAME(MaxUsd::MaxSceneBuilderOptions::SlateMaterialHandling::AllMaterials);
};

void wrapMaxSceneBuilderOptions()
{
    TfPyWrapEnum<MaxUsd::MaxSceneBuilderOptions::ImportTimeMode>("ImportTimeMode");
    TfPyWrapEnum<MaxUsd::MaxSceneBuilderOptions::SlateMaterialHandling>("SlateMaterialHandling");
    // defined in wrapUSDSceneBuilderOptions
    // TfPyWrapEnum<MaxUsd::Log::Level>("LogLevel");

    pyboost::class_<MaxSceneBuilderOptionsWrapper> c(
        "MaxSceneBuilderOptions",
        "The class MaxSceneBuilderOptions which exposes the import arguments from the current "
        "import context.");
    c.def(pyboost::init<const MaxSceneBuilderOptionsWrapper&>())
        .def(pyboost::init<const std::string&>())
        .def(
            "GetTranslateMaterials",
            &MaxUsd::MaxSceneBuilderOptions::GetTranslateMaterials,
            pyboost::arg("self"),
            "Checks if the materials are imported back into 3ds Max")
        .def(
            "SetStageInitialLoadSet",
            &MaxUsd::MaxSceneBuilderOptions::SetStageInitialLoadSet,
            pyboost::args("self", "load_state"),
            "Sets the USD stage's initial load set to use for the import of content into 3ds Max")
        .def(
            "GetStageInitialLoadSet",
            &MaxUsd::MaxSceneBuilderOptions::GetStageInitialLoadSet,
            pyboost::arg("self"),
            "Gets the USD Stage initial load set to use for the import of content into 3ds Max")
        .def(
            "SetStartTimeCode",
            &MaxUsd::MaxSceneBuilderOptions::SetStartTimeCode,
            pyboost::args("self", "time_code"),
            "Set the Start Time Code of the time range the import of content into 3ds Max")
        .def(
            "GetStartTimeCode",
            &MaxUsd::MaxSceneBuilderOptions::GetStartTimeCode,
            pyboost::arg("self"),
            "Return the Start Time Code value of the time range to use for the import of content "
            "into 3ds Max")
        .def(
            "SetEndTimeCode",
            &MaxUsd::MaxSceneBuilderOptions::SetEndTimeCode,
            pyboost::args("self", "time_code"),
            "Set the End Time Code of the time range the import of content into 3ds Max")
        .def(
            "GetEndTimeCode",
            &MaxUsd::MaxSceneBuilderOptions::GetEndTimeCode,
            pyboost::arg("self"),
            "Return the End Time Code value of the time range to use for the import of content "
            "into 3ds Max")
        .def(
            "GetTimeMode",
            &MaxUsd::MaxSceneBuilderOptions::GetTimeMode,
            pyboost::arg("self"),
            "Return the ImportTimeMode value to use for the import of content into 3ds Max")
        .def(
            "SetTimeMode",
            &MaxUsd::MaxSceneBuilderOptions::SetTimeMode,
            pyboost::args("self", "time_mode"),
            "Set ImportTimeMode value to use for the import of content into 3ds Max")
        .def(
            "SetShadingModes",
            &MaxSceneBuilderOptionsWrapper::SetShadingModes,
            pyboost::args("self", "preferred_material"),
            "Sets the shading modes to use at import (see `ShadingMode` definition)")
        .def(
            "GetShadingModes",
            &MaxUsd::MaxSceneBuilderOptions::GetShadingModes,
            pyboost::return_value_policy<TfPySequenceToList>(),
            pyboost::arg("self"),
            "Gets the shading modes to use at import.")
        .def(
            "SetPreferredMaterial",
            &MaxUsd::MaxSceneBuilderOptions::SetPreferredMaterial,
            pyboost::args("self", "preferred_material"),
            "Sets the user preferred material to convert to at import.")
        .def(
            "GetPreferredMaterial",
            &MaxUsd::MaxSceneBuilderOptions::GetPreferredMaterial,
            pyboost::arg("self"),
            "Gets the user preferred material to convert to at import.")
        .def(
            "SetStageMaskPaths",
            &MaxSceneBuilderOptionsWrapper::SetStageMaskPathsList,
            pyboost::args("self", "paths"),
            "Sets the stage mask's paths. Only USD prims at or below these paths will be imported.")
        .def(
            "GetStageMaskPaths",
            &MaxUsd::MaxSceneBuilderOptions::GetStageMaskPaths,
            pyboost::return_value_policy<pxr::TfPySequenceToList>(),
            pyboost::arg("self"),
            "Returns the currently configured stage mask paths. Only USD prims at or below these "
            "paths will be imported.")
        .def(
            "SetMetaData",
            &MaxUsd::MaxSceneBuilderOptions::SetMetaData,
            pyboost::args("self", "filters"),
            "Sets the list of MaxUsd::MetaData::MetaDataType that will be included during import")
        .def(
            "GetMetaData",
            &MaxUsd::MaxSceneBuilderOptions::GetMetaData,
            pyboost::return_value_policy<pxr::TfPySequenceToSet>(),
            pyboost::arg("self"),
            "Returns the list of MaxUsd::MetaData::MetaDataType that will be included during "
            "import.")
        .def(
            "GetLogPath",
            &MaxSceneBuilderOptionsWrapper::GetLogPath,
            pyboost::arg("self"),
            "Gets the path to the log file.")
        .def(
            "SetLogPath",
            &MaxSceneBuilderOptionsWrapper::SetLogPath,
            pyboost::args("self", "logPath"),
            "Sets the path to the log file.")
        .def(
            "GetLogLevel",
            &MaxSceneBuilderOptionsWrapper::GetLogLevel,
            pyboost::arg("self"),
            "Gets the log level (maxUsd.Log.Level).")
        .def(
            "SetLogLevel",
            &MaxSceneBuilderOptionsWrapper::SetLogLevel,
            pyboost::args("self", "logLevel"),
            "Sets the log level (maxUsd.Log.Level).")

        // PrimvarMappingOptions helpers
        .def(
            "SetPrimvarChannelMappingDefaults",
            &MaxSceneBuilderOptionsWrapper::SetPrimvarChannelMappingDefaults,
            pyboost::arg("self"),
            "Sets defaults primvar to channels mappings")
        .def(
            "GetImportUnmappedPrimvars",
            &MaxSceneBuilderOptionsWrapper::GetImportUnmappedPrimvars,
            pyboost::arg("self"),
            "Gets the channel name from a primvar.")
        .def(
            "SetImportUnmappedPrimvars",
            &MaxSceneBuilderOptionsWrapper::SetImportUnmappedPrimvars,
            (pyboost::arg("self"), pyboost::arg("import_unmapped_primvars")),
            "Sets whether or not to import primvars that are not explicitly mapped. If true, try "
            "to find the most appropriate channels for each unmapped primvar, based on their "
            "types.")
        .def(
            "SetPrimvarChannel",
            &MaxSceneBuilderOptionsWrapper::SetPrimvarChannel,
            (pyboost::arg("self"), pyboost::arg("primvar"), pyboost::arg("channel")),
            "Sets the channel of a primvar")
        .def(
            "GetPrimvarChannel",
            &MaxSceneBuilderOptionsWrapper::GetPrimvarChannel,
            (pyboost::arg("self"), pyboost::arg("primvar")),
            "Gets the channel name from a primvar.")
        .def(
            "GetMappedPrimvars",
            &MaxSceneBuilderOptionsWrapper::GetMappedPrimvars,
            pyboost::return_value_policy<TfPySequenceToList>(),
            pyboost::arg("self"),
            "Returns the list of all currently mapped primvars.")
        .def(
            "IsMappedPrimvar",
            &MaxSceneBuilderOptionsWrapper::IsMappedPrimvar,
            (pyboost::arg("self"), pyboost::arg("primvar")),
            "Checks if a primvar is currently mapped to a channel.")
        .def(
            "ClearMappedPrimvars",
            &MaxSceneBuilderOptionsWrapper::ClearMappedPrimvars,
            pyboost::arg("self"),
            "Clears all primvar mappings.")

        .def(
            "GetChaserNames",
            &MaxUsd::MaxSceneBuilderOptions::GetChaserNames,
            pyboost::return_value_policy<pxr::TfPySequenceToSet>(),
            (pyboost::arg("self")),
            "Gets the list of import chasers to be called at USD import.")
        .def(
            "SetChaserNames",
            &MaxUsd::MaxSceneBuilderOptions::SetChaserNames,
            pyboost::return_value_policy<pxr::TfPySequenceToSet>(),
            (pyboost::args("self", "chaserNames")),
            "Sets the list of import chasers to be called at USD import.")
        .def(
            "GetAllChaserArgs",
            &MaxSceneBuilderOptionsWrapper::GetAllChaserArgs,
            (pyboost::arg("self")),
            "Gets the dictionary of import chasers with their specified arguments.")
        .def(
            "SetAllChaserArgs",
            &MaxSceneBuilderOptionsWrapper::SetAllChaserArgsFromDict,
            (pyboost::args("self", "allChaserArgs")),
            "Sets the dictionary of import chasers with their specified arguments, from a "
            "dictionary.")
        .def(
            "SetAllChaserArgs",
            &MaxSceneBuilderOptionsWrapper::SetAllChaserArgsFromList,
            (pyboost::args("self", "allChaserArgs")),
            "Sets the dictionary of import chasers with their specified arguments, from a list.")
        .def(
            "GetContextNames",
            &MaxUsd::MaxSceneBuilderOptions::GetContextNames,
            pyboost::return_value_policy<pxr::TfPySequenceToSet>(),
            (pyboost::arg("self")),
            "Gets the list of imports context being used at USD import.")
        .def(
            "SetContextNames",
            &MaxUsd::MaxSceneBuilderOptions::SetContextNames,
            (pyboost::arg("self")),
            "Sets the list of import contexts being used at USD import.")
        .def(
            "GetUseProgressBar",
            &MaxUsd::MaxSceneBuilderOptions::GetUseProgressBar,
            (pyboost::arg("self")),
            "Check if the 3ds Max progress bar should be used during export.")
        .def(
            "SetUseProgressBar",
            &MaxUsd::MaxSceneBuilderOptions::SetUseProgressBar,
            (pyboost::args("self", "useProgressBar")),
            "Sets if the 3ds Max progress bar should be used during export.")
        .def(
            "GetSlateMaterialHandling",
            &MaxUsd::MaxSceneBuilderOptions::GetSlateMaterialHandling,
            (pyboost::arg("self")),
            "Get the Slate material handling mode.")
        .def(
            "SetSlateMaterialHandling",
            &MaxUsd::MaxSceneBuilderOptions::SetSlateMaterialHandling,
            (pyboost::args("self", "slateMaterialHandling")),
            "Set the Slate material handling mode.")
        .def(
            "SetDefaults",
            &MaxUsd::MaxSceneBuilderOptions::SetDefaults,
            (pyboost::arg("self")),
            "Sets default options.")
        .def(
            "GetJobContextOptions",
            &MaxUsd::MaxSceneBuilderOptions::GetJobContextOptions,
            pyboost::return_value_policy<pxr::TfPyMapToDictionary>(),
            (pyboost::arg("self"), pyboost::arg("jobContext")),
            "Gets the job context options for the given job context.")
        .def(
            "Serialize",
            &MaxSceneBuilderOptionsWrapper::Serialize,
            pyboost::arg("self"),
            "Serialize the options to JSON format");
}