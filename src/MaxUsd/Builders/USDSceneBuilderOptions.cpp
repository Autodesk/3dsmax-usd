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

#include "USDSceneBuilderOptions.h"

#include <MaxUsd/Translators/ShadingModeRegistry.h>
#include <MaxUsd/USDCore.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/VtDictionaryUtils.h>

#include <pxr/UsdImaging/UsdImaging/tokens.h>
#include <pxr/base/tf/diagnostic.h>

PXR_NAMESPACE_USING_DIRECTIVE

PXR_NAMESPACE_OPEN_SCOPE
TF_DEFINE_PUBLIC_TOKENS(MaxUsdUsdSceneBuilderOptionsTokens, PXR_MAXUSD_USD_SCENE_BUILDER_TOKENS);
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAXUSD_NS_DEF {

const double USDSceneBuilderOptions::MIN_SAMPLES_PER_FRAME = 0.01;
const double USDSceneBuilderOptions::MAX_SAMPLES_PER_FRAME = 100.0;

USDSceneBuilderOptions::USDSceneBuilderOptions() noexcept { SetDefaults(); }

USDSceneBuilderOptions::USDSceneBuilderOptions(const pxr::VtDictionary& dict) noexcept
    : animationRollupData()
{
    options = dict;
    DictUtils::CoerceDictToGuideType(options, GetDefaultDictionary());

    // Since the Coercion is not recursive, it's needed to coerce the nested dictionaries as well,
    // making sure that they are valid.
    if (VtDictionaryIsHolding<VtDictionary>(
            options, MaxUsdUsdSceneBuilderOptionsTokens->meshConversionOptions)) {
        VtDictionary meshConversionOptions = VtDictionaryGet<VtDictionary>(
            options, MaxUsdUsdSceneBuilderOptionsTokens->meshConversionOptions);
        DictUtils::CoerceDictToGuideType(
            meshConversionOptions, MaxMeshConversionOptions().GetOptions());

        if (VtDictionaryIsHolding<VtDictionary>(
                meshConversionOptions, MaxUsdMaxMeshConversionOptions->channelToPrimvarConfig)) {
            VtDictionary channelToPrimvarConfig = VtDictionaryGet<VtDictionary>(
                meshConversionOptions, MaxUsdMaxMeshConversionOptions->channelToPrimvarConfig);

            for (auto& entry : channelToPrimvarConfig) {
                VtDictionary channel = entry.second.Get<VtDictionary>();
                const auto&  defaultChannel = VtDictionaryGet<VtDictionary>(
                    MaxMeshConversionOptions::GetDefaultChannelPrimvarMappings(), entry.first);
                DictUtils::CoerceDictToGuideType(channel, defaultChannel);
                VtDictionaryOver(channel, defaultChannel);
                channelToPrimvarConfig.SetValueAtPath(entry.first, VtValue(channel));
            }

            VtDictionaryOver(
                channelToPrimvarConfig,
                MaxMeshConversionOptions::GetDefaultChannelPrimvarMappings());
            meshConversionOptions.SetValueAtPath(
                MaxUsdMaxMeshConversionOptions->channelToPrimvarConfig,
                VtValue(channelToPrimvarConfig));
        }

        options.SetValueAtPath(
            MaxUsdUsdSceneBuilderOptionsTokens->meshConversionOptions,
            VtValue(meshConversionOptions));
    }
    options = VtDictionaryOver(options, GetDefaultDictionary());
}

void USDSceneBuilderOptions::SetOptions(const USDSceneBuilderOptions& options)
{

    convertMaterialsTo = options.convertMaterialsTo;
    animationRollupData = options.animationRollupData;

    this->options = options.options;
}

void USDSceneBuilderOptions::SetDefaults() { options = GetDefaultDictionary(); }

/* static */
const pxr::VtDictionary& USDSceneBuilderOptions::GetDefaultDictionary()
{
    static VtDictionary   defaultDict;
    static std::once_flag once;
    std::call_once(once, []() {
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->version] = 1;
        // Base defaults.
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->contentSource]
            = static_cast<int>(ContentSource::RootNode);
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->translateMeshes] = true;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->translateShapes] = true;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->translateLights] = true;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->translateCameras] = true;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->translateMaterials] = true;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->usdStagesAsReferences] = true;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->allowNestedGprims] = false;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->translateHidden] = true;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->translateSkin] = false;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->translateMorpher] = false;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->useUSDVisibility] = false;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->useProgressBar] = true;

        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->timeMode]
            = static_cast<int>(TimeMode::CurrentFrame);
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->startFrame] = 0.0;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->endFrame] = 0.0;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->samplesPerFrame] = 1.0;

        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->shadingMode]
            = MaxUsdShadingModeTokens->useRegistry;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->allMaterialConversions]
            = std::set<pxr::TfToken> { pxr::UsdImagingTokens->UsdPreviewSurface };

        defaultDict[MaxUsdSceneBuilderOptionsTokens->contextNames] = std::set<std::string>();
        defaultDict[MaxUsdSceneBuilderOptionsTokens->jobContextOptions] = pxr::VtDictionary();
        defaultDict[MaxUsdSceneBuilderOptionsTokens->chaserNames] = std::vector<std::string>();
        defaultDict[MaxUsdSceneBuilderOptionsTokens->chaserArgs]
            = std::map<std::string, ChaserArgs>();
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->openInUsdView] = false;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->fileFormat]
            = static_cast<int>(FileFormat::Binary);
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->rootPrimPath] = SdfPath("/root");
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->upAxis] = static_cast<int>(UpAxis::Z);

        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->meshConversionOptions]
            = MaxMeshConversionOptions().GetOptions();
        defaultDict[MaxUsdSceneBuilderOptionsTokens->logLevel] = static_cast<int>(Log::Level::Off);
