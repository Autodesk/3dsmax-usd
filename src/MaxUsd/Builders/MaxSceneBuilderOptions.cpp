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

#include "MaxSceneBuilderOptions.h"

#include <MaxUsd/Translators/ShadingModeRegistry.h>
#include <MaxUsd/Utilities/MetaDataUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/VtDictionaryUtils.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/usdcFileFormat.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

using namespace MaxUsd;
PXR_NAMESPACE_USING_DIRECTIVE

PXR_NAMESPACE_OPEN_SCOPE
TF_DEFINE_PUBLIC_TOKENS(MaxUsdMaxSceneBuilderOptionsTokens, PXR_MAXUSD_MAX_SCENE_BUILDER_TOKENS);
TF_DEFINE_PUBLIC_TOKENS(MaxUsdShadingModesTokens, PXR_MAXUSD_SHADING_MODES_TOKENS);
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAXUSD_NS_DEF {

MaxSceneBuilderOptions::MaxSceneBuilderOptions()
{
    // default settings must be applied manually
    // it implies loading the USD plugin material conversion types
    // which must take place outside the DLL initialization
}

MaxSceneBuilderOptions::MaxSceneBuilderOptions(const VtDictionary& dict)
{
    VtDictionary tmp = dict;
    DictUtils::CoerceDictToGuideType(tmp, MaxSceneBuilderOptions::GetDefaultDictionary());

    if (VtDictionaryIsHolding<ShadingModes>(
            tmp, MaxUsdMaxSceneBuilderOptionsTokens->shadingModes)) {
        auto shadingModes
            = VtDictionaryGet<ShadingModes>(tmp, MaxUsdMaxSceneBuilderOptionsTokens->shadingModes);
        for (VtDictionary& shadingMode : shadingModes) {
            DictUtils::CoerceDictToGuideType(shadingMode, GetShadingModeDefaultDictionary());
        }
        tmp.SetValueAtPath(MaxUsdMaxSceneBuilderOptionsTokens->shadingModes, VtValue(shadingModes));
    }

    options = VtDictionaryOver(tmp, GetDefaultDictionary());
}

namespace {
const std::string importUnmappedPrimvarsKey
    = MaxUsdMaxSceneBuilderOptionsTokens->primvarMappingOptions.GetString() + ":"
    + MaxUsdPrimvarMappingOptions->importUnmappedPrimvars.GetString();
} // namespace

/* static */
const pxr::VtDictionary& MaxSceneBuilderOptions::GetDefaultDictionary()
{
    static VtDictionary   defaultDict;
    static std::once_flag once;
    std::call_once(once, []() {
        defaultDict[MaxUsdMaxSceneBuilderOptionsTokens->version] = 1;
        // Base defaults.
        defaultDict[MaxUsdMaxSceneBuilderOptionsTokens->initialLoadSet]
            = static_cast<int>(UsdStage::LoadAll);
        defaultDict[MaxUsdMaxSceneBuilderOptionsTokens->timeMode]
            = static_cast<int>(ImportTimeMode::AllRange);
        defaultDict[MaxUsdMaxSceneBuilderOptionsTokens->stageMaskPaths]
            = std::vector<SdfPath> { SdfPath::AbsoluteRootPath() };
        defaultDict[MaxUsdMaxSceneBuilderOptionsTokens->metaDataIncludes]
            = std::set<int> { MetaData::Kind, MetaData::Purpose, MetaData::Hidden };
        defaultDict[MaxUsdMaxSceneBuilderOptionsTokens->preferredMaterial]
            = MaxUsdPreferredMaterialTokens->none;
        defaultDict[MaxUsdMaxSceneBuilderOptionsTokens->useProgressBar] = true;

        defaultDict[MaxUsdSceneBuilderOptionsTokens->contextNames] = std::set<std::string>();
        defaultDict[MaxUsdSceneBuilderOptionsTokens->jobContextOptions] = VtDictionary();
        defaultDict[MaxUsdSceneBuilderOptionsTokens->chaserNames] = std::vector<std::string>();
        defaultDict[MaxUsdSceneBuilderOptionsTokens->chaserArgs]
            = std::map<std::string, ChaserArgs>();

        defaultDict[MaxUsdSceneBuilderOptionsTokens->logLevel] = static_cast<int>(Log::Level::Off);

        defaultDict[MaxUsdMaxSceneBuilderOptionsTokens->primvarMappingOptions]
            = PrimvarMappingOptions().GetOptions();
        defaultDict.SetValueAtPath(importUnmappedPrimvarsKey, VtValue(true));
        defaultDict[MaxUsdMaxSceneBuilderOptionsTokens->shadingModes] = ShadingModes();

        defaultDict[MaxUsdMaxSceneBuilderOptionsTokens->startTimeCode] = 0.0;
        defaultDict[MaxUsdMaxSceneBuilderOptionsTokens->endTimeCode] = 0.0;
    });
    // Purposefully left out of the call_once in order to always fetch the latest value for
    // "APP_TEMP_DIR", it might change during the session.
    defaultDict[MaxUsdSceneBuilderOptionsTokens->logPath]
        = fs::path(std::wstring(MaxSDKSupport::GetString(GetCOREInterface()->GetDir(APP_TEMP_DIR)))
                       .append(L"\\MaxUsdImport.log"));
    // Purposefully left out the call_once, as new shading modes might be added during the session.
    SetDefaultShadingModes(defaultDict);

    return defaultDict;
}

/* static */
const pxr::VtDictionary& MaxSceneBuilderOptions::GetShadingModeDefaultDictionary()
{
    static VtDictionary   defaultDict;
    static std::once_flag once;
    std::call_once(once, []() {
        // Base defaults.
        defaultDict[MaxUsdShadingModesTokens->mode] = MaxUsdShadingModeTokens->useRegistry;
        defaultDict[MaxUsdShadingModesTokens->materialConversion]
            = UsdImagingTokens->UsdPreviewSurface;
    });

    return defaultDict;
}

void MaxSceneBuilderOptions::SetOptions(const MaxSceneBuilderOptions& options)
{
    this->options = options.options;
}

void MaxSceneBuilderOptions::SetDefaults() { options = GetDefaultDictionary(); }

void MaxSceneBuilderOptions::SetDefaultShadingModes(VtDictionary& dict)
{
    ShadingModes shadingModes;

    // Define the lambda to simplify the emplace_back operation
    auto addShadingMode = [&shadingModes](const TfToken& mode, const TfToken& materialConversion) {
        shadingModes.emplace_back(VtDictionary {
            { MaxUsdShadingModesTokens->mode, VtValue(mode) },
            { MaxUsdShadingModesTokens->materialConversion, VtValue(materialConversion) } });
    };

    for (const auto& c : MaxUsdShadingModeRegistry::ListMaterialConversions()) {
        if (c != UsdImagingTokens->UsdPreviewSurface) {
            auto const& info = MaxUsdShadingModeRegistry::GetMaterialConversionInfo(c);
            if (info.hasImporter) {
                addShadingMode(MaxUsdShadingModeTokens->useRegistry, c);
            }
        }
    }
    for (const auto& s : MaxUsdShadingModeRegistry::ListImporters()) {
        if (s != MaxUsdShadingModeTokens->useRegistry) {
            addShadingMode(s, MaxUsdShadingModeTokens->none);
        }
    }
    addShadingMode(MaxUsdShadingModeTokens->useRegistry, UsdImagingTokens->UsdPreviewSurface);

    dict.SetValueAtPath(MaxUsdMaxSceneBuilderOptionsTokens->shadingModes, VtValue(shadingModes));
}

void MaxSceneBuilderOptions::SetDefaultShadingModes() { SetDefaultShadingModes(options); }

void MaxSceneBuilderOptions::SetStageInitialLoadSet(UsdStage::InitialLoadSet initialLoadSet)
{
    options[MaxUsdMaxSceneBuilderOptionsTokens->initialLoadSet] = static_cast<int>(initialLoadSet);
}

pxr::UsdStage::InitialLoadSet MaxSceneBuilderOptions::GetStageInitialLoadSet() const
{
    return static_cast<UsdStage::InitialLoadSet>(
        VtDictionaryGet<int>(options, MaxUsdMaxSceneBuilderOptionsTokens->initialLoadSet));
}

void MaxSceneBuilderOptions::SetTimeMode(const ImportTimeMode& timeMode)
{
    options[MaxUsdMaxSceneBuilderOptionsTokens->timeMode] = static_cast<int>(timeMode);
}

MaxSceneBuilderOptions::ImportTimeMode MaxSceneBuilderOptions::GetTimeMode() const
{
    return static_cast<ImportTimeMode>(
        VtDictionaryGet<int>(options, MaxUsdMaxSceneBuilderOptionsTokens->timeMode));
}

void MaxSceneBuilderOptions::SetStartTimeCode(double startTimeCode)
{
    options[MaxUsdMaxSceneBuilderOptionsTokens->startTimeCode] = startTimeCode;
    if (GetEndTimeCode() < startTimeCode) {
        SetEndTimeCode(startTimeCode);
    }
}

void MaxSceneBuilderOptions::SetEndTimeCode(double endTimeCode)
{
    options[MaxUsdMaxSceneBuilderOptionsTokens->endTimeCode] = endTimeCode;
    if (GetStartTimeCode() > endTimeCode) {
        SetStartTimeCode(endTimeCode);
    }
}

double MaxSceneBuilderOptions::GetStartTimeCode() const
{
    return VtDictionaryGet<double>(options, MaxUsdMaxSceneBuilderOptionsTokens->startTimeCode);
}

double MaxSceneBuilderOptions::GetEndTimeCode() const
{
    return VtDictionaryGet<double>(options, MaxUsdMaxSceneBuilderOptionsTokens->endTimeCode);
}

MaxUsd::ImportTimeConfig
MaxSceneBuilderOptions::GetResolvedTimeConfig(const pxr::UsdStagePtr& stage) const
{
    MaxUsd::ImportTimeConfig resultTimeConfig {};
    if (!stage) {
        return resultTimeConfig;
    }

    double startTime = 0;
    double endTime = 0;
    switch (GetTimeMode()) {
    case MaxSceneBuilderOptions::ImportTimeMode::AllRange: {
        // this has to be set for this case so that the user's don't have to manually do this
        startTime = stage->GetStartTimeCode();
        endTime = stage->GetEndTimeCode();
        break;
    }
    case MaxSceneBuilderOptions::ImportTimeMode::CustomRange: {
        ImportTimeConfig timeConfig(GetStartTimeCode(), GetEndTimeCode());
        startTime = timeConfig.GetStartTimeCode();
        endTime = timeConfig.GetEndTimeCode();
        break;
    }
    case MaxSceneBuilderOptions::ImportTimeMode::StartTime: {
        startTime = stage->GetStartTimeCode();
        endTime = stage->GetStartTimeCode();
        break;
    }
    case MaxSceneBuilderOptions::ImportTimeMode::EndTime: {
        startTime = stage->GetEndTimeCode();
        endTime = stage->GetEndTimeCode();
        break;
    }
    default: DbgAssert(_T("Unhandled TimeMode mode type. Importing using default values.")); break;
    }

    resultTimeConfig.SetStartTimeCode(startTime);
    resultTimeConfig.SetEndTimeCode(endTime);

    return resultTimeConfig;
}

void MaxSceneBuilderOptions::SetStageMaskPaths(const std::vector<pxr::SdfPath>& paths)
{
    options[MaxUsdMaxSceneBuilderOptionsTokens->stageMaskPaths] = paths;
}

const std::vector<pxr::SdfPath>& MaxSceneBuilderOptions::GetStageMaskPaths() const
{
    return VtDictionaryGet<std::vector<pxr::SdfPath>>(
        options, MaxUsdMaxSceneBuilderOptionsTokens->stageMaskPaths);
}

void MaxSceneBuilderOptions::SetMetaData(const std::set<int>& filters)
{
    options[MaxUsdMaxSceneBuilderOptionsTokens->metaDataIncludes] = filters;
}

const std::set<int>& MaxSceneBuilderOptions::GetMetaData() const
{
    return VtDictionaryGet<std::set<int>>(
        options, MaxUsdMaxSceneBuilderOptionsTokens->metaDataIncludes);
}

PrimvarMappingOptions MaxSceneBuilderOptions::GetPrimvarMappingOptions() const
{
    return PrimvarMappingOptions(VtDictionaryGet<VtDictionary>(
        options, MaxUsdMaxSceneBuilderOptionsTokens->primvarMappingOptions));
}

void MaxSceneBuilderOptions::SetPrimvarMappingOptions(
    const PrimvarMappingOptions& primvarMappingOptions)
{
    options.SetValueAtPath(
        MaxUsdMaxSceneBuilderOptionsTokens->primvarMappingOptions,
        VtValue(primvarMappingOptions.GetOptions()));
}

bool MaxSceneBuilderOptions::GetTranslateMaterials() const
{
    const auto& shadingModes = GetShadingModes();
    // if the ShadingModes is not set to 'none', materials are imported
    auto modesIter
        = std::find_if(shadingModes.begin(), shadingModes.end(), [](const VtDictionary& sm) {
              if (VtDictionaryIsHolding<TfToken>(sm, MaxUsdShadingModesTokens->mode)) {
                  return VtDictionaryGet<TfToken>(sm, MaxUsdShadingModesTokens->mode)
                      == MaxUsdShadingModeTokens->none;
              }
              return false;
          });
    return modesIter == shadingModes.end();
}

void MaxSceneBuilderOptions::SetShadingModes(const ShadingModes& modes)
{
    auto SetNoneShadingMode = [this]() {
        options.SetValueAtPath(
            MaxUsdMaxSceneBuilderOptionsTokens->shadingModes,
            VtValue(ShadingModes({ VtDictionary {
                { MaxUsdShadingModesTokens->mode, VtValue(MaxUsdShadingModeTokens->none) },
                { MaxUsdShadingModesTokens->materialConversion,
                  VtValue(MaxUsdPreferredMaterialTokens->none) } } })));
    };
    if (modes.empty()) {
        SetNoneShadingMode();
        return;
    }
    if (modes.size() > 1) {
        auto modesIter = std::find_if(modes.begin(), modes.end(), [](const VtDictionary& sm) {
            if (VtDictionaryIsHolding<TfToken>(sm, MaxUsdShadingModesTokens->mode)) {
                return VtDictionaryGet<TfToken>(sm, MaxUsdShadingModesTokens->mode)
                    == MaxUsdShadingModeTokens->none;
            }
            return false;
        });
        if (modesIter != modes.end()) {
            Log::Error(
                "Cannot set multiple import ShadingModes when one of those modes is set to 'none'. "
                "Keeping only 'none' as the import ShadingMode - no material will get imported.");
            SetNoneShadingMode();
            return;
        }
    }
    options.SetValueAtPath(MaxUsdMaxSceneBuilderOptionsTokens->shadingModes, VtValue(modes));
}

const MaxSceneBuilderOptions::ShadingModes& MaxSceneBuilderOptions::GetShadingModes() const
{
    return VtDictionaryGet<ShadingModes>(options, MaxUsdMaxSceneBuilderOptionsTokens->shadingModes);
}

pxr::TfToken MaxSceneBuilderOptions::GetMaterialConversion() const
{
    const auto& shadingModes = GetShadingModes();
    return shadingModes.empty()
        ? TfToken()
        : VtDictionaryGet<TfToken>(
              shadingModes.front(), MaxUsdShadingModesTokens->materialConversion);
}

void MaxSceneBuilderOptions::SetPreferredMaterial(const pxr::TfToken& targetMaterial)
{
    options[MaxUsdMaxSceneBuilderOptionsTokens->preferredMaterial] = targetMaterial;
}

pxr::TfToken MaxSceneBuilderOptions::GetPreferredMaterial() const
{
    return VtDictionaryGet<TfToken>(
        options,
        MaxUsdMaxSceneBuilderOptionsTokens->preferredMaterial,
        VtDefault = MaxUsdPreferredMaterialTokens->none);
}

MaxSceneBuilderOptions MaxSceneBuilderOptions::OptionsWithAppliedContexts() const
{
    auto thisCopy = *this;

    VtDictionary allContextArgs;
    if (MergeJobContexts(false, this->GetContextNames(), allContextArgs)) {
        if (!allContextArgs.empty()) {
            if (allContextArgs.count(MaxUsdSceneBuilderOptionsTokens->chaserNames) > 0) {
                auto copyChaserNames = thisCopy.GetChaserNames();
                if (copyChaserNames.empty()) {
                    copyChaserNames = DictUtils::ExtractVector<std::string>(
                        allContextArgs, MaxUsdSceneBuilderOptionsTokens->chaserNames);
                } else // merge args
                {
                    auto values = DictUtils::ExtractVector<std::string>(
                        allContextArgs, MaxUsdSceneBuilderOptionsTokens->chaserNames);
                    for (auto element : values) {
                        if (std::find(copyChaserNames.begin(), copyChaserNames.end(), element)
                            == copyChaserNames.end()) {
                            copyChaserNames.push_back(element);
                        }
                    }
                }
                thisCopy.SetChaserNames(copyChaserNames);
            }
            if (allContextArgs.count(MaxUsdSceneBuilderOptionsTokens->chaserArgs) > 0) {
                auto copyChaserArgs = thisCopy.GetAllChaserArgs();
                if (copyChaserArgs.empty()) {
                    copyChaserArgs = ExtractChaserArgs(
                        allContextArgs, MaxUsdSceneBuilderOptionsTokens->chaserArgs);
                } else // merge args
                {
                    auto values = ExtractChaserArgs(
                        allContextArgs, MaxUsdSceneBuilderOptionsTokens->chaserArgs);
                    for (auto element : values) {
                        if (copyChaserArgs.find(element.first) == copyChaserArgs.end()) {
                            copyChaserArgs[element.first] = element.second;
                        } else {
                            auto& currentChaserArgs = copyChaserArgs[element.first];
                            for (auto arg : element.second) {
                                if (currentChaserArgs.find(arg.first) == currentChaserArgs.end()) {
                                    currentChaserArgs[arg.first] = arg.second;
                                } else {
                                    if (currentChaserArgs[arg.first] != arg.second) {
                                        TF_WARN(TfStringPrintf(
                                            "Multiple argument value for '%s' associated to chaser "
                                            "'%s'. Keeping "
                                            "the argument value set to '%s' from Context.",
                                            arg.first.c_str(),
                                            element.first.c_str(),
                                            arg.second));
                                        // take the argument from the context, and forget the user's
                                        currentChaserArgs[arg.first] = arg.second;
                                    }
                                }
                            }
                        }
                    }
                }
                thisCopy.SetAllChaserArgs(copyChaserArgs);
            }
        }
    } else {
        MaxUsd::Log::Error("Errors while processing import contexts. Using base import options.");
    }

    return thisCopy;
}

bool MaxSceneBuilderOptions::GetUseProgressBar() const
{
    return VtDictionaryGet<bool>(
        options, MaxUsdMaxSceneBuilderOptionsTokens->useProgressBar, VtDefault = true);
}

void MaxSceneBuilderOptions::SetUseProgressBar(bool useProgressBar)
{
    options[MaxUsdMaxSceneBuilderOptionsTokens->useProgressBar] = useProgressBar;
}

} // namespace MAXUSD_NS_DEF
