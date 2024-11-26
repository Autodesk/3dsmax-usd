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
#include "MaxUsdObject3d.h"

#include "MaxUsdUndoMakeVisibleCommand.h"
#include "MaxUsdUndoVisibleCommand.h"

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

MaxUsdObject3d::Ptr MaxUsdObject3d::create(const UsdUfe::UsdSceneItem::Ptr& item)
{
    return std::make_shared<MaxUsdObject3d>(item);
}

void MaxUsdObject3d::setVisibility(bool vis)
{
    const auto& visAttr = pxr::UsdGeomImageable(prim()).GetVisibilityAttr();
    visAttr.Set(vis ? pxr::UsdGeomTokens->inherited : pxr::UsdGeomTokens->invisible);
}

Ufe::UndoableCommand::Ptr MaxUsdObject3d::setVisibleCmd(bool vis)
{
    return MaxUsdUndoVisibleCommand::create(prim(), vis);
}

Ufe::UndoableCommand::Ptr MaxUsdObject3d::makeVisibleCmd(bool vis)
{
    return MaxUsdUndoMakeVisibleCommand::create(prim(), vis);
}

} // namespace ufe
} // namespace MAXUSD_NS_DEF
