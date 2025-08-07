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

#include "ExportToStageCommand.h"

#include "Builders/USDSceneBuilder.h"
#include "Builders/USDSceneBuilderOptions.h"
#include "Utilities/DiagnosticDelegate.h"

#include <pxr/usd/usdGeom/metrics.h>

namespace MAXUSD_NS_DEF {

ExportToStageCommand::Ptr ExportToStageCommand::create(
    const USDSceneBuilderOptions& buildOptions,
    const UsdStageRefPtr&         stage,
    bool                          allowOverwrite,
    const Matrix3&                rootTransform)
{
    return std::make_shared<ExportToStageCommand>(
        buildOptions, stage, allowOverwrite, rootTransform);
}

void ExportToStageCommand::execute()
{
    if (_buildOptions.GetUseSeparateMaterialLayer()) {
        Tf_PostWarningHelper(
            TfCallContext("ExportToStageCommand.cpp", __func__, 40, __FUNCSIG__),
            "Warning :Exporting materials to a seperate layer is not supported when exporting to "
            "live "
            "stages.");
        _buildOptions.SetUseSeparateMaterialLayer(false);
    }

    auto upAxis = UsdGeomGetStageUpAxis(_stage);
    auto stageAxis = upAxis == UsdGeomTokens->y ? USDSceneBuilderOptions::UpAxis::Y
                                                : USDSceneBuilderOptions::UpAxis::Z;

    if (stageAxis != _buildOptions.GetUpAxis()) {
        TF_WARN("Export option up axis does not match the stage's up axis. It will be ignored.");
        _buildOptions.SetUpAxis(stageAxis);
    }

    MaxUsd::Log::Session exportLog("USDExport", _buildOptions.GetLogOptions());
    MaxUsd::Log::Info("Starting export to stage");
    const auto diagnosticDelegate
        = MaxUsd::Diagnostics::ScopedDelegate::Create<MaxUsd::Diagnostics::LogDelegate>();

    UsdUfe::UsdUndoManager::instance().trackLayerStates(_stage->GetEditTarget().GetLayer());

    UsdUfe::UsdUndoBlock undoBlock { &_undoableItem };

    USDSceneBuilder usdSceneBuilder;
    usdSceneBuilder.Build(
        _buildOptions, _cancelled, _stage, _editedLayers, _allowOverwrite, _rootTransform);

    if (_cancelled) {
        undo();
    }

    GetCOREInterface()->ForceCompleteRedraw();
}

} // namespace MAXUSD_NS_DEF