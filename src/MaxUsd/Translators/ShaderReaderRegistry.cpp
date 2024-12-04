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
#include "ShaderReaderRegistry.h"

#include "RegistryHelper.h"

#include <MaxUsd/DebugCodes.h>

#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/debug.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/stl.h>
#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>

#include <string>
#include <unordered_map>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (MaxUsd)
        (ShaderReader)
);
// clang-format on

namespace {
struct _RegistryEntry
{
    MaxUsdShaderReaderRegistry::ContextPredicateFn _pred;
    MaxUsdShaderReaderRegistry::ReaderFactoryFn    _fn;
    int                                            _index;
};

typedef std::unordered_multimap<TfToken, _RegistryEntry, TfToken::HashFunctor> _Registry;
static _Registry                                                               _reg;
static int                                                                     _indexCounter = 0;

_Registry::const_iterator
_Find(const TfToken& usdInfoId, const MaxUsd::MaxSceneBuilderOptions& importArgs)
{
    using ContextSupport = MaxUsdShaderReader::ContextSupport;

    _Registry::const_iterator ret = _reg.cend();
    _Registry::const_iterator first, last;
    std::tie(first, last) = _reg.equal_range(usdInfoId);
    while (first != last) {
        ContextSupport support = first->second._pred(importArgs);
        if (support == ContextSupport::Supported) {
            ret = first;
            break;
        } else if (support == ContextSupport::Fallback && ret == _reg.end()) {
            ret = first;
        }
        ++first;
    }

    return ret;
}
} // namespace

/* static */
void MaxUsdShaderReaderRegistry::Register(
    TfToken                                        usdInfoId,
    MaxUsdShaderReaderRegistry::ContextPredicateFn pred,
    MaxUsdShaderReaderRegistry::ReaderFactoryFn    fn,
    bool                                           fromPython)
{
    int index = _indexCounter++;
    TF_DEBUG(PXR_MAXUSD_REGISTRY)
        .Msg(
            "Registering MaxUsdShaderReader for info:id %s with index %d.\n",
            usdInfoId.GetText(),
            index);
    auto id = usdInfoId.GetString();
    _reg.insert(std::make_pair(usdInfoId, _RegistryEntry { pred, fn, index }));

    // The unloader uses the index to know which entry to erase when there are
    // more than one for the same usdInfoId.
    //
    // TODO: The decision to remember that a type was not registered will have
    //       to be revisited at some point. Loading a plugin creates an
    //       opportunity to discover more readers... as long as we stop
    //       registering nullptrs.
    if (fn) {
        MaxUsd_RegistryHelper::AddUnloader(
            [usdInfoId, index]() {
                _Registry::const_iterator it, itEnd;
                std::tie(it, itEnd) = _reg.equal_range(usdInfoId);
                for (; it != itEnd; ++it) {
                    if (it->second._index == index) {
                        _reg.erase(it);
                        break;
                    }
                }
            },
            fromPython);
    }
}

/* static */
MaxUsdShaderReaderRegistry::ReaderFactoryFn MaxUsdShaderReaderRegistry::Find(
    const TfToken&                        usdInfoId,
    const MaxUsd::MaxSceneBuilderOptions& importArgs)
{
    using ContextSupport = MaxUsdShaderReader::ContextSupport;
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdShaderReaderRegistry>();

    _Registry::const_iterator it = _Find(usdInfoId, importArgs);

    if (it != _reg.end()) {
        return it->second._fn;
    }

    // Try adding more writers via plugin load:
    static const TfTokenVector SCOPE = { _tokens->MaxUsd, _tokens->ShaderReader };
    MaxUsd_RegistryHelper::FindAndLoadMaxPlug(SCOPE, usdInfoId);

    it = _Find(usdInfoId, importArgs);

    if (it != _reg.end()) {
        return it->second._fn;
    }

    if (_reg.count(usdInfoId) == 0) {
        // Nothing registered at all, remember that:
        Register(
            usdInfoId,
            [](const MaxUsd::MaxSceneBuilderOptions&) { return ContextSupport::Fallback; },
            nullptr);
    }

    return nullptr;
}

PXR_NAMESPACE_CLOSE_SCOPE
