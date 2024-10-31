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
#pragma once

#include "RegistryHelper.h"
#include "ShaderWriter.h"
#include "WriteJobContext.h"

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>

#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <functional>
#include <maxtypes.h>

PXR_NAMESPACE_OPEN_SCOPE

struct MaxUsdShaderWriterRegistry
{
    // Writer factory function, i.e. a function that creates a shader writer
    // for the given 3ds Max node/USD paths and context.
    using WriterFactoryFn
        = std::function<MaxUsdShaderWriterSharedPtr(Mtl*, const SdfPath&, MaxUsdWriteJobContext&)>;

    // Predicate function, i.e. a function that can tell the level of support
    // the writer function will provide for a given set of export options.
    using ContextPredicateFn
        = std::function<MaxUsdShaderWriter::ContextSupport(const MaxUsd::USDSceneBuilderOptions&)>;

    // Function indicating it is a target agnostic material writer.
    // The writer is registered on a material that can be exported to any target and
    // does not need to be exported to each specific target.
    // The ShaderWriter static function can be optionally declared.
    // A default method exists which returns 'false'. (see RegisterHelper
    // IsMaterialTargetAgnosticFunc)
    using TargetAgnosticFn = std::function<bool()>;

    // Register 'fn' as a factory function providing a
    // MaxUsdShaderWriter subclass that can be used to write the material ClassID.
    // If you can't provide a valid MaxUsdShaderWriter for the given arguments,
    // return a null pointer from the factory function 'fn'.
    MaxUSDAPI static void Register(
        const TfToken&     maxClassName,
        ContextPredicateFn pred,
        WriterFactoryFn    fn,
        TargetAgnosticFn   targetAgnosticFn,
        bool               fromPython = false);
    MaxUSDAPI static void Register(
        const Class_ID&    maxClassID,
        ContextPredicateFn pred,
        WriterFactoryFn    fn,
        TargetAgnosticFn   targetAgnosticFn,
        bool               fromPython = false);

    // Finds a writer if one exists for 3ds Max material ClassID using the context
    // found in 'exportArgs'
    // If there is no writer plugin for 3ds Max material ClassID, returns nullptr.
    MaxUSDAPI static WriterFactoryFn
    Find(const Class_ID& maxTypeName, const MaxUsd::USDSceneBuilderOptions& exportArgs);

    MaxUSDAPI static std::vector<Class_ID> GetAllTargetAgnosticMaterials();
};

#define PXR_MAXUSD_REGISTER_SHADER_WRITER(maxClassID, writerClass)                       \
    TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdShaderWriterRegistry, writerClass)               \
    {                                                                                    \
        static_assert(                                                                   \
            std::is_base_of<MaxUsdShaderWriter, writerClass>::value,                     \
            #writerClass " must derive from MaxUsdShaderWriter");                        \
        static_assert(                                                                   \
            HasCanExport<writerClass>::value,                                            \
            #writerClass " must define a static CanExport() function");                  \
        MaxUsdShaderWriterRegistry::Register(                                            \
            maxClassID,                                                                  \
            &writerClass::CanExport,                                                     \
            [](Mtl* material, const SdfPath& usd_path, MaxUsdWriteJobContext& job_ctx) { \
                return std::make_shared<writerClass>(material, usd_path, job_ctx);       \
            },                                                                           \
            IsMaterialTargetAgnosticFunc<writerClass>());                                \
    }

PXR_NAMESPACE_CLOSE_SCOPE
