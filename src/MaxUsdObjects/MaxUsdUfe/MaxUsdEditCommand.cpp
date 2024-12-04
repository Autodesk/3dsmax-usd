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
#include "MaxUsdEditCommand.h"

#include <UFEUI/editCommand.h>

#include <MaxUsd/Utilities/ListenerUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <usdUfe/undo/UsdUndoManager.h>

#include <max.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

void MaxUsdEditCommand::post()
{
    // Trigger a redraw.
    GetCOREInterface()->RedrawViews(GetCOREInterface()->GetTime());
}

// When executing, undoing, or redoing commands, output any exception's message to the listener.
// UsdUfe does throw exceptions, and in maya-usd, they are similarly displayed in the listener.

void MaxUsdEditCommand::execute()
{
    const auto stage = getStage();
    if (!stage) {
        return;
    }

    // On first execution of the command, remember what the edit target was. Any subsequent
    // undo/redo will need to run on the same edit target.
    std::call_once(storeTargetFlag, [this, &stage]() {
        // Save current edit target - need to make sure undo/redo happens on the same edit target.
        originalEditTarget = stage->GetEditTarget();
        // Also make sure that edit target layer changes are tracked, for undo-redo.
        UsdUfe::UsdUndoManager::instance().trackLayerStates(originalEditTarget.GetLayer());
    });

    if (!checkEditTarget()) {
        return;
    }

    try {
        EditCommand::execute();
    } catch (const std::exception& ex) {
        Listener::Write(MaxUsd::UsdStringToMaxString(ex.what()).data(), true);
    }
}

void MaxUsdEditCommand::undo()
{
    if (!checkEditTarget()) {
        return;
    }

    try {
        EditCommand::undo();
    } catch (const std::exception& ex) {
        Listener::Write(MaxUsd::UsdStringToMaxString(ex.what()).data(), true);
    }
}

void MaxUsdEditCommand::redo()
{
    if (!checkEditTarget()) {
        return;
    }

    try {
        EditCommand::redo();
    } catch (const std::exception& ex) {
        Listener::Write(MaxUsd::UsdStringToMaxString(ex.what()).data(), true);
    }
}

pxr::UsdStageWeakPtr MaxUsdEditCommand::getStage()
{
    const auto prim = ufePathToPrim(itemPath());
    if (!prim.IsValid()) {
        return nullptr;
    }
    return prim.GetStage();
}

bool MaxUsdEditCommand::checkEditTarget()
{
    const auto stage = getStage();
    if (!stage) {
        return false;
    }

    if (undoRedoTargetFailure || stage->GetEditTarget() != originalEditTarget) {
        Listener::Write(L"Unable to undo/redo USD edit. The edit target was changed.", true);
        undoRedoTargetFailure |= true;
        return false;
    }
    return true;
}

} // namespace ufe
} // namespace MAXUSD_NS_DEF
