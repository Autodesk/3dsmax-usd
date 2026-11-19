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
#include <MaxUsdObjects/MaxUsdObjectsAPI.h>

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include <AdskUsdComponentCreator/Containers.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <filesystem>
#include <string>
#include <vector>

class INode;
namespace AdskUsdComponentCreator {
class Options;
}

namespace MaxUsd::ComponentCreator {

// A set of prims (by path) within one source USD stage, used as the geometry source
// for the "from USD prims" component-creation / add flows.
struct UsdPrimSource
{
    pxr::UsdStageRefPtr       stage;
    std::vector<pxr::SdfPath> primPaths;
};

// Returns `base` with the overrides required to export selected nodes into a component source
// stage (flat NodeList export at the pseudo-root); other user settings are preserved.
MaxUSDObjectsAPI MaxUsd::USDSceneBuilderOptions
                 BuildSourceStageExportOptions(const MaxUsd::USDSceneBuilderOptions& base);

MaxUSDObjectsAPI bool IsAdskUsdComponent(const pxr::UsdStageRefPtr& stage);
MaxUSDObjectsAPI bool IsUnsavedAdskUsdComponent(const pxr::UsdStageRefPtr& stage);
// The layers a SaveComponent(stage) would affect, for the dirty-count badge / save flow.
MaxUSDObjectsAPI      std::vector<std::string>
                      GetComponentLayersToSave(const pxr::UsdStageRefPtr& stage);
MaxUSDObjectsAPI bool SaveAdskUsdComponent(const pxr::UsdStageRefPtr& stage);
// Reload the component from disk (discarding in-memory edits), via
// ComponentManager::ReloadComponent. Returns false for a null stage or on error.
MaxUSDObjectsAPI bool ReloadAdskUsdComponent(const pxr::UsdStageRefPtr& stage);

// Build the included-paths set for a flat node export (RootPrimPath = "/"): geometry prims
// (top-level pseudo-root children) land flat under /root/geo, and the individual material
// prims under /root/mtl rather than nesting the whole scope as /root/mtl/mtl. Shared by the
// create-from-selection and add-to-component node-export paths. Honours opts.IncludeMaterials():
// when materials are excluded, the exported /mtl scope is skipped entirely.
// `exportOptions` are the options the stage was exported with, so that a skinned export's
// root prim is stepped over rather than included.
MaxUSDObjectsAPI AdskUsdComponentCreator::PathsSet IncludedPathsForFlatExport(
    const pxr::UsdStageRefPtr&              stage,
    const AdskUsdComponentCreator::Options& opts,
    const MaxUsd::USDSceneBuilderOptions&   exportOptions);

// Export the given Max nodes into a fresh in-memory USD stage, flat under the pseudo-root
// (top-level nodes only, each expanded with its descendants) for the component creator to
// consume. Shared by the create-from-selection and add-to-component node paths.
// `rootTransform` offsets the exported prims, as the export-to-stage root transform does: pass
// the target component's stage object transform to have the nodes keep their placement relative
// to it ("Inherit Stage Object Transform"), or identity to export in world space.
MaxUSDObjectsAPI pxr::UsdStageRefPtr ExportINodesToSourceStage(
    const std::vector<INode*>&            nodes,
    const MaxUsd::USDSceneBuilderOptions& exportOptions,
    const Matrix3&                        rootTransform = Matrix3());

// Author the exported scene's units, up axis and frame rate onto a finished component. The
// exporter only authors these for a stage it creates itself, and the creator builds the
// component's layers from scratch, so they are otherwise left at USD's defaults. False if the
// stage is not a component.
MaxUSDObjectsAPI bool ApplySceneMetadataToComponent(
    const pxr::UsdStageRefPtr&            componentStage,
    const MaxUsd::USDSceneBuilderOptions& exportOptions);

// Whether an exported source stage holds skinned content, i.e. whether the component it feeds
// needs a SkelRoot (see MakeComponentPrimSkelRoot).
MaxUSDObjectsAPI bool SourceStageHasSkinnedContent(const pxr::UsdStageRefPtr& stage);

// Type the component's root prim as a SkelRoot so the component itself is the enclosing scope for
// its skinned content. No-op if the component prim is missing. Returns false on failure.
MaxUSDObjectsAPI bool MakeComponentPrimSkelRoot(const pxr::UsdStageRefPtr& componentStage);

// Add the given Max nodes to an existing component's variant (named by
// variantSelections). Only top-level nodes are used and each brings its child hierarchy
// (a node and its descendant are not added twice). Clones the component's options, applies
// the variant target / replace / purpose, and merges the exported nodes into the component
// stage. Returns false for a null component, empty nodes, or a component with no stage.
// This is the node-explicit core of MaxHost::AddSelectionToComponent (which
// supplies the current scene selection).
MaxUSDObjectsAPI bool AddINodesToComponent(
    const AdskUsdComponentCreator::ComponentDescription::Ptr& componentDesc,
    const std::vector<INode*>&                                nodes,
    const AdskUsdComponentCreator::VariantSelections&         variantSelections,
    bool                                                      isReplacing,
    const MaxUsd::USDSceneBuilderOptions&                     exportOptions,
    const std::string&                                        purpose = std::string(),
    const Matrix3&                                            rootTransform = Matrix3());

// Add the given USD prims (the included prims of one or more source stages) to an
// existing component's variant (named by variantSelections). Only top-level prims per source
// are used (a prim's descendants are folded in). Like AddINodesToComponent but USD-sourced:
// clones the component's options, applies the variant target / replace / purpose, and merges
// the prims into the component stage as an undoable add. Returns false for a null component or
// empty sources.
MaxUSDObjectsAPI bool AddPrimsToComponent(
    const AdskUsdComponentCreator::ComponentDescription::Ptr& componentDesc,
    const std::vector<UsdPrimSource>&                         primSources,
    const AdskUsdComponentCreator::VariantSelections&         variantSelections,
    bool                                                      isReplacing,
    const std::string&                                        purpose = std::string());

// Duplicate the given Max nodes into the component's currently targeted variant.
// Returns false (no-op) when no variant is targeted for edition; otherwise
// routes to AddINodesToComponent against the targeted variant's selections.
MaxUSDObjectsAPI bool DuplicateINodesToComponent(
    const AdskUsdComponentCreator::ComponentDescription::Ptr& componentDesc,
    const std::vector<INode*>&                                nodes,
    const MaxUsd::USDSceneBuilderOptions&                     exportOptions,
    const Matrix3&                                            rootTransform = Matrix3());

// The prim whose variants the Variant Manager should show for a selection at `primPath`: that
// prim if it has variant sets, otherwise its nearest ancestor that has. Invalid prim if none does.
MaxUSDObjectsAPI pxr::UsdPrim
FindPrimWithVariantSets(const pxr::UsdStageRefPtr& stage, const pxr::SdfPath& primPath);

MaxUSDObjectsAPI std::filesystem::path AllocateComponentTempFolder();
MaxUSDObjectsAPI void RemoveComponentTempFolder(const std::filesystem::path& folder);

// Return `nodes` plus all of their descendants (deduplicated). The Max node-list export only
// emits the exact nodes listed, so a node's child hierarchy must be expanded explicitly to be
// included.
MaxUSDObjectsAPI std::vector<INode*> CollectINodesAndDescendants(const std::vector<INode*>& nodes);

// Repoint the USDStageObject whose root layer currently resolves to oldRootLayerFilename
// at newRootLayerFilename (e.g. after a component is moved or renamed on disk).
// This is the shared implementation behind both MaxHost::HandleRenamedStage (the library
// rename callback) and the first-save relocation flow.
MaxUSDObjectsAPI void RepointStageRootLayer(
    const std::string& oldRootLayerFilename,
    const std::string& newRootLayerFilename);

// Save a component to disk for the first time (no UI): flush its dirty layers, move it to
// destFolder/name, and repoint the USDStageObject at the new root layer. Returns the new
// root-layer path, or an empty string on failure. This is the dialog-free core shared with the
// layer editor's unified save flow.
MaxUSDObjectsAPI std::string MoveComponentForSave(
    const pxr::UsdStageRefPtr& componentStage,
    const std::string&         destFolder,
    const std::string&         name);

// Register the component-save hooks with the layer editor (one-time, at plugin init) so the
// general Save Layers dialog can save component stages without linking against MaxUsd.
MaxUSDObjectsAPI void RegisterComponentHooks();

} // namespace MaxUsd::ComponentCreator
