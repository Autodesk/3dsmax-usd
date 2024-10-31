//
// Copyright 2018 Pixar
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

#include "WriteJobContext.h"

#include <pxr/usd/usd/common.h>

class Mtl;

PXR_NAMESPACE_OPEN_SCOPE

/// \brief The ShaderWriter base class from which material writers need to inherit from. A ShaderWriter instance is created for each material needing translation.
///
/// Two methods need to be implemented to have functional ShaderWriter:
/// - `CanExport(exportArgs)` – a static classmethod which returns an enum value stating if the
/// export context is `maxUsd.ShaderWriter.ContextSupport::Supported`, or `Unsupported`, or that the
/// class acts as a Fallback.
/// - `Write()` – the write method called to properly export the material
class MaxUsdShaderWriter
///
{
public:
    MaxUSDAPI
    MaxUsdShaderWriter(Mtl* material, const SdfPath& usdPath, MaxUsdWriteJobContext& jobCtx);

    virtual ~MaxUsdShaderWriter() = default;

    /// The level of support a writer can offer for a given context
    ///
    /// A basic writer that gives correct results across most contexts should
    /// report `Fallback`, while a specialized writer that really shines in a
    /// given context should report `Supported` when the context is right and
    /// `Unsupported` if the context is not as expected.
    enum class ContextSupport
    {
        Unsupported, //> Material type is not supported
        Supported,   ///> Material type is supported
        Fallback     //> Material type is not supported, use the fallback (default) writer
    };

    /// A static function is expected for all shader writers and allows
    /// declaring how well this class can support the current context.
    ///
    /// The prototype is:
    ///
    /// ```static ContextSupport CanExport(const USDSceneBuilderOptions& exportArgs);```
    ///

    /// Another static function can be declared (not required) and allows
    /// declaring if the material(s) the Writer is registered for are target agnostic.
    ///	A target agnostic material is a material that can be exported to any target, and doesn't
    ///need to be exported to each specific target.
    ///
    /// The prototype is:
    ///
    /// ```static bool IsMaterialTargetAgnostic();```
    ///

    /// \brief Main export function that runs when the applicable material gets hit.
    MaxUSDAPI virtual void Write() { }

    /// \brief Reports whether the ShaderWriter needs additional dependent materials to be exported.
    MaxUSDAPI virtual bool HasMaterialDependencies() const { return false; }

    /// \brief Retrieve the dependent materials
    /// \param subMtl The array to report the list of dependent materials
    MaxUSDAPI virtual void GetSubMtlDependencies(std::vector<Mtl*>& subMtl) const;

    /// \brief Method called after all materials are exported
    MaxUSDAPI virtual void PostWrite() { }

    /// \brief Gets the USD stage that we're writing to.
    MaxUSDAPI const UsdStageRefPtr& GetUsdStage() const;

    /// \brief The path of the destination USD prim to which we are writing.
    MaxUSDAPI const SdfPath& GetUsdPath() const;

    //// \brief The destination USD prim to which we are writing.
    MaxUSDAPI const UsdPrim& GetUsdPrim() const;

    /// \brief The 3ds Max material element being written by this writer.
    MaxUSDAPI Mtl* GetMaterial() const;

    /// \brief The filename to which the WriteJob exports to
    MaxUSDAPI const std::string& GetFilename() const;

    /// \brief Whether or not the exported file is a USDZ file
    MaxUSDAPI const bool IsUSDZFile() const;

protected:
    /// \brief Sets the destination USD prim to which we are writing. (Should only be used once in the
    /// constructor)
    MaxUSDAPI void SetUsdPrim(const UsdPrim& usdPrim);

    /// \brief Gets the current global export args in effect.
    MaxUSDAPI const MaxUsd::USDSceneBuilderOptions& GetExportArgs() const;

    /// \brief Gets the current map of exported materials and their paths
    MaxUSDAPI const std::map<Mtl*, SdfPath>& GetMaterialsToPrimsMap() const;

    UsdPrim                usdPrim;
    MaxUsdWriteJobContext& writeJobCtx;

private:
    Mtl*    material;
    SdfPath usdPath;
};

typedef std::shared_ptr<MaxUsdShaderWriter> MaxUsdShaderWriterSharedPtr;

PXR_NAMESPACE_CLOSE_SCOPE
