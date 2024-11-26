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

#include <MaxUsd/Utilities/DictionaryOptionProvider.h>
#include <MaxUsd/Utilities/Logging.h>

#include <pxr/base/vt/dictionary.h>
#include <pxr/usd/sdf/path.h>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
#define PXR_MAXUSD_SCENE_BUILDER_OPTIONS_TOKENS \
    /* Dictionary keys */ \
    (convertMaterialsTo) \
    (contextNames) \
    (jobContext) \
    (jobContextOptions) \
    (chaserNames) \
    /* 'chaser' is a deprecated option replaced with 'chaserNames' */ \
    (chaser) \
    (chaserArgs) \
    /* Log Options */ \
    (logPath) \
    (logLevel)

// clang-format on

TF_DECLARE_PUBLIC_TOKENS(
    MaxUsdSceneBuilderOptionsTokens,
    MaxUSDAPI,
    PXR_MAXUSD_SCENE_BUILDER_OPTIONS_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

namespace MAXUSD_NS_DEF {

// \brief Merges all the jobContext arguments dictionaries found while exploring the jobContexts into a
// single one. Also checks for conflicts and errors.
//
// \param[in] isExport if we are calling the import or the export jobContext callback.
// \param[in] contexts jobContexts to merge.
//
// \param[out] allContextArgs dictionary of all extra jobContext arguments merged together.
// \return true if the merge was successful, false if a conflict or an error was detected.
MaxUSDAPI bool MergeJobContexts(
    bool                         isExport,
    const std::set<std::string>& contexts,
    pxr::VtDictionary&           allContextArgs);

// The chaser args are stored as vectors of vectors (since this is how you
// would need to pass them in the Max Python API). Convert this to a
// map of maps.
MaxUSDAPI std::map<std::string, std::map<std::string, std::string>>
          ExtractChaserArgs(const pxr::VtDictionary& userArgs, const pxr::TfToken& key);

/**
 * \brief Class for getting and setting builder options.
 */
class SceneBuilderOptions : public DictionaryOptionProvider
{
public:
    typedef std::map<std::string, std::string> ChaserArgs;

    MaxUSDAPI virtual ~SceneBuilderOptions() = default;

    /**
     * \brief Returns the builder's logging options.
     * \return The logging options.
     */
    MaxUSDAPI const Log::Options& GetLogOptions() const;

    /**
     * \brief Sets the logging options for the builder.
     * \param logOptions The logging options to set.
     */
    MaxUSDAPI void SetLogOptions(const Log::Options& logOptions);

    /**
     * \brief Sets the log path.
     * \param logPath The log path.
     */
    MaxUSDAPI void SetLogPath(fs::path logPath);
    /**
     * \brief Returns the log path.
     * \return The log's path.
     */
    MaxUSDAPI const fs::path& GetLogPath() const;
    /**
     * \brief Sets the log level.
     * \param logLevel The log level.
     */
    MaxUSDAPI void SetLogLevel(MaxUsd::Log::Level logLevel);
    /**
     * \brief Returns the logging level (info, warn, error, etc.)
     * \return The log level.
     */
    MaxUSDAPI MaxUsd::Log::Level GetLogLevel() const;

    /**
     * \brief Gets the list of export chasers to be called at USD export
     * \return All export chasers to be called at USD export
     */
    MaxUSDAPI const std::vector<std::string>& GetChaserNames() const;

    /**
     * \brief Sets the chaser list to use at export
     * \param chasers The list of export chaser to call at export
     */
    MaxUSDAPI void SetChaserNames(const std::vector<std::string>& chasers);

    /**
     * \brief Gets the map of export chasers with their specified arguments
     * \return The map of export chasers with their specified arguments
     */
    MaxUSDAPI const std::map<std::string, ChaserArgs>& GetAllChaserArgs() const;

    /**
     * \brief Sets the export chasers' arguments map
     * \param chaserArgs a map of arguments
     */
    MaxUSDAPI void SetAllChaserArgs(const std::map<std::string, ChaserArgs>& chaserArgs);

    /**
     * \brief Sets the context list to use at export
     * \param contexts The list of context to apply at export
     */
    MaxUSDAPI void SetContextNames(const std::set<std::string> contexts);

    /**
     * \brief Gets the list of contexts (plug-in configurations) to be applied on USD export
     * \return All contexts names to be applied on USD export
     */
    MaxUSDAPI const std::set<std::string>& GetContextNames() const;

    /**
     * \brief Get the dictionary holding the options for the given job context.
     * \param jobContext The job context to get the options for.
     * \return The dictionary holding the options for the given job context.
     */
    MaxUSDAPI const pxr::VtDictionary& GetJobContextOptions(const pxr::TfToken& jobContext) const;

    /**
     * \brief Set the options for the given job context.
     * \param jobContext The job context to set the options for.
     * \param ctxOptions The dictionary options to set.
     */
    MaxUSDAPI void
    SetJobContextOptions(const pxr::TfToken& jobContext, const pxr::VtDictionary& options);
};

} // namespace MAXUSD_NS_DEF
