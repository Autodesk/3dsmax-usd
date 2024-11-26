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

#ifndef UFEUI_GENERIC_COMMAND_H
#define UFEUI_GENERIC_COMMAND_H

#include "UFEUIAPI.h"

#include <ufe/undoableCommand.h>

#include <functional>
#include <memory.h>

namespace UfeUI {

/** GenericCommand can be used to make some generic functionality undo-able.
 * To use this class, pass a std::function<void(Mode mode)> to the constructor.
 * This function will be called when the command is undone or redone passing the
 * respective Mode.
 * When using a lambda to do so, please make sure that the lambda is copyable or
 * move-able and consider the life-time of the captures inside the lambda. */
class GenericCommand : public Ufe::UndoableCommand
{
public:
    using Ptr = std::shared_ptr<GenericCommand>;

    enum class Mode
    {
        kRedo,
        kUndo
    };

    using Callback = std::function<void(Mode mode)>;

    /** Constructor
     * \param callback The functionality to be called during undo and redo.
     * \param commandString User friendly string representation of the command. */
    GenericCommand(const Callback& callback, const std::string& commandString)
        : _callback { callback }
        , _commandString { commandString }
    {
    }

    /** Constructor
     * \param callback The functionality to be called during undo and redo.
     * \param commandString User friendly string representation of the command. */
    GenericCommand(Callback&& callback, const std::string& commandString)
        : _callback { callback }
        , _commandString { commandString }
    {
    }

    // Get a user friendly string representation of the command.
    std::string commandString() const override { return _commandString; }

    /** On undo this command executes the given callback function in UNDO Mode. */
    void undo() override
    {
        if (_callback) {
            _callback(Mode::kUndo);
        }
    }

    /** On undo this command executes the given callback function in REDO Mode. */
    void redo() override
    {
        if (_callback) {
            _callback(Mode::kRedo);
        }
    }

    static Ptr create(const Callback& callback, const std::string& commandString)
    {
        return std::make_shared<GenericCommand>(callback, commandString);
    }

    static Ptr create(Callback&& callback, const std::string& commandString)
    {
        return std::make_shared<GenericCommand>(callback, commandString);
    }

private:
    Callback    _callback;
    std::string _commandString;
};

} // namespace UfeUI

#endif // UFEUI_REPLACE_SELECTION_COMMAND_H