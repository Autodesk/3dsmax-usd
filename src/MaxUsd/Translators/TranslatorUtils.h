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

#include <maxtypes.h>

PXR_NAMESPACE_OPEN_SCOPE

class UsdPrim;

/// \brief Provides helper functions for other readers/writers to use.
struct MaxUsdTranslatorUtil
{
    // Creates a "dummy" helper node for the given prim
    // \param usdPrim The prim translate to a helper
    // \param name The name to assign this node
    // \param jobCtx The current read job context
    // \return True if the prim was properly translated to a helper, false otherwise
    MaxUSDAPI static bool CreateDummyHelperNode(
        const UsdPrim&        usdPrim,
        const TfToken&        name,
        MaxUsdReadJobContext& jobCtx);

    // Attribute setter function signature for the ReadUsdAttribute helper
    // \param value The USD value extracted from the USD attribute
    // \param usdTimeCode The USD time code when the value was read
    // \param timeValue The 3ds Max corresponding time value the value was read
    typedef std::function<bool(const VtValue&, const UsdTimeCode&, const TimeValue&)>
        AttributeSetterFunction;
    // Read the value from a given USD attribute. The attribute, animated or not, is read on the
    // desired import interval and is assigned using the specified setter function
    // \param usdAttr The USD attribute to extract the value from
    // \param func The setter function to call to set the extracted value to the node's parameter
    // \param content The Read job Context associated to current import job (get the import interval to use)
    // \param onlyWhenAuthored Read the attribute value only when authored in the USD prim (default true); this
    //   can be useful if the setter function can set a parameter default value
    MaxUSDAPI static bool ReadUsdAttribute(
        const UsdAttribute&            usdAttr,
        const AttributeSetterFunction& func,
        const MaxUsdReadJobContext&    context,
        bool                           onlyWhenAuthored = true);
};

PXR_NAMESPACE_CLOSE_SCOPE