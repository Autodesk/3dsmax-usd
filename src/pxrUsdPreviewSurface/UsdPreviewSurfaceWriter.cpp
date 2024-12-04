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
#include "UsdPreviewSurfaceWriter.h"

#include <MaxUsd/Translators/ShadingModeRegistry.h>

#include <pxr/usdImaging/usdImaging/tokens.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(
    PxrMaxUsdPreviewSurfaceTokens,
    PXRUSDPREVIEWSURFACE_USD_PREVIEW_SURFACE_TOKENS);

REGISTER_SHADING_MODE_EXPORT_MATERIAL_CONVERSION(
    UsdImagingTokens->UsdPreviewSurface,
    UsdShadeTokens->universalRenderContext,
    PxrMaxUsdPreviewSurfaceTokens->niceName,
    PxrMaxUsdPreviewSurfaceTokens->exportDescription);

PXR_NAMESPACE_CLOSE_SCOPE
