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

#include <pxr/base/tf/token.h>
#pragma warning(push)
#pragma warning(disable : 4275) // non dll-interface class 'boost::python::api::object' used as base
                                // for dll-interface struct 'boost::python::detail::list_base'
#include <pxr/usd/usd/stage.h>
#pragma warning(pop)

#include "SceneBuilderOptions.h"

#include <MaxUsd/MaxUSDAPI.h>
#include <MaxUsd/MeshConversion/PrimvarMappingOptions.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

PXR_NAMESPACE_OPEN_SCOPE
// clang-format off
#define PXR_MAXUSD_MAX_SCENE_BUILDER_TOKENS \
	/* Dictionary keys */ \
	(version) \
	(initialLoadSet) \
	(timeMode) \
	(stageMaskPaths) \
	(metaDataIncludes) \
	(preferredMaterial) \
	(useProgressBar) \
	(primvarMappingOptions) \
	(shadingModes) \
	(startTimeCode) \
	(endTimeCode)


#define PXR_MAXUSD_SHADING_MODES_TOKENS \
	/* Dictionary keys */ \
	/* importer to use */ \
	(mode) \
	/* material to import */ \
	(materialConversion)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(
    MaxUsdMaxSceneBuilderOptionsTokens,
    MaxUSDAPI,
    PXR_MAXUSD_MAX_SCENE_BUILDER_TOKENS);