#ifdef IS_MAX2024_OR_GREATER
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->mtlSwitcherExportStyle]
            = static_cast<int>(MtlSwitcherExportStyle::AsVariantSets);
#endif
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->materialLayerPath]
            = std::string("<filename>_mtl.usda");
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->separateMaterialLayer] = false;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->materialPrimPath] = SdfPath("mtl");
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->useLastResortUSDPreviewSurfaceWriter]
            = true;
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->animationsPrimName]
            = pxr::TfToken("Animations");
        defaultDict[MaxUsdUsdSceneBuilderOptionsTokens->bonesPrimName] = pxr::TfToken("Bones");
    });
    // Purposefully left out of the call_once, in order to always fetch the latest value for
    // "APP_TEMP_DIR".
    defaultDict[MaxUsdSceneBuilderOptionsTokens->logPath]
        = fs::path(std::wstring(MaxSDKSupport::GetString(GetCOREInterface()->GetDir(APP_TEMP_DIR)))
                       .append(L"\\MaxUsdExport.log"));

    return defaultDict;
}

USDSceneBuilderOptions USDSceneBuilderOptions::OptionsWithAppliedContexts() const
{
    auto thisCopy = *this;

    VtDictionary allContextArgs;
    if (MergeJobContexts(true, this->GetContextNames(), allContextArgs)) {
        if (!allContextArgs.empty()) {
            if (allContextArgs.count(MaxUsdSceneBuilderOptionsTokens->chaserNames) > 0) {
                if (thisCopy.GetChaserNames().empty()) {
                    thisCopy.SetChaserNames(DictUtils::ExtractVector<std::string>(
                        allContextArgs, MaxUsdSceneBuilderOptionsTokens->chaserNames));
                } else // merge args
                {
                    auto values = DictUtils::ExtractVector<std::string>(
                        allContextArgs, MaxUsdSceneBuilderOptionsTokens->chaserNames);
                    if (!values.empty()) {
                        auto chaserNamesCopy = thisCopy.GetChaserNames();
                        for (auto element : values) {
                            if (std::find(chaserNamesCopy.begin(), chaserNamesCopy.end(), element)
                                == chaserNamesCopy.end()) {
                                chaserNamesCopy.push_back(element);
                            }
                        }
                        thisCopy.SetChaserNames(chaserNamesCopy);
                    }
                }
            }
            if (allContextArgs.count(MaxUsdSceneBuilderOptionsTokens->chaserArgs) > 0) {
                if (thisCopy.GetAllChaserArgs().empty()) {
                    thisCopy.SetAllChaserArgs(ExtractChaserArgs(
                        allContextArgs, MaxUsdSceneBuilderOptionsTokens->chaserArgs));
                } else // merge args
                {
                    auto values = ExtractChaserArgs(
                        allContextArgs, MaxUsdSceneBuilderOptionsTokens->chaserArgs);
                    auto allChaserArgsCopy = thisCopy.GetAllChaserArgs();
                    for (auto element : values) {
                        if (allChaserArgsCopy.find(element.first) == allChaserArgsCopy.end()) {
                            allChaserArgsCopy[element.first] = element.second;
                        } else {
                            auto& currentChaserArgs = allChaserArgsCopy[element.first];
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
                        thisCopy.SetAllChaserArgs(allChaserArgsCopy);
                    }
                }
            }
            if (allContextArgs.count(MaxUsdSceneBuilderOptionsTokens->convertMaterialsTo) > 0) {
                // merge args
                auto values = DictUtils::ExtractTokenSet(
                    allContextArgs, MaxUsdSceneBuilderOptionsTokens->convertMaterialsTo);

                auto matConversions = thisCopy.GetAllMaterialConversions();
                matConversions.insert(values.begin(), values.end());
                thisCopy.SetAllMaterialConversions(matConversions);
            }
        }
    } else {
        MaxUsd::Log::Error("Errors while processing export contexts. Using base export options.");
    }

    return thisCopy;
}

void USDSceneBuilderOptions::SetContentSource(const ContentSource& contentSource)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->contentSource] = static_cast<int>(contentSource);
}

