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
#include "ComponentUfeCommand.h"

#include "MaxVariantEditor.h"
#include "UtilComponentCreator.h"

#include <usdUfe/undo/UsdUndoBlock.h>

#include <pxr/usd/usdSkel/root.h>

#include <AdskUsdComponentCreator/Commands/AddVariantCommand.h>
#include <AdskUsdComponentCreator/Commands/AddVariantSetCommand.h>
#include <AdskUsdComponentCreator/Commands/ComponentCommand.h>
#include <AdskUsdComponentCreator/Commands/CreateFromFileCommand.h>
#include <AdskUsdComponentCreator/Commands/CreateFromStageCommand.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Descriptions/VariantDescription.h>
#include <AdskUsdComponentCreator/Utils/VariantsUtils.h>
#include <filesystem>
#include <stdexcept>

namespace {

// Opens the payload layers (one per scope, e.g. geo and mtl) of the variant that `selections`
// leads to, following nested variant sets. Empty if the component has no such variant yet.
std::vector<pxr::SdfLayerRefPtr> VariantPayloadLayers(
    const AdskUsdComponentCreator::ComponentDescription::Ptr& desc,
    const AdskUsdComponentCreator::VariantSelections&         selections)
{
    if (!desc || selections.empty()) {
        return {};
    }
    // The last entry of the nesting is the variant the selections lead to. The lookup throws when
    // the component has no such variant yet.
    AdskUsdComponentCreator::VariantNesting nesting;
    try {
        nesting = desc->GetVariantNestingFromSelections(selections);
    } catch (const std::runtime_error&) {
        return {};
    }
    const auto variant
        = std::dynamic_pointer_cast<AdskUsdComponentCreator::VariantDescription>(nesting.back());
    if (!variant) {
        return {};
    }

    // Get each payload's layer: the one already loaded (the stage's) when there is one. The paths
    // also cover variants nested under this one, which the clear leaves untouched; snapshotting
    // them too is harmless.
    const std::string rootDir
        = std::filesystem::path(desc->GetRootLayerFilename()).parent_path().string();
    std::vector<std::string> paths;
    AdskUsdComponentCreator::CollectVariantPayloadPaths(desc, variant, rootDir, paths);
    std::vector<pxr::SdfLayerRefPtr> layers;
    for (const auto& path : paths) {
        if (auto layer = pxr::SdfLayer::FindOrOpen(path)) {
            layers.push_back(layer);
        }
    }
    return layers;
}

pxr::SdfLayerRefPtr CopyOf(const pxr::SdfLayerRefPtr& layer)
{
    auto copy = pxr::SdfLayer::CreateAnonymous();
    copy->TransferContent(layer);
    return copy;
}

// Runs the command, putting back a SkelRoot type on the component prim if the command's rerun of
// the creator's setup redefined it as an Xform, which would leave skinned content without the
// SkelRoot it binds through.
bool ExecuteKeepingSkelRoot(const std::shared_ptr<AdskUsdComponentCreator::ComponentCommand>& cmd)
{
    const bool redefinesComponentPrim
        = std::dynamic_pointer_cast<AdskUsdComponentCreator::AddVariantCommand>(cmd)
        || std::dynamic_pointer_cast<AdskUsdComponentCreator::AddVariantSetCommand>(cmd)
        || std::dynamic_pointer_cast<AdskUsdComponentCreator::CreateFromStageCommand>(cmd)
        || std::dynamic_pointer_cast<AdskUsdComponentCreator::CreateFromFileCommand>(cmd);
    if (!redefinesComponentPrim) {
        return cmd->Execute();
    }

    const pxr::UsdStageRefPtr stage
        = AdskUsdComponentCreator::ComponentDescription::GetStageFromComponentDescription(
            cmd->GetComponentDescription());
    const auto desc = stage
        ? AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(stage)
        : nullptr;
    if (!desc) {
        return cmd->Execute();
    }
    const pxr::SdfPath rootPath = desc->GetRootPrimPath();

    const bool wasSkelRoot = stage->GetPrimAtPath(rootPath).IsA<pxr::UsdSkelRoot>();
    const bool ok = cmd->Execute();
    if (ok && wasSkelRoot && !stage->GetPrimAtPath(rootPath).IsA<pxr::UsdSkelRoot>()) {
        MaxUsd::ComponentCreator::MakeComponentPrimSkelRoot(stage);
    }
    return ok;
}

} // namespace

ComponentUfeCommand::ComponentUfeCommand(
    std::shared_ptr<AdskUsdComponentCreator::ComponentCommand> command)
    : command(std::move(command))
{
}

void ComponentUfeCommand::snapshotReplacedPayloads()
{
    // Only a replace into an existing component clears payloads the undo block cannot restore.
    const auto createCmd
        = std::dynamic_pointer_cast<AdskUsdComponentCreator::CreateFromStageCommand>(command);
    if (!createCmd) {
        return;
    }
    const auto& inputs = createCmd->GetInputs();
    if (!inputs.existingComponentDesc || !inputs.options.ReplaceVariantContent()) {
        return;
    }
    // The cleared state is only reachable once undo has reverted the recorded edits.
    for (const auto& layer : VariantPayloadLayers(
             inputs.existingComponentDesc, inputs.options.ComponentVariants())) {
        replacedPayloads.push_back({ layer, CopyOf(layer), nullptr });
    }
}

void ComponentUfeCommand::execute()
{
    bool ok = false;
    if (command->WantsAutomaticUndo()) {
        snapshotReplacedPayloads();
        // Track the command's layers, then capture its edits into undoableItem.
        command->TrackStageChanges();
        UsdUfe::UsdUndoBlock undoBlock(&undoableItem);
        ok = ExecuteKeepingSkelRoot(command);
    } else {
        ok = command->Execute();
    }
    // Throw on failure so MaxUfeUndoableCommandMgr::executeCmd cancels the hold cleanly.
    if (!ok) {
        throw std::runtime_error("ComponentCommand execution failed");
    }
}

void ComponentUfeCommand::undo()
{
    if (command->WantsAutomaticUndo()) {
        command->TrackStageChanges();
        undoableItem.undo();
        // Reverting the recorded edits leaves the payloads as the clear left them: keep that for
        // redo, then put the original content back.
        for (auto& snapshot : replacedPayloads) {
            snapshot.afterClear = CopyOf(snapshot.layer);
            snapshot.layer->TransferContent(snapshot.beforeClear);
        }
    }
    command->Undo();
    refreshVariantEditor();
}

void ComponentUfeCommand::redo()
{
    if (command->WantsAutomaticUndo()) {
        command->TrackStageChanges();
        // Back to the cleared state, so the replayed edits rebuild the replacement on top of it.
        for (const auto& snapshot : replacedPayloads) {
            if (snapshot.afterClear) {
                snapshot.layer->TransferContent(snapshot.afterClear);
            }
        }
        undoableItem.redo();
    }
    command->Redo();
    refreshVariantEditor();
}

std::string ComponentUfeCommand::commandString() const { return command->GetUndoLabel(); }

void ComponentUfeCommand::refreshVariantEditor() const
{
    // The command edits the library stage (a different instance sharing the same layers as
    // the displayed stage), so rebuild the editor's displayed component after undo/redo.
    if (MaxVariantEditor::IsCreated()) {
        MaxVariantEditor::Instance()->RefreshCurrent();
    }
}
