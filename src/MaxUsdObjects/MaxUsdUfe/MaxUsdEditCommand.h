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

#include "UfeUtils.h"

#include <UFEUI/editCommand.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

class MaxUsdEditCommand : public UfeUi::EditCommand
{
public:
    using UfeUi::EditCommand::EditCommand;

    void post() override;

protected:
    //! Undoable command overrides.
    void execute() override;
    void undo() override;
    void redo() override;

    pxr::UsdStageWeakPtr getStage();
    bool                 checkEditTarget();

private:
    /// On first execution of the command, store the current edit target.
    std::once_flag     storeTargetFlag;
    pxr::UsdEditTarget originalEditTarget;
    /// This flags tells us if an undo/redo operation was previously prevented
    /// because of a change to the edit target since first execution of the command.
    /// Once that happens, we should no longer perform any undo/redos, even if the target
    /// changes back to the initial one, to avoid state inconsistencies.
    bool undoRedoTargetFailure = false;
};

} // namespace ufe
} // namespace MAXUSD_NS_DEF