USDSceneBuilderOptions::ContentSource USDSceneBuilderOptions::GetContentSource() const
{
    return static_cast<ContentSource>(
        VtDictionaryGet<int>(options, MaxUsdUsdSceneBuilderOptionsTokens->contentSource));
}

void USDSceneBuilderOptions::SetTranslateMeshes(bool translateMeshes)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->translateMeshes] = translateMeshes;
}

bool USDSceneBuilderOptions::GetTranslateMeshes() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->translateMeshes);
}

void USDSceneBuilderOptions::SetTranslateShapes(bool translateShapes)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->translateShapes] = translateShapes;
}

bool USDSceneBuilderOptions::GetTranslateShapes() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->translateShapes);
}

void USDSceneBuilderOptions::SetTranslateLights(bool translateLights)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->translateLights] = translateLights;
}

bool USDSceneBuilderOptions::GetTranslateLights() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->translateLights);
}

void USDSceneBuilderOptions::SetTranslateCameras(bool translateCameras)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->translateCameras] = translateCameras;
}

bool USDSceneBuilderOptions::GetTranslateCameras() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->translateCameras);
}

void USDSceneBuilderOptions::SetTranslateMaterials(bool translateMaterials)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->translateMaterials] = translateMaterials;

    // set default shading mode if none was set and materials are to be exported
    if (translateMaterials && GetShadingMode() == MaxUsdShadingModeTokens->none) {
        SetShadingMode(MaxUsdShadingModeTokens->useRegistry);
    }
}

bool USDSceneBuilderOptions::GetTranslateMaterials() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->translateMaterials);
}

void USDSceneBuilderOptions::SetTranslateSkin(bool translateSkin)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->translateSkin] = translateSkin;
}

bool USDSceneBuilderOptions::GetTranslateSkin() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->translateSkin);
}

void USDSceneBuilderOptions::SetTranslateMorpher(bool translateMorpher)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->translateMorpher] = translateMorpher;
}

bool USDSceneBuilderOptions::GetTranslateMorpher() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->translateMorpher);
}

void USDSceneBuilderOptions::SetShadingMode(const pxr::TfToken& shadingMode)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->shadingMode] = shadingMode;

    // overrule the 'translateMaterials' boolean if no shading mode is set
    if (shadingMode == MaxUsdShadingModeTokens->none && GetTranslateMaterials()) {
        SetTranslateMaterials(false);
    }
}

pxr::TfToken USDSceneBuilderOptions::GetShadingMode() const
{
    return VtDictionaryGet<TfToken>(options, MaxUsdUsdSceneBuilderOptionsTokens->shadingMode);
}

void USDSceneBuilderOptions::SetAllMaterialConversions(
    const std::set<pxr::TfToken>& materialConversions)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->allMaterialConversions] = materialConversions;
    if (materialConversions.empty()) {
        SetShadingMode(pxr::MaxUsdShadingModeTokens->none);
    } else {
        SetShadingMode(pxr::MaxUsdShadingModeTokens->useRegistry);
    }
}

