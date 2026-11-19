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

#include <ufe/undoableCommand.h>

#include <functional>
#include <memory>
#include <vector>

namespace MaxUsd {

// Wraps one or more forwarded USD edit callbacks in an undoable UFE command, so
// edit-forwarding operations are captured on the USD undo system and can be
// undone/redone.
class MaxUsdEditForwardCommand : public Ufe::UndoableCommand
{
public:
    using Ptr = std::shared_ptr<MaxUsdEditForwardCommand>;
    using Callbacks = std::vector<std::function<void()>>;

    // Public for std::make_shared(); prefer create().
    explicit MaxUsdEditForwardCommand(Callbacks callbacks);
    ~MaxUsdEditForwardCommand() override = default;

    static Ptr create(const Callbacks& callbacks);
    static Ptr create(const std::function<void()>& callback);

    std::string commandString() const override;

private:
    void execute() override;
    void undo() override;
    void redo() override;

    UsdUfe::UsdUndoableItem _undoableItem;
    Callbacks               _callbacks;
};

} // namespace MaxUsd
