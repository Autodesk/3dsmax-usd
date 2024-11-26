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
#include "MaxUsdHierarchyHandler.h"

#include "MaxUsdRootChildHierarchy.h"

#include <usdUfe/ufe/UsdHierarchy.h>
#include <usdUfe/ufe/UsdSceneItem.h>
#include <usdUfe/ufe/Utils.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

MaxUsdHierarchyHandler::MaxUsdHierarchyHandler()
    : UsdUfe::UsdHierarchyHandler()
{
}

MaxUsdHierarchyHandler::~MaxUsdHierarchyHandler() { }

/*static*/
MaxUsdHierarchyHandler::Ptr MaxUsdHierarchyHandler::create()
{
    return std::make_shared<MaxUsdHierarchyHandler>();
}

Ufe::Hierarchy::Ptr MaxUsdHierarchyHandler::hierarchy(const Ufe::SceneItem::Ptr& item) const
{
    PXR_NAMESPACE_USING_DIRECTIVE
    UsdUfe::UsdSceneItem::Ptr usdItem = std::dynamic_pointer_cast<UsdUfe::UsdSceneItem>(item);
    if (!TF_VERIFY(usdItem)) {
        return nullptr;
    }
    if (UsdUfe::isRootChild(usdItem->path())) {
        return MaxUsdRootChildHierarchy::create(usdItem);
    }
    return UsdUfe::UsdHierarchy::create(usdItem);
}

} // namespace ufe
} // namespace MAXUSD_NS_DEF