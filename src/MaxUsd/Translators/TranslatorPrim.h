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
#include <pxr/usd/usd/prim.h>

PXR_NAMESPACE_OPEN_SCOPE

/// \brief Provides helper functions for reading UsdPrim.
struct MaxUsdTranslatorPrim
{
    // Utility method to the create the 3ds Max node from an Object, to register the INode in the
    // MaxUsdReadJobContext and attach the node to its parent in the 3ds Max scene tree
    // \param prim The prim which generated the 3ds Max Object
    // \param object The 3ds Max Object for which we want an INode created
    // \param name The name to be given to the 3ds Max node
    // \param context The current MaxUsdReadJobContext to register the 3ds Node to
    // \param attachToParent Whether or not to attach the 3ds Max node to its parent, if any
    MaxUSDAPI static INode* CreateAndRegisterNode(
        const UsdPrim&        prim,
        Object*               object,
        const TfToken&        name,
        MaxUsdReadJobContext& context,
        bool                  attachToParent = true);

    // Helper method to read UsdPrim
    // Purpose, visibility state and 3ds Max custom attributes
    // \param prim The prim associated to the 3ds Max Node
    // \param maxNode The 3ds Max INode on which to applies attributes
    // \param context The current MaxUsdReadJobContext to register the 3ds Node to
    MaxUSDAPI static void Read(const UsdPrim& prim, INode* maxNode, MaxUsdReadJobContext& context);

private:
    // Helper method to read 3ds Max custom attributes (Purpose, Kind, Hidden) from the UsdPrim
    // \param prim The prim associated to the 3ds Max Node
    // \param maxNode The 3ds Max INode on which to applies attributes
    // \param context The current MaxUsdReadJobContext to register the 3ds Node to
    void static ReadMaxCustomAttributes(
        const UsdPrim&        prim,
        INode*                maxNode,
        MaxUsdReadJobContext& context);
};

PXR_NAMESPACE_CLOSE_SCOPE
