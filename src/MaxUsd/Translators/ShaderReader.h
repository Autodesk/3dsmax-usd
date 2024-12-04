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

#include "PrimReader.h"

#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/pxr.h>
#include <pxr/usd/usdShade/shader.h>

#include <Materials/Mtl.h>
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class TfToken;
class MaxUsdPrimReaderArgs;
class MaxUsdShadingModeImportContext;
class UsdShadeShader;

/// Base class for USD prim readers that import USD shader prims as 3ds Max materials.
class MaxUsdShaderReader : public MaxUsdPrimReader
{
public:
    MaxUSDAPI MaxUsdShaderReader(const UsdPrim&, MaxUsdReadJobContext&);

    /// This static function is expected for all shader readers and allows
    /// declaring how well this class can support the current context:
    MaxUSDAPI static ContextSupport CanImport(const MaxUsd::MaxSceneBuilderOptions& importArgs);

    /// Gets the 3ds Max material that was created by this reader
    MaxUSDAPI virtual Mtl*
    GetCreatedMaterial(const MaxUsdShadingModeImportContext&, const UsdPrim& prim) const;
};

typedef std::shared_ptr<MaxUsdShaderReader> MaxUsdShaderReaderSharedPtr;

PXR_NAMESPACE_CLOSE_SCOPE
