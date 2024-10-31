//
// Copyright 2021 Apple
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
// © 2023 Autodesk, Inc. All rights reserved.
//
#include "ImportChaserRegistry.h"

#include <MaxUsd/DebugCodes.h>
#include <MaxUsd/Translators/PrimWriterRegistry.h>
#include <MaxUsd/Translators/RegistryHelper.h>

#include <pxr/base/tf/instantiateSingleton.h>

#include <map>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
	_tokens,

	(MaxUsd)
		(ImportChaser)
);
// clang-format on

namespace {
static const TfTokenVector SCOPE = { _tokens->MaxUsd, _tokens->ImportChaser };
}

namespace {
struct _HashInfo
{
    size_t operator()(const MaxUsdImportChaserRegistry::ChaserInfo& info) const
    {
        return info.chaser.Hash();
    }
};

struct _EqualInfo
{
    bool operator()(
        const MaxUsdImportChaserRegistry::ChaserInfo& lhs,
        const MaxUsdImportChaserRegistry::ChaserInfo& rhs) const
    {
        return rhs.chaser == lhs.chaser;
    }
};

using _ImportChaserRegistry
    = std::unordered_set<MaxUsdImportChaserRegistry::ChaserInfo, _HashInfo, _EqualInfo>;
static _ImportChaserRegistry _importChaserRegistry;
} // namespace

TF_INSTANTIATE_SINGLETON(MaxUsdImportChaserRegistry);

MaxUsdImportChaserRegistry::FactoryContext::FactoryContext(
    Usd_PrimFlagsPredicate& returnPredicate,
    MaxUsdReadJobContext&   context,
    const fs::path&         filename)
    : context(context)
    , filename(filename)
{
}

MaxUsdReadJobContext& MaxUsdImportChaserRegistry::FactoryContext::GetContext() const
{
    return this->context;
}

UsdStagePtr MaxUsdImportChaserRegistry::FactoryContext::GetStage() const
{
    return this->context.GetStage();
}

const MaxUsd::MaxSceneBuilderOptions& MaxUsdImportChaserRegistry::FactoryContext::GetJobArgs() const
{
    return this->context.GetArgs();
}

const fs::path& MaxUsdImportChaserRegistry::FactoryContext::GetFilename() const
{
    return this->filename;
}

MaxUsdImportChaserRegistry::MaxUsdImportChaserRegistry() { }

MaxUsdImportChaserRegistry::~MaxUsdImportChaserRegistry() { }

bool MaxUsdImportChaserRegistry::RegisterFactory(
    const std::string& chaser,
    const std::string& niceName,
    const std::string& description,
    FactoryFn          fn,
    bool               fromPython)
{
    TF_DEBUG(PXR_MAXUSD_REGISTRY).Msg("Registering chaser '%s'.\n", chaser.c_str());
    TfToken    key(chaser);
    ChaserInfo newInfo { key, TfToken(niceName), TfToken(description), fn };
    auto       itFound = _importChaserRegistry.find(newInfo);
    if (itFound == _importChaserRegistry.end()) {
        _importChaserRegistry.insert(newInfo);
        MaxUsd_RegistryHelper::AddUnloader(
            [key]() {
                ChaserInfo toErase { key, {}, {}, {} };
                _importChaserRegistry.erase(toErase);
            },
            fromPython);
    } else {
        TF_CODING_ERROR("Multiple import chasers named '%s'", chaser.c_str());
        return false;
    }
    return true;
}

MaxUSDAPI MaxUsdImportChaserRegistry& MaxUsdImportChaserRegistry::GetInstance()
{
    return TfSingleton<MaxUsdImportChaserRegistry>::GetInstance();
}

MaxUSDAPI TfTokenVector MaxUsdImportChaserRegistry::_GetAllRegisteredChasers() const
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdImportChaserRegistry>();

    TfTokenVector ret;
    ret.reserve(_importChaserRegistry.size());
    for (const auto& p : _importChaserRegistry) {
        ret.push_back(p.chaser);
    }
    return ret;
}

MaxUSDAPI const MaxUsdImportChaserRegistry::ChaserInfo&
                MaxUsdImportChaserRegistry::_GetChaserInfo(const TfToken& chaser) const
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdImportChaserRegistry>();
    ChaserInfo              key { chaser, {}, {}, {} };
    auto                    it = _importChaserRegistry.find(key);
    static const ChaserInfo _emptyInfo;
    return it != _importChaserRegistry.end() ? *it : _emptyInfo;
}

MaxUSDAPI MaxUsdImportChaserRefPtr
MaxUsdImportChaserRegistry::_Create(const std::string& name, const FactoryContext& context) const
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdImportChaserRegistry>();

    ChaserInfo key { TfToken(name), {}, {}, {} };
    auto       it = _importChaserRegistry.find(key);
    return it != _importChaserRegistry.end() ? TfCreateRefPtr(it->chaserFactory(context))
                                             : TfNullPtr;
}

PXR_NAMESPACE_CLOSE_SCOPE