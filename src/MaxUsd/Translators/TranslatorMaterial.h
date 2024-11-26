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
#include "WriteJobContext.h"

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>
#include <MaxUsd/Utilities/MaxProgressBar.h>

#include <pxr/usd/usdShade/material.h>

class Mtl;
class INode;

PXR_NAMESPACE_OPEN_SCOPE

/// \brief Provides helper functions for writing UsdShadeMaterial
struct MaxUsdTranslatorMaterial
{
    /// Reads material according to the shading mode found in buildOptions. Some shading modes
    /// may want to know the boundPrim. This returns a 3ds Max Mtl object.
    MaxUSDAPI static Mtl* Read(
        const MaxUsd::MaxSceneBuilderOptions& buildOptions,
        const UsdShadeMaterial&               shadeMaterial,
        const UsdGeomGprim&                   boundPrim,
        MaxUsdReadJobContext&                 context);

    /// Given a prim, assigns a material to it according to the shading mode found in
    /// buildOption. This will see which UsdShadeMaterial is bound to prim. If the material
    /// has not been read already, it will read it. The created/retrieved 3ds Max Mtl will be
    /// assigned to the node.
    MaxUSDAPI static bool AssignMaterial(
        const MaxUsd::MaxSceneBuilderOptions& buildOptions,
        const UsdGeomGprim&                   prim,
        INode*                                node,
        MaxUsdReadJobContext&                 context);

    /// Finds materials in the 3ds Max scene and exports them to the USD
    /// stage contained in \p writeJobContext.
    MaxUSDAPI static void ExportMaterials(
        MaxUsdWriteJobContext&                                  writeJobContext,
        const pxr::TfHashSet<pxr::SdfPath, pxr::SdfPath::Hash>& primsToMaterialBind,
        MaxUsd::MaxProgressBar&                                 progress);
};

PXR_NAMESPACE_CLOSE_SCOPE
