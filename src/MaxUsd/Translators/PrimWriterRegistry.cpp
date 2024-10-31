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
// © 2022 Autodesk, Inc. All rights reserved.
//
#include "PrimWriterRegistry.h"

#include "RegistryHelper.h"

#include <MaxUsd/debugCodes.h>

#include <pxr/base/tf/debug.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/stl.h>
#include <pxr/pxr.h>

#include <map>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (MaxUsd)
        (PrimWriter)
);
// clang-format on

struct PrimWriterRegistryEntry
{
    MaxUsdPrimWriterRegistry::WriterFactoryFn    factoryFunction;
    MaxUsdPrimWriterRegistry::ContextPredicateFn predicateFunction;
};

using _Registry = std::map<std::string, PrimWriterRegistryEntry>;
static _Registry _reg;

static std::vector<PrimWriterRegistryEntry> _baseWriters;

void MaxUsdPrimWriterRegistry::Register(
    const std::string& key,
    WriterFactoryFn    fn,
    ContextPredicateFn pred,
    bool               fromPython)
{
    TF_DEBUG(PXR_MAXUSD_REGISTRY).Msg("Registering MaxUsdPrimWriter %s.\n", key.c_str());

    std::pair<_Registry::iterator, bool> insertStatus = _reg.insert({ key, { fn, pred } });
    if (insertStatus.second) {
        MaxUsd_RegistryHelper::AddUnloader([key]() { _reg.erase(key); }, fromPython);
    } else {
        TF_CODING_ERROR("Multiple writers for sharing unique name %s", key.c_str());
    }
}

void MaxUsdPrimWriterRegistry::RegisterBaseWriter(WriterFactoryFn fn, ContextPredicateFn pred)
{
    _baseWriters.push_back({ fn, pred });
}

void MaxUsdPrimWriterRegistry::Unregister(const std::string& key) { _reg.erase(key); }

MaxUsdPrimWriterSharedPtr MaxUsdPrimWriterRegistry::FindWriter(
    const MaxUsdWriteJobContext& jobCtx,
    INode*                       node,
    size_t&                      numRegistered)
{
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdPrimWriterRegistry>();

    // Add prim writers via plugin load:
    static const TfTokenVector SCOPE = { _tokens->MaxUsd, _tokens->PrimWriter };
    MaxUsd_RegistryHelper::FindAndLoadMaxUsdPlugs(SCOPE);

    std::vector<PrimWriterRegistryEntry> writers;
    std::vector<PrimWriterRegistryEntry> fallbackWriters;

    auto addWriter =
        [&writers, &fallbackWriters, &jobCtx, node](const PrimWriterRegistryEntry& writerBuilder) {
            const auto supportLevel = writerBuilder.predicateFunction(node, jobCtx.GetArgs());
            switch (supportLevel) {
            case MaxUsdPrimWriter::ContextSupport::Supported:
                writers.push_back(writerBuilder);
                break;
            case MaxUsdPrimWriter::ContextSupport::Fallback:
                fallbackWriters.push_back(writerBuilder);
                break;
            case MaxUsdPrimWriter::ContextSupport::Unsupported:
            default: return;
            }
        };

    TF_FOR_ALL(entry, _reg)
    {
        auto writerBuilder = entry->second;
        addWriter(writerBuilder);
    }

    numRegistered = writers.size() + fallbackWriters.size();

    TF_FOR_ALL(entry, _baseWriters)
    {
        auto writerBuilder = *entry;
        addWriter(writerBuilder);
    }

    writers.insert(writers.end(), fallbackWriters.begin(), fallbackWriters.end());

    if (writers.empty()) {
        return nullptr;
    }

    return writers[0].factoryFunction(jobCtx, node);
}

bool MaxUsdPrimWriterRegistry::CanBeExported(
    INode*                                node,
    const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    auto canBeExported = [&exportArgs, node](const PrimWriterRegistryEntry& writerBuilder) {
        if (writerBuilder.predicateFunction(node, exportArgs)
            != MaxUsdPrimWriter::ContextSupport::Unsupported) {
            return true;
        }
        return false;
    };

    TF_FOR_ALL(entry, _reg)
    {
        auto writerBuilder = entry->second;
        if (canBeExported(writerBuilder)) {
            return true;
        }
    }

    TF_FOR_ALL(entry, _baseWriters)
    {
        auto writerBuilder = *entry;
        if (canBeExported(writerBuilder)) {
            return true;
        }
    }

    return false;
}

PXR_NAMESPACE_CLOSE_SCOPE
