//
// Copyright 2024 Autodesk
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

#ifndef UFEUI_REPLACE_SELECTION_COMMAND_H
#define UFEUI_REPLACE_SELECTION_COMMAND_H

#include "UFEUIAPI.h"

#include <ufe/selection.h>
#include <ufe/undoableCommand.h>

#include <functional>
#include <memory>
#include <unordered_map>

namespace UfeUi {

/** ReplaceSelectionCommand is used to make selection changes undoable. */
class UFEUIAPI ReplaceSelectionCommand : public Ufe::UndoableCommand
{
public:
    using Ptr = std::shared_ptr<ReplaceSelectionCommand>;

    /** Constructor
     * \param selection The new selection. */
    ReplaceSelectionCommand(const Ufe::Selection& selection);

    // Get a user friendly string representation of the command.
    std::string commandString() const override { return "Select"; }

    /** On undo this command replaces the Ufe::GlobalSelection with the
     * _previousSelection captured during the construction of the command. */
    void undo() override;

    /** On redo this command replaces the Ufe::GlobalSelection with the new
     * _selection passed to the constructor of the command. */
    void redo() override;

private:
    Ufe::Selection _previousSelection;
    Ufe::Selection _selection;
};

} // namespace UfeUi

#endif // UFEUI_REPLACE_SELECTION_COMMAND_H