const std::set<pxr::TfToken>& USDSceneBuilderOptions::GetAllMaterialConversions() const
{
    return VtDictionaryGet<std::set<pxr::TfToken>>(
        options, MaxUsdUsdSceneBuilderOptionsTokens->allMaterialConversions);
}

void USDSceneBuilderOptions::SetUsdStagesAsReferences(bool usdStagesAsReferences)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->usdStagesAsReferences] = usdStagesAsReferences;
}

bool USDSceneBuilderOptions::GetUsdStagesAsReferences() const
{
    return VtDictionaryGet<bool>(
        options, MaxUsdUsdSceneBuilderOptionsTokens->usdStagesAsReferences);
}

void USDSceneBuilderOptions::SetTranslateHidden(bool translateHidden)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->translateHidden] = translateHidden;
}

bool USDSceneBuilderOptions::GetTranslateHidden() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->translateHidden);
}

void USDSceneBuilderOptions::SetUseUSDVisibility(bool useUSDVibility)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->useUSDVisibility] = useUSDVibility;
}

bool USDSceneBuilderOptions::GetUseUSDVisibility() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->useUSDVisibility);
}

void USDSceneBuilderOptions::SetFileFormat(const FileFormat& saveFormat)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->fileFormat] = static_cast<int>(saveFormat);
}

USDSceneBuilderOptions::FileFormat USDSceneBuilderOptions::GetFileFormat() const
{
    return static_cast<FileFormat>(
        VtDictionaryGet<int>(options, MaxUsdUsdSceneBuilderOptionsTokens->fileFormat));
}

namespace {
const std::string normalsModeKey
    = MaxUsdUsdSceneBuilderOptionsTokens->meshConversionOptions.GetString() + ":"
    + MaxUsdMaxMeshConversionOptions->normalMode.GetString();
const std::string meshFormatKey
    = MaxUsdUsdSceneBuilderOptionsTokens->meshConversionOptions.GetString() + ":"
    + MaxUsdMaxMeshConversionOptions->meshFormat.GetString();
} // namespace

void USDSceneBuilderOptions::SetNormalsMode(
    const MaxMeshConversionOptions::NormalsMode& normalsMode)
{
    options.SetValueAtPath(normalsModeKey, VtValue(static_cast<int>(normalsMode)));
}

MaxMeshConversionOptions::NormalsMode USDSceneBuilderOptions::GetNormalsMode() const
{
    const auto& val = options.GetValueAtPath(normalsModeKey);
    return static_cast<MaxMeshConversionOptions::NormalsMode>(val->Get<int>());
}

void USDSceneBuilderOptions::SetMeshFormat(const MaxMeshConversionOptions::MeshFormat& meshFormat)
{
    options.SetValueAtPath(meshFormatKey, VtValue(static_cast<int>(meshFormat)));
}

MaxMeshConversionOptions::MeshFormat USDSceneBuilderOptions::GetMeshFormat() const
{
    const auto& val = options.GetValueAtPath(meshFormatKey);
    return static_cast<MaxMeshConversionOptions::MeshFormat>(val->Get<int>());
}

void USDSceneBuilderOptions::SetUpAxis(const UpAxis& upAxis)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->upAxis] = static_cast<int>(upAxis);
}

USDSceneBuilderOptions::UpAxis USDSceneBuilderOptions::GetUpAxis() const
{
    return static_cast<UpAxis>(
        VtDictionaryGet<int>(options, MaxUsdUsdSceneBuilderOptionsTokens->upAxis));
}

void USDSceneBuilderOptions::SetMeshConversionOptions(
    const MaxMeshConversionOptions& meshConversionOptions)
{
    options.SetValueAtPath(
        MaxUsdUsdSceneBuilderOptionsTokens->meshConversionOptions,
        VtValue(meshConversionOptions.GetOptions()));
}

const MaxMeshConversionOptions USDSceneBuilderOptions::GetMeshConversionOptions() const
{
    return MaxMeshConversionOptions(VtDictionaryGet<VtDictionary>(
        options, MaxUsdUsdSceneBuilderOptionsTokens->meshConversionOptions));
}

void USDSceneBuilderOptions::SetNodesToExport(const Tab<INode*>& nodes) { nodesToExport = nodes; }

