//
// Copyright 2022 Autodesk
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
#include "JobContextRegistry.h"

#include <MaxUsd/DebugCodes.h>
#include <MaxUsd/Translators/RegistryHelper.h>

#include <pxr/base/tf/debug.h>
#include <pxr/base/tf/instantiateSingleton.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/token.h>

#include <string>
#include <unordered_set>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (MaxUsd)
        (JobContextPlugin)
);
// clang-format on

namespace {
static const TfTokenVector PLUGIN_SCOPE = { _tokens->MaxUsd, _tokens->JobContextPlugin };
} // namespace

namespace {
struct _HashInfo
{
    size_t operator()(const MaxUsdJobContextRegistry::ContextInfo& info) const noexcept
    {
        return info.jobContext.Hash();
    }
};

struct _EqualInfo
{
    bool operator()(
        const MaxUsdJobContextRegistry::ContextInfo& lhs,
        const MaxUsdJobContextRegistry::ContextInfo& rhs) const noexcept
    {
        return rhs.jobContext == lhs.jobContext;
    }
};

using _JobContextRegistry
    = std::unordered_set<MaxUsdJobContextRegistry::ContextInfo, _HashInfo, _EqualInfo>;
static _JobContextRegistry _jobContextReg;
} // namespace

void MaxUsdJobContextRegistry::RegisterExportJobContext(
    const std::string& jobContext,
    const std::string& niceName,
    const std::string& description,
    EnablerFn          enablerFct,
    bool               fromPython)
{
    TF_DEBUG(PXR_MAXUSD_REGISTRY).Msg("Registering export job context %s.\n", jobContext.c_str());
    TfToken     key(jobContext);
    ContextInfo newInfo { key, TfToken(niceName), TfToken(description), enablerFct };
    auto        itFound = _jobContextReg.find(newInfo);
    if (itFound == _jobContextReg.end()) {
        _jobContextReg.insert(newInfo);
        MaxUsd_RegistryHelper::AddUnloader(
            [key]() {
                ContextInfo toErase { key };
                _jobContextReg.erase(toErase);
            },
            fromPython);
    } else {
        if (!itFound->exportEnablerCallback) {
            if (niceName != itFound->niceName) {
                TF_CODING_ERROR(
                    "Export enabler has differing nice name: %s != %s",
                    niceName.c_str(),
                    itFound->niceName.GetText());
            }
            // Keep the import part, fill in the export part:
            ContextInfo updatedInfo { key,
                                      itFound->niceName,
                                      TfToken(description),
                                      enablerFct,
                                      {},
                                      itFound->importDescription,
                                      itFound->importEnablerCallback,
                                      itFound->importOptionsCallback };
            _jobContextReg.erase(updatedInfo);
            _jobContextReg.insert(updatedInfo);
        } else {
            TF_CODING_ERROR("Multiple enablers for export job context %s", jobContext.c_str());
        }
    }
}

void MaxUsdJobContextRegistry::SetExportOptionsUI(
    const std::string& jobContext,
    OptionsFn          optionsFct,
    bool               fromPython)
{
    TF_DEBUG(PXR_MAXUSD_REGISTRY)
        .Msg("Registering export options ui callback for job context %s.\n", jobContext.c_str());
    TfToken key(jobContext);

    ContextInfo searchInfo { key };
    auto        itFound = _jobContextReg.find(searchInfo);
    if (itFound != _jobContextReg.end()) {
        ContextInfo updatedInfo { key,
                                  itFound->niceName,
                                  itFound->exportDescription,
                                  itFound->exportEnablerCallback,
                                  optionsFct,
                                  itFound->importDescription,
                                  itFound->importEnablerCallback,
                                  itFound->importOptionsCallback };
        _jobContextReg.erase(updatedInfo);
        _jobContextReg.insert(updatedInfo);
    } else {
        TF_CODING_ERROR("No export job context found named %s", jobContext.c_str());
    }
}

