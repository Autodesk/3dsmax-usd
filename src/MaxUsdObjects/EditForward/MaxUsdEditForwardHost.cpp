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
#include "MaxUsdEditForwardHost.h"

#include "MaxUsdEditForwardCommand.h"

#include <MaxUsd/Utilities/ListenerUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <usdUfe/ufe/Utils.h>
#include <usdUfe/undo/UsdUndoBlock.h>
#include <usdUfe/undo/UsdUndoManager.h>

#include <pxr/base/tf/envSetting.h>

#include <ufe/undoableCommandMgr.h>

#include <AdskUsdEditForward/Record.h>
#include <QtCore/QTimer>
#include <hold.h>

PXR_NAMESPACE_USING_DIRECTIVE

TF_DEFINE_ENV_SETTING(
    MAXUSD_ENABLE_EF_ECHO,
    false,
    "This env flag controls whether forwarded edits are echoed to the 3ds Max listener.");

namespace {

// Standalone forward path: a marked-forwardable edit made outside any command (an interactive
// transform) is batched and flushed on idle as its own undoable command. Max is single-threaded
// on the main loop, so file-scope state is safe here.
MaxUsd::MaxUsdEditForwardCommand::Callbacks g_pendingForwardCallbacks;
bool                                        g_idleFlushQueued = false;

// Command-attached forward path: while a capture scope is open (a UFE command is
// running), forwards are attached to the current command's PendingForward so the
// command's undo entry reverts the forward too.
int                         g_captureDepth = 0;
MaxUsd::PendingForward::Ptr g_currentCapture;
// True while a captured forward's callbacks run, so the forwarder does not
// recursively forward its own edits.
bool g_runningForward = false;

} // namespace

namespace MaxUsd {

void MaxUsdEditForwardHost::ExecuteInCmd(std::function<void()> callback, bool immediate)
{
    // Never forward the edits made by running a captured forward (re-entrancy).
    if (g_runningForward) {
        return;
    }

    // Explicit/on-demand forwarding (e.g. Forwarder::Execute()): run right away as
    // one undoable command.
    if (immediate) {
        if (callback) {
            Ufe::UndoableCommandMgr::instance().executeCmd(
                MaxUsdEditForwardCommand::create(callback));
        }
        return;
    }

    // Forward only edits from a running command (captured) or a marked transform. A bare edit
    // outside a command isn't forwarded -- its original never reaches the undo stack.
    if (!UsdUfe::NoUsdUndoBlockGuard::wantsForwarding() && g_captureDepth == 0) {
        return;
    }

    // Authoring into layers from inside a stage-change notice is unsafe, so always
    // defer the forward to the next idle.
    if (g_captureDepth > 0) {
        // A UFE command is running: accumulate the forward on the current command's
        // PendingForward. MaxUfeUndoableCommandMgr runs it synchronously right after
        // the command commits (deterministic; no Qt idle needed for the command path,
        // and authoring is safe once we are past the stage-change notice).
        if (!g_currentCapture) {
            g_currentCapture = std::make_shared<PendingForward>();
        }
        if (callback) {
            g_currentCapture->callbacks.push_back(std::move(callback));
        }
        return;
    }

    // Marked transform outside a command: no command to attach to, so batch and flush on the
    // next idle as its own undoable command.
    if (callback) {
        g_pendingForwardCallbacks.push_back(std::move(callback));
    }
    if (!g_idleFlushQueued) {
        g_idleFlushQueued = true;
        QTimer::singleShot(0, []() {
            MaxUsdEditForwardCommand::Callbacks batch;
            batch.swap(g_pendingForwardCallbacks);
            g_idleFlushQueued = false;
            if (batch.empty()) {
                return;
            }
            Ufe::UndoableCommandMgr::instance().executeCmd(MaxUsdEditForwardCommand::create(batch));
        });
    }
}

void MaxUsdEditForwardHost::BeginForwardCapture() { ++g_captureDepth; }

PendingForward::Ptr MaxUsdEditForwardHost::EndForwardCapture()
{
    if (g_captureDepth > 0) {
        --g_captureDepth;
    }
    if (g_captureDepth == 0) {
        const PendingForward::Ptr captured = g_currentCapture;
        g_currentCapture.reset();
        return captured;
    }
    return nullptr;
}

void MaxUsdEditForwardHost::ClearPendingForwards()
{
    // Drop deferred standalone forwards and any capture state. The pending standalone
    // QTimer (if scheduled) will then fire with an empty batch (a no-op).
    g_pendingForwardCallbacks.clear();
    g_currentCapture.reset();
    g_captureDepth = 0;
}

void MaxUsdEditForwardHost::RunCapturedForward(const PendingForward::Ptr& forward)
{
    if (!forward || forward->callbacks.empty() || g_runningForward) {
        return;
    }
    g_runningForward = true;
    {
        UsdUfe::UsdUndoBlock block(&forward->item);
        for (const auto& cb : forward->callbacks) {
            if (cb) {
                cb();
            }
        }
    }
    g_runningForward = false;
}

bool MaxUsdEditForwardHost::IsEditForwardingPaused() const
{
    // Pause while running a captured forward (its edits must not re-forward).
    if (_paused || g_runningForward) {
        return true;
    }
    // Otherwise pause during undo/redo -- the USD undo system restores the data directly.
    if (theHold.RestoreOrRedoing()) {
        return true;
    }
    return false;
}

void MaxUsdEditForwardHost::PauseEditForwarding(bool pause) { _paused = pause; }

void MaxUsdEditForwardHost::TrackLayerStates(const pxr::SdfLayerHandle& layer)
{
    UsdUfe::UsdUndoManager::instance().trackLayerStates(layer);
}

bool MaxUsdEditForwardHost::WantsEcho() const { return TfGetEnvSetting(MAXUSD_ENABLE_EF_ECHO); }

void MaxUsdEditForwardHost::Echo(const AdskUsdEditForward::Record& record)
{
    MaxUsd::Listener::Write(
        MaxUsd::UsdStringToMaxString("[Edit Forwarding] " + record.ToString()).data(), false);
}

} // namespace MaxUsd
