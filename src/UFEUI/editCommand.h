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

#include "UFEUIAPI.h"

#include <ufe/undoableCommand.h>

#include <functional>
#include <memory>

namespace UfeUi {

/**
 * \brief An edit command wraps a UFE undoable command, and allows to configure
 * pre and post execution behaviors for execute, undo, and redo by deriving from
 * it.
 */
class UFEUIAPI EditCommand : public Ufe::UndoableCommand
{
public:
    using Ptr = std::shared_ptr<EditCommand>;

    /**
     * \brief Constructor, prefer using create().
     * \param itemPath The path of the item concerned.
     * \param wrapped The undoable command to wrap.
     */
    EditCommand(
        const Ufe::Path&                 itemPath,
        const Ufe::UndoableCommand::Ptr& wrapped,
        const std::string&               commandString)
        : _wrappedCmd(wrapped)
        , _itemPath(itemPath)
        , _commandString(commandString)
    {
    }

    /**
     * \brief Get a user friendly string representation of the command.
     * \note For example, an application can print out the executed command to
     *       give an hint to the user of what was successfully done or to
     *       describe an undo step on the undo stack.
     */
    std::string commandString() const override { return _commandString; }

    /**
     * \brief Code executed before the command is executed, undone, or redone.
     */
    virtual void pre() { }

    /**
     * \brief Code executed after the command is executed, undone, or redone.
     */
    virtual void post() { }

    /**
     * \brief The UFE path of the item on which the command is performed.
     * \return The UFE path.
     */
    Ufe::Path& itemPath() { return _itemPath; }

    /**
     * \brief Creates a new edit command.
     * \param itemPath The path of the item concerned.
     * \param wrapped The undoable command to wrap.
     */
    static EditCommand::Ptr create(
        const Ufe::Path&                 path,
        const Ufe::UndoableCommand::Ptr& wrapped,
        const std::string&               commandString);

    using CreatorFunc = std::function<
        EditCommand::Ptr(const Ufe::Path&, const Ufe::UndoableCommand::Ptr&, const std::string&)>;
    /**
     * \brief Initializes the create function used to create EditCommands. This
     * allows the DCC to derive EditCommand, and have the derived class be used
     * by UFEUI.
     * \param creatorFunc The EditCommand creation function.
     */
    static void initializeCreator(CreatorFunc creatorFunc);

protected:
    //! Undoable command overrides.
    void execute() override
    {
        pre();
        _wrappedCmd->execute();
        post();
    }
    void undo() override
    {
        pre();
        _wrappedCmd->undo();
        post();
    }
    void redo() override
    {
        pre();
        _wrappedCmd->redo();
        post();
    }

    Ufe::UndoableCommand::Ptr _wrappedCmd;
    Ufe::Path                 _itemPath;
    std::string               _commandString;

    static CreatorFunc editCmdCreatorFunc;
};

} // namespace UfeUi