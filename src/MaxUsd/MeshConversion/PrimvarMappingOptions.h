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

#include <MaxUsd/MaxUSDAPI.h>
#include <MaxUsd/Utilities/DictionaryOptionProvider.h>

#include <MaxUsd.h>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
#define PXR_MAXUSD_PRIMVAR_MAPPING_OPTIONS_TOKENS \
	/* Dictionary keys */ \
	(version) \
	(primvarToChannelMappings) \
	(importUnmappedPrimvars)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(
    MaxUsdPrimvarMappingOptions,
    MaxUSDAPI,
    PXR_MAXUSD_PRIMVAR_MAPPING_OPTIONS_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

namespace MAXUSD_NS_DEF {

// This class exposes methods for getting and setting primvar/channel mapping options.
class PrimvarMappingOptions : public DictionaryOptionProvider
{
public:
    /**
     * \brief Constructor.
     */
    MaxUSDAPI PrimvarMappingOptions();

    /**
     * \brief Constructor.
     * \param dict The dictionary to initialize the options from.
     */
    MaxUSDAPI PrimvarMappingOptions(const pxr::VtDictionary& dict);

    MaxUSDAPI const pxr::VtDictionary& GetOptions() const;

    /**
     * \brief Sets defaults primvar to channels mappings.
     */
    MaxUSDAPI void SetDefaultPrimvarChannelMappings();

    /**
     * \brief Returns the primvar to channel map.
     * \return The mapping table.
     */
    MaxUSDAPI const pxr::VtDictionary& GetPrimvarMappings() const;

    /**
     * \brief Sets a primvar to channel mapping.
     * \param primvar The name of the primvar.
     * \param channel The channel this primvar should be imported to.
     */
    MaxUSDAPI void SetPrimvarChannelMapping(const std::string& primvar, int channel);

    /**
     * \brief Gets the channel to which a primvar maps to.
     * \param primvar The name of the primvar for which to retrieve the target channel.
     * \return The target channel for this primvar.
     */
    MaxUSDAPI int GetPrimvarChannelMapping(const std::string& primvar) const;

    /**
     * \brief Returns the list of all currently mapped primvars.
     * \param primvars The vector of primvar names to be filled.
     */
    MaxUSDAPI void GetMappedPrimvars(std::vector<std::wstring>& primvars) const;

    /**
     * \brief Checks if a primvar is currently mapped to a channel.
     * \param primvar The primvar to check.
     * \return True if the primvar is mapped, false otherwise.
     */
    MaxUSDAPI bool IsMappedPrimvar(const std::string& primvar) const;

    /**
     * \brief Gets whether or not to import primvars that are not explicitly mapped.
     * \return True if unmapped primvars should be imported, false otherwise.
     */
    MaxUSDAPI bool GetImportUnmappedPrimvars() const;

    /**
     * \brief Sets whether or not to import primvars that are not explicitly mapped. If true, try to
     * find the most appropriate channels for each unmapped primvar, based on their types.
     * \param importUnmappedPrimvars If true, unmapped primvars will be imported.
     */
    MaxUSDAPI void SetImportUnmappedPrimvars(bool importUnmappedPrimvars);

    /**
     * \brief Clears all primvar mappings.
     */
    MaxUSDAPI void ClearMappedPrimvars();

    static MaxUSDAPI const int invalidChannel = INT_MIN;

private:
    /**
     * \brief Sets the default primvar to channel mappings.
     * \param primvarMappings The dictionary to fill with default mappings.
     */
    static void SetDefaultPrimvarChannelMappings(pxr::VtDictionary& primvarMappings);

    /**
     * \brief The dictionary holding the default state of all the options.
     * \return The default options dictionary.
     */
    static const pxr::VtDictionary& GetDefaultDictionary();

    /**
     * \brief Returns the default primvar mappings.
     * \return The VtDictionary containing the default primvar mappings.
     */
    static const pxr::VtDictionary& GetDefaultPrimvarMappings();
};

} // namespace MAXUSD_NS_DEF
