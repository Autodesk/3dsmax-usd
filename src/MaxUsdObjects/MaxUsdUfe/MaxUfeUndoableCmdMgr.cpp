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
#include "MaxUfeUndoableCommandMgr.h"

#include <MaxUsd/Utilities/DiagnosticDelegate.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <hold.h>

/**
 * \brief A 3dsMax restore object, holding on to a UFE undoable command.
 * The undo/redo operations are handled entirely in the UFE command.
 */
class UfeRestoreObj : public RestoreObj
{
public:
    UfeRestoreObj(const Ufe::UndoableCommand::Ptr& cmd)
        : ufeCmd(cmd)
    {
    }

    void Restore(int isUndo) override { ufeCmd->undo(); }
    void Redo() override { ufeCmd->redo(); }
    int  Size() override { return sizeof(Ufe::UndoableCommand::Ptr); }
    TSTR Description() override { return TSTR(_T("UFE Undoable command.")); }

private:
    //! The undoable UFE command.
    Ufe::UndoableCommand::Ptr ufeCmd;
};

MaxUfeUndoableCommandMgr::~MaxUfeUndoableCommandMgr() { }

void MaxUfeUndoableCommandMgr::executeCmd(const Ufe::UndoableCommand::Ptr& cmd) const
{
    auto execute = [this, &cmd]() {
        const auto del
            = MaxUsd::Diagnostics::ScopedDelegate::Create<MaxUsd::Diagnostics::ListenerDelegate>();
        UndoableCommandMgr::executeCmd(cmd);
    };

    // Insert the UFE command in the 3dsMax undo stack.
    if (!theHold.Holding()) {
        theHold.Begin();
        theHold.Put(new UfeRestoreObj(cmd));
        execute();
        theHold.Accept(TSTR::FromUTF8(cmd->commandString().c_str()));
    } else {
        if (!theHold.IsSuspended()) {
            theHold.Put(new UfeRestoreObj(cmd));
        }
        execute();
    }
}

/** Small helper class to override the command string of an Ufe undoable
 * command. 3dsMax, e.g. expects undoable commands to provide non-empty command
 * names to populate the undo history in the UI. As some commands don't provide
 * that, this class can be used to override the command name.
 * \see MaxUfeUndoableCommandMgr::named */
class NameOverrideUndoableCommand : public Ufe::UndoableCommand
{
public:
    NameOverrideUndoableCommand(const Ufe::UndoableCommand::Ptr& cmd, const std::string& name)
        : _cmd(cmd)
        , _name(name)
    {
    }

    void undo() override { _cmd->undo(); }
    void redo() override { _cmd->redo(); }
    void execute() override { _cmd->execute(); }

    std::string commandString() const override { return _name; }

private:
    Ufe::UndoableCommand::Ptr _cmd;
    std::string               _name;
};

Ufe::UndoableCommand::Ptr
MaxUfeUndoableCommandMgr::named(const Ufe::UndoableCommand::Ptr& cmd, const std::string& name)
{
    return std::make_shared<NameOverrideUndoableCommand>(cmd, name);
}
