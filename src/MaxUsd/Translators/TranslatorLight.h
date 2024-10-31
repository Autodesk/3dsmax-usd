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

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>
#include <MaxUsd/MaxUSDAPI.h>

PXR_NAMESPACE_OPEN_SCOPE

/// \brief Provides helper functions for translating to/from UsdLux
struct MaxUsdTranslatorLight
{
    /// Import a UsdLuxLightAPI schema as a corresponding 3ds Max light.
    /// Return true if the 3ds Max light was properly created and imported
    MaxUSDAPI static bool Read(
        const UsdPrim&                        prim,
        const MaxUsd::MaxSceneBuilderOptions& args,
        MaxUsdReadJobContext&                 context);
};

PXR_NAMESPACE_CLOSE_SCOPE