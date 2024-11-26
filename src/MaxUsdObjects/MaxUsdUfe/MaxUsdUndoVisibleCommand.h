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

//! \brief MaxUsdUndoVisibleCommand
class MaxUsdUndoVisibleCommand : public Ufe::UndoableCommand
{
public:
    typedef std::shared_ptr<MaxUsdUndoVisibleCommand> Ptr;

    // Public for std::make_shared() access, use create() instead.
    MaxUsdUndoVisibleCommand(const PXR_NS::UsdPrim& prim, bool vis);
    ~MaxUsdUndoVisibleCommand() override;

    // Delete the copy/move constructors assignment operators.
    MaxUsdUndoVisibleCommand(const MaxUsdUndoVisibleCommand&) = delete;
    MaxUsdUndoVisibleCommand& operator=(const MaxUsdUndoVisibleCommand&) = delete;
    MaxUsdUndoVisibleCommand(MaxUsdUndoVisibleCommand&&) = delete;
    MaxUsdUndoVisibleCommand& operator=(MaxUsdUndoVisibleCommand&&) = delete;

    //! Create a MaxUsdUndoVisibleCommand object
    static MaxUsdUndoVisibleCommand::Ptr create(const PXR_NS::UsdPrim& prim, bool vis);

private:
    void execute() override;
    void undo() override;
    void redo() override;

    PXR_NS::UsdPrim         _prim;
    bool                    _visible;
    UsdUfe::UsdUndoableItem _undoableItem;

}; // UsdUndoVisibleCommand

} // namespace ufe
} // namespace MAXUSD_NS_DEF
