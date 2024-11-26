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
#include "ShaderReader.h"

#include "ReadJobContext.h"
#include "ShadingModeImporter.h"

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>

#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdShaderReader::MaxUsdShaderReader(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx)
    : MaxUsdPrimReader(prim, jobCtx)
{
}

/* static */
MaxUsdShaderReader::ContextSupport
MaxUsdShaderReader::CanImport(const MaxUsd::MaxSceneBuilderOptions&)
{
    // Default value for all readers is Fallback. More specialized writers can
    // override the base CanImport to report Supported/Unsupported as necessary.
    return ContextSupport::Fallback;
}

Mtl* MaxUsdShaderReader::GetCreatedMaterial(
    const MaxUsdShadingModeImportContext& context,
    const UsdPrim&                        prim) const
{
    Mtl* maxObject = nullptr;
    context.GetCreatedMaterial(prim, &maxObject);
    return maxObject;
}

PXR_NAMESPACE_CLOSE_SCOPE
