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

#include <pxr/usd/sdf/layer.h>

#include <ufe/undoableCommand.h>

#include <memory>
#include <string>
#include <vector>

namespace AdskUsdComponentCreator {
class ComponentCommand;
}

// Runs a ComponentCommand as a UFE undoable command so component ops share the single
// UFE undo / edit-forward path (MaxUfeUndoableCommandMgr) instead of a bespoke restore object.
class ComponentUfeCommand : public Ufe::UndoableCommand
{
public:
    explicit ComponentUfeCommand(
        std::shared_ptr<AdskUsdComponentCreator::ComponentCommand> command);

    void        execute() override;
    void        undo() override;
    void        redo() override;
    std::string commandString() const override;

private:
    void refreshVariantEditor() const;
    void snapshotReplacedPayloads();

    std::shared_ptr<AdskUsdComponentCreator::ComponentCommand> command;
    // For WantsAutomaticUndo commands: captures the command's layer edits at execute
    // time so undo/redo can revert/replay them.
    UsdUfe::UsdUndoableItem undoableItem;

    // Replacing a variant's content clears its payload layers with SdfLayer::Clear(), which a
    // crate-backed layer performs as a wholesale data swap the undo block cannot record. Their
    // content is snapshotted around the clear and restored by hand.
    // TODO: remove once the component creator clears these layers spec by spec, which the undo
    // block can record.
    struct PayloadSnapshot
    {
        pxr::SdfLayerRefPtr layer;
        pxr::SdfLayerRefPtr beforeClear;
        pxr::SdfLayerRefPtr afterClear;
    };
    std::vector<PayloadSnapshot> replacedPayloads;
};
