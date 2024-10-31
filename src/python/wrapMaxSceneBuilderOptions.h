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

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>
#include <MaxUsd/MappedAttributeBuilder.h>

class MaxSceneBuilderOptionsWrapper : public MaxUsd::MaxSceneBuilderOptions
{
public:
    MaxSceneBuilderOptionsWrapper();

    /**
     * \brief Constructor.
     * \param importArgs the options to wrap.
     */
    MaxSceneBuilderOptionsWrapper(const MaxUsd::MaxSceneBuilderOptions& importArgs);

    /**
     * \brief Constructor from a Json formatted string.
     * \param json the Json formatted string to create the options from.
     */
    MaxSceneBuilderOptionsWrapper(const std::string& json);

    /**
     * \brief Sets the stage mask's paths. Only USD prims at or below these paths will be imported.
     * \param paths The mask paths.
     */
    void SetStageMaskPathsList(const boost::python::list& paths);

    /**
     * \brief Returns the export log path.
     * \return the log's path.
     */
    std::string GetLogPath() const;

    /**
     * \brief Sets the export's log path.
     * \param logPath The log path.
     */
    void SetLogPath(const std::string& logPath);

    /**
     * \brief Return the map of chasers and their respective arguments map
     * \return The map of chasers' arguments
     */
    boost::python::dict GetAllChaserArgs() const;

    /**
     * \brief Sets all of the chasers' arguments from a dictionnary.
     * {'chaser' : {'param' : 'val'}}
     * \param args The chasers' args.
     */
    void SetAllChaserArgsFromDict(boost::python::dict args);

    /**
     * \brief Set all of the chasers' arguments from a list
     * {'chaser', 'param1', 'val', 'chaser2', 'param', 'val'}
     * \param args The chasers' args.
     */
    void SetAllChaserArgsFromList(boost::python::list args);

    // PrimvarMappingOptions helpers
    /**
     * \brief Sets defaults primvar to channels mappings.
     */
    void SetPrimvarChannelMappingDefaults();

    /**
     * \brief Gets whether or not to import primvars that are not explicitly mapped.
     * \return True if unmapped primvars should be imported, false otherwise.
     */
    bool GetImportUnmappedPrimvars() const;

    /**
     * \brief Sets whether or not to import primvars that are not explicitly mapped. If true, try to
     * find the most appropriate channels for each unmapped primvar, based on their types.
     * \param importUnmappedPrimvars If true, unmapped primvars will be imported.
     */
    void SetImportUnmappedPrimvars(bool importUnmappedPrimvars);

    /**
     * \brief Sets a primvar to channel mapping.
     * \param primvar The name of the primvar.
     * \param channel The channel this primvar should be imported to.
     */
    void SetPrimvarChannel(const std::string& primvarName, int channel);

    /**
     * \brief Gets the channel to which a primvar maps to.
     * \param primvarName The name of the primvar for which to retrieve the target channel.
     * \return The target channel for this primvar.
     */
    int GetPrimvarChannel(const std::string& primvarName) const;

    /**
     * \brief Returns the list of all currently mapped primvars.
     * \param primvars The vector of primvar names to be filled.
     */
    std::vector<std::wstring> GetMappedPrimvars() const;

    /**
     * \brief Checks if a primvar is currently mapped to a channel.
     * \param primvar The primvar to check.
     * \return True if the primvar is mapped, false otherwise.
     */
    bool IsMappedPrimvar(const std::string& primvarName) const;

    /**
     * \brief Clears all primvar mappings.
     */
    void ClearMappedPrimvars();

    /**
     * Set the shading modes to use at import
     * @param args A list of dictionaries, each dictionary is expected to contain two keys,
     * 'materialConversion' and 'mode'
     */
    void SetShadingModes(boost::python::list args);

    /**
     * \brief Serialize the options to a Json formatted string.
     * \return The Json formatted string representing the options.
     */
    std::string Serialize();
};
