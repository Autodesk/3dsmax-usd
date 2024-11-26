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

#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

/// \brief Provides helper functions for reading UsdGeomXformable.
struct MaxUsdTranslatorXformable
{
    /// \brief reads xform attributes from xformable and converts them into 3ds Max transform values.
    // \param prim The prim associated to the 3ds Max Node
    // \param maxNode The 3ds Max node to position
    // \param context The current MaxUsdReadJobContext to register the 3ds Node to
    // \param correction Any correction to apply on the UsdPrim transform to properly position the 3ds Max node
    MaxUSDAPI static void Read(
        const UsdPrim&        prim,
        INode*                maxNode,
        MaxUsdReadJobContext& context,
        const Matrix3&        correction = Matrix3::Identity);
};

PXR_NAMESPACE_CLOSE_SCOPE