const Tab<INode*>& USDSceneBuilderOptions::GetNodesToExport() const { return nodesToExport; }

void USDSceneBuilderOptions::SetTimeMode(const TimeMode& timeMode)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->timeMode] = static_cast<int>(timeMode);
}

USDSceneBuilderOptions::TimeMode USDSceneBuilderOptions::GetTimeMode() const
{
    return static_cast<TimeMode>(
        VtDictionaryGet<int>(options, MaxUsdUsdSceneBuilderOptionsTokens->timeMode));
}

void USDSceneBuilderOptions::SetStartFrame(double startFrame)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->startFrame] = startFrame;
}

double USDSceneBuilderOptions::GetStartFrame() const
{
    return VtDictionaryGet<double>(options, MaxUsdUsdSceneBuilderOptionsTokens->startFrame);
}

void USDSceneBuilderOptions::SetEndFrame(double endFrame)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->endFrame] = endFrame;
}

double USDSceneBuilderOptions::GetEndFrame() const
{
    return VtDictionaryGet<double>(options, MaxUsdUsdSceneBuilderOptionsTokens->endFrame);
}

void USDSceneBuilderOptions::SetSamplesPerFrame(double samplesPerFrame)
{
    TimeConfig::ValidateSamplePerFrame(samplesPerFrame);
    options[MaxUsdUsdSceneBuilderOptionsTokens->samplesPerFrame] = samplesPerFrame;
}

double USDSceneBuilderOptions::GetSamplesPerFrame() const
{
    return VtDictionaryGet<double>(options, MaxUsdUsdSceneBuilderOptionsTokens->samplesPerFrame);
}

TimeConfig USDSceneBuilderOptions::GetResolvedTimeConfig() const
{
    TimeConfig timeConfig { GetStartFrame(), GetEndFrame(), GetSamplesPerFrame() };

    switch (GetTimeMode()) {
    case TimeMode::CurrentFrame:
        return TimeConfig { GetCOREInterface()->GetTime(),
                            GetCOREInterface()->GetTime(),
                            timeConfig.GetSamplesPerFrame() };
    case TimeMode::ExplicitFrame:
        return TimeConfig { timeConfig.GetStartTime(),
                            timeConfig.GetStartTime(),
                            timeConfig.GetSamplesPerFrame() };
    case TimeMode::AnimationRange:
        return TimeConfig { GetCOREInterface()->GetAnimRange().Start(),
                            GetCOREInterface()->GetAnimRange().End(),
                            timeConfig.GetSamplesPerFrame() };
    case TimeMode::FrameRange:
        return TimeConfig { timeConfig.GetStartTime(),
                            timeConfig.GetEndTime(),
                            timeConfig.GetSamplesPerFrame() };
    default: DbgAssert(0 && _T("Unknown TimeMode enum.")); return TimeConfig {};
    }
}

void USDSceneBuilderOptions::SetRootPrimPath(const SdfPath& rootPrimPath)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->rootPrimPath] = rootPrimPath;
}

const SdfPath& USDSceneBuilderOptions::GetRootPrimPath() const
{
    return VtDictionaryGet<SdfPath>(options, MaxUsdUsdSceneBuilderOptionsTokens->rootPrimPath);
}

const pxr::TfToken& USDSceneBuilderOptions::GetBonesPrimName() const
{
    return VtDictionaryGet<TfToken>(options, MaxUsdUsdSceneBuilderOptionsTokens->bonesPrimName);
}

void USDSceneBuilderOptions::SetBonesPrimName(const pxr::TfToken& bonesPrimName)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->bonesPrimName] = bonesPrimName;
}

const pxr::TfToken& USDSceneBuilderOptions::GetAnimationsPrimName() const
{
    return VtDictionaryGet<TfToken>(
        options, MaxUsdUsdSceneBuilderOptionsTokens->animationsPrimName);
}

void USDSceneBuilderOptions::SetAnimationsPrimName(const pxr::TfToken& animationsPrimName)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->animationsPrimName] = animationsPrimName;
}

void USDSceneBuilderOptions::SetOpenInUsdview(bool openInUsdview)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->openInUsdView] = openInUsdview;
}

bool USDSceneBuilderOptions::GetOpenInUsdview() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->openInUsdView);
}

