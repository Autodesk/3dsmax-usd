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
#include "MaxUsdRootChildHierarchy.h"

#include <usdUfe/ufe/Global.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

MaxUsdRootChildHierarchy::MaxUsdRootChildHierarchy(const UsdUfe::UsdSceneItem::Ptr& item)
    : UsdRootChildHierarchy(item)
{
}

MaxUsdRootChildHierarchy::~MaxUsdRootChildHierarchy() { }

MaxUsdRootChildHierarchy::Ptr
MaxUsdRootChildHierarchy::create(const UsdUfe::UsdSceneItem::Ptr& item)
{
    return std::make_shared<MaxUsdRootChildHierarchy>(item);
}

bool MaxUsdRootChildHierarchy::childrenHook(
    const pxr::UsdPrim& child,
    Ufe::SceneItemList& children,
    bool                filterInactive) const
{
    const auto pathSegment = Ufe::PathSegment { child.GetName(), UsdUfe::getUsdRunTimeId(), '/' };
    if (!filterInactive || child.IsActive()) {
        children.emplace_back(
            UsdUfe::UsdSceneItem::create(sceneItem()->path() + pathSegment, child));
    }
    return true;
}

} // namespace ufe
} // namespace MAXUSD_NS_DEF