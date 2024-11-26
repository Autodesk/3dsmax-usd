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
#include "MaxUsdUndoVisibleCommand.h"

#include <usdUfe/base/tokens.h>
#include <usdUfe/ufe/UsdUndoVisibleCommand.h>
#include <usdUfe/ufe/Utils.h>
#include <usdUfe/undo/UsdUndoBlock.h>

#include <pxr/usd/usdGeom/imageable.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

MaxUsdUndoVisibleCommand::MaxUsdUndoVisibleCommand(const UsdPrim& prim, bool vis)
    : Ufe::UndoableCommand()
    , _prim(prim)
    , _visible(vis)
{
}

MaxUsdUndoVisibleCommand::~MaxUsdUndoVisibleCommand() { }

MaxUsdUndoVisibleCommand::Ptr MaxUsdUndoVisibleCommand::create(const UsdPrim& prim, bool vis)
{
    if (!prim) {
        return nullptr;
    }
    return std::make_shared<MaxUsdUndoVisibleCommand>(prim, vis);
}

void MaxUsdUndoVisibleCommand::execute()
{
    UsdGeomImageable     primImageable(_prim);
    UsdUfe::UsdUndoBlock undoBlock(&_undoableItem);
    const auto&          visAttr = pxr::UsdGeomImageable(_prim).GetVisibilityAttr();
    visAttr.Set(_visible ? pxr::UsdGeomTokens->inherited : pxr::UsdGeomTokens->invisible);
}

void MaxUsdUndoVisibleCommand::redo() { _undoableItem.redo(); }

void MaxUsdUndoVisibleCommand::undo() { _undoableItem.undo(); }

} // namespace ufe
} // namespace MAXUSD_NS_DEF