void MaxUsdJobContextRegistry::RegisterImportJobContext(
    const std::string& jobContext,
    const std::string& niceName,
    const std::string& description,
    EnablerFn          enablerFct,
    bool               fromPython)
{
    TF_DEBUG(PXR_MAXUSD_REGISTRY).Msg("Registering import job context %s.\n", jobContext.c_str());
    TfToken     key(jobContext);
    ContextInfo newInfo { key, TfToken(niceName), {}, {}, {}, TfToken(description), enablerFct };
    auto        itFound = _jobContextReg.find(newInfo);
    if (itFound == _jobContextReg.end()) {
        _jobContextReg.insert(newInfo);
        MaxUsd_RegistryHelper::AddUnloader(
            [key]() {
                ContextInfo toErase { key };
                _jobContextReg.erase(toErase);
            },
            fromPython);
    } else {
        if (!itFound->importEnablerCallback) {
            if (niceName != itFound->niceName) {
                TF_CODING_ERROR(
                    "Import enabler has differing nice name: %s != %s",
                    niceName.c_str(),
                    itFound->niceName.GetText());
            }
            // Keep the export part, fill in the import part:
            ContextInfo updatedInfo { key,
                                      itFound->niceName,
                                      itFound->exportDescription,
                                      itFound->exportEnablerCallback,
                                      itFound->exportOptionsCallback,
                                      TfToken(description),
                                      enablerFct };
            _jobContextReg.erase(updatedInfo);
            _jobContextReg.insert(updatedInfo);
        } else {
            TF_CODING_ERROR("Multiple enablers for import job context %s", jobContext.c_str());
        }
    }
}

void MaxUsdJobContextRegistry::SetImportOptionsUI(
    const std::string& jobContext,
    OptionsFn          optionsFct,
    bool               fromPython)
{
    TF_DEBUG(PXR_MAXUSD_REGISTRY)
        .Msg("Registering import options ui callback for job context %s.\n", jobContext.c_str());
    TfToken key(jobContext);

    ContextInfo searchInfo { key };
    auto        itFound = _jobContextReg.find(searchInfo);
    if (itFound != _jobContextReg.end()) {
        ContextInfo updatedInfo { key,
                                  itFound->niceName,
                                  itFound->exportDescription,
                                  itFound->exportEnablerCallback,
                                  itFound->exportOptionsCallback,
                                  itFound->importDescription,
                                  itFound->importEnablerCallback,
                                  optionsFct };
        _jobContextReg.erase(updatedInfo);
        _jobContextReg.insert(updatedInfo);
    } else {
        TF_CODING_ERROR("No import job context found named %s", jobContext.c_str());
    }
}

TfTokenVector MaxUsdJobContextRegistry::_ListJobContexts()
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdJobContextRegistry>();
    TfTokenVector ret;
    ret.reserve(_jobContextReg.size());
    for (const auto& e : _jobContextReg) {
        ret.push_back(e.jobContext);
    }
    return ret;
}

const MaxUsdJobContextRegistry::ContextInfo&
MaxUsdJobContextRegistry::_GetJobContextInfo(const TfToken& jobContext)
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(PLUGIN_SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdJobContextRegistry>();
    ContextInfo              key { jobContext, {}, {}, {}, {}, {} };
    auto                     it = _jobContextReg.find(key);
    static const ContextInfo _emptyInfo;
    return it != _jobContextReg.end() ? *it : _emptyInfo;
}

TF_INSTANTIATE_SINGLETON(MaxUsdJobContextRegistry);

MaxUsdJobContextRegistry& MaxUsdJobContextRegistry::GetInstance()
{
    return TfSingleton<MaxUsdJobContextRegistry>::GetInstance();
}

MaxUsdJobContextRegistry::MaxUsdJobContextRegistry() { }

MaxUsdJobContextRegistry::~MaxUsdJobContextRegistry() { }

PXR_NAMESPACE_CLOSE_SCOPE
