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

#include "WriteJobContext.h"

#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/output.h>
#include <pxr/usd/usdShade/shader.h>

class Mtl;

PXR_NAMESPACE_OPEN_SCOPE

namespace MaxUsdShadingUtils {

MaxUSDAPI MaterialBindings FetchMaterials(
    const MaxUsdWriteJobContext&                            writeJobContext,
    const pxr::TfHashSet<pxr::SdfPath, pxr::SdfPath::Hash>& primsToMaterialBind);

// Breaks the instancing of the given instance prim and copies the specified subset from the
// prototype child prim to the instance prim.
MaxUSDAPI UsdPrim BreakInstancingAndCopySubset(
    const UsdStageRefPtr&             stage,
    const UsdPrim&                    instancePrim,
    const UsdPrim&                    prototypeChildPrim,
    const std::vector<UsdGeomSubset>& subsetToCopy);

MaxUSDAPI UsdShadeOutput CreateShaderOutputAndConnectMaterial(
    UsdShadeShader&   shader,
    UsdShadeMaterial& material,
    const TfToken&    terminalName,
    const TfToken&    renderContext);

} // namespace MaxUsdShadingUtils

PXR_NAMESPACE_CLOSE_SCOPE
