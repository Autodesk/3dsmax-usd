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
#include "MaxMeshConversionOptions.h"

#include "MeshConverter.h"

#include <MaxUsd/MaxTokens.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/VtDictionaryUtils.h>

PXR_NAMESPACE_OPEN_SCOPE
TF_DEFINE_PUBLIC_TOKENS(MaxUsdMaxMeshConversionOptions, MAXUSD_MAXMESH_CONVERSION_OPTIONS_TOKENS);
PXR_NAMESPACE_CLOSE_SCOPE

using namespace MaxUsd;
using namespace pxr;

MaxMeshConversionOptions::MaxMeshConversionOptions() { SetDefaults(); }

MaxMeshConversionOptions::MaxMeshConversionOptions(const VtDictionary& dict) { options = dict; }

const VtDictionary& MaxMeshConversionOptions::GetDefaultDictionary()
{
    static VtDictionary   defaultDict;
    static std::once_flag once;
    std::call_once(once, []() {
        defaultDict[MaxUsdMaxMeshConversionOptions->version] = 1;
        defaultDict[MaxUsdMaxMeshConversionOptions->normalMode]
            = static_cast<int>(NormalsMode::AsPrimvar);
        defaultDict[MaxUsdMaxMeshConversionOptions->meshFormat]
            = static_cast<int>(MeshFormat::FromScene);
        defaultDict[MaxUsdMaxMeshConversionOptions->primvarLayoutInference]
            = static_cast<int>(PrimvarLayoutInference::IfStatic);
        defaultDict[MaxUsdMaxMeshConversionOptions->bakeObjectOffsetTransform] = true;
        defaultDict[MaxUsdMaxMeshConversionOptions->preserveEdgeOrientation] = false;
        defaultDict[MaxUsdMaxMeshConversionOptions->channelToPrimvarConfig]
            = GetDefaultChannelPrimvarMappings();
    });

    return defaultDict;
}

void MaxMeshConversionOptions::SetDefaults() { options = GetDefaultDictionary(); }

const VtDictionary& MaxMeshConversionOptions::GetDefaultChannelPrimvarMappings()
{
    static VtDictionary   defaultDict;
    static std::once_flag once;

    std::call_once(once, []() {
        // Lambda to create a primvar entry as a dictionary.
        auto createPrimvarEntry = [](const TfToken&               primvarName,
                                     MappedAttributeBuilder::Type primvarType,
                                     bool                         autoExpandType) -> VtDictionary {
            return VtDictionary {
                { MaxUsdMappedAttributeBuilder->primvarName, VtValue(primvarName) },
                { MaxUsdMappedAttributeBuilder->primvarType,
                  VtValue(static_cast<int>(primvarType)) },
                { MaxUsdMappedAttributeBuilder->autoExpandType, VtValue(autoExpandType) }
            };
        };

        // -2 : Alpha channel
        defaultDict[std::to_string(MAP_ALPHA)] = createPrimvarEntry(
            MaxUsdPrimvarTokens->displayOpacity, MappedAttributeBuilder::Type::FloatArray, false);
        // -1 : Shading channel.
        defaultDict[std::to_string(MAP_SHADING)] = createPrimvarEntry(
            MaxUsdPrimvarTokens->mapShading, MappedAttributeBuilder::Type::Color3fArray, false);
        // 0 : Vertex color.
        defaultDict["0"] = createPrimvarEntry(
            MaxUsdPrimvarTokens->vertexColor, MappedAttributeBuilder::Type::Color3fArray, false);

        const std::string primName = "st";
        // 1-99 : st-st98
        defaultDict["1"] = createPrimvarEntry(
            TfToken(primName), MappedAttributeBuilder::Type::TexCoord2fArray, false);
        for (int i = 2; i < MAX_MESHMAPS; i++) {
            defaultDict[std::to_string(i)] = createPrimvarEntry(
                TfToken(primName + std::to_string(i - 1)),
                MappedAttributeBuilder::Type::TexCoord2fArray,
                false);
        }
    });

    return defaultDict;
}

