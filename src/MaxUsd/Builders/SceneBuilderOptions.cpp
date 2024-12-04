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
#include "SceneBuilderOptions.h"

#include "JobContextRegistry.h"
#include "UsdSceneBuilderOptions.h"

#include <MaxUsd/Utilities/VtDictionaryUtils.h>

#include <pxr/base/tf/diagnostic.h>

PXR_NAMESPACE_OPEN_SCOPE
TF_DEFINE_PUBLIC_TOKENS(MaxUsdSceneBuilderOptionsTokens, PXR_MAXUSD_SCENE_BUILDER_OPTIONS_TOKENS);
PXR_NAMESPACE_CLOSE_SCOPE

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAXUSD_NS_DEF {

// Merges all the jobContext arguments dictionaries found while exploring the jobContexts into a
// single one. Also checks for conflicts and errors.
bool MergeJobContexts(
    bool                         isExport,
    const std::set<std::string>& contexts,
    VtDictionary&                allContextArgs)
{
    // List of all argument dictionaries found while exploring jobContexts
    std::vector<VtDictionary> contextArgs;
    bool                      canMergeContexts = true;
    // This first loop gathers all job context argument dictionaries found in the userArgs
    const TfToken& jcKey = MaxUsdSceneBuilderOptionsTokens->jobContext;
    for (const std::string& v : contexts) {
        const TfToken                                jobContext(v);
        const MaxUsdJobContextRegistry::ContextInfo& ci
            = MaxUsdJobContextRegistry::GetJobContextInfo(jobContext);
        auto enablerCallback = isExport ? ci.exportEnablerCallback : ci.importEnablerCallback;
        if (enablerCallback) {
            VtDictionary extraArgs = enablerCallback();
            // Add the job context name to the args (for reference when merging):
            VtDictionary::iterator jobContextNamesIt = extraArgs.find(jcKey);
            if (jobContextNamesIt != extraArgs.end()) {
                // We already have a vector. Ensure it is of size 1 and contains only the
                // current context name:
                const std::vector<VtValue>& currContextNames
                    = VtDictionaryGet<std::vector<VtValue>>(extraArgs, jcKey);
                if ((currContextNames.size() == 1 && currContextNames.front() != v)
                    || currContextNames.size() > 1) {
                    TF_RUNTIME_ERROR(TfStringPrintf(
                        "Arguments for job context '%s' can not include extra contexts.",
                        jobContext.GetText()));
                    canMergeContexts = false;
                }
            }
            std::vector<VtValue> jobContextNames;
            jobContextNames.push_back(VtValue(v));
            extraArgs[jcKey] = jobContextNames;
            contextArgs.push_back(extraArgs);
        } else {
            MaxUsd::Log::Warn("Ignoring unknown job context '{0}'.", jobContext.GetText());
        }
    }
    // Convenience map holding the jobContext that first introduces an argument to the final
    // dictionary. Allows printing meaningful error messages.
    std::map<std::string, std::string> argInitialSource;
    // Traverse argument dictionaries and look for merge conflicts while building the returned
    // allContextArgs.
    for (auto const& dict : contextArgs) {
        // We made sure the value exists in the above loop, so we can fetch without fear:
        const std::string& sourceName = VtDictionaryGet<std::vector<VtValue>>(dict, jcKey)
                                            .front()
                                            .UncheckedGet<std::string>();
        for (auto const& dictTuple : dict) {
            const std::string& k = dictTuple.first;
            const VtValue&     v = dictTuple.second;
            auto               allContextIt = allContextArgs.find(k);
            if (allContextIt == allContextArgs.end()) {
                // First time we see this argument. Store and remember source.

                // special treatment on a deprecated base option ('chaser')
                if (k == MaxUsdSceneBuilderOptionsTokens->chaser) {
                    TF_WARN(TfStringPrintf(
                        "Deprecated option key '%s' was found. Key shoud be replaced with '%s' "
                        "unless otherwise required.",
                        MaxUsdSceneBuilderOptionsTokens->chaser,
                        MaxUsdSceneBuilderOptionsTokens->chaserNames));
                }

                allContextArgs[k] = v;
                argInitialSource[k] = sourceName;
            } else {
                // We have already seen this argument from another jobContext. Look for conflicts:
                const VtValue& allContextValue = allContextIt->second;
                if (allContextValue.IsHolding<std::vector<VtValue>>()) {
                    if (v.IsHolding<std::vector<VtValue>>()) {
                        // We merge arrays:
                        std::vector<VtValue> mergedValues
                            = allContextValue.UncheckedGet<std::vector<VtValue>>();
                        for (const VtValue& element : v.UncheckedGet<std::vector<VtValue>>()) {
                            if (element.IsHolding<std::vector<VtValue>>()) {
                                // vector<vector<string>> is common for chaserArgs and shadingModes
                                auto findElement = [&element](const VtValue& a) {
                                    // make this comparison simple for now
                                    return element == a;
                                };
                                if (std::find_if(
                                        mergedValues.begin(), mergedValues.end(), findElement)
                                    == mergedValues.end()) {
                                    mergedValues.push_back(element);
                                }
                            } else {
                                if (std::find(mergedValues.begin(), mergedValues.end(), element)
                                    == mergedValues.end()) {
                                    mergedValues.push_back(element);
                                }
                            }
                        }
                        allContextArgs[k] = mergedValues;
                    } else {
                        // We have both an array and a scalar under the same argument name.
                        TF_RUNTIME_ERROR(TfStringPrintf(
                            "Context '%s' and context '%s' do not agree on type of argument '%s'.",
                            sourceName.c_str(),
                            argInitialSource[k].c_str(),
                            k.c_str()));
                        canMergeContexts = false;
                    }
                } else {
                    // Scalar value already exists. Check for value conflicts:
                    if (allContextValue != v) {
                        TF_RUNTIME_ERROR(TfStringPrintf(
                            "Context '%s' and context '%s' do not agree on argument '%s'.",
                            sourceName.c_str(),
                            argInitialSource[k].c_str(),
                            k.c_str()));
                        canMergeContexts = false;
                    }
                }
            }
        }
    }
    return canMergeContexts;
}

std::map<std::string, SceneBuilderOptions::ChaserArgs>
ExtractChaserArgs(const pxr::VtDictionary& userArgs, const pxr::TfToken& key)
{
    const std::vector<std::vector<VtValue>> chaserArgs
        = DictUtils::ExtractVector<std::vector<VtValue>>(userArgs, key);

    std::map<std::string, SceneBuilderOptions::ChaserArgs> result;
    for (const std::vector<VtValue>& argTriple : chaserArgs) {
        if (argTriple.size() != 3) {
            TF_CODING_ERROR("Each chaser arg must be a triple (chaser, arg, value)");
            return std::map<std::string, SceneBuilderOptions::ChaserArgs>();
        }

        const std::string& chaser = argTriple[0].Get<std::string>();
        const std::string& arg = argTriple[1].Get<std::string>();
        const std::string& value = argTriple[2].Get<std::string>();

        // any conflicts present
        auto chaserArgs = result.find(chaser);
        if (chaserArgs == result.end()) {
            result[chaser][arg] = value;
        } else {
            auto chaserArg = chaserArgs->second.find(arg);
            if (chaserArg == chaserArgs->second.end()) {
                chaserArgs->second[arg] = value;
            } else {
                if (chaserArg->second != value) {
                    // keep the argument value from the first context to use that argument, and
                    // forget the other values
                    TF_WARN(TfStringPrintf(
                        "Multiple argument value for '%s' associated to chaser '%s'. Keeping value "
                        "set to '%s'.",
                        arg.c_str(),
                        chaser.c_str(),
                        chaserArg->second));
                }
            }
        }
    }
    return result;
}

const Log::Options& SceneBuilderOptions::GetLogOptions() const
{
    static Log::Options logOptions;
    logOptions.level = static_cast<Log::Level>(
        VtDictionaryGet<int>(options, MaxUsdSceneBuilderOptionsTokens->logLevel));
    logOptions.path = VtDictionaryGet<fs::path>(options, MaxUsdSceneBuilderOptionsTokens->logPath);
    return logOptions;
}

void SceneBuilderOptions::SetLogOptions(const Log::Options& logOptions)
{
    options[MaxUsdSceneBuilderOptionsTokens->logLevel] = static_cast<int>(logOptions.level);
    options[MaxUsdSceneBuilderOptionsTokens->logPath] = logOptions.path;
}

void SceneBuilderOptions::SetLogPath(fs::path logPath)
{
    options[MaxUsdSceneBuilderOptionsTokens->logPath] = logPath;
}

const fs::path& SceneBuilderOptions::GetLogPath() const
{
    return VtDictionaryGet<fs::path>(options, MaxUsdSceneBuilderOptionsTokens->logPath);
}

void SceneBuilderOptions::SetLogLevel(Log::Level logLevel)
{
    options[MaxUsdSceneBuilderOptionsTokens->logLevel] = static_cast<int>(logLevel);
}

Log::Level SceneBuilderOptions::GetLogLevel() const
{
    return static_cast<Log::Level>(
        VtDictionaryGet<int>(options, MaxUsdSceneBuilderOptionsTokens->logLevel));
}

const pxr::VtDictionary&
SceneBuilderOptions::GetJobContextOptions(const pxr::TfToken& jobContext) const
{
    auto jobCtxOpts = VtDictionaryGet<VtDictionary>(
        options, MaxUsdSceneBuilderOptionsTokens->jobContextOptions);
    if (VtDictionaryIsHolding<VtDictionary>(jobCtxOpts, jobContext)) {
        return VtDictionaryGet<VtDictionary>(jobCtxOpts, jobContext);
    }

    static const VtDictionary emptyDict;
    return emptyDict;
}

void SceneBuilderOptions::SetJobContextOptions(
    const pxr::TfToken&      jobContext,
    const pxr::VtDictionary& ctxOptions)
{
    if (VtDictionaryIsHolding<VtDictionary>(
            options, MaxUsdSceneBuilderOptionsTokens->jobContextOptions)) {
        std::string key = MaxUsdSceneBuilderOptionsTokens->jobContextOptions.GetString() + ":"
            + jobContext.GetString();
        options.SetValueAtPath(key, VtValue(ctxOptions));
    }
}

const std::vector<std::string>& SceneBuilderOptions::GetChaserNames() const
{
    return VtDictionaryGet<std::vector<std::string>>(
        options, MaxUsdSceneBuilderOptionsTokens->chaserNames);
}

void SceneBuilderOptions::SetChaserNames(const std::vector<std::string>& chasers)
{
    options[MaxUsdSceneBuilderOptionsTokens->chaserNames] = chasers;
}

const std::map<std::string, SceneBuilderOptions::ChaserArgs>&
SceneBuilderOptions::GetAllChaserArgs() const
{
    return VtDictionaryGet<std::map<std::string, ChaserArgs>>(
        options, MaxUsdSceneBuilderOptionsTokens->chaserArgs);
}

void SceneBuilderOptions::SetAllChaserArgs(const std::map<std::string, ChaserArgs>& chaserArgs)
{
    options[MaxUsdSceneBuilderOptionsTokens->chaserArgs] = chaserArgs;
}

void SceneBuilderOptions::SetContextNames(const std::set<std::string> contexts)
{
    options[MaxUsdSceneBuilderOptionsTokens->contextNames] = contexts;
}

const std::set<std::string>& SceneBuilderOptions::GetContextNames() const
{
    return VtDictionaryGet<std::set<std::string>>(
        options, MaxUsdSceneBuilderOptionsTokens->contextNames);
}

} // namespace MAXUSD_NS_DEF