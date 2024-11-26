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

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>

class USDSceneBuilderOptionsWrapper : public MaxUsd::USDSceneBuilderOptions
{
public:
    USDSceneBuilderOptionsWrapper() = default;

    /**
     * \brief Constructor.
     * \param exportArgs the options to wrap.
     */
    USDSceneBuilderOptionsWrapper(const MaxUsd::USDSceneBuilderOptions& exportArgs);

    /**
     * \brief Constructor from a Json formatted string.
     * \param json the Json formatted string to create the options from.
     */
    USDSceneBuilderOptionsWrapper(const std::string& json);

    /**
     * \brief Gets the primvar name of a given channel.
     * \param channel The channel from which we want the primvar name.
     * \return The primvar name of the channel.
     */
    const pxr::TfToken& GetChannelPrimvarName(int channel) const;

    /**
     * \brief Sets the primvar name associated with a given map channel.
     * \param channel The 3dsMax map channel.
     * \param name The primvar's name.
     */
    void SetChannelPrimvarName(int channel, const pxr::TfToken& name);

    /**
     * \brief Gets the primvar type associated with a given max channel on export.
     * \param channel The max mapped channel.
     * \return The primvar type.
     */
    MaxUsd::MappedAttributeBuilder::Type GetChannelPrimvarType(int channel) const;

    /**
     * \brief Sets the primvar type associated with a given map channel.
     * \param channel The 3dsMax map channel
     * \param type The primvar's type.
     */
    void SetChannelPrimvarType(int channel, MaxUsd::MappedAttributeBuilder::Type type);

    /**
     * \brief Gets whether to auto-expand the primvar type based on the data (for example TexCoord2F
     * -> TexCoord3f if some UVs are using the W component)
     * \param channel The channel from which we want the auto expand type.
     * \return The auto expand type of the channel.
     */
    bool GetChannelPrimvarAutoExpandType(int channel) const;

    /**
     * \brief Sets whether to auto-expand the primvar type based on the data (for example TexCoord2F
     * -> TexCoord3f if some UVs are using the W component)
     * \param channel The 3dsMax map channel.
     * \param autoExpand True it the type should auto-expand, false otherwise.
     */
    void SetChannelPrimvarAutoExpandType(int channel, bool autoExpand);

    /**
     * \brief Gets whether or not to bake the object's offset transform in the geometry.
     * \return True if we should bake, false otherwise.
     */
    bool GetBakeObjectOffsetTransform() const;

    /**
     * \brief Sets whether or not to bake the object's offset transform in the geometry.
     * \param bakeObjectOffset True to bake the object offset, false otherwise.
     */
    void SetBakeObjectOffsetTransform(bool bakeObjectOffset);

    /**
     * \brief Gets whether or not to preserve edge orientation
     * \return True if preserving edge orientation, false otherwise.
     */
    bool GetPreserveEdgeOrientation() const;

    /**
     * \brief Sets whether or not to preserve edge orientation
     * \param preserveEdgeOrientation True to preserve the edge orientation, false otherwise.
     */
    void SetPreserveEdgeOrientation(bool preserveEdgeOrientation);

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
     * \brief Returns the logging level (info, warn, error, etc.)
     * \return The log level.
     */
    MaxUsd::Log::Level GetLogLevel() const;

    /**
     * \brief Sets the export's log level.
     * \param logLevel The log level.
     */
    void SetLogLevel(MaxUsd::Log::Level logLevel);

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

    /**
     * \brief Sets all the material conversions that should be considered in the export.
     * \param materialConversions The set of all material conversions, as strings.
     */
    void SetAllMaterialConversions(const std::set<std::string>& materialConversions);

    /**
     * \brief Gets the file path to where materials are exported to.
     * \return The file path.
     */
    std::string GetMaterialLayerPath() const;

    /**
     * \brief Gets the prim path where materials are exported to.
     * \return The prim path.
     */
    std::string GetMaterialPrimPath() const;

    /**
     * \brief Serialize the options to a Json formatted string.
     * \return The Json formatted string representing the options.
     */
    std::string Serialize();

private:
    const MaxUsd::MappedAttributeBuilder::Config GetValidPrimvarConfig(int channel) const;
};