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
#include "MaxUSDAPI.h"

#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>

#pragma once

namespace MAXUSD_NS_DEF {
namespace MetaData {
const pxr::TfToken matId("3dsmax:matId");
}
} // namespace MAXUSD_NS_DEF

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
#define MAX_USD_METADATA_TOKENS \
    (kind) \
    (purpose) \
    (hidden) \
    (creator)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(MaxUsdMetadataTokens, MaxUSDAPI, MAX_USD_METADATA_TOKENS);

// clang-format off
#define MAX_USD_PRIMVAR_TOKENS \
    (st) \
    (uv) \
    (mapShading) \
    (vertexColor) \
    (displayColor) \
    (displayOpacity)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(MaxUsdPrimvarTokens, MaxUSDAPI, MAX_USD_PRIMVAR_TOKENS);

// clang-format off
#define MAX_USD_PRIM_TYPE_TOKENS \
    (Xform) \
    (DiskLight) \
    (RectLight) \
    (CylinderLight) \
    (SphereLight) \
    (DistantLight) \
    (Camera) \
    (Mesh) \
    (BasisCurves) \
    (SkelRoot) \
    (Skeleton) \
    (Over) \
    (Class)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(MaxUsdPrimTypeTokens, MaxUSDAPI, MAX_USD_PRIM_TYPE_TOKENS);

// clang-format off
#define MAX_USDPREVIEWSURFACE_TOKENS \
    (diffuseColor) \
    (specularColor) \
    (metallic) \
    (normal) \
    (occlusion) \
    (emissiveColor) \
    (opacity) \
    (displacement) \
    (ior) \
    (clearcoat) \
    (clearcoatRoughness) \
    (roughness)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(MaxUsdUsdPreviewSurfaceTokens, MaxUSDAPI, MAX_USDPREVIEWSURFACE_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE