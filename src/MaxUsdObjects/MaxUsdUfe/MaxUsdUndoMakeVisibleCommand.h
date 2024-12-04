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

#include <usdUfe/undo/UsdUndoableItem.h>

#include <pxr/usd/usd/prim.h>

#include <ufe/undoableCommand.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

//! \brief MaxUsdUndoMakeVisibleCommand
class MaxUsdUndoMakeVisibleCommand : public Ufe::UndoableCommand
{
public:
    typedef std::shared_ptr<MaxUsdUndoMakeVisibleCommand> Ptr;

    // Public for std::make_shared() access, use create() instead.
    MaxUsdUndoMakeVisibleCommand(const pxr::UsdPrim& prim, bool vis);
    ~MaxUsdUndoMakeVisibleCommand() override;

    // Delete the copy/move constructors assignment operators.
    MaxUsdUndoMakeVisibleCommand(const MaxUsdUndoMakeVisibleCommand&) = delete;
    MaxUsdUndoMakeVisibleCommand& operator=(const MaxUsdUndoMakeVisibleCommand&) = delete;
    MaxUsdUndoMakeVisibleCommand(MaxUsdUndoMakeVisibleCommand&&) = delete;
    MaxUsdUndoMakeVisibleCommand& operator=(MaxUsdUndoMakeVisibleCommand&&) = delete;

    //! Create a MaxUsdUndoMakeVisibleCommand object
    static MaxUsdUndoMakeVisibleCommand::Ptr create(const pxr::UsdPrim& prim, bool vis);

private:
    void execute() override;
    void undo() override;
    void redo() override;

    pxr::UsdPrim            _prim;
    bool                    _visible;
    UsdUfe::UsdUndoableItem _undoableItem;

}; // MaxUsdUndoMakeVisibleCommand

} // namespace ufe
} // namespace MAXUSD_NS_DEF