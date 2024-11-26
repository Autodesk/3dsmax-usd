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
#pragma once

#include "PrimReader.h"
#include "ReadJobContext.h"

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>
#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

/// \class MaxUsdPrimReaderRegistry
/// \brief Provides functionality to register and lookup usd 3ds Max reader
/// plugins.
///
/// Use PXR_MAXUSD_DEFINE_READER(MyUsdType, usdPrim, args, ctx) to register a new reader
/// for 3ds Max.
///
/// In order for the core system to discover the plugin, you should also
/// have a plugInfo.json file that contains the type and 3ds Max plugin to load:
/// \code
/// {
///     "MaxUsd": {
///         "PrimReader": {
///             "providesTranslator": [
///                 "MyUsdType"
///             ]
///         }
///     }
/// }
/// \endcode
struct MaxUsdPrimReaderRegistry
{
    /// Reader factory function, i.e. a function that creates a prim reader
    /// for the given prim reader args.
    typedef std::function<MaxUsdPrimReaderSharedPtr(const UsdPrim&, MaxUsdReadJobContext&)>
        ReaderFactoryFn;

    /// Predicate function, i.e. a function that can tell the level of support
    /// the reader function will provide for a given context.
    typedef std::function<
        MaxUsdPrimReader::ContextSupport(const MaxUsd::MaxSceneBuilderOptions&, const UsdPrim&)>
        ContextPredicateFn;

    /// Reader function, i.e. a function that reads a prim. This is the
    /// signature of the function declared in the PXR_MAXUSD_DEFINE_READER
    /// macro.
    typedef std::function<
        bool(const UsdPrim&, const MaxUsd::MaxSceneBuilderOptions&, MaxUsdReadJobContext&)>
        ReaderFn;

    /// \brief Register fn as a reader provider for type and provide the supportability.
    MaxUSDAPI static void Register(
        const TfType&      type,
        ContextPredicateFn pred,
        ReaderFactoryFn    fn,
        bool               fromPython = false);

    /// \brief Register a reader provider for <T>.
    ///
    /// Example for registering a reader factory in your custom plugin, assuming
    /// that MyType is registered with the TfType system:
    /// \code{.cpp}
    /// class MyReader : public MaxUsdPrimReader {
    ///     static MaxUsdPrimReaderSharedPtr Create(
    ///             const UsdPrim&, MaxUsdReadJobContext&);
    /// };
    /// TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimReaderRegistry, MyType) {
    ///     MaxUsdPrimReaderRegistry::Register<MyType>(MyReader::Create);
    /// }
    /// \endcode
    template <typename T> static void Register(ReaderFactoryFn fn, bool fromPython = false)
    {
        if (TfType t = TfType::Find<T>()) {
            Register(t, MaxUsdPrimReader::CanImport, fn, fromPython);
        } else {
            TF_CODING_ERROR("Cannot register unknown TfType: %s.", ArchGetDemangled<T>().c_str());
        }
    }

    /// \brief Register fn as a reader provider for T and provide the supportability.
    /// Use "Supported" to override default reader
    ///
    /// Example for registering a reader factory in your custom plugin, assuming
    /// that MyType is registered with the TfType system:
    /// \code{.cpp}
    /// class MyReader : public MaxUsdPrimReader {
    ///     static MaxUsdPrimReaderSharedPtr Create(
    ///             const MaxSceneBuilderOptions&);
    ///     static CanImport(const MaxSceneBuilderOptions& importArgs, const UsdPrim& prim) {
    ///         return Supported; // After consulting the arguments
    ///     }
    /// };
    /// TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimReaderRegistry, MyType) {
    ///     MaxUsdPrimReaderRegistry::Register<MyType>(MyReader::CanImport, MyReader::Create);
    /// }
    /// \endcode
    template <typename T>
    static void Register(ContextPredicateFn pred, ReaderFactoryFn fn, bool fromPython = false)
    {
        if (TfType t = TfType::Find<T>()) {
            Register(t, pred, fn, fromPython);
        } else {
            TF_CODING_ERROR("Cannot register unknown TfType: %s.", ArchGetDemangled<T>().c_str());
        }
    }