TF_DECLARE_PUBLIC_TOKENS(MaxUsdShadingModesTokens, MaxUSDAPI, PXR_MAXUSD_SHADING_MODES_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

namespace MAXUSD_NS_DEF {

/**
 * \brief 3ds Max Scene Build configuration options.
 * \remarks In the future, additional properties will be included to support transfer of more refined import controls.
 * This includes start/end Time Codes for animation, variants, etc.
 */
class MaxSceneBuilderOptions : public SceneBuilderOptions
{
public:
    /**
     * \brief Time mode for import.
     * AllRange considers the stage's entire range
     * CustomRange will use the range defined in the import configuration (from StartTimeCode to
     * EndTimeCode) StartTime will use the stage's start time code EndTime will use the stage's end
     * time code
     */
    enum class MaxUSDAPI ImportTimeMode
    {
        AllRange,
        CustomRange,
        StartTime,
        EndTime
    };

    /**
     * \brief Constructor.
     */
    MaxUSDAPI MaxSceneBuilderOptions();

    /**
     * \brief This is used internally to initialize the options from a dictionary,
     * and it is a costly operation due to the validation of the dictionary.
     * \param dict The dictionary to initialize the options from.
     */
    MaxUSDAPI MaxSceneBuilderOptions(const pxr::VtDictionary& dict);

    /**
     * \brief Copies the values from an existing options object.
     */
    MaxUSDAPI void SetOptions(const MaxSceneBuilderOptions& options);

    /**
     * \brief Resets the importer options to default values.
     */
    MaxUSDAPI void SetDefaults();

    /**
     * \brief Resets the importer Shading Modes option to default values.
     */
    MaxUSDAPI void SetDefaultShadingModes();

    /**
     * \brief Check if materials should be translated
     * \return "true" if materials should be translated
     */
    MaxUSDAPI bool GetTranslateMaterials() const;

    /**
     * \brief Sets the USD stage's initial load set to use for the import of content into 3ds Max.
     * \param initialLoadSet USD Stage initial load set to use for the import of content into 3ds Max.
     */
    MaxUSDAPI void SetStageInitialLoadSet(pxr::UsdStage::InitialLoadSet initialLoadSet);

    /**
     * \brief Return the USD Stage initial load set to use for the import of content into 3ds Max.
     * \return The USD Stage initial load set to use for the import of content into 3ds Max.
     */
    MaxUSDAPI pxr::UsdStage::InitialLoadSet GetStageInitialLoadSet() const;

    /**
     * \brief Sets the TimeMode at which the translation should take place.
     */
    MaxUSDAPI void SetTimeMode(const ImportTimeMode& timeMode);

    /**
     * \brief Set the usd timeCode for the starting time range at which the import should take place.
     * \param startTimeCode The timeCode value for the initial value for the import time range.
     */
    MaxUSDAPI void SetStartTimeCode(double startTimeCode);

    /**
     * \brief Set the usd timeCode for the end time range at which the import should take place.
     * \param endTimeCode The timeCode value for the end value for the import time range.
     */
    MaxUSDAPI void SetEndTimeCode(double endTimeCode);

    /**
     * \brief Gets usd timeCode value set for the start of the import time range.
     * \return The TimeCode value set for start time for the import time range.
     */
    MaxUSDAPI double GetStartTimeCode() const;

    /**
     * \brief Gets usd timeCode value for the end of the import time range.
     * \return The TimeCode value set for end time for the import time range.
     */
    MaxUSDAPI double GetEndTimeCode() const;

    /**
     * \brief Gets the TimeMode at which the translation should take place.
     * \return The TimeMode to be used.
     */
    MaxUSDAPI ImportTimeMode GetTimeMode() const;

    /**
     * \brief Resolves the Time Configuration at which the conversion should take place when translating the USD stage
     * to Max data.
     * \param stage A reference to the stage to be translated.
     */
    MaxUSDAPI virtual MaxUsd::ImportTimeConfig
    GetResolvedTimeConfig(const pxr::UsdStagePtr& stage) const;

    /**
     * \brief Sets the stage mask's paths. Only USD prims at or below these paths will be imported.
     * \param The mask paths.
     */
    MaxUSDAPI void SetStageMaskPaths(const std::vector<pxr::SdfPath>& paths);

    /**
     * \brief Returns the currently configured stage mask paths. Only USD prims at or below these paths
     * will be imported.
     * \return The mask's paths.
     */
    MaxUSDAPI const std::vector<pxr::SdfPath>& GetStageMaskPaths() const;

    /**
     * \param filters Contains a list of MaxUsd::MetaData::MetaDataType that will be included during import
     */
    MaxUSDAPI void SetMetaData(const std::set<int>& filters);

    /**
     * \return Returns the list of MaxUsd::MetaData::MetaDataType that will be included during import
     */
    MaxUSDAPI const std::set<int>& GetMetaData() const;

    /**
     * \brief Returns the primvar/channel mapping options.
     * \return The primvar/channel mapping options.
     */
    MaxUSDAPI PrimvarMappingOptions GetPrimvarMappingOptions() const;

    /**
     * \brief Sets the primvar/channel mapping options.
     * \param primvarMappingOptions The new primvar/channel mapping options.
     */
    MaxUSDAPI void SetPrimvarMappingOptions(const PrimvarMappingOptions& primvarMappingOptions);

    using ShadingModes = std::vector<pxr::VtDictionary>;
    /**
     * Set the shading modes to use at import
     * \param modes A vector of VtDictionary, each dictionary is expected to contain two keys, 'materialConversion' and
     * 'mode'
     */
    MaxUSDAPI void SetShadingModes(const ShadingModes& modes);

    /**
     * Get the shading modes to use at import
     * \return the array of ShadingMode to use
     */
    MaxUSDAPI const ShadingModes& GetShadingModes() const;

    /**
     * Get the current material conversion
     */
    MaxUSDAPI pxr::TfToken GetMaterialConversion() const;
    /**
     * \brief Sets the preferred conversion material to use for material import
     * \param targetMaterial The preferred conversion material to use for material import
     */
    MaxUSDAPI void SetPreferredMaterial(const pxr::TfToken& targetMaterial);
    /**
     * \brief Gets the preferred conversion material set for material import
     * \return The preferred conversion material set for material import
     */
    MaxUSDAPI pxr::TfToken GetPreferredMaterial() const;

    /**
     * \brief Returns a copy of the current MaxSceneBuilderOptions with
     * the JobContext option overrides applied on that copy
     */
    MaxUSDAPI MaxSceneBuilderOptions OptionsWithAppliedContexts() const;

    /**
     * \brief Gets whether to use the progress bar or not.
     * \return True if the progress bar should be used.
     */
    MaxUSDAPI bool GetUseProgressBar() const;

    /**
     * \brief Sets whether to use the progress bar.
     */
    MaxUSDAPI void SetUseProgressBar(bool useProgressBar);

private:
    /**
     * \brief Returns the default dictionary for the importer options.
     * \return The default dictionary for the importer options.
     */
    static MaxUSDAPI const pxr::VtDictionary& GetDefaultDictionary();

    /**
     * \brief Returns the default dictionary for the importer shading modes.
     * \return The default dictionary for the importer shading modes.
     */
    static MaxUSDAPI const pxr::VtDictionary& GetShadingModeDefaultDictionary();

    static MaxUSDAPI void SetDefaultShadingModes(pxr::VtDictionary& dict);
};

} // namespace MAXUSD_NS_DEF
