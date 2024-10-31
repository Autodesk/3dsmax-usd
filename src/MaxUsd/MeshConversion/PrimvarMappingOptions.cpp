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
#include "PrimvarMappingOptions.h"

#include <MaxUsd/MaxTokens.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

using namespace pxr;

PXR_NAMESPACE_OPEN_SCOPE
TF_DEFINE_PUBLIC_TOKENS(MaxUsdPrimvarMappingOptions, PXR_MAXUSD_PRIMVAR_MAPPING_OPTIONS_TOKENS);
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAXUSD_NS_DEF {

PrimvarMappingOptions::PrimvarMappingOptions() { options = GetDefaultDictionary(); }

PrimvarMappingOptions::PrimvarMappingOptions(const VtDictionary& dict) { options = dict; }

const VtDictionary& PrimvarMappingOptions::GetDefaultDictionary()
{
    static VtDictionary   defaultDict;
    static std::once_flag once;
    std::call_once(once, []() {
        defaultDict[MaxUsdPrimvarMappingOptions->version] = 1;
        defaultDict[MaxUsdPrimvarMappingOptions->importUnmappedPrimvars] = false;
        defaultDict[MaxUsdPrimvarMappingOptions->primvarToChannelMappings]
            = GetDefaultPrimvarMappings();
    });

    return defaultDict;
}

void PrimvarMappingOptions::SetDefaultPrimvarChannelMappings()
{
    options.SetValueAtPath(
        MaxUsdPrimvarMappingOptions->primvarToChannelMappings,
        VtValue(GetDefaultPrimvarMappings()));
}

void PrimvarMappingOptions::SetDefaultPrimvarChannelMappings(VtDictionary& primvarMappings)
{
    primvarMappings.clear();

    // Vertex color, alpha, shading...
    primvarMappings[MaxUsdPrimvarTokens->displayOpacity] = -2; // USD well-known primvar.
    primvarMappings[MaxUsdPrimvarTokens->mapShading] = -1;     // Round trip from max.
    primvarMappings[MaxUsdPrimvarTokens->vertexColor] = 0;     // Round trip from max.

    const auto prefix = std::string("map");
    for (int i = 1; i < MAX_MESHMAPS; ++i) {
        primvarMappings[prefix + std::to_string(i)] = i;
    }

    // Pixar legacy, "st" very often used for UVs.
    primvarMappings[MaxUsdPrimvarTokens->st] = 1;
    // Some exporters use "uv".
    primvarMappings[MaxUsdPrimvarTokens->uv] = 1;
    for (int i = 0; i < MAX_MESHMAPS - 1; ++i) {
        // Support st_# and st#. st == st0 == st_0.
        primvarMappings[MaxUsdPrimvarTokens->st.GetString() + std::to_string(i)] = i + 1;
        primvarMappings[MaxUsdPrimvarTokens->st.GetString() + std::string("_") + std::to_string(i)]
            = i + 1;
        // Support uv_# and uv#. uv == uv0 == uv_0.
        primvarMappings[MaxUsdPrimvarTokens->uv.GetString() + std::to_string(i)] = i + 1;
        primvarMappings[MaxUsdPrimvarTokens->uv.GetString() + std::string("_") + std::to_string(i)]
            = i + 1;
    }
}

const pxr::VtDictionary& PrimvarMappingOptions::GetDefaultPrimvarMappings()
{
    static VtDictionary primvarMappings;

    static std::once_flag once;
    std::call_once(once, []() { SetDefaultPrimvarChannelMappings(primvarMappings); });

    return primvarMappings;
}

const VtDictionary& PrimvarMappingOptions::GetOptions() const { return options; }

const VtDictionary& PrimvarMappingOptions::GetPrimvarMappings() const
{
    return VtDictionaryGet<VtDictionary>(
        options, MaxUsdPrimvarMappingOptions->primvarToChannelMappings);
}

void PrimvarMappingOptions::SetPrimvarChannelMapping(const std::string& primvar, int channel)
{
    static std::string baseKey
        = MaxUsdPrimvarMappingOptions->primvarToChannelMappings.GetString() + ":";
    options.SetValueAtPath(baseKey + primvar, VtValue(channel));
}

int PrimvarMappingOptions::GetPrimvarChannelMapping(const std::string& primvar) const
{
    const VtDictionary& mapping = GetPrimvarMappings();
    const auto          it = mapping.find(primvar);
    if (it != mapping.end() && it->second.IsHolding<int>()) {
        return it->second.UncheckedGet<int>();
    }
    return invalidChannel;
}

void PrimvarMappingOptions::GetMappedPrimvars(std::vector<std::wstring>& primvars) const
{
    primvars.clear();
    const auto& primvarToChannelMappings = GetPrimvarMappings();
    primvars.reserve(primvarToChannelMappings.size());
    for (const auto& mapping : primvarToChannelMappings) {
        primvars.push_back(MaxUsd::UsdStringToMaxString(mapping.first).data());
    }
}

void PrimvarMappingOptions::ClearMappedPrimvars()
{
    options.SetValueAtPath(
        MaxUsdPrimvarMappingOptions->primvarToChannelMappings, VtValue(VtDictionary()));
}

bool PrimvarMappingOptions::IsMappedPrimvar(const std::string& primvar) const
{
    const auto& mapping = VtDictionaryGet<VtDictionary>(
        options, MaxUsdPrimvarMappingOptions->primvarToChannelMappings, VtDefault = VtDictionary());
    const auto it = mapping.find(primvar);
    return it != mapping.end();
}

bool PrimvarMappingOptions::GetImportUnmappedPrimvars() const
{
    return VtDictionaryGet<bool>(options, MaxUsdPrimvarMappingOptions->importUnmappedPrimvars);
}

void PrimvarMappingOptions::SetImportUnmappedPrimvars(bool importUnmappedPrimvars)
{
    options[MaxUsdPrimvarMappingOptions->importUnmappedPrimvars] = importUnmappedPrimvars;
}

} // namespace MAXUSD_NS_DEF