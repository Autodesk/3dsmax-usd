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

void MaxSceneBuilderOptionsWrapper::SetStageMaskPathsList(const boost::python::list& paths)
{
    std::vector<SdfPath> pathArray;
    try {
        for (int i = 0; i < len(paths); ++i) {
            pathArray.push_back(SdfPath(boost::python::extract<std::string>(paths[i])));
        }
    } catch (const boost::python::error_already_set&) {
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

boost::python::dict MaxSceneBuilderOptionsWrapper::GetAllChaserArgs() const
{
    boost::python::dict allChaserArgs;
    for (auto&& perChaser : MaxUsd::MaxSceneBuilderOptions::GetAllChaserArgs()) {
        auto perChaserDict = boost::python::dict();
        for (auto&& perItem : perChaser.second) {
            perChaserDict[perItem.first] = perItem.second;
        }
        allChaserArgs[perChaser.first] = perChaserDict;
    }
    return allChaserArgs;
}

void MaxSceneBuilderOptionsWrapper::SetAllChaserArgsFromDict(boost::python::dict args)
{
    std::map<std::string, ChaserArgs> allArgs;
    try {
        auto items = args.items();
        for (boost::python::ssize_t i = 0; i < boost::python::len(items); ++i) {
            std::string chaserKey = boost::python::extract<std::string>(items[i][0]);

            ChaserArgs chaserArgs;

            auto paramDict = boost::python::dict { items[i][1] };

            auto params = paramDict.items();
            for (boost::python::ssize_t i = 0; i < boost::python::len(params); ++i) {
                std::string name = boost::python::extract<std::string>(params[i][0]);
                std::string value = boost::python::extract<std::string>(params[i][1]);
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

void MaxSceneBuilderOptionsWrapper::SetAllChaserArgsFromList(boost::python::list args)
{
    static const std::string badArgMsg(
        "Badly formed list. Expecting 3 elements per argument entry (<chaser>, <key>, <value>).");

    if (boost::python::len(args) % 3) {
        throw std::invalid_argument(badArgMsg);
    }

    std::map<std::string, ChaserArgs> allArgs;
    try {

        for (int i = 0; i < len(args); i = i + 3) {
            const std::string chaser = boost::python::extract<std::string>(args[i]);
            const std::string param = boost::python::extract<std::string>(args[i + 1]);
            const std::string value = boost::python::extract<std::string>(args[i + 2]);
            ChaserArgs&       chaserArgs = allArgs[chaser];
            chaserArgs[param] = value;
        }
    } catch (...) {
        throw std::invalid_argument(badArgMsg);
    }
    MaxSceneBuilderOptions::SetAllChaserArgs(allArgs);
}

void MaxSceneBuilderOptionsWrapper::SetShadingModes(boost::python::list args)
{
    static const std::string badArgMsg("Badly formed list. Expecting a vector of dictionaries, "
                                       "each dictionary containing two entries, "
                                       "'materialConversion' and 'mode'.");

    ShadingModes shadingModes;
    try {
        for (int i = 0; i < len(args); ++i) {
            boost::python::dict dict = boost::python::extract<boost::python::dict>(args[i]);
            std::string         materialConversion
                = boost::python::extract<std::string>(dict["materialConversion"]);
            std::string  mode = boost::python::extract<std::string>(dict["mode"]);
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
};

void wrapMaxSceneBuilderOptions()
{
    using namespace boost::python;

    TfPyWrapEnum<MaxUsd::MaxSceneBuilderOptions::ImportTimeMode>("ImportTimeMode");
    // defined in wrapUSDSceneBuilderOptions
    // TfPyWrapEnum<MaxUsd::Log::Level>("LogLevel");

    boost::python::class_<MaxSceneBuilderOptionsWrapper> c(
        "MaxSceneBuilderOptions",
        "The class MaxSceneBuilderOptions which exposes the import arguments from the current "
        "import context.");
    c.def(init<const MaxSceneBuilderOptionsWrapper&>())
        .def(init<const std::string&>())
        .def(
            "GetTranslateMaterials",
            &MaxUsd::MaxSceneBuilderOptions::GetTranslateMaterials,
            boost::python::arg("self"),
            "Checks if the materials are imported back into 3ds Max")
        .def(
            "SetStageInitialLoadSet",
            &MaxUsd::MaxSceneBuilderOptions::SetStageInitialLoadSet,
            boost::python::args("self", "load_state"),
            "Sets the USD stage's initial load set to use for the import of content into 3ds Max")
        .def(
            "GetStageInitialLoadSet",
            &MaxUsd::MaxSceneBuilderOptions::GetStageInitialLoadSet,
            boost::python::arg("self"),
            "Gets the USD Stage initial load set to use for the import of content into 3ds Max")
        .def(
            "SetStartTimeCode",
            &MaxUsd::MaxSceneBuilderOptions::SetStartTimeCode,
            boost::python::args("self", "time_code"),
            "Set the Start Time Code of the time range the import of content into 3ds Max")
        .def(
            "GetStartTimeCode",
            &MaxUsd::MaxSceneBuilderOptions::GetStartTimeCode,
            boost::python::arg("self"),
            "Return the Start Time Code value of the time range to use for the import of content "
            "into 3ds Max")
        .def(
            "SetEndTimeCode",
            &MaxUsd::MaxSceneBuilderOptions::SetEndTimeCode,
            boost::python::args("self", "time_code"),
            "Set the End Time Code of the time range the import of content into 3ds Max")
        .def(
            "GetEndTimeCode",
            &MaxUsd::MaxSceneBuilderOptions::GetEndTimeCode,
            boost::python::arg("self"),
            "Return the End Time Code value of the time range to use for the import of content "
            "into 3ds Max")
        .def(
            "GetTimeMode",
            &MaxUsd::MaxSceneBuilderOptions::GetTimeMode,
            boost::python::arg("self"),
            "Return the ImportTimeMode value to use for the import of content into 3ds Max")
        .def(
            "SetTimeMode",
            &MaxUsd::MaxSceneBuilderOptions::SetTimeMode,
            boost::python::args("self", "time_mode"),
            "Set ImportTimeMode value to use for the import of content into 3ds Max")
        .def(
            "SetShadingModes",
            &MaxSceneBuilderOptionsWrapper::SetShadingModes,
            boost::python::args("self", "preferred_material"),
            "Sets the shading modes to use at import (see `ShadingMode` definition)")
        .def(
            "GetShadingModes",
            &MaxUsd::MaxSceneBuilderOptions::GetShadingModes,
            return_value_policy<TfPySequenceToList>(),
            boost::python::arg("self"),
            "Gets the shading modes to use at import.")
        .def(
            "SetPreferredMaterial",
            &MaxUsd::MaxSceneBuilderOptions::SetPreferredMaterial,
            boost::python::args("self", "preferred_material"),
            "Sets the user preferred material to convert to at import.")
        .def(
            "GetPreferredMaterial",
            &MaxUsd::MaxSceneBuilderOptions::GetPreferredMaterial,
            boost::python::arg("self"),
            "Gets the user preferred material to convert to at import.")
        .def(
            "SetStageMaskPaths",
            &MaxSceneBuilderOptionsWrapper::SetStageMaskPathsList,
            boost::python::args("self", "paths"),
            "Sets the stage mask's paths. Only USD prims at or below these paths will be imported.")
        .def(
            "GetStageMaskPaths",
            &MaxUsd::MaxSceneBuilderOptions::GetStageMaskPaths,
            return_value_policy<TfPySequenceToList>(),
            boost::python::arg("self"),
            "Returns the currently configured stage mask paths. Only USD prims at or below these "
            "paths will be imported.")
        .def(
            "SetMetaData",
            &MaxUsd::MaxSceneBuilderOptions::SetMetaData,
            boost::python::args("self", "filters"),
            "Sets the list of MaxUsd::MetaData::MetaDataType that will be included during import")
        .def(
            "GetMetaData",
            &MaxUsd::MaxSceneBuilderOptions::GetMetaData,
            return_value_policy<TfPySequenceToSet>(),
            boost::python::arg("self"),
            "Returns the list of MaxUsd::MetaData::MetaDataType that will be included during "
            "import.")
        .def(
            "GetLogPath",
            &MaxSceneBuilderOptionsWrapper::GetLogPath,
            boost::python::arg("self"),
            "Gets the path to the log file.")
        .def(
            "SetLogPath",
            &MaxSceneBuilderOptionsWrapper::SetLogPath,
            boost::python::args("self", "logPath"),
            "Sets the path to the log file.")
        .def(
            "GetLogLevel",
            &MaxSceneBuilderOptionsWrapper::GetLogLevel,
            boost::python::arg("self"),
            "Gets the log level (maxUsd.Log.Level).")
        .def(
            "SetLogLevel",
            &MaxSceneBuilderOptionsWrapper::SetLogLevel,
            boost::python::args("self", "logLevel"),
            "Sets the log level (maxUsd.Log.Level).")

        // PrimvarMappingOptions helpers
        .def(
            "SetPrimvarChannelMappingDefaults",
            &MaxSceneBuilderOptionsWrapper::SetPrimvarChannelMappingDefaults,
            boost::python::arg("self"),
            "Sets defaults primvar to channels mappings")
        .def(
            "GetImportUnmappedPrimvars",
            &MaxSceneBuilderOptionsWrapper::GetImportUnmappedPrimvars,
            boost::python::arg("self"),
            "Gets the channel name from a primvar.")
        .def(
            "SetImportUnmappedPrimvars",
            &MaxSceneBuilderOptionsWrapper::SetImportUnmappedPrimvars,
            (boost::python::arg("self"), boost::python::arg("import_unmapped_primvars")),
            "Sets whether or not to import primvars that are not explicitly mapped. If true, try "
            "to find the most appropriate channels for each unmapped primvar, based on their "
            "types.")
        .def(
            "SetPrimvarChannel",
            &MaxSceneBuilderOptionsWrapper::SetPrimvarChannel,
            (boost::python::arg("self"),
             boost::python::arg("primvar"),
             boost::python::arg("channel")),
            "Sets the channel of a primvar")
        .def(
            "GetPrimvarChannel",
            &MaxSceneBuilderOptionsWrapper::GetPrimvarChannel,
            (boost::python::arg("self"), boost::python::arg("primvar")),
            "Gets the channel name from a primvar.")
        .def(
            "GetMappedPrimvars",
            &MaxSceneBuilderOptionsWrapper::GetMappedPrimvars,
            boost::python::return_value_policy<TfPySequenceToList>(),
            boost::python::arg("self"),
            "Returns the list of all currently mapped primvars.")
        .def(
            "IsMappedPrimvar",
            &MaxSceneBuilderOptionsWrapper::IsMappedPrimvar,
            (boost::python::arg("self"), boost::python::arg("primvar")),
            "Checks if a primvar is currently mapped to a channel.")
        .def(
            "ClearMappedPrimvars",
            &MaxSceneBuilderOptionsWrapper::ClearMappedPrimvars,
            boost::python::arg("self"),
            "Clears all primvar mappings.")

        .def(
            "GetChaserNames",
            &MaxUsd::MaxSceneBuilderOptions::GetChaserNames,
            return_value_policy<TfPySequenceToSet>(),
            (boost::python::arg("self")),
            "Gets the list of import chasers to be called at USD import.")
        .def(
            "SetChaserNames",
            &MaxUsd::MaxSceneBuilderOptions::SetChaserNames,
            return_value_policy<TfPySequenceToSet>(),
            (boost::python::args("self", "chaserNames")),
            "Sets the list of import chasers to be called at USD import.")
        .def(
            "GetAllChaserArgs",
            &MaxSceneBuilderOptionsWrapper::GetAllChaserArgs,
            (boost::python::arg("self")),
            "Gets the dictionary of import chasers with their specified arguments.")
        .def(
            "SetAllChaserArgs",
            &MaxSceneBuilderOptionsWrapper::SetAllChaserArgsFromDict,
            (boost::python::args("self", "allChaserArgs")),
            "Sets the dictionary of import chasers with their specified arguments, from a "
            "dictionary.")
        .def(
            "SetAllChaserArgs",
            &MaxSceneBuilderOptionsWrapper::SetAllChaserArgsFromList,
            (boost::python::args("self", "allChaserArgs")),
            "Sets the dictionary of import chasers with their specified arguments, from a list.")
        .def(
            "GetContextNames",
            &MaxUsd::MaxSceneBuilderOptions::GetContextNames,
            return_value_policy<TfPySequenceToSet>(),
            (boost::python::arg("self")),
            "Gets the list of imports context being used at USD import.")
        .def(
            "SetContextNames",
            &MaxUsd::MaxSceneBuilderOptions::SetContextNames,
            (boost::python::arg("self")),
            "Sets the list of import contexts being used at USD import.")
        .def(
            "GetUseProgressBar",
            &MaxUsd::MaxSceneBuilderOptions::GetUseProgressBar,
            (boost::python::arg("self")),
            "Check if the 3ds Max progress bar should be used during export.")
        .def(
            "SetUseProgressBar",
            &MaxUsd::MaxSceneBuilderOptions::SetUseProgressBar,
            (boost::python::args("self", "useProgressBar")),
            "Sets if the 3ds Max progress bar should be used during export.")
        .def(
            "SetDefaults",
            &MaxUsd::MaxSceneBuilderOptions::SetDefaults,
            (boost::python::arg("self")),
            "Sets default options.")
        .def(
            "GetJobContextOptions",
            &MaxUsd::MaxSceneBuilderOptions::GetJobContextOptions,
            return_value_policy<TfPyMapToDictionary>(),
            (boost::python::arg("self"), boost::python::arg("jobContext")),
            "Gets the job context options for the given job context.")
        .def(
            "Serialize",
            &MaxSceneBuilderOptionsWrapper::Serialize,
            boost::python::arg("self"),
            "Serialize the options to JSON format");
}