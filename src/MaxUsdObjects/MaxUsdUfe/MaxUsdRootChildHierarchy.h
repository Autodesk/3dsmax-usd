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

#include <usdUfe/ufe/UsdRootChildHierarchy.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

/**
 * \brief USD runtime hierarchy interface for children of the USD root prim.
 * This class modifies its base class implementation to fit the MaxUsd UFE
 * path layout. When appending children to the path of a stage object, we need
 * to create the new USD path segment. The stage object path contains a single
 * segment with the GUID, and maps to the USD pseudo-root prim. Actual USD Prims
 * always have paths in the form [Stage object path segment, usd path segment]
 */
class MaxUsdRootChildHierarchy : public UsdUfe::UsdRootChildHierarchy
{
public:
    typedef std::shared_ptr<MaxUsdRootChildHierarchy> Ptr;

    MaxUsdRootChildHierarchy(const UsdUfe::UsdSceneItem::Ptr& item);
    ~MaxUsdRootChildHierarchy() override;

    // Delete the copy/move constructors assignment operators.
    MaxUsdRootChildHierarchy(const MaxUsdRootChildHierarchy&) = delete;
    MaxUsdRootChildHierarchy& operator=(const MaxUsdRootChildHierarchy&) = delete;
    MaxUsdRootChildHierarchy(MaxUsdRootChildHierarchy&&) = delete;
    MaxUsdRootChildHierarchy& operator=(MaxUsdRootChildHierarchy&&) = delete;

    //! Create a MaxUsdRootChildHierarchy.
    static MaxUsdRootChildHierarchy::Ptr create(const UsdUfe::UsdSceneItem::Ptr& item);

protected:
    // UsdHierarchy overrides
    bool childrenHook(
        const PXR_NS::UsdPrim& child,
        Ufe::SceneItemList&    children,
        bool                   filterInactive) const override;
};

} // namespace ufe
} // namespace MAXUSD_NS_DEF