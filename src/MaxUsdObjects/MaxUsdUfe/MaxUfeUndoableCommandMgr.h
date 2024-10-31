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

#include <ufe/undoableCommandMgr.h>

class MaxUfeUndoableCommandMgr : public Ufe::UndoableCommandMgr
{
public:
    MaxUfeUndoableCommandMgr() = default;

    MaxUfeUndoableCommandMgr(const UndoableCommandMgr&) = delete;
    MaxUfeUndoableCommandMgr& operator=(const UndoableCommandMgr&) = delete;
    MaxUfeUndoableCommandMgr(MaxUfeUndoableCommandMgr&&) = delete;
    MaxUfeUndoableCommandMgr& operator=(MaxUfeUndoableCommandMgr&&) = delete;

    ~MaxUfeUndoableCommandMgr() override;

    void executeCmd(const Ufe::UndoableCommand::Ptr& cmd) const override;

    /** Helper function to override the command string of an Ufe undoable
     * command. 3dsMax, e.g. expects undoable commands to provide non-empty
     * command names to populate the undo history in the UI. As some commands
     * don't provide that, this function can be used to override the command
     * name. */
    static Ufe::UndoableCommand::Ptr
    named(const Ufe::UndoableCommand::Ptr& cmd, const std::string& name);
};
