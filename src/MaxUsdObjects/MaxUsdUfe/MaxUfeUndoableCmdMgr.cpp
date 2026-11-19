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
#ifdef ADSK_COMPONENT_CREATOR_ENABLED
#include "EditForward/MaxUsdEditForwardHost.h"
#endif
#include "MaxUfeUndoableCommandMgr.h"
#include "MaxUsdEditCommand.h"
#include "StageObjectMap.h"

#include <MaxUsd/Utilities/DiagnosticDelegate.h>
#include <MaxUsd/Utilities/ListenerUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <usdUfe/undo/UsdUndoManager.h>

#ifdef ADSK_COMPONENT_CREATOR_ENABLED
#include <AdskUsdEditForward/Host.h>
#endif
#include <hold.h>
#include <memory>

/**
 * \brief A 3dsMax restore object, holding on to a UFE undoable command.
 * The undo/redo operations are handled entirely in the UFE command.
 */
class UfeRestoreObj : public RestoreObj
{
public:
#ifdef ADSK_COMPONENT_CREATOR_ENABLED
    UfeRestoreObj(
        const Ufe::UndoableCommand::Ptr& cmd,
        MaxUsd::PendingForward::Ptr      forward = nullptr)
        : ufeCmd(cmd)
        , forward(std::move(forward))
    {
    }
#else
    UfeRestoreObj(const Ufe::UndoableCommand::Ptr& cmd)
        : ufeCmd(cmd)
    {
    }
#endif

    void Restore(int isUndo) override
    {
        try {
#ifdef ADSK_COMPONENT_CREATOR_ENABLED
            // The forward was applied after the original edit, so undo it first,
            // then undo the command itself.
            if (forward) {
                forward->item.undo();
            }
#endif
            ufeCmd->undo();
        } catch (const std::exception& e) {
            MaxUsd::Listener::Write(MaxUsd::UsdStringToMaxString(e.what()).data(), true);
        }
    }

    void Redo() override
    {
        try {
            ufeCmd->redo();
#ifdef ADSK_COMPONENT_CREATOR_ENABLED
            if (forward) {
                forward->item.redo();
            }
#endif
        } catch (const std::exception& e) {
            MaxUsd::Listener::Write(MaxUsd::UsdStringToMaxString(e.what()).data(), true);
        }
    }

    int  Size() override { return sizeof(Ufe::UndoableCommand::Ptr); }
    TSTR Description() override { return TSTR(_T("UFE Undoable command.")); }

private:
    //! The undoable UFE command.
    Ufe::UndoableCommand::Ptr ufeCmd;
#ifdef ADSK_COMPONENT_CREATOR_ENABLED
    //! The edit-forward operation triggered by this command (filled on idle), undone
    //! and redone together with the command so they form a single undo step.
    MaxUsd::PendingForward::Ptr forward;
#endif
};

MaxUfeUndoableCommandMgr::~MaxUfeUndoableCommandMgr() { }

void MaxUfeUndoableCommandMgr::trackEditTargetLayersForUndo(const Ufe::UndoableCommand::Ptr& cmd)
{
    // MaxUsdEditCommand::execute() already calls trackLayerStates on the edit target for
    // itemPath() before running the wrapped command.
    if (cmd && std::dynamic_pointer_cast<MAXUSD_NS_DEF::ufe::MaxUsdEditCommand>(cmd)) {
        return;
    }

    // Workaround: LDX and other UFE commands may not go through MaxUsdEditCommand, so they
    // don't call trackLayerStates even though usdUfe needs it before undo can work.
    // Registering at stage setup (SetUSDStage, etc.) would be nicer, but that seems to
    // break layer dirty state after a .max reload on older USD (see test_save_load_dirty_layer).
    //
    // So we register the current edit target on every open stage. The command doesn't tell
    // us which layer it will touch; we assume the edit target, which is usually right but
    // not always (e.g. edits on another layer in the stack).
    for (USDStageObject* stageObject : StageObjectMap::GetInstance()->GetAllStageObjects()) {
        const pxr::UsdStageWeakPtr usdStage = stageObject->GetUSDStage();
        if (!usdStage) {
            continue;
        }
        const pxr::SdfLayerHandle editTargetLayer = usdStage->GetEditTarget().GetLayer();
        if (!editTargetLayer) {
            continue;
        }
        UsdUfe::UsdUndoManager::instance().trackLayerStates(editTargetLayer);
    }
}

void MaxUfeUndoableCommandMgr::executeCmd(const Ufe::UndoableCommand::Ptr& cmd) const
{
    trackEditTargetLayersForUndo(cmd);

#ifdef ADSK_COMPONENT_CREATOR_ENABLED
    auto host = std::dynamic_pointer_cast<MaxUsd::MaxUsdEditForwardHost>(
        AdskUsdEditForward::Host::GetInstance());

    // Collected forward for this command (filled below, after the command commits).
    MaxUsd::PendingForward::Ptr forward;
#endif

#ifdef ADSK_COMPONENT_CREATOR_ENABLED
    auto runCmd = [this, &cmd, &host, &forward]() {
#else
    auto runCmd = [this, &cmd]() {
#endif
        // Run the command before we populate the undo stack - if the command fails (throws)
        // we do not want a ghost command on the stack.
        const auto del
            = MaxUsd::Diagnostics::ScopedDelegate::Create<MaxUsd::Diagnostics::ListenerDelegate>();

#ifdef ADSK_COMPONENT_CREATOR_ENABLED
        // Open an edit-forward capture scope around the command so any forward it
        // triggers is attached to this command's undo entry (reverted/replayed with
        // it), rather than forming a standalone entry.
        if (host) {
            host->BeginForwardCapture();
        }
#endif

        bool ok = false;
        try {
            UndoableCommandMgr::executeCmd(cmd);
            ok = true;
        } catch (const std::exception& ex) {
            MaxUsd::Listener::Write(MaxUsd::UsdStringToMaxString(ex.what()).data(), true);
        }

#ifdef ADSK_COMPONENT_CREATOR_ENABLED
        // Always balance BeginForwardCapture; collect the forward (if any) for the
        // restore object.
        if (host) {
            forward = host->EndForwardCapture();
        }
        if (ok) {
            theHold.Put(new UfeRestoreObj(cmd, forward));
        }
#else
        if (ok) {
            theHold.Put(new UfeRestoreObj(cmd));
        }
#endif
        return ok;
    };

    // Insert the UFE command in the 3dsMax undo stack.
    if (!theHold.Holding()) {
        theHold.Begin();
        if (runCmd()) {
            theHold.Accept(TSTR::FromUTF8(cmd->commandString().c_str()));
        } else {
            theHold.Cancel();
        }
    } else {
        if (!theHold.IsSuspended()) {
            runCmd();
        }
    }

#ifdef ADSK_COMPONENT_CREATOR_ENABLED
    if (host && forward) {
        host->RunCapturedForward(forward);
    }
#endif
}

void MaxUfeUndoableCommandMgr::registerCmd(const Ufe::UndoableCommand::Ptr& cmd) const
{
    // Called in the UFE UndoableCommandGuard by the UFE CompositeCommandMgr after it has
    // executed all commands in its scope and composed them into a single composite. We
    // register that composite here so the entire guarded operation appears as one undo entry.
    trackEditTargetLayersForUndo(cmd);

    if (!theHold.Holding()) {
        theHold.Begin();
        theHold.Put(new UfeRestoreObj(cmd));
        theHold.Accept(TSTR::FromUTF8(cmd->commandString().c_str()));
    } else if (!theHold.IsSuspended()) {
        theHold.Put(new UfeRestoreObj(cmd));
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
