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
#pragma once

#include <MaxUsd/MeshConversion/PrimvarMappingOptions.h>

#include <MaxUsd.h>
#include <max.h>

namespace MAXUSD_NS_DEF {
namespace mxs {

/**
 * \brief Sets the target channel for a given primvar.
 * \param options The options on which to set the mapping.
 * \param primvarName The primvar name.
 * \param channel The target channel.
 */
MaxUSDAPI void SetPrimvarChannelMapping(
    PrimvarMappingOptions& options,
    const wchar_t*         primvarName,
    Value*                 channel);

/**
 * \brief Returns the target channel for a given primvar.
 * \param options The options from which to get the primvar/channel mapping.
 * \param primvarName The primvar's name
 * \return The channel, or undefined.
 */
MaxUSDAPI Value*
GetPrimvarChannel(const PrimvarMappingOptions& options, const wchar_t* primvarName);

/**
 * \brief Returns the names of all mapped primvars (primvars which target 3dsMax channels)
 * \param options The optins to get the mapped primvars from.
 * \return The names of mapped primvars.
 */
MaxUSDAPI Tab<const wchar_t*> GetMappedPrimvars(const PrimvarMappingOptions& options);

/**
 * \brief Return whether or not a primvar is mapped. A primvar can be explicitely mapped to "undefined",
 * meaning it should be ignored.
 * \param options The options to get the info from.
 * \param primvarName The primvar name to check for a channel mapping.
 * \return True if the given primvar is mapped to a channel.
 */
MaxUSDAPI bool IsMappedPrimvar(const PrimvarMappingOptions& options, const wchar_t* primvarName);

} // namespace mxs
} // namespace MAXUSD_NS_DEF