void MaxMeshConversionOptions::SetDefaultChannelPrimvarMappings()
{
    SetChannelMappings(GetDefaultChannelPrimvarMappings());
}

const VtDictionary& MaxMeshConversionOptions::GetChannelMappings() const
{
    return VtDictionaryGet<VtDictionary>(
        options, MaxUsdMaxMeshConversionOptions->channelToPrimvarConfig);
}

void MaxMeshConversionOptions::SetChannelMappings(const VtDictionary& mappings)
{
    options.SetValueAtPath(
        MaxUsdMaxMeshConversionOptions->channelToPrimvarConfig, VtValue(mappings));
}

namespace {
const std::string channelPrimvarConfigKey
    = MaxUsdMaxMeshConversionOptions->channelToPrimvarConfig.GetString() + ":";
}

void MaxMeshConversionOptions::SetChannelPrimvarConfig(
    int                                   channel,
    const MappedAttributeBuilder::Config& config)
{
    options.SetValueAtPath(
        channelPrimvarConfigKey + std::to_string(channel), VtValue(config.GetOptions()));
}

MappedAttributeBuilder::Config
MaxMeshConversionOptions::GetChannelPrimvarConfig(int channelId) const
{
    auto val = options.GetValueAtPath(channelPrimvarConfigKey + std::to_string(channelId));
    return { val->UncheckedGet<VtDictionary>() };
}

MaxMeshConversionOptions::NormalsMode MaxMeshConversionOptions::GetNormalMode() const
{
    return static_cast<NormalsMode>(
        VtDictionaryGet<int>(options, MaxUsdMaxMeshConversionOptions->normalMode));
}

void MaxMeshConversionOptions::SetNormalsMode(NormalsMode normalMode)
{
    options[MaxUsdMaxMeshConversionOptions->normalMode] = static_cast<int>(normalMode);
}

MaxMeshConversionOptions::MeshFormat MaxMeshConversionOptions::GetMeshFormat() const
{
    return static_cast<MeshFormat>(
        VtDictionaryGet<int>(options, MaxUsdMaxMeshConversionOptions->meshFormat));
}

void MaxMeshConversionOptions::SetMeshFormat(MeshFormat meshFormat)
{
    options[MaxUsdMaxMeshConversionOptions->meshFormat] = static_cast<int>(meshFormat);
}

MaxMeshConversionOptions::PrimvarLayoutInference
MaxMeshConversionOptions::GetPrimvarLayoutInference() const
{
    return static_cast<PrimvarLayoutInference>(
        VtDictionaryGet<int>(options, MaxUsdMaxMeshConversionOptions->primvarLayoutInference));
}

void MaxMeshConversionOptions::SetPrimvarLayoutInference(PrimvarLayoutInference layoutInference)
{
    options[MaxUsdMaxMeshConversionOptions->primvarLayoutInference]
        = static_cast<int>(layoutInference);
}

void MaxMeshConversionOptions::SetBakeObjectOffsetTransform(bool bakeObjectOffset)
{
    options[MaxUsdMaxMeshConversionOptions->bakeObjectOffsetTransform] = bakeObjectOffset;
}

bool MaxMeshConversionOptions::GetBakeObjectOffsetTransform() const
{
    return VtDictionaryGet<bool>(
        options, MaxUsdMaxMeshConversionOptions->bakeObjectOffsetTransform);
}

void MaxMeshConversionOptions::SetPreserveEdgeOrientation(bool preserve)
{
    options[MaxUsdMaxMeshConversionOptions->preserveEdgeOrientation] = preserve;
}

bool MaxMeshConversionOptions::GetPreserveEdgeOrientation() const
{
    return VtDictionaryGet<bool>(options, MaxUsdMaxMeshConversionOptions->preserveEdgeOrientation);
}
