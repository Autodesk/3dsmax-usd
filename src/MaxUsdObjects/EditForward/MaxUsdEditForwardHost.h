//
// Copyright 2026 Autodesk
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

#include <AdskUsdEditForward/Host.h>
#include <functional>
#include <memory>
#include <vector>

namespace MaxUsd {

// The forward operation captured for a single undoable command: the forwarding
// callbacks (run on the next idle) and the USD undo item recording their edits, so
// the owning command's undo/redo can revert/replay the forward as part of the same
// undo entry. Held by the command's UfeRestoreObj (see MaxUfeUndoableCmdMgr).
struct PendingForward
{
    using Ptr = std::shared_ptr<PendingForward>;
    std::vector<std::function<void()>> callbacks;
    UsdUfe::UsdUndoableItem            item;
    bool                               scheduled = false;
};

// 3dsMax implementation of the USD Edit Forward host interface. Drives forwarded edits through the
// UFE undoable command system so they are captured on the 3dsMax/USD undo stack, and defers
// notice-driven forwarding to the next idle (authoring into layers from inside a stage-change
// notice is unsafe).
class MaxUsdEditForwardHost : public AdskUsdEditForward::Host
{
public:
    MaxUsdEditForwardHost() = default;
    ~MaxUsdEditForwardHost() override = default;

    void ExecuteInCmd(std::function<void()> callback, bool immediate) override;

    bool IsEditForwardingPaused() const override;
    void PauseEditForwarding(bool pause) override;
    void TrackLayerStates(const pxr::SdfLayerHandle& layer) override;

    bool WantsEcho() const override;
    void Echo(const AdskUsdEditForward::Record& record) override;

    // Capture scope: while open, notice-driven forwards are attached to the current
    // command (their edits become part of its undo entry) instead of forming a
    // standalone undo entry. MaxUfeUndoableCommandMgr opens this around each command
    // and stores the returned PendingForward on the command's UfeRestoreObj. Nested
    // (joined) commands share the outermost capture; EndForwardCapture returns the
    // captured forward only when the outermost scope closes (null otherwise).
    void                BeginForwardCapture();
    PendingForward::Ptr EndForwardCapture();

    // Run a captured forward's callbacks now, recording their edits into its undo
    // item. Called by MaxUfeUndoableCommandMgr right after the owning command commits
    // (outside the stage-change notice, so authoring is safe) -- this is synchronous
    // and deterministic, unlike the standalone (non-command) path which defers to a
    // Qt idle. No-op while already running a forward (re-entrancy guard).
    void RunCapturedForward(const PendingForward::Ptr& forward);

    // Drop any pending (not-yet-run) forward state. Called on stage teardown / scene
    // reset so deferred standalone forwards and capture state never leak across
    // operations (or, in the test harness, across tests where no event loop runs
    // between them). State is process-global, so this is static.
    static void ClearPendingForwards();

private:
    bool _paused = false;
};

} // namespace MaxUsd
