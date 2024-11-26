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
#include "MaxUsdObject3dHandler.h"

#include "MaxUsdObject3d.h"

#include <usdufe/ufe/usdsceneitem.h>

#include <ufe/runtimemgr.h>
#include <ufe/sceneItem.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

/*static*/
MaxUsdObject3dHandler::Ptr MaxUsdObject3dHandler::create()
{
    return std::make_shared<MaxUsdObject3dHandler>();
}

Ufe::Object3d::Ptr MaxUsdObject3dHandler::object3d(const Ufe::SceneItem::Ptr& item) const
{
    if (canCreateObject3dForItem(item)) {
        UsdUfe::UsdSceneItem::Ptr usdItem = std::dynamic_pointer_cast<UsdUfe::UsdSceneItem>(item);
        return MaxUsdObject3d::create(usdItem);
    }
    return {};
}

} // namespace ufe
} // namespace MAXUSD_NS_DEF
