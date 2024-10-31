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
#include <MaxUsd/Translators/PrimReaderRegistry.h>
#include <MaxUsd/Translators/TranslatorUtils.h>

#include <pxr/pxr.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdShade/material.h>

PXR_NAMESPACE_OPEN_SCOPE

static bool _IsShadingNode(const UsdPrim& prim)
{
    // Note, UsdShadeShader prims are used in other contexts that aren't just
    // surface shading, so we just look for UsdShadeMaterial nodes.
    return prim.IsA<UsdShadeMaterial>();
}

PXR_MAXUSD_DEFINE_READER(UsdGeomScope, usdPrim, args, context)
{
    // If this scope contains only "shading" nodes (as in the "Looks" or
    // "Material" scopes that is often in assets), just skip.
    bool hasShadingData = false;
    bool hasNonShadingData = false;
    auto usdPrimName = usdPrim.GetName().GetString();

    for (const auto& child : usdPrim.GetChildren()) {
        auto usdPrimChildName = child.GetName().GetString();
        if (_IsShadingNode(child)) {
            hasShadingData = true;
        } else {
            hasNonShadingData = true;
            break;
        }
    }
    if (hasShadingData && !hasNonShadingData) {
        context.SetPruneChildren(true);
        return false;
    }

    return MaxUsdTranslatorUtil::CreateDummyHelperNode(usdPrim, usdPrim.GetName(), context);
}

PXR_NAMESPACE_CLOSE_SCOPE