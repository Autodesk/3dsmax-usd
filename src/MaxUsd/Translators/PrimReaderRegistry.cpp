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
#include "PrimReaderRegistry.h"

#include "FallbackPrimReader.h"
#include "FunctorPrimReader.h"
#include "RegistryHelper.h"

#include <MaxUsd/DebugCodes.h>

#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/stl.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/type.h>
#include <pxr/usd/usd/schemaBase.h>

#include <map>
#include <string>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (MaxUsd)
    (PrimReader)
);
// clang-format on

namespace {
struct _RegistryEntry
{
    MaxUsdPrimReaderRegistry::ContextPredicateFn _pred;
    MaxUsdPrimReaderRegistry::ReaderFactoryFn    _fn;
    int                                          _index;
};

typedef std::unordered_multimap<TfToken, _RegistryEntry, TfToken::HashFunctor> _Registry;
static _Registry                                                               _reg;
static int                                                                     _indexCounter = 0;

_Registry::const_iterator _Find(
    const TfType&                         tfType,
    const MaxUsd::MaxSceneBuilderOptions& importArgs,
    const UsdPrim&                        importPrim)
{
    using ContextSupport = MaxUsdPrimReader::ContextSupport;

    _Registry::const_iterator ret = _reg.cend();

    if (tfType.IsUnknown()) {
        return ret;
    }

    _Registry::const_iterator first, last;

    std::vector<TfType> ancestorTypes;
    tfType.GetAllAncestorTypes(&ancestorTypes);
    for (int i = 0; i < ancestorTypes.size(); ++i) {
        // the second element in the list is the parent type
        TfToken typeName(ancestorTypes[i].GetTypeName());

        std::tie(first, last) = _reg.equal_range(typeName);
        while (first != last) {
            ContextSupport support = first->second._pred(importArgs, importPrim);
            // Look for a "Supported" reader. If no "Supported" reader is found, use a "Fallback"
            // reader
            if (support == ContextSupport::Supported) {
                return first;
            } else if (support == ContextSupport::Fallback && ret == _reg.end()) {
                ret = first;
            }
            ++first;
        }
    }

    return ret;
}
} // namespace

/* static */
void MaxUsdPrimReaderRegistry::Register(
    const TfType&                                t,
    MaxUsdPrimReaderRegistry::ContextPredicateFn pred,
    MaxUsdPrimReaderRegistry::ReaderFactoryFn    fn,
    bool                                         fromPython)
{
    TfToken tfTypeName(t.GetTypeName());
    int     index = _indexCounter++;
    TF_DEBUG(PXR_MAXUSD_REGISTRY)
        .Msg("Registering MaxUsdPrimReader for TfType %s.\n", tfTypeName.GetText());
    _reg.insert(std::make_pair(tfTypeName, _RegistryEntry { pred, fn, index }));

    // The unloader uses the index to know which entry to erase when there are
    // more than one for the same usdInfoId.
    if (fn) {
        MaxUsd_RegistryHelper::AddUnloader(
            [tfTypeName, index]() {
                _Registry::const_iterator it, itEnd;
                std::tie(it, itEnd) = _reg.equal_range(tfTypeName);
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
void MaxUsdPrimReaderRegistry::RegisterRaw(const TfType& t, MaxUsdPrimReaderRegistry::ReaderFn fn)
{
    Register(
        t,
        [](const MaxUsd::MaxSceneBuilderOptions&, const UsdPrim&) {
            return MaxUsdPrimReader::ContextSupport::Fallback;
        },
        MaxUsd_FunctorPrimReader::CreateFactory(fn));
}

/* static */
MaxUsdPrimReaderRegistry::ReaderFactoryFn MaxUsdPrimReaderRegistry::Find(
    const TfToken&                        usdTypeName,
    const MaxUsd::MaxSceneBuilderOptions& importArgs,
    const UsdPrim&                        importPrim)
{
    TfRegistryManager::GetInstance().SubscribeTo<MaxUsdPrimReaderRegistry>();

    // unfortunately, usdTypeName is diff from the tfTypeName which we use to
    // register.  do the conversion here.
    TfType          tfType = PlugRegistry::FindDerivedTypeByName<UsdSchemaBase>(usdTypeName);
    std::string     typeNameStr = tfType.GetTypeName();
    TfToken         typeName(typeNameStr);
    ReaderFactoryFn ret = nullptr;

    _Registry::const_iterator it = _Find(tfType, importArgs, importPrim);

    if (it != _reg.end()) {
        return it->second._fn;
    }

    static const TfTokenVector SCOPE = { _tokens->MaxUsd, _tokens->PrimReader };
    // try to find a plugin suitable for the type and its ancestors
    std::vector<TfType> ancestorTypes;
    tfType.GetAllAncestorTypes(&ancestorTypes);
    for (int i = 0; i < ancestorTypes.size(); ++i) {
        MaxUsd_RegistryHelper::FindAndLoadMaxPlug(SCOPE, ancestorTypes[i].GetTypeName());

        // ideally something just registered itself.  if not, we at least put it in
        // the registry in case we encounter it again.
        it = _Find(tfType, importArgs, importPrim);
        if (it != _reg.end()) {
            return it->second._fn;
        }
    }

    if (_reg.count(typeName) == 0) {
        TF_DEBUG(PXR_MAXUSD_REGISTRY)
            .Msg("No MaxUsd reader plugin for TfType %s. No 3ds Max plugin.\n", typeName.GetText());
        Register(
            tfType,
            [](const MaxUsd::MaxSceneBuilderOptions&, const UsdPrim&) {
                return MaxUsdPrimReader::ContextSupport::Fallback;
            },
            nullptr);
    }
    return ret;
}

/* static */
MaxUsdPrimReaderRegistry::ReaderFactoryFn MaxUsdPrimReaderRegistry::FindOrFallback(
    const TfToken&                        usdTypeName,
    const MaxUsd::MaxSceneBuilderOptions& importArgs,
    const UsdPrim&                        importPrim)
{
    if (ReaderFactoryFn fn = Find(usdTypeName, importArgs, importPrim)) {
        return fn;
    }

    return MaxUsd_FallbackPrimReader::CreateFactory();
}

PXR_NAMESPACE_CLOSE_SCOPE
