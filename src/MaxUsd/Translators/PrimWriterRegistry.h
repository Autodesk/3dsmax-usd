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
#pragma once
#include "PrimWriter.h"
#include "RegistryHelper.h"
#include "WriteJobContext.h"

#include <pxr/pxr.h>

#include <functional>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

/**
 * \brief Provides functionality to register and lookup USD writer plugins
 * for 3dsMax nodes.
 *
 * Use PXR_MAXUSD_REGISTER_WRITER(uniqueName, writerClass) to register a writer
 * class with the registry.
 *
 * Prim writers derive from the PrimWriter class and are expected to implement
 * several methods to specify what objects they are able to support, and how to actually
 * perform the conversion.

 * In order for the core system to discover the plugin at export time, you need a
 * plugInfo.json specifying the plugin's type.
 * \code
 * {
 *   "Plugins":[
 *      {
 *         "Info":{
 *            "MaxUsd":{
 *               "PrimWriter" : {}
 *            }
 *         },
 *         "Name":"myTranslatorPlugin",
 *         "Type":"library",
 *         "LibraryPath":"myTranslatorPlugin.dll"
 *      }
 *   ]
 * }
 * \endcode
 */
struct MaxUsdPrimWriterRegistry
{
    // Writer factory function, i.e. a function that creates a prim writer
    // for the given 3dsMax export job context.
    typedef std::function<MaxUsdPrimWriterSharedPtr(const MaxUsdWriteJobContext&, INode*)>
        WriterFactoryFn;

    // Predicate function, i.e. a function that can tell the level of support
    // the writer function will provide for a given set of export options.
    using ContextPredicateFn = std::function<
        MaxUsdPrimWriter::ContextSupport(INode*, const MaxUsd::USDSceneBuilderOptions&)>;

    /**
     * \brief Register a new prim writer via it's factory function.
     * \param A unique key for the writer. If not unique, an error is returned.
     * \param fn A factory function providing a MaxUsdPrimWriter subclass that can be used to write.
     * \param pred Predicate function used to know if this PrimWriter can be used to translate a node.
     * \param fromPython  true if the writer is registered from python.
     */
    MaxUSDAPI static void Register(
        const std::string& key,
        WriterFactoryFn    fn,
        ContextPredicateFn pred,
        bool               fromPython = false);

    /**
     * \brief Register a new base prim writer. Base Writers are the last evaluated prim writers.
     * They are not part of the prim reader registry since they act as a fallback mechanism for the
     * exporter. The order by which they are registered is also important to counter the 3ds Max
     * node 'polymorphism'.
     * \param fn A factory function providing a MaxUsdPrimWriter subclass that can be used to write.
     * \param pred Predicate function used to know if this PrimWriter can be used to translate a node.
     */
    MaxUSDAPI static void RegisterBaseWriter(WriterFactoryFn fn, ContextPredicateFn pred);

    /**
     * \brief Unregisters a prim writer.
     * \param key The unique key for the writer.
     */
    MaxUSDAPI static void Unregister(const std::string& key);

    /**
     * \brief Returns prim writers which can be used to translate a given object, given from a list of candidate writers.
     * Prioritize ContextSupport::Supported over ContextSupport::Fallback. Other than this, writers
     * are returned in the order they are they are received as candidates.
     * \param node The 3dsMax node we are finding a writer for.
     * \param candidateWriters A vector of candidate prim writers, to be filtered.
     * \return Ordered list of applicable prim writers (Writers with explicit support first, followed with writers that can be
     * used as fallback for this object).
     */
    MaxUSDAPI static MaxUsdPrimWriterSharedPtr
    FindWriter(const MaxUsdWriteJobContext& jobCtx, INode* node, size_t& numRegistered);

    /**
     * \brief Checks if a node can be exported by any of the available prim writers, considering
     * the given export options.
     * \param node The node to check for exportability.
     * \param exportArgs Export options to consider.
     * \return True, if the node can be exported by a prim writer.
     */
    MaxUSDAPI static bool
    CanBeExported(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs);
};

#define PXR_MAXUSD_REGISTER_WRITER(writerClass)                                         \
    TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimWriterRegistry, writerName##_##writerClass) \
    {                                                                                   \
        static_assert(                                                                  \
            std::is_base_of<MaxUsdPrimWriter, writerClass>::value,                      \
            #writerClass " must derive from MaxUsdPrimWriter");                         \
        static_assert(                                                                  \
            HasCanExport<writerClass>::value,                                           \
            #writerClass " must define a static CanExport() function");                 \
                                                                                        \
        MaxUsdPrimWriterRegistry::Register(                                             \
            #writerClass,                                                               \
            [](const MaxUsdWriteJobContext& jobCtx, INode* node) {                      \
                return std::make_shared<writerClass>(jobCtx, node);                     \
            },                                                                          \
            &writerClass::CanExport);                                                   \
    }

PXR_NAMESPACE_CLOSE_SCOPE