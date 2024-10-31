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
#include "MxsUtils.h"

#include "TranslationUtils.h"

#include <maxscript/foundation/numbers.h>

namespace MAXUSD_NS_DEF {
namespace mxs {

void SetPrimvarChannelMapping(
    MaxUsd::PrimvarMappingOptions& options,
    const wchar_t*                 primvarName,
    Value*                         channel)
{
    int channelId;
    if (channel == &undefined) {
        channelId = MaxUsd::PrimvarMappingOptions::invalidChannel;
    } else {
        channelId = channel->to_int();
        // Validation.
        if (!MaxUsd::IsValidChannel(channelId)) {
            const auto errorMsg = std::to_wstring(channelId)
                + std::wstring(L" is not a valid map channel. Valid channels are from -2 to 99 "
                               L"inclusively.");
            throw RuntimeError(errorMsg.c_str());
        }
    }
    if (primvarName == nullptr) {
        WStr errorMsg(L"'undefined' is not a valid primvar name");
        throw RuntimeError(errorMsg.data());
    }

    // Make sure the given primvar name is supported...
    std::string validIdentifier;
    if (!MaxUsd::GetValidIdentifier(primvarName, validIdentifier)) {
        const auto errorMsg
            = primvarName
            + std::wstring(
                  L" is not a valid primvar name. The name must start with a letter or underscore, "
                  L"and must contain only letters, underscores, and numerals..");
        throw RuntimeError(errorMsg.c_str());
    }

    options.SetPrimvarChannelMapping(validIdentifier, channelId);
}

Value* GetPrimvarChannel(const MaxUsd::PrimvarMappingOptions& options, const wchar_t* primvarName)
{
    if (primvarName == nullptr) {
        WStr errorMsg(L"'undefined' is not a valid primvar name");
        throw RuntimeError(errorMsg.data());
    }

    // Make sure the given primvar name is supported...
    std::string validIdentifier;
    if (!MaxUsd::GetValidIdentifier(primvarName, validIdentifier)) {
        const auto errorMsg
            = primvarName
            + std::wstring(
                  L" is not a valid primvar name. The name must start with a letter or underscore, "
                  L"and must contain only letters, underscores, and numerals..");
        throw RuntimeError(errorMsg.c_str());
    }

    if (!options.IsMappedPrimvar(validIdentifier)) {
        return &undefined;
    }

    int channel = options.GetPrimvarChannelMapping(validIdentifier);
    if (channel == MaxUsd::PrimvarMappingOptions::invalidChannel) {
        return &undefined;
    }
    return Integer::intern(channel);
}

Tab<const wchar_t*> GetMappedPrimvars(const MaxUsd::PrimvarMappingOptions& options)
{
    static std::vector<std::wstring> primvars;
    options.GetMappedPrimvars(primvars);

    // Build the tab
    Tab<const wchar_t*> tab;
    tab.SetCount(int(primvars.size()));
    for (size_t i = 0; i < primvars.size(); ++i) {
        tab[i] = primvars[i].c_str();
    }
    // RVO.
    return tab;
}

bool IsMappedPrimvar(const MaxUsd::PrimvarMappingOptions& options, const wchar_t* primvarName)
{
    if (primvarName == nullptr) {
        WStr errorMsg(L"'undefined' is not a valid primvar name");
        throw RuntimeError(errorMsg.data());
    }

    // Make sure the given primvar name is supported...
    std::string primvar;
    if (!MaxUsd::GetValidIdentifier(primvarName, primvar)) {
        const auto errorMsg
            = primvarName
            + std::wstring(
                  L" is not a valid primvar name. The name must start with a letter or underscore, "
                  L"and must contain only letters, underscores, and numerals..");
        throw RuntimeError(errorMsg.c_str());
    }
    return options.IsMappedPrimvar(primvar);
}

} // namespace mxs
} // namespace MAXUSD_NS_DEF
