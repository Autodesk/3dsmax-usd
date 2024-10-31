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
#include "ReadJobContext.h"

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>
#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdShade/material.h>

#include <functional>

PXR_NAMESPACE_OPEN_SCOPE

class MaxUsdShadingModeImportContext
{
public:
    const UsdShadeMaterial& GetShadeMaterial() const { return shadeMaterial; }
    const UsdGeomGprim&     GetBoundPrim() const { return boundPrim; }

    MaxUsdShadingModeImportContext(
        const UsdShadeMaterial& shadeMaterial,
        const UsdGeomGprim&     boundPrim,
        MaxUsdReadJobContext&   jobCtx)
        : shadeMaterial(shadeMaterial)
        , boundPrim(boundPrim)
        , jobContext(jobCtx)
    {
    }

    /// \name Reuse ReferenceTarget on Import
    /// @{
    /// For example, if a shader node is used by multiple other nodes, you can
    /// use these functions to ensure that only 1 gets created.
    ///
    /// If your importer wants to try to re-use objects that were created by
    /// an earlier invocation (or by other things in the importer), you can
    /// add/retrieve them using these functions.

    /// This will return true and matRef will be set to the
    /// previously created 3ds Max material.  Otherwise, this returns false;
    /// If prim is an invalid UsdPrim, this will return false.
    MaxUSDAPI bool GetCreatedMaterial(const UsdPrim& prim, Mtl** matRef) const;

    /// If you want to register a prim so that other parts of the import
    /// uses them, this function registers mat as being created.
    /// If prim is an invalid UsdPrim, nothing will get stored and mat
    /// will be returned.
    MaxUSDAPI Mtl* AddCreatedMaterial(const UsdPrim& prim, Mtl* mat);

    /// If you want to register a path so that other parts of the import
    /// uses them, this function registers mat as being created.
    /// If path is an empty SdfPath, nothing will get stored and mat
    /// will be returned.
    MaxUSDAPI Mtl* AddCreatedMaterial(const SdfPath& path, Mtl* mat);
    /// @}

    /// Returns the reader job context for this shading mode import.
    MaxUSDAPI MaxUsdReadJobContext& GetReadJobContext() const;

private:
    const UsdShadeMaterial& shadeMaterial;
    const UsdGeomGprim&     boundPrim;
    MaxUsdReadJobContext&   jobContext;
};
typedef std::function<Mtl*(MaxUsdShadingModeImportContext*, const MaxUsd::MaxSceneBuilderOptions&)>
    MaxUsdShadingModeImporter;

PXR_NAMESPACE_CLOSE_SCOPE