void USDSceneBuilderOptions::SetAllowNestedGprims(bool allowNestedGprims)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->allowNestedGprims] = allowNestedGprims;
}

bool USDSceneBuilderOptions::GetAllowNestedGprims() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->allowNestedGprims);
}

#ifdef IS_MAX2024_OR_GREATER
void USDSceneBuilderOptions::SetMtlSwitcherExportStyle(const MtlSwitcherExportStyle& exportStyle)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->mtlSwitcherExportStyle]
        = static_cast<int>(exportStyle);
}

const USDSceneBuilderOptions::MtlSwitcherExportStyle
USDSceneBuilderOptions::GetMtlSwitcherExportStyle() const
{
    return static_cast<MtlSwitcherExportStyle>(
        VtDictionaryGet<int>(options, MaxUsdUsdSceneBuilderOptionsTokens->mtlSwitcherExportStyle));
}
#endif

bool USDSceneBuilderOptions::GetUseProgressBar() const
{
    return VtDictionaryGet<bool>(options, MaxUsdUsdSceneBuilderOptionsTokens->useProgressBar);
}

void USDSceneBuilderOptions::SetUseProgressBar(bool useProgressBar)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->useProgressBar] = useProgressBar;
}

void USDSceneBuilderOptions::SetMaterialLayerPath(const std::string& matPath)
{
    auto path = USDCore::sanitizedFilename(matPath, ".usda").string();
    options[MaxUsdUsdSceneBuilderOptionsTokens->materialLayerPath] = path;
}

const std::string& USDSceneBuilderOptions::GetMaterialLayerPath() const
{
    return VtDictionaryGet<std::string>(
        options, MaxUsdUsdSceneBuilderOptionsTokens->materialLayerPath);
}

void USDSceneBuilderOptions::SetMaterialPrimPath(const pxr::SdfPath& matRootpath)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->materialPrimPath] = matRootpath;
}

const pxr::SdfPath& USDSceneBuilderOptions::GetMaterialPrimPath() const
{
    return VtDictionaryGet<SdfPath>(options, MaxUsdUsdSceneBuilderOptionsTokens->materialPrimPath);
}

void USDSceneBuilderOptions::SetUseSeparateMaterialLayer(bool useMaterialLayer)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->separateMaterialLayer] = useMaterialLayer;
}

bool USDSceneBuilderOptions::GetUseSeparateMaterialLayer() const
{
    return VtDictionaryGet<bool>(
        options, MaxUsdUsdSceneBuilderOptionsTokens->separateMaterialLayer);
}

void USDSceneBuilderOptions::SetUseLastResortUSDPreviewSurfaceWriter(
    bool useLastResortUSDFallbackMaterial)
{
    options[MaxUsdUsdSceneBuilderOptionsTokens->useLastResortUSDPreviewSurfaceWriter]
        = useLastResortUSDFallbackMaterial;
}

bool USDSceneBuilderOptions::GetUseLastResortUSDPreviewSurfaceWriter() const
{
    return VtDictionaryGet<bool>(
        options, MaxUsdUsdSceneBuilderOptionsTokens->useLastResortUSDPreviewSurfaceWriter);
}

void USDSceneBuilderOptions::FetchAnimationRollupData(
    AnimationRollupData& animationRollupData) const
{
    animationRollupData.frameNumberDefault = this->animationRollupData.frameNumberDefault;
    animationRollupData.frameNumber = this->animationRollupData.frameNumber;
    animationRollupData.frameRangeDefault = this->animationRollupData.frameRangeDefault;
    animationRollupData.frameRangeStart = this->animationRollupData.frameRangeStart;
    animationRollupData.frameRangeEnd = this->animationRollupData.frameRangeEnd;
}

void USDSceneBuilderOptions::SaveAnimationRollupData(const AnimationRollupData& animationRollupData)
{
    this->animationRollupData.frameNumberDefault = animationRollupData.frameNumberDefault;
    this->animationRollupData.frameNumber = animationRollupData.frameNumber;
    this->animationRollupData.frameRangeDefault = animationRollupData.frameRangeDefault;
    this->animationRollupData.frameRangeStart = animationRollupData.frameRangeStart;
    this->animationRollupData.frameRangeEnd = animationRollupData.frameRangeEnd;
}
} // namespace MAXUSD_NS_DEF