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

#include "ReadJobContext.h"
#include "ShaderReader.h"

#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <functional>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

/// \class MaxUsdShaderReaderRegistry
/// \brief Provides functionality to register and lookup USD shader reader
/// plugins for 3ds Max nodes.
///
/// Use PXR_MAXUSD_REGISTER_SHADER_READER(usdInfoId, readerClass) to register a
/// reader class with the registry.
///
/// In order for the core system to discover the plugin, you need a
/// \c plugInfo.json that contains the usdInfoId:
/// \code
/// {
///   "Plugins": [
///     {
///       "Info": {
///         "MaxUsd": {
///           "ShaderReader": {
///             "providesTranslator": [
///               "myCustomShaderId"
///             ]
///           }
///         }
///       },
///       "Name": "myUsdPlugin",
///       "LibraryPath": "../myUsdPlugin.[dll|dylib|so]",
///       "Type": "library"
///     }
///   ]
/// }
/// \endcode
///
/// The plugin at LibraryPath will be loaded via the regular USD plugin loading
/// mechanism.
///
/// The registry contains information for both 3ds Max built-in node types
/// and for any user-defined plugin types. If MaxUsd does not ship with a
/// reader plugin for some 3ds Max built-in type, you can register your own
/// plugin for that 3ds Max built-in type.
struct MaxUsdShaderReaderRegistry
{
    /// Predicate function, i.e. a function that can tell the level of support
    /// the reader function will provide for a given context.
    typedef std::function<MaxUsdShaderReader::ContextSupport(const MaxUsd::MaxSceneBuilderOptions&)>
        ContextPredicateFn;

    /// Reader factory function, i.e. a function that creates a prim reader
    /// for the given prim reader args.
    typedef std::function<MaxUsdPrimReaderSharedPtr(const UsdPrim&, MaxUsdReadJobContext&)>
        ReaderFactoryFn;

    /// \brief Register fn as a factory function providing a
    /// MaxUsdShaderReader subclass that can be used to read usdInfoId.
    /// If you can't provide a valid MaxUsdShaderReader for the given arguments,
    /// return a null pointer from the factory function fn.
    ///
    /// Example for registering a reader factory in your custom plugin:
    /// \code{.cpp}
    /// class MyReader : public MaxUsdShaderReader {
    ///     static MaxUsdPrimReaderSharedPtr Create(
    ///             const UsdPrim&, MaxUsdReadJobContext&);
    ///     static CanImport(const MaxSceneBuilderOptions& importArgs) {
    ///         return Supported; // After consulting the arguments
    ///     }
    /// };
    /// TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdShaderReaderRegistry, MyReader) {
    ///     MaxUsdShaderReaderRegistry::Register("myCustomInfoId",
    ///             MyReader::CanImport,
    ///             MyReader::Create);
    /// }
    /// \endcode
    MaxUSDAPI static void Register(
        TfToken            usdInfoId,
        ContextPredicateFn pred,
        ReaderFactoryFn    fn,
        bool               fromPython = false);

    /// \brief Finds a reader if one exists for usdInfoId. The returned reader will have declared
    /// support given the current importArgs.
    ///
    /// If there is no supported reader plugin for usdInfoId, returns nullptr.
    MaxUSDAPI static ReaderFactoryFn
    Find(const TfToken& usdInfoId, const MaxUsd::MaxSceneBuilderOptions& importArgs);
};

/// \brief Registers a pre-existing reader class for the given USD info:id;
/// the reader class should be a subclass of MaxUsdShaderReader with a
/// constructor that takes <tt>(const UsdPrim& prim, const MaxUsdReadJobContext& readerArgs)</tt>
/// as argument.
///
/// Example:
/// \code{.cpp}
/// class MyReader : public MaxUsdShaderReader {
///     MyReader(
///             const UsdPrim& prim, const MaxUsdReadJobContext& readerArgs) {
///         // ...
///     }
///     static CanImport(const MaxSceneBuilderOptions& importArgs) {
///         return Supported; // After consulting the arguments
///     }
/// };
/// PXR_MAXUSD_REGISTER_SHADER_READER(myCustomInfoId, MyReader);
/// \endcode
#define PXR_MAXUSD_REGISTER_SHADER_READER(usdInfoId, readerClass)                        \
    TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdShaderReaderRegistry, usdInfoId##_##readerClass) \
    {                                                                                    \
        static_assert(                                                                   \
            std::is_base_of<MaxUsdShaderReader, readerClass>::value,                     \
            #readerClass " must derive from MaxUsdShaderReader");                        \
        MaxUsdShaderReaderRegistry::Register(                                            \
            TfToken(#usdInfoId),                                                         \
            readerClass::CanImport,                                                      \
            [](const UsdPrim& prim, const MaxSceneBuilderOptions& readerArgs) {          \
                return std::make_shared<readerClass>(readerArgs);                        \
            });                                                                          \
    }

PXR_NAMESPACE_CLOSE_SCOPE
