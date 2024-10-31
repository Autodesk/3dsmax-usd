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
#include "IUSDImportOptions.h"

#include <USDImport/USDImport.h>

#include <MaxUsd/Chaser/ImportChaserRegistry.h>
#include <MaxUsd/Utilities/MetaDataUtils.h>
#include <MaxUsd/Utilities/MxsUtils.h>
#include <MaxUsd/Utilities/OptionUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <maxscript/foundation/MXSDictionaryValue.h>
#include <maxscript/foundation/arrays.h>
#include <maxscript/foundation/numbers.h>
#include <maxscript/foundation/strings.h>

namespace MAXUSD_NS_DEF {

// clang-format off
FPInterfaceDesc IUSDImportOptionsDesc(
	IUSDImportOptions_INTERFACE_ID, _T("IUSDImportOptions"), 0, NULL, FP_MIXIN,
	// Functions
	IUSDImportOptions::fnIdReset, _T("Reset"), "Reset to defaults options", TYPE_VOID, FP_NO_REDRAW, 0,
	IUSDImportOptions::fidSerialize, _T("Serialize"), "Serialize the options to a JSON formatted string.", TYPE_STRING, FP_NO_REDRAW, 0,
	IUSDImportOptions::fnIdGetTranslateMaterials, _T("Materials"), "Returns if materials should be translated", TYPE_BOOL, FP_NO_REDRAW, 0,
	IUSDImportOptions::fnIdSetPrimvarChannelMappingDefaults, _T("SetPrimvarChannelMappingDefaults"), "Reset to defaults primvar to channel mappings.", TYPE_VOID, FP_NO_REDRAW, 0,
	IUSDImportOptions::fnIdSetPrimvarChannelMapping, _T("SetPrimvarChannelMapping"), "Sets a primvar to channel mapping", TYPE_VOID, FP_NO_REDRAW, 2,
		_T("primvar"), 0, TYPE_STRING,
		_T("targetChannel"), 0, TYPE_VALUE,
	IUSDImportOptions::fnIdGetPrimvarChannel, _T("GetPrimvarChannel"), "Returns the channel the given primvar should map too.", TYPE_VALUE, FP_NO_REDRAW, 1,
		_T("primvar"), 0, TYPE_STRING,
	IUSDImportOptions::fnIdIsMappedPrimvar, _T("IsMappedPrimvar"), "Returns whether this primvar is mapped to a channel.", TYPE_BOOL, FP_NO_REDRAW, 1,
		_T("primvar"), 0, TYPE_STRING,
	IUSDImportOptions::fnIdGetMappedPrimvars, _T("GetMappedPrimvars"), "Returns the list of currently mapped primvars.", TYPE_STRING_TAB_BV, FP_NO_REDRAW, 0,
	IUSDImportOptions::fnIdClearMappedPrimvars, _T("ClearMappedPrimvars"), "Clears all primvar to channel mappings.", TYPE_VOID, FP_NO_REDRAW, 0,
	IUSDImportOptions::fnIdGetAvailableChasers, _T("AvailableChasers"), "Returns an array of all available import chasers", TYPE_TSTR_TAB_BV, FP_NO_REDRAW, 0, properties,
	IUSDImportOptions::fnIdGetChaserNames, IUSDImportOptions::fnIdSetChaserNames, _T("ChaserNames"), 0, TYPE_TSTR_TAB_BV,
	IUSDImportOptions::fnIdGetAllChaserArgs, IUSDImportOptions::fnIdSetAllChaserArgs, _T("AllChaserArgs"), 0, TYPE_VALUE,
	IUSDImportOptions::fnIdGetContextNames, IUSDImportOptions::fnIdSetContextNames, _T("ContextNames"), 0, TYPE_TSTR_TAB_BV,
	IUSDImportOptions::fnIdGetStageMask, IUSDImportOptions::fnIdSetStageMask, _T("StageMask"), FP_NO_REDRAW, TYPE_STRING_TAB,
	IUSDImportOptions::fnIdGetMetaDataIncludes, IUSDImportOptions::fnIdSetMetaDataIncludes, _T("MetaData"), FP_NO_REDRAW, TYPE_ENUM_TAB_BV, IUSDImportOptions::eIdMetaData,
	IUSDImportOptions::fnIdGetStartTimeCode, IUSDImportOptions::fnIdSetStartTimeCode, _T("StartTimeCode"), FP_NO_REDRAW, TYPE_DOUBLE,
	IUSDImportOptions::fnIdGetEndTimeCode, IUSDImportOptions::fnIdSetEndTimeCode, _T("EndTimeCode"), FP_NO_REDRAW, TYPE_DOUBLE,
	IUSDImportOptions::fnIdGetTimeMode, IUSDImportOptions::fnIdSetTimeMode, _T("TimeMode"), FP_NO_REDRAW, TYPE_ENUM, IUSDImportOptions::eIdTimeMode,
	IUSDImportOptions::fnIdGetInitialLoadSet, IUSDImportOptions::fnIdSetInitialLoadSet, _T("InitialLoadSet"), FP_NO_REDRAW, TYPE_ENUM, IUSDImportOptions::eIdInitialLoadSet,
	IUSDImportOptions::fnIdGetLogPath, IUSDImportOptions::fnIdSetLogPath, _T("LogPath"), FP_NO_REDRAW, TYPE_STRING,
	IUSDImportOptions::fnIdGetLogLevel, IUSDImportOptions::fnIdSetLogLevel, _T("LogLevel"), FP_NO_REDRAW, TYPE_ENUM, IUSDImportOptions::eIdLogLevel,
	IUSDImportOptions::fnIdGetImportUnmappedPrimvars, IUSDImportOptions::fnIdSetImportUnmappedPrimvars, _T("ImportUnmappedPrimvars"), FP_NO_REDRAW, TYPE_BOOL, 
	IUSDImportOptions::fnIdGetUseProgressBar, IUSDImportOptions::fnIdSetUseProgressBar, _T("UseProgressBar"), 0, TYPE_BOOL, 
	IUSDImportOptions::fnIdGetPreferredMaterial, IUSDImportOptions::fnIdSetPreferredMaterial, _T("PreferredMaterial"), 0, TYPE_STRING,
	IUSDImportOptions::fnIdGetShadingModes, IUSDImportOptions::fnIdSetShadingModes, _T("ShadingModes"), 0, TYPE_VALUE, enums,
	IUSDImportOptions::eIdTimeMode, 4,
		_T("AllRange"), MaxSceneBuilderOptions::ImportTimeMode::AllRange,
		_T("CustomRange"), MaxSceneBuilderOptions::ImportTimeMode::CustomRange,
		_T("StartTime"), MaxSceneBuilderOptions::ImportTimeMode::StartTime,
		_T("EndTime"), MaxSceneBuilderOptions::ImportTimeMode::EndTime,
	IUSDImportOptions::eIdInitialLoadSet, 2,
		_T("loadAll"), pxr::UsdStage::InitialLoadSet::LoadAll,
		_T("loadNone"), pxr::UsdStage::InitialLoadSet::LoadNone,
	IUSDImportOptions::eIdLogLevel, 4,
		_T("off"), MaxUsd::Log::Level::Off,
		_T("info"), MaxUsd::Log::Level::Info,
		_T("warn"), MaxUsd::Log::Level::Warn,
		_T("error"), MaxUsd::Log::Level::Error,
	IUSDImportOptions::eIdMetaData, 3,
		_T("kind"), MaxUsd::MetaData::Kind,
		_T("purpose"), MaxUsd::MetaData::Purpose,
		_T("hidden"), MaxUsd::MetaData::Hidden,
	p_end
);
// clang-format on

IUSDImportOptions::IUSDImportOptions()
{
    // default settings must be applied manually
    // it implies loading the USD plugin material conversion types
    // which must take place outside the DLL initialization
}

IUSDImportOptions::IUSDImportOptions(const IUSDImportOptions& options) { SetOptions(options); }

IUSDImportOptions::IUSDImportOptions(const MaxSceneBuilderOptions& options) { SetOptions(options); }

const IUSDImportOptions& IUSDImportOptions::operator=(const IUSDImportOptions& options)
{
    SetOptions(options);
    return *this;
}

Tab<const wchar_t*> IUSDImportOptions::GetStageMaskPaths() const
{
    auto& paths = MaxSceneBuilderOptions::GetStageMaskPaths();
    // Build the tab
    Tab<const wchar_t*> tab;
    tab.SetCount(int(paths.size()));
    for (size_t i = 0; i < paths.size(); ++i) {
        tab[i] = MaxUsd::UsdStringToMaxString(paths[i].GetString()).data();
    }
    // RVO.
    return tab;
}

void IUSDImportOptions::SetStageMaskPaths(Tab<const wchar_t*> value)
{
    std::vector<pxr::SdfPath> paths;
    for (int i = 0; i < value.Count(); i++) {
        const auto& path = pxr::SdfPath(MaxUsd::MaxStringToUsdString(value[i]).c_str());
        // Make sure the given paths are valid.
        if (!path.IsAbsolutePath() || !path.IsAbsoluteRootOrPrimPath()) {
            const auto errorMsg
                = std::wstring(
                      L"Stage mask could not be set. Invalid USD absolute prim path found : ")
                      .append(value[i]);
            throw RuntimeError(errorMsg.c_str());
        }
        paths.emplace_back(path);
    }
    MaxSceneBuilderOptions::SetStageMaskPaths(paths);
};

Tab<int> IUSDImportOptions::GetMetaDataIncludes() const
{
    auto& metaDataIncludes = GetMetaData();
    // Build the tab
    Tab<int> tab;
    int      size = int(metaDataIncludes.size());
    for (int incl : metaDataIncludes) {
        tab.Append(1, &incl, size);
    }
    return tab;
};

void IUSDImportOptions::SetMetaDataIncludes(Tab<int> value)
{
    std::set<int> metaDataIncludes;
    for (int i = 0; i < value.Count(); i++) {
        metaDataIncludes.emplace(value[i]);
    }
    SetMetaData(metaDataIncludes);
}

void IUSDImportOptions::SetTimeMode(int value)
{
    const auto timeMode = static_cast<ImportTimeMode>(value);
    // Make sure the value is valid.
    if (timeMode != ImportTimeMode::AllRange && timeMode != ImportTimeMode::CustomRange
        && timeMode != ImportTimeMode::EndTime && timeMode != ImportTimeMode::StartTime) {
        const WStr errorMsg(
            L"Incorrect TimeMode select for import. Accepted values are #AllRange, #CustomRange, "
            L"#StartTime and #EndTime.");
        throw RuntimeError(errorMsg.data());
    }
    MaxSceneBuilderOptions::SetTimeMode(timeMode);
}

int IUSDImportOptions::GetTimeMode() const
{
    return static_cast<int>(MaxSceneBuilderOptions::GetTimeMode());
}

void IUSDImportOptions::SetInitialLoadSet(int value)
{
    const auto loadSet = pxr::UsdStage::InitialLoadSet(value);
    // Make sure the value is valid.
    if (loadSet != pxr::UsdStage::InitialLoadSet::LoadAll
        && loadSet != pxr::UsdStage::InitialLoadSet::LoadNone) {
        WStr errorMsg(
            L"Incorrect InitialLoadSet value. Accepted values are #loadAll and #loadNone.");
        throw RuntimeError(errorMsg.data());
    }
    SetStageInitialLoadSet(loadSet);
}

int IUSDImportOptions::GetInitialLoadSet() { return GetStageInitialLoadSet(); }

void IUSDImportOptions::SetPrimvarChannelMappingDefaults()
{
    auto mappingOptions = GetPrimvarMappingOptions();
    mappingOptions.SetDefaultPrimvarChannelMappings();
    SetPrimvarMappingOptions(mappingOptions);
}

bool IUSDImportOptions::GetImportUnmappedPrimvars() const
{
    return GetPrimvarMappingOptions().GetImportUnmappedPrimvars();
}

void IUSDImportOptions::SetImportUnmappedPrimvars(bool importUnmappedPrimvars)
{
    auto mappingOptions = GetPrimvarMappingOptions();
    mappingOptions.SetImportUnmappedPrimvars(importUnmappedPrimvars);
    SetPrimvarMappingOptions(mappingOptions);
}

void IUSDImportOptions::SetPrimvarChannelMapping(const wchar_t* primvarName, Value* channel)
{
    PrimvarMappingOptions mappingOptions = GetPrimvarMappingOptions();
    MaxUsd::mxs::SetPrimvarChannelMapping(mappingOptions, primvarName, channel);
    SetPrimvarMappingOptions(mappingOptions.GetOptions());
}

Value* IUSDImportOptions::GetPrimvarChannel(const wchar_t* primvarName)
{
    return MaxUsd::mxs::GetPrimvarChannel(GetPrimvarMappingOptions(), primvarName);
}

Tab<const wchar_t*> IUSDImportOptions::GetMappedPrimvars() const
{
    return MaxUsd::mxs::GetMappedPrimvars(GetPrimvarMappingOptions());
}

bool IUSDImportOptions::IsMappedPrimvar(const wchar_t* primvarName)
{
    return MaxUsd::mxs::IsMappedPrimvar(GetPrimvarMappingOptions(), primvarName);
}

void IUSDImportOptions::ClearMappedPrimvars()
{
    auto mappingOptions = GetPrimvarMappingOptions();
    mappingOptions.ClearMappedPrimvars();
    SetPrimvarMappingOptions(mappingOptions);
}

void IUSDImportOptions::SetShadingModes(Value* shadingModesValue)
{
    if (is_array(shadingModesValue)) {
        MaxSceneBuilderOptions::ShadingModes shadingModes;
        Array* shadingModesArray = dynamic_cast<Array*>(shadingModesValue);
        for (int i = 0; i < shadingModesArray->size; ++i) {
            auto item = (*shadingModesArray)[i];
            if (is_array(item)) {
                Array* shadingModePair = dynamic_cast<Array*>(item);
                if (shadingModePair->size != 2) {
                    throw RuntimeError(L"Badly formed ShadingMode pair. Expecting 2 elements "
                                       L"(<mode>, <materialConversion>).");
                }
                Value* mode = (*shadingModePair)[0];
                Value* materialConversion = (*shadingModePair)[1];
                if (!is_string(mode) || !is_string(materialConversion)) {
                    throw RuntimeError(L"Badly formed dictionary entry. Expecting material "
                                       L"conversion string for shading mode.");
                }
                shadingModes.push_back(pxr::VtDictionary {
                    { pxr::MaxUsdShadingModesTokens->mode,
                      pxr::VtValue(pxr::TfToken(MaxUsd::MaxStringToUsdString(mode->to_string()))) },
                    { pxr::MaxUsdShadingModesTokens->materialConversion,
                      pxr::VtValue(pxr::TfToken(
                          MaxUsd::MaxStringToUsdString(materialConversion->to_string()))) } });
            }
        }

        MaxSceneBuilderOptions::SetShadingModes(shadingModes);
    }
}

Value* IUSDImportOptions::GetShadingModes()
{
    shadingModesMxsHolder.set_value(
        new Array(static_cast<int>(MaxSceneBuilderOptions::GetShadingModes().size())));

    ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;
    MAXScript_TLS*                   _tls = scopedMaxScriptEvaluationContext.Get_TLS();
    four_typed_value_locals_tls(
        Array * shadingModesArray,
        Array * shadingModePair,
        Value * mode,
        Value * materialConversion)

        vl.shadingModesArray
        = dynamic_cast<Array*>(shadingModesMxsHolder.get_value());

    for (const auto& shadingMode : MaxSceneBuilderOptions::GetShadingModes()) {
        vl.mode = new String(
            MaxUsd::UsdStringToMaxString(
                pxr::VtDictionaryGet<pxr::TfToken>(shadingMode, pxr::MaxUsdShadingModesTokens->mode)
                    .GetString())
                .data());
        vl.materialConversion
            = new String(MaxUsd::UsdStringToMaxString(
                             pxr::VtDictionaryGet<pxr::TfToken>(
                                 shadingMode, pxr::MaxUsdShadingModesTokens->materialConversion)
                                 .GetString())
                             .data());
        vl.shadingModePair = new Array(0);
        vl.shadingModePair->append(vl.mode);
        vl.shadingModePair->append(vl.materialConversion);
        vl.shadingModesArray->append(vl.shadingModePair);
    }
    return shadingModesMxsHolder.get_value();
}

void IUSDImportOptions::SetPreferredMaterial(const wchar_t* targetMaterial)
{
    BaseClass::SetPreferredMaterial(pxr::TfToken(MaxUsd::MaxStringToUsdString(targetMaterial)));
}

const wchar_t* IUSDImportOptions::GetPreferredMaterial() const
{
    return (MaxUsd::UsdStringToMaxString(BaseClass::GetPreferredMaterial()).data());
}

Tab<TSTR*> IUSDImportOptions::GetAvailableChasers() const
{
    Tab<TSTR*> chaserArray;
    auto       chasers = pxr::MaxUsdImportChaserRegistry::GetAllRegisteredChasers();
    for (const auto& chaser : chasers) {
        auto mxsCopy = new TSTR(UsdStringToMaxString(chaser.GetString()));
        chaserArray.Append(1, &mxsCopy);
    }
    return chaserArray;
}

void IUSDImportOptions::SetChaserNamesMxs(const Tab<TSTR*>& chaserArray)
{
    std::vector<std::string> chaserNames;
    for (int i = 0; i < chaserArray.Count(); ++i) {
        chaserNames.push_back(MaxUsd::MaxStringToUsdString(chaserArray[i]->data()));
    }
    SetChaserNames(chaserNames);
}

Tab<TSTR*> IUSDImportOptions::GetChaserNamesMxs() const
{
    Tab<TSTR*> chaserArray;
    for (const auto& chaserName : MaxSceneBuilderOptions::GetChaserNames()) {
        auto mxsCopy = new TSTR(UsdStringToMaxString(chaserName));
        chaserArray.Append(1, &mxsCopy);
    }
    return chaserArray;
}

void IUSDImportOptions::SetAllChaserArgs(Value* chaserArgsValue)
{
    if (is_dictionary(chaserArgsValue)) {
        MXSDictionaryValue* dict = dynamic_cast<MXSDictionaryValue*>(chaserArgsValue);
        Array*              chasers = dict->getKeys();
        auto                allChaserArgs = MaxSceneBuilderOptions::GetAllChaserArgs();
        for (int i = 0; i < chasers->size; ++i) {
            Value* chaserName = (*chasers)[i];
            Value* chaserArgs = dict->get(chaserName);
            if (!is_dictionary(chaserArgs)) {
                throw RuntimeError(
                    L"Badly formed dictionary entry. Expecting Dictionary to for arguments.");
            }

            ChaserArgs          args;
            MXSDictionaryValue* dictArgs = dynamic_cast<MXSDictionaryValue*>(chaserArgs);
            Array*              argKeys = dictArgs->getKeys();

            for (int j = 0; j < argKeys->size; ++j) {
                Value* argKey = (*argKeys)[j];
                Value* argValue = dictArgs->get(argKey);

                args[MaxUsd::MaxStringToUsdString(argKey->to_string())]
                    = MaxUsd::MaxStringToUsdString(argValue->to_string());
            }
            allChaserArgs[MaxStringToUsdString(chaserName->to_string())] = args;
        }
        MaxSceneBuilderOptions::SetAllChaserArgs(allChaserArgs);
    } else if (is_array(chaserArgsValue)) {
        auto   allChaserArgs = MaxSceneBuilderOptions::GetAllChaserArgs();
        Array* argsArray = dynamic_cast<Array*>(chaserArgsValue);
        if (argsArray->size % 3) {
            throw RuntimeError(L"Badly formed Array. Expecting 3 elements per argument entry "
                               L"(<chaser>, <key>, <value>).");
        }
        for (int i = 0; i < argsArray->size; i = i + 3) {
            Value* chaserName = (*argsArray)[i];
            Value* argKey = (*argsArray)[i + 1];
            Value* argValue = (*argsArray)[i + 2];

            ChaserArgs& chaserArgs = allChaserArgs[MaxStringToUsdString(chaserName->to_string())];
            chaserArgs[MaxUsd::MaxStringToUsdString(argKey->to_string())]
                = MaxUsd::MaxStringToUsdString(argValue->to_string());
        }
        MaxSceneBuilderOptions::SetAllChaserArgs(allChaserArgs);
    } else {
        throw RuntimeError(
            L"Invalid parameter type provided. Expecting a maxscript Dictionary or Array.");
    }
}

Value* IUSDImportOptions::GetAllChaserArgs()
{
    if (allChaserArgsMxsHolder.get_value() == nullptr) {
        allChaserArgsMxsHolder.set_value(new MXSDictionaryValue(MXSDictionaryValue::key_string));
    }

    ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;
    MAXScript_TLS*                   _tls = scopedMaxScriptEvaluationContext.Get_TLS();
    five_typed_value_locals_tls(
        MXSDictionaryValue * allChaserArgsDict,
        MXSDictionaryValue * argsDict,
        Value * chaserName,
        Value * argKey,
        Value * argValue)

        vl.allChaserArgsDict
        = dynamic_cast<MXSDictionaryValue*>(allChaserArgsMxsHolder.get_value());
    // remove the previous args if any
    vl.allChaserArgsDict->free();
    for (const auto& chaserArgs : MaxSceneBuilderOptions::GetAllChaserArgs()) {
        vl.argsDict = new MXSDictionaryValue(MXSDictionaryValue::key_string);
        vl.chaserName = new String(MaxUsd::UsdStringToMaxString(chaserArgs.first).data());
        for (const auto& chaserArgsValue : chaserArgs.second) {
            vl.argKey = new String(MaxUsd::UsdStringToMaxString(chaserArgsValue.first).data());
            vl.argValue = new String(MaxUsd::UsdStringToMaxString(chaserArgsValue.second).data());
            vl.argsDict->put(vl.argKey, vl.argValue);
        }
        vl.allChaserArgsDict->put(vl.chaserName, vl.argsDict);
    }
    return allChaserArgsMxsHolder.get_value();
}

void IUSDImportOptions::SetContextNamesMxs(const Tab<TSTR*>& contextArray)
{
    std::set<std::string> contextNames;
    for (int i = 0; i < contextArray.Count(); ++i) {
        contextNames.insert(MaxUsd::MaxStringToUsdString(contextArray[i]->data()));
    }
    SetContextNames(contextNames);
}

Tab<TSTR*> IUSDImportOptions::GetContextNamesMxs() const
{
    Tab<TSTR*> contextArray;
    for (const auto& contextName : MaxSceneBuilderOptions::GetContextNames()) {
        auto mxsCopy = new TSTR(UsdStringToMaxString(contextName));
        contextArray.Append(1, &mxsCopy);
    }
    return contextArray;
}

const MCHAR* IUSDImportOptions::Serialize() const
{
    auto        jsonString = OptionUtils::SerializeOptionsToJson(*this);
    static WStr maxString;
    maxString = UsdStringToMaxString(jsonString);
    return maxString.data();
}

FPInterfaceDesc* IUSDImportOptions::GetDesc() { return &IUSDImportOptionsDesc; }

} // namespace MAXUSD_NS_DEF
