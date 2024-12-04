//
// Copyright 2016 Pixar
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
#include "ExportChaserRegistry.h"

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
		(ExportChaser)
);
// clang-format on

namespace {
static const TfTokenVector SCOPE = { _tokens->MaxUsd, _tokens->ExportChaser };
}

MaxUsdExportChaserRegistry::FactoryContext::FactoryContext(
    const UsdStagePtr&                    stage,
    const PrimToNodeMap&                  primToNodeMap,
    const MaxUsd::USDSceneBuilderOptions& jobArgs,
    const fs::path&                       filename)
    : stage(stage)
    , primToNodeMap(primToNodeMap)
    , jobArgs(jobArgs)
    , filename(filename)
{
}

UsdStagePtr MaxUsdExportChaserRegistry::FactoryContext::GetStage() const { return stage; }

const MaxUsdExportChaserRegistry::FactoryContext::PrimToNodeMap&
MaxUsdExportChaserRegistry::FactoryContext::GetPrimToNodeMap() const
{
    return primToNodeMap;
}

const MaxUsd::USDSceneBuilderOptions& MaxUsdExportChaserRegistry::FactoryContext::GetJobArgs() const
{
    return jobArgs;
}

const fs::path& MaxUsdExportChaserRegistry::FactoryContext::GetFilename() const { return filename; }

TF_INSTANTIATE_SINGLETON(MaxUsdExportChaserRegistry);

namespace {
struct _HashInfo
{
    size_t operator()(const MaxUsdExportChaserRegistry::ChaserInfo& info) const
    {
        return info.chaser.Hash();
    }
};

struct _EqualInfo
{
    bool operator()(
        const MaxUsdExportChaserRegistry::ChaserInfo& lhs,
        const MaxUsdExportChaserRegistry::ChaserInfo& rhs) const
    {
        return rhs.chaser == lhs.chaser;
    }
};

using _ExportChaserRegistry
    = std::unordered_set<MaxUsdExportChaserRegistry::ChaserInfo, _HashInfo, _EqualInfo>;
static _ExportChaserRegistry _exportChaserRegistry;
} // namespace

bool MaxUsdExportChaserRegistry::RegisterFactory(
    const std::string& chaser,
    const std::string& niceName,
    const std::string& description,
    FactoryFn          fn,
    bool               fromPython)
{
    TF_DEBUG(PXR_MAXUSD_REGISTRY).Msg("Registering chaser '%s'.\n", chaser.c_str());
    TfToken    key(chaser);
    ChaserInfo newInfo { key, TfToken(niceName), TfToken(description), fn };
    auto       itFound = _exportChaserRegistry.find(newInfo);
    if (itFound == _exportChaserRegistry.end()) {
        _exportChaserRegistry.insert(newInfo);
        MaxUsd_RegistryHelper::AddUnloader(
            [key]() {
                ChaserInfo toErase { key, {}, {}, {} };
                _exportChaserRegistry.erase(toErase);
            },
            fromPython);
    } else {
        TF_CODING_ERROR("Multiple export chasers named '%s'", chaser.c_str());
        return false;
    }
    return true;
}

MaxUsdExportChaserRefPtr
MaxUsdExportChaserRegistry::_Create(const std::string& name, const FactoryContext& context) const
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdExportChaserRegistry>();

    ChaserInfo key { TfToken(name), {}, {}, {} };
    auto       it = _exportChaserRegistry.find(key);
    return it != _exportChaserRegistry.end() ? TfCreateRefPtr(it->chaserFactory(context))
                                             : TfNullPtr;
}

TfTokenVector MaxUsdExportChaserRegistry::_GetAllRegisteredChasers() const
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdExportChaserRegistry>();

    TfTokenVector ret;
    ret.reserve(_exportChaserRegistry.size());
    for (const auto& p : _exportChaserRegistry) {
        ret.push_back(p.chaser);
    }
    return ret;
}

const MaxUsdExportChaserRegistry::ChaserInfo&
MaxUsdExportChaserRegistry::_GetChaserInfo(const TfToken& chaser) const
{
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(SCOPE);
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdExportChaserRegistry>();
    ChaserInfo              key { chaser, {}, {}, {} };
    auto                    it = _exportChaserRegistry.find(key);
    static const ChaserInfo _emptyInfo;
    return it != _exportChaserRegistry.end() ? *it : _emptyInfo;
}

// static
MaxUsdExportChaserRegistry& MaxUsdExportChaserRegistry::GetInstance()
{
    return TfSingleton<MaxUsdExportChaserRegistry>::GetInstance();
}

MaxUsdExportChaserRegistry::MaxUsdExportChaserRegistry() { }

MaxUsdExportChaserRegistry::~MaxUsdExportChaserRegistry() { }

PXR_NAMESPACE_CLOSE_SCOPE