    /// \brief Wraps fn in a ReaderFactoryFn and registers that factory
    /// function as a reader provider for <T>.
    /// This is a helper method for the macro PXR_MAXUSD_DEFINE_READER;
    /// you probably want to use PXR_MAXUSD_DEFINE_READER directly instead.
    MaxUSDAPI static void RegisterRaw(const TfType& type, ReaderFn fn);

    /**
     * \brief Unregisters a prim writer.
     * \param key The unique key for the writer.
     */
    MaxUSDAPI static void Unregister(const std::string& key);

    // takes a usdType (i.e. prim.GetTypeName())
    /// \brief Finds a reader factory if one exists for usdTypeName.
    ///
    /// \param usdTypeName should be a usd typeName, for example,
    /// \code
    /// prim.GetTypeName()
    /// \endcode
    MaxUSDAPI static ReaderFactoryFn Find(
        const TfToken&                        usdTypeName,
        const MaxUsd::MaxSceneBuilderOptions& importArgs,
        const UsdPrim&                        importPrim);

    /// Similar to Find(), but returns a "fallback" prim reader factory if none
    /// can be found for usdTypeName. Thus, this always returns a valid
    /// reader factory.
    MaxUSDAPI static ReaderFactoryFn FindOrFallback(
        const TfToken&                        usdTypeName,
        const MaxUsd::MaxSceneBuilderOptions& importArgs,
        const UsdPrim&                        importPrim);
};

// Lookup TfType by name instead of static C++ type when
// registering prim reader functions.
#define PXR_MAXUSD_DEFINE_READER(T, primVarName, argsVarName, ctxVarName)              \
    static bool MaxUsd_PrimReader_##T(                                                 \
        const UsdPrim&, const MaxUsd::MaxSceneBuilderOptions&, MaxUsdReadJobContext&); \
    TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimReaderRegistry, T)                         \
    {                                                                                  \
        if (TfType t = TfType::FindByName(#T)) {                                       \
            MaxUsdPrimReaderRegistry::RegisterRaw(t, MaxUsd_PrimReader_##T);           \
        } else {                                                                       \
            TF_CODING_ERROR("Cannot register unknown TfType: %s.", #T);                \
        }                                                                              \
    }                                                                                  \
    bool MaxUsd_PrimReader_##T(                                                        \
        const UsdPrim&                        primVarName,                             \
        const MaxUsd::MaxSceneBuilderOptions& argsVarName,                             \
        MaxUsdReadJobContext&                 ctxVarName)

// Lookup TfType by name instead of static C++ type when
// registering prim reader functions. This allows readers to be
// registered for codeless schemas, which are declared in the
// TfType system but have no corresponding C++ code.
#define PXR_MAXUSD_DEFINE_READER_FOR_USD_TYPE(T, usdPrimVarName, argsVarName, ctxVarName) \
    static bool MaxUsd_PrimReader_##T(                                                    \
        const UsdPrim&, const MaxUsd::MaxSceneBuilderOptions&, MaxUsdReadJobContext&);    \
    TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimReaderRegistry, T)                            \
    {                                                                                     \
        const TfType& tfType = PlugRegistry::FindDerivedTypeByName<UsdSchemaBase>(#T);    \
        if (tfType) {                                                                     \
            MaxUsdPrimReaderRegistry::RegisterRaw(tfType, MaxUsd_PrimReader_##T);         \
        } else {                                                                          \
            TF_CODING_ERROR("Cannot register unknown TfType for usdType: %s.", #T);       \
        }                                                                                 \
    }                                                                                     \
    bool MaxUsd_PrimReader_##T(                                                           \
        const UsdPrim&                        usdPrimVarName,                             \
        const MaxUsd::MaxSceneBuilderOptions& argsVarName,                                \
        MaxUsdReadJobContext&                 ctxVarName)

PXR_NAMESPACE_CLOSE_SCOPE