//
// Copyright 2025 Autodesk
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

#include "Builders/USDSceneBuilderOptions.h"

#include <usdufe/undo/UsdUndoBlock.h>

#include <ufe/undoableCommand.h>
#include <ufe/undoableCommandMgr.h>

namespace MAXUSD_NS_DEF {

//! \brief ExportToStageCommand
class MaxUSDAPI ExportToStageCommand : public Ufe::UndoableCommand
{
public:
    typedef std::shared_ptr<ExportToStageCommand> Ptr;

    // Public for std::make_shared() access, use create() instead.
    ExportToStageCommand(
        const USDSceneBuilderOptions& buildOptions,
        const pxr::UsdStageRefPtr&    stage,
        bool                          allowOverwrite,
        const Matrix3&                rootTransform)
        : _buildOptions(buildOptions)
        , _stage(stage)
        , _allowOverwrite(allowOverwrite)
        , _rootTransform(rootTransform)
    {
    }

    ~ExportToStageCommand() = default;

    // Delete the copy/move constructors assignment operators.
    ExportToStageCommand(const ExportToStageCommand&) = delete;
    ExportToStageCommand& operator=(const ExportToStageCommand&) = delete;
    ExportToStageCommand(ExportToStageCommand&&) = delete;
    ExportToStageCommand& operator=(ExportToStageCommand&&) = delete;

    //! Create a ExportToStageCommand object
    static Ptr create(
        const USDSceneBuilderOptions& buildOptions,
        const pxr::UsdStageRefPtr&    stage,
        bool                          allowOverwrite,
        const Matrix3&                rootTransform);

    std::string commandString() const override { return "Export to USD Stage"; }

private:
    void execute() override;
    void undo() override { _undoableItem.undo(); }
    void redo() override { _undoableItem.redo(); }

    UsdUfe::UsdUndoableItem _undoableItem;
    USDSceneBuilderOptions  _buildOptions;
    UsdStageRefPtr          _stage;
    bool                    _allowOverwrite;
    Matrix3                 _rootTransform;

    bool                                       _cancelled = false;
    std::map<std::string, pxr::SdfLayerRefPtr> _editedLayers;
};

} // namespace MAXUSD_NS_DEF
