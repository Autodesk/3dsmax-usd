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
#include "UtilComponentCreator.h"

#include "MaxComponentManager.h"
#include "MaxVariantEditor.h"

#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>
#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/ExportToStageCommand.h>
#include <MaxUsd/Utilities/MathUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <UsdLayerEditor/layerEditorDCCFunctions.h>
#include <usdUfe/undo/UsdUndoBlock.h>
#include <usdUfe/undo/UsdUndoManager.h>
#include <usdUfe/undo/UsdUndoableItem.h>

#include <pxr/base/js/json.h>
#include <pxr/base/js/value.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdSkel/root.h>
#include <pxr/usd/usdSkel/skeleton.h>

#include <Qt/QmaxMainWindow.h>
#include <ufe/pathString.h>
#include <ufe/undoableCommand.h>
#include <ufe/undoableCommandMgr.h>

#include <AdskUsdComponentCreator/Commands/CreateFromStageCommand.h>
#include <AdskUsdComponentCreator/ComponentAPI.h>
#include <AdskUsdComponentCreator/ComponentFileNames.h>
#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Host.h>
#include <AdskUsdComponentCreator/Infos/DataPayloadInfo.h>
#include <AdskUsdComponentCreator/Infos/PrimTypeInfo.h>
#include <AdskUsdComponentCreator/MoveComponent.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#include <AdskUsdComponentCreator/Utils/VariantsUtils.h>
#include <GetCOREInterface.h>
#include <IPathConfigMgr.h>
#include <QMessageBox>
#include <Tab.h>
#include <algorithm>
#include <filesystem>
#include <hold.h>
#include <inode.h>
#include <max.h>
#include <units.h>
#include <sstream>
#include <unordered_set>

namespace MaxUsd::ComponentCreator {

namespace {
// One 3ds Max tick is 1/4800th of a second.
double SceneFramesPerSecond() { return 4800.0 / double(GetTicksPerFrame()); }
} // namespace

bool IsAdskUsdComponent(const pxr::UsdStageRefPtr& stage)
{
    if (!stage) {
        return false;
    }
    try {
        return AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(stage)
            != nullptr;
    } catch (...) {
        return false;
    }
}

bool IsUnsavedAdskUsdComponent(const pxr::UsdStageRefPtr& stage)
{
    if (!IsAdskUsdComponent(stage)) {
        return false;
    }
    auto root = stage->GetRootLayer();
    if (!root) {
        return false;
    }
    // A component is unsaved if its root layer lives inside the system temp directory.
    // Components are created in AllocateComponentTempFolder() which uses temp_directory_path().
    //
    // Use a string prefix check rather than path-iterator std::mismatch — on Windows,
    // temp_directory_path() returns a path with a trailing backslash (e.g. "...Temp\")
    // which lexically_normal() preserves, causing the iterator comparison to fail.
    namespace fs = std::filesystem;
    std::string tmpStr = fs::temp_directory_path().string();
    std::string rootStr = fs::path(root->GetRealPath()).make_preferred().string();

    // Strip trailing separators so the prefix comparison is clean.
    while (!tmpStr.empty() && (tmpStr.back() == '\\' || tmpStr.back() == '/')) {
        tmpStr.pop_back();
    }

    if (rootStr.size() <= tmpStr.size()) {
        return false;
    }
    // Windows paths are case-insensitive.
    const bool prefixMatches = _strnicmp(rootStr.c_str(), tmpStr.c_str(), tmpStr.size()) == 0;
    const char sep = rootStr[tmpStr.size()];
    return prefixMatches && (sep == '\\' || sep == '/');
}

std::vector<std::string> GetComponentLayersToSave(const pxr::UsdStageRefPtr& stage)
{
    if (!stage || !IsAdskUsdComponent(stage)) {
        return {};
    }
    try {
        return MaxComponentManager::Instance()->GetSaveInfo(stage);
    } catch (const std::exception& e) {
        TF_RUNTIME_ERROR("GetComponentLayersToSave: %s", e.what());
        return {};
    }
}

bool SaveAdskUsdComponent(const pxr::UsdStageRefPtr& stage)
{
    if (!stage) {
        return false;
    }
    try {
        MaxComponentManager::Instance()->SaveComponent(stage);
        return true;
    } catch (const std::exception& e) {
        TF_RUNTIME_ERROR("SaveAdskUsdComponent: %s", e.what());
        return false;
    }
}

bool ReloadAdskUsdComponent(const pxr::UsdStageRefPtr& stage)
{
    if (!stage) {
        return false;
    }
    try {
        MaxComponentManager::Instance()->ReloadComponent(stage);
        return true;
    } catch (const std::exception& e) {
        TF_RUNTIME_ERROR("ReloadAdskUsdComponent: %s", e.what());
        return false;
    }
}

// Build the included-paths set for a flat node export: geometry prims land flat under
// /root/geo, and the individual material prims under /root/mtl (rather than nesting the
// whole material scope as /root/mtl/mtl). When opts.IncludeMaterials() is false, the exported
// material scope is skipped entirely so the component carries no materials.
AdskUsdComponentCreator::PathsSet IncludedPathsForFlatExport(
    const pxr::UsdStageRefPtr&              stage,
    const AdskUsdComponentCreator::Options& opts,
    const MaxUsd::USDSceneBuilderOptions&   exportOptions)
{
    // Derived from the options the export ran with rather than assumed, so the two cannot drift.
    const pxr::SdfPath exportRoot = BuildSourceStageExportOptions(exportOptions).GetRootPrimPath();
    const MaxUsd::USDSceneBuilderOptions defaults;
    const pxr::SdfPath                   materialScope
        = pxr::SdfPath::AbsoluteRootPath().AppendPath(defaults.GetMaterialPrimPath());
    const bool                        includeMaterials = opts.IncludeMaterials();
    AdskUsdComponentCreator::PathsSet paths;
    for (const auto& child : stage->GetPseudoRoot().GetChildren()) {
        if (child.GetPath() == materialScope) {
            if (!includeMaterials) {
                continue; // materials excluded -> drop the whole /mtl scope
            }
            for (const auto& mat : child.GetChildren()) {
                paths.insert(mat.GetPath());
            }
        } else if (!exportRoot.IsAbsoluteRootPath() && child.GetPath() == exportRoot) {
            // Step over the export-only root a skinned export is rooted under, so its contents
            // land flat under /root/geo like any other node rather than a level deeper.
            for (const auto& skinnedChild : child.GetChildren()) {
                paths.insert(skinnedChild.GetPath());
            }
        } else {
            // Include the top-level geometry prim; the creator copies its whole subtree, so a
            // node's child hierarchy (e.g. a teapot parented under a box, exported as an Xform
            // with nested meshes) comes along.
            paths.insert(child.GetPath());
        }
    }
    return paths;
}

MaxUsd::USDSceneBuilderOptions
BuildSourceStageExportOptions(const MaxUsd::USDSceneBuilderOptions& base)
{
    MaxUsd::USDSceneBuilderOptions opts = base;
    opts.SetContentSource(MaxUsd::USDSceneBuilderOptions::ContentSource::NodeList);

    if (opts.GetTranslateSkin()) {
        // MaxUsdSkinMorpherWriter drops a skinned mesh unless VerifyOrMakeSkelRoot can establish
        // a SkelRoot at or above it, and that searches for the rootmost Xform ancestor -- which
        // the flat layout, where the meshes and the bones are sibling root prims, does not have.
        // Keep the root prim the options carry, falling back to the export default if they ask
        // for the pseudo-root. It stays out of the component (see IncludedPathsForFlatExport).
        const pxr::SdfPath rootPrimPath = opts.GetRootPrimPath();
        if (!rootPrimPath.IsAbsolutePath() || rootPrimPath.IsAbsoluteRootPath()) {
            // The pseudo-root carries nothing, and export-to-stage options hold a token standing
            // for the target stage's default prim, which a source stage does not have.
            opts.SetRootPrimPath(MaxUsd::USDSceneBuilderOptions().GetRootPrimPath());
        }
        // The material scope resolves relative to the root prim, which would otherwise pull the
        // materials in with the geometry; anchor them so they stay their own top-level scope.
        const pxr::SdfPath matPath = opts.GetMaterialPrimPath();
        if (!matPath.IsAbsolutePath()) {
            opts.SetMaterialPrimPath(pxr::SdfPath::AbsoluteRootPath().AppendPath(matPath));
        }
    } else {
        opts.SetRootPrimPath(pxr::SdfPath::AbsoluteRootPath());
    }
    return opts;
}

static void ApplySceneStageMetadata(
    const pxr::UsdStageRefPtr&            stage,
    const MaxUsd::USDSceneBuilderOptions& exportOptions)
{
    if (!stage) {
        return;
    }
    pxr::UsdGeomSetStageUpAxis(
        stage,
        exportOptions.GetUpAxis() == MaxUsd::USDSceneBuilderOptions::UpAxis::Y
            ? pxr::UsdGeomTokens->y
            : pxr::UsdGeomTokens->z);
    // Rounded the same way the exporter rounds it, so the two agree bit for bit.
    const double unitScale = MaxUsd::MathUtils::RoundToSignificantDigit(
        GetSystemUnitScale(UNITS_METERS), std::numeric_limits<float>::digits10);
    pxr::UsdGeomSetStageMetersPerUnit(stage, unitScale);

    // Time samples are written at frame numbers, so a stage left on USD's default rate resolves
    // them at the wrong instants and the animation drifts against the scene it came from.
    const MaxUsd::TimeConfig timeConfig = exportOptions.GetResolvedTimeConfig();
    if (timeConfig.IsAnimated()) {
        const double framesPerSecond = SceneFramesPerSecond();
        // Per layer, not just the stage: composition scales a sublayer's time samples by
        // composingLayerTcps / sublayerTcps (see PcpLayerStack::_BuildLayerStack), so a component
        // whose payloads keep the default rate plays its animation stretched by that ratio.
        stage->SetTimeCodesPerSecond(framesPerSecond);
        stage->SetFramesPerSecond(framesPerSecond);
        stage->SetStartTimeCode(timeConfig.GetStartFrame());
        stage->SetEndTimeCode(timeConfig.GetEndFrame());
    }
}

pxr::UsdStageRefPtr ExportINodesToSourceStage(
    const std::vector<INode*>&            nodes,
    const MaxUsd::USDSceneBuilderOptions& exportOptions,
    const Matrix3&                        rootTransform)
{
    auto srcStage = pxr::UsdStage::CreateInMemory();
    if (!srcStage) {
        return nullptr;
    }
    // The exporter only authors stage metadata for a stage it creates itself; given one, it keeps
    // whatever that stage declares and bends the geometry to fit -- which for a fresh in-memory
    // stage means USD's defaults, so the scene would be rescaled into centimetres and rotated to
    // Y-up. Declare the scene's own units and axis instead, so nothing has to be adapted.
    ApplySceneStageMetadata(srcStage, exportOptions);

    // Start from the caller's export options; force the flat NodeList/pseudo-root layout the
    // component creator expects (geometry lands directly under /root/geo/<nodeName>).
    MaxUsd::USDSceneBuilderOptions exportOpts = BuildSourceStageExportOptions(exportOptions);
    // Expand each node with its full subtree; the collector dedups, so a parent+child selection
    // isn't double-counted (node-list export emits only the nodes listed, so descendants must be
    // listed explicitly).
    Tab<INode*> nodeTab;
    for (auto* n : CollectINodesAndDescendants(nodes)) {
        nodeTab.Append(1, &n);
    }
    exportOpts.SetNodesToExport(nodeTab);
    // The source stage is created empty here, so nothing can conflict and the caller's
    // "allow prim overwrite" choice has nothing to apply to; name conflicts against the
    // component's existing content are the creator's business (see isReplacing).
    const auto exportCmd = MaxUsd::ExportToStageCommand::create(
        exportOpts, srcStage, /*allowOverwrite=*/true, rootTransform);
    Ufe::UndoableCommandMgr::instance().executeCmd(exportCmd);
    return srcStage;
}

bool SourceStageHasSkinnedContent(const pxr::UsdStageRefPtr& stage)
{
    if (!stage) {
        return false;
    }
    for (const auto& prim : stage->Traverse()) {
        if (prim.IsA<pxr::UsdSkelSkeleton>()) {
            return true;
        }
    }
    return false;
}

bool ApplySceneMetadataToComponent(
    const pxr::UsdStageRefPtr&            componentStage,
    const MaxUsd::USDSceneBuilderOptions& exportOptions)
{
    if (!componentStage) {
        return false;
    }
    const auto desc
        = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(componentStage);
    if (!desc) {
        return false;
    }
    // Stage metadata is only read from a stage's root layer, which is locked outside of the
    // creator's own edits.
    AdskUsdComponentCreator::UnlockComponentContext unlock(desc);
    pxr::UsdEditContext editContext(componentStage, componentStage->GetRootLayer());
    ApplySceneStageMetadata(componentStage, exportOptions);

    if (!exportOptions.GetResolvedTimeConfig().IsAnimated()) {
        return true;
    }
    // Composition scales a sublayer's time samples by composingLayerTcps / sublayerTcps (see
    // PcpLayerStack::_BuildLayerStack), so every layer of the component has to carry the rate,
    // not only the root. The manager is asked as well as the stage: a variant that is not the
    // selected one contributes no layer to the composition, and would otherwise keep the default
    // rate and play stretched once selected.
    pxr::SdfLayerHandleVector layers = componentStage->GetUsedLayers();
    try {
        const auto held = MaxComponentManager::Instance()->GetDirtyLayers(
            componentStage->GetRootLayer()->GetIdentifier());
        layers.insert(layers.end(), held.begin(), held.end());
    } catch (const std::exception& e) {
        TF_RUNTIME_ERROR("ApplySceneMetadataToComponent: %s", e.what());
    }
    const double framesPerSecond = SceneFramesPerSecond();
    for (const auto& layer : layers) {
        if (!layer || layer->IsAnonymous()) {
            continue;
        }
        const bool wasEditable = layer->PermissionToEdit();
        if (!wasEditable) {
            layer->SetPermissionToEdit(true);
        }
        layer->SetTimeCodesPerSecond(framesPerSecond);
        if (!wasEditable) {
            layer->SetPermissionToEdit(false);
        }
    }
    return true;
}

bool MakeComponentPrimSkelRoot(const pxr::UsdStageRefPtr& componentStage)
{
    if (!componentStage) {
        return false;
    }
    const auto desc
        = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(componentStage);
    if (!desc) {
        return false;
    }
    const pxr::UsdPrim componentPrim = componentStage->GetPrimAtPath(desc->GetRootPrimPath());
    if (!componentPrim) {
        return false;
    }
    // The component's root layer holds the component prim, and is locked outside of the creator's
    // own edits.
    AdskUsdComponentCreator::UnlockComponentContext unlock(desc);
    pxr::UsdEditContext editContext(componentStage, componentStage->GetRootLayer());
    return static_cast<bool>(pxr::UsdSkelRoot::Define(componentStage, componentPrim.GetPath()));
}

// Clone the component's creation options and retarget them at the given variant, for the
// add-to-existing-component paths. Adding must never change which variant is the default.
static AdskUsdComponentCreator::Options cloneOptionsForVariant(
    const AdskUsdComponentCreator::ComponentDescription::Ptr& componentDesc,
    const AdskUsdComponentCreator::VariantSelections&         variantSelections,
    bool                                                      isReplacing)
{
    auto                             baseOptions = componentDesc->GetOptions();
    AdskUsdComponentCreator::Options options
        = baseOptions ? baseOptions->Clone() : AdskUsdComponentCreator::Options {};
    options.SetComponentVariants(variantSelections);
    options.SetReplaceVariantContent(isReplacing);
    options.SetIsDefaultVariant(false);
    return options;
}

// Generate the purpose scope name from the template options: custom names (Options'
// purpose-scope-names map) override the default, which is the lowercased purpose.
static std::string
getPurposeScopeName(const AdskUsdComponentCreator::Options& options, const std::string& purpose)
{
    if (purpose.empty()) {
        return {};
    }
    std::string lower = purpose;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    const auto& names = options.GetPurposeScopeNames();
    const auto  it = names.find(lower);
    return it != names.end() ? it->second : lower;
}

// Bake the purpose into the creation options so the creator places the input prims under a
// purpose scope with the imageable purpose set: remap each input prim path to sit under /<scope>,
// set the imageable purpose token, and add the scope to the included paths.
static void updateOptionsForPurpose(
    AdskUsdComponentCreator::Options& options,
    const std::string&                purpose,
    const std::vector<pxr::SdfPath>&  primPaths)
{
    if (purpose.empty()) {
        return;
    }

    const std::string  scopeName = getPurposeScopeName(options, purpose);
    const pxr::SdfPath scopePath
        = pxr::SdfPath::AbsoluteRootPath().AppendChild(pxr::TfToken(scopeName));

    // Imageable purpose token; unknown values fall back to proxy.
    std::string lower = purpose;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    pxr::TfToken purposeToken = pxr::UsdGeomTokens->proxy;
    if (lower == "render") {
        purposeToken = pxr::UsdGeomTokens->render;
    } else if (lower == "guide") {
        purposeToken = pxr::UsdGeomTokens->guide;
    }
    options.SetImageablePurpose(purposeToken.GetString());

    // Map every input prim path under the purpose scope path.
    AdskUsdComponentCreator::PathsMap mapped;
    for (const auto& p : primPaths) {
        mapped[p] = p.ReplacePrefix(pxr::SdfPath::AbsoluteRootPath(), scopePath);
    }
    options.SetMappedPaths(mapped);

    // Include the purpose scope path.
    AdskUsdComponentCreator::PathsSet included = options.IncludedPaths();
    included.insert(scopePath);
    options.SetIncludedPaths(included);
}

// Ensure the purpose-scope prim (placed by the mapped-paths options) is a typed USD Scope in a
// variant payload. The mapped-paths placement can leave it as an untyped over, so re-define it as
// a Scope. Captured as an undoable edit on the payload layer.
class AddPurposeScopeCommand : public Ufe::UndoableCommand
{
public:
    AddPurposeScopeCommand(pxr::UsdStageRefPtr payloadStage, pxr::SdfPath scopePath)
        : _stage(std::move(payloadStage))
        , _scopePath(std::move(scopePath))
    {
    }

    void execute() override
    {
        UsdUfe::UsdUndoManager::instance().trackLayerStates(_stage->GetEditTarget().GetLayer());
        UsdUfe::UsdUndoBlock block(&_undoItem);
        if (!pxr::UsdGeomScope::Define(_stage, _scopePath)) {
            TF_RUNTIME_ERROR(
                "AddPurposeScopeCommand: failed to define scope %s", _scopePath.GetText());
        }
    }

    void undo() override { _undoItem.undo(); }
    void redo() override { _undoItem.redo(); }

private:
    pxr::UsdStageRefPtr     _stage;
    pxr::SdfPath            _scopePath;
    UsdUfe::UsdUndoableItem _undoItem;
};

// After an add whose options placed geometry under a purpose scope, ensure that scope prim is a
// typed USD Scope in each variant payload (mirrors maya's _create_purpose_scope). The mapped-paths
// placement can leave the scope prim as an untyped over. Best-effort: failures are logged, not
// propagated (the add already succeeded).
static void createPurposeScope(
    const std::shared_ptr<AdskUsdComponentCreator::CreateFromStageCommand>& createCmd,
    const std::string&                                                      purpose)
{
    if (purpose.empty() || !createCmd) {
        return;
    }
    const auto creatorInfo = createCmd->GetOutputs().creatorInfo;
    if (!creatorInfo) {
        return;
    }
    const auto options = creatorInfo->GetOptions();
    if (!options) {
        return;
    }
    const std::string scopeName = getPurposeScopeName(*options, purpose);
    if (scopeName.empty()) {
        return;
    }

    // Type the scope in each variant payload the creator produced.
    for (const auto& primTypeInfo : creatorInfo->GetPrimTypeInfos()) {
        if (!primTypeInfo) {
            continue;
        }
        const auto payloadInfo = primTypeInfo->GetPayloadInfo();
        if (!payloadInfo) {
            continue;
        }
        const pxr::UsdStageRefPtr payloadStage = payloadInfo->GetPayloadStage();
        if (!payloadStage) {
            continue;
        }
        const pxr::UsdPrim defaultPrim = payloadStage->GetDefaultPrim();
        if (!defaultPrim) {
            continue;
        }
        const pxr::SdfPath scopePath = defaultPrim.GetPath().AppendChild(pxr::TfToken(scopeName));
        if (payloadStage->GetPrimAtPath(scopePath)) {
            Ufe::UndoableCommandMgr::instance().executeCmd(
                std::make_shared<AddPurposeScopeCommand>(payloadStage, scopePath));
        }
    }
}

bool AddINodesToComponent(
    const AdskUsdComponentCreator::ComponentDescription::Ptr& componentDesc,
    const std::vector<INode*>&                                nodes,
    const AdskUsdComponentCreator::VariantSelections&         variantSelections,
    bool                                                      isReplacing,
    const MaxUsd::USDSceneBuilderOptions&                     exportOptions,
    const std::string&                                        purpose,
    const Matrix3&                                            rootTransform)
{
    if (!componentDesc || nodes.empty()) {
        return false;
    }

    AdskUsdComponentCreator::Options options
        = cloneOptionsForVariant(componentDesc, variantSelections, isReplacing);

    // Group the whole operation (the add plus any purpose-scope typing) into one undo entry.
    // Guard against an outer hold (e.g. the "Duplicate to Component" quad menu) so we never
    // double-commit.
    const bool ownHold = !theHold.Holding();
    if (ownHold) {
        theHold.Begin();
    }

    // The creator copies the source stage's up axis onto the component, so exporting in any other
    // axis both reorients the added geometry and rewrites the component's root layer. Defer to
    // the component's own axis, the way ExportToStageCommand defers to a target stage's.
    MaxUsd::USDSceneBuilderOptions addOptions = exportOptions;
    if (const auto componentStage
        = AdskUsdComponentCreator::ComponentDescription::GetStageFromComponentDescription(
            componentDesc)) {
        addOptions.SetUpAxis(
            pxr::UsdGeomGetStageUpAxis(componentStage) == pxr::UsdGeomTokens->y
                ? MaxUsd::USDSceneBuilderOptions::UpAxis::Y
                : MaxUsd::USDSceneBuilderOptions::UpAxis::Z);
    }

    // Export once, bake the purpose (if any) into the creation options so the creator places the
    // imported prims under the purpose scope, add, then ensure that scope prim is a typed Scope.
    // An empty purpose leaves the options and post-step as no-ops, i.e. a plain add.
    bool ok = false;
    auto srcStage = ExportINodesToSourceStage(nodes, addOptions, rootTransform);
    if (!srcStage) {
        TF_RUNTIME_ERROR("AddINodesToComponent: failed to export nodes");
    } else {
        AdskUsdComponentCreator::UnlockComponentContext unlock(componentDesc);
        options.SetIncludedPaths(IncludedPathsForFlatExport(srcStage, options, addOptions));
        // The purpose remap targets the exported top-level prims (pseudo-root children).
        std::vector<pxr::SdfPath> importedPaths;
        for (const auto& child : srcStage->GetPseudoRoot().GetChildren()) {
            importedPaths.push_back(child.GetPath());
        }
        updateOptionsForPurpose(options, purpose, importedPaths);

        auto createCmd = AdskUsdComponentCreator::CreateFromStageCommand::Create(
            componentDesc, srcStage, options);
        if (createCmd && AdskUsdComponentCreator::TheHost::GetHost()->ExecuteWithUndo(createCmd)) {
            createPurposeScope(createCmd, purpose);
            ok = true;
        }
    }

    if (ownHold) {
        if (ok) {
            theHold.Accept(_T("Add Selection to Component"));
        } else {
            theHold.Cancel();
        }
    }
    return ok;
}

bool AddPrimsToComponent(
    const AdskUsdComponentCreator::ComponentDescription::Ptr& componentDesc,
    const std::vector<UsdPrimSource>&                         primSources,
    const AdskUsdComponentCreator::VariantSelections&         variantSelections,
    bool                                                      isReplacing,
    const std::string&                                        purpose)
{
    if (!componentDesc || primSources.empty()) {
        return false;
    }

    AdskUsdComponentCreator::Options options
        = cloneOptionsForVariant(componentDesc, variantSelections, isReplacing);

    // Group the whole operation (the adds plus any purpose-scope typing) into one undo entry.
    // Guard against an outer hold so we never double-commit.
    const bool ownHold = !theHold.Holding();
    if (ownHold) {
        theHold.Begin();
    }

    bool                                                             ok = false;
    std::shared_ptr<AdskUsdComponentCreator::CreateFromStageCommand> lastCreateCmd;
    {
        AdskUsdComponentCreator::UnlockComponentContext unlock(componentDesc);
        bool                                            first = true;
        for (const auto& src : primSources) {
            if (!src.stage || src.primPaths.empty()) {
                continue;
            }
            // Keep only top-level prims so a selected prim and its descendant aren't added twice.
            const std::vector<pxr::SdfPath> topLevel = RemoveDescendantPaths(src.primPaths);
            options.SetIncludedPaths(
                AdskUsdComponentCreator::PathsSet(topLevel.begin(), topLevel.end()));
            // The first source honors isReplacing; subsequent sources add so that
            // multiple source stages accumulate into the same variant.
            if (!first) {
                options.SetReplaceVariantContent(false);
            }
            // Bake the purpose (if any) into a per-source copy so the selected prims land under the
            // purpose scope; an empty purpose leaves it a plain add.
            AdskUsdComponentCreator::Options creationOpts = options;
            updateOptionsForPurpose(creationOpts, purpose, topLevel);

            auto createCmd = AdskUsdComponentCreator::CreateFromStageCommand::Create(
                componentDesc, src.stage, creationOpts);
            if (!createCmd
                || !AdskUsdComponentCreator::TheHost::GetHost()->ExecuteWithUndo(createCmd)) {
                if (ownHold) {
                    theHold.Cancel();
                }
                return false;
            }
            lastCreateCmd = createCmd;
            ok = true;
            first = false;
        }
        // Ensure the purpose scope prim is a typed Scope in the produced variant payloads.
        createPurposeScope(lastCreateCmd, purpose);
    }

    if (ownHold) {
        if (ok) {
            theHold.Accept(_T("Add Selection to Component"));
        } else {
            theHold.Cancel();
        }
    }
    return ok;
}

bool DuplicateINodesToComponent(
    const AdskUsdComponentCreator::ComponentDescription::Ptr& componentDesc,
    const std::vector<INode*>&                                nodes,
    const MaxUsd::USDSceneBuilderOptions&                     exportOptions,
    const Matrix3&                                            rootTransform)
{
    if (!componentDesc || nodes.empty()) {
        return false;
    }

    // Duplicate-to-component only applies when a variant is targeted for edition.
    // The duplicated geometry is appended (not replacing) to the targeted variant.
    const auto target = AdskUsdComponentCreator::ComponentAPI::GetVariantTarget(componentDesc);
    if (target.empty()) {
        return false;
    }

    const auto variantSelections = AdskUsdComponentCreator::GetVariantSelectionsFromNesting(target);
    return AddINodesToComponent(
        componentDesc,
        nodes,
        variantSelections,
        /*isReplacing=*/false,
        exportOptions,
        /*purpose=*/std::string(),
        rootTransform);
}

pxr::UsdPrim
FindPrimWithVariantSets(const pxr::UsdStageRefPtr& stage, const pxr::SdfPath& primPath)
{
    if (!stage || primPath.IsEmpty()) {
        return {};
    }
    for (pxr::UsdPrim prim = stage->GetPrimAtPath(primPath); prim && prim.IsValid();
         prim = prim.GetParent()) {
        if (prim.HasVariantSets() && !prim.GetVariantSets().GetNames().empty()) {
            return prim;
        }
    }
    return {};
}

std::filesystem::path AllocateComponentTempFolder()
{
    const auto tmp = std::filesystem::temp_directory_path();

    AdskUsdComponentCreator::Options nameOptions;
    nameOptions.SetComponentFolder(tmp.string());

    for (;;) {
        nameOptions = AdskUsdComponentCreator::GenerateUniqueComponentName(nameOptions);
        const auto      folder = tmp / nameOptions.ComponentName();
        std::error_code ec;
        if (std::filesystem::create_directories(folder, ec)) {
            // Creating the folder is what marks the name taken : the generator treats an existing
            // component folder as a used name.
            return folder;
        }
        if (ec) {
            throw std::runtime_error(
                "AllocateComponentTempFolder: failed to create " + folder.string() + ": "
                + ec.message());
        }
        // No error means it appeared via a race; ask for the next name.
    }
}

void RemoveComponentTempFolder(const std::filesystem::path& folder)
{
    if (!folder.empty() && std::filesystem::exists(folder)) {
        // Best-effort: never throw from temp-folder cleanup. The component's layer
        // files may still be open in the USD layer registry (e.g. while undoing a
        // create), which locks them on Windows; the OS reclaims %TEMP% later.
        std::error_code ec;
        std::filesystem::remove_all(folder, ec);
        if (ec) {
            TF_WARN(
                "RemoveComponentTempFolder: could not remove %s: %s",
                folder.string().c_str(),
                ec.message().c_str());
        }
    }
}

namespace {
void CollectNodeAndDescendants(
    INode*                      node,
    std::vector<INode*>&        out,
    std::unordered_set<INode*>& seen)
{
    if (!node || !seen.insert(node).second) {
        return;
    }
    out.push_back(node);
    for (int i = 0; i < node->NumberOfChildren(); ++i) {
        CollectNodeAndDescendants(node->GetChildNode(i), out, seen);
    }
}
} // namespace

std::vector<INode*> CollectINodesAndDescendants(const std::vector<INode*>& nodes)
{
    std::vector<INode*>        out;
    std::unordered_set<INode*> seen;
    for (INode* n : nodes) {
        CollectNodeAndDescendants(n, out, seen);
    }
    return out;
}

void RepointStageRootLayer(
    const std::string& oldRootLayerFilename,
    const std::string& newRootLayerFilename)
{
    namespace fs = std::filesystem;
    // USD returns forward-slash paths on Windows; make_preferred() converts to backslashes
    // for consistent case-insensitive comparison.
    const std::string oldNorm = fs::path(oldRootLayerFilename).make_preferred().string();

    for (auto* stageObj : StageObjectMap::GetInstance()->GetAllStageObjects()) {
        if (!stageObj) {
            continue;
        }
        const pxr::UsdStageRefPtr stage = stageObj->GetUSDStage();
        if (!stage) {
            continue;
        }
        auto rootLayer = stage->GetRootLayer();
        if (!rootLayer) {
            continue;
        }
        const std::string realPath = fs::path(rootLayer->GetRealPath()).make_preferred().string();
        if (_stricmp(realPath.c_str(), oldNorm.c_str()) != 0) {
            continue;
        }
        const auto newPath = MaxUsd::UsdStringToMaxString(
            fs::path(newRootLayerFilename).make_preferred().string());
        stageObj->SetRootLayer(newPath.data(), L"/", /*payloadsLoaded=*/true);

        // Transfer over the session layer, which carries the variant target information.
        if (const pxr::UsdStageRefPtr newStage = stageObj->GetUSDStage()) {
            newStage->GetSessionLayer()->TransferContent(stage->GetSessionLayer());
        }

        // The stage object now backs a new stage at the relocated path. If the Variant Editor was
        // displaying the old stage, repoint it to the new one (no-op if it wasn't showing it).
        MaxVariantEditor::Instance()->RepointStage(stage, stageObj->GetUSDStage());

        // The old stage's root layer is gone; any undo record logged before this repoint (e.g. an
        // edit made before saving) may still hold a weak pointer to it, which would crash on
        // undo.
        GetCOREInterface()->FlushUndoBuffer();
        return;
    }
}

std::string MoveComponentForSave(
    const pxr::UsdStageRefPtr& componentStage,
    const std::string&         destFolder,
    const std::string&         name)
{
    if (!componentStage) {
        return {};
    }
    auto desc
        = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(componentStage);
    if (!desc) {
        return {};
    }

    const std::string oldRootLayer = componentStage->GetRootLayer()->GetRealPath();

    SaveAdskUsdComponent(componentStage);

    // MoveComponent requires the root layer unlocked (AddComponent locks it). The caller's
    // overwrite guard guarantees an empty target, so a conflict here is a genuine failure.
    AdskUsdComponentCreator::UnlockComponentContext unlock(desc);
    auto                                            moved
        = AdskUsdComponentCreator::MoveComponent(desc, destFolder, name, /*allowOverwrite=*/false);
    if (!moved.first) {
        return {};
    }

    namespace fs = std::filesystem;
    const std::string origExt = fs::path(oldRootLayer).extension().string();
    const std::string ext = origExt.empty() ? "usda" : origExt.substr(1);
    const std::string newRootLayer
        = (fs::path(destFolder) / name / (name + "." + ext)).make_preferred().string();

    RepointStageRootLayer(oldRootLayer, newRootLayer);

    // The component at the old location no longer exists -- its files were just moved. Untrack it
    // so the manager releases the strong references it holds to that component's layers.
    try {
        MaxComponentManager::Instance()->RemoveComponent(componentStage);
    } catch (const std::exception& e) {
        TF_WARN("MoveComponentForSave: could not untrack the pre-move component: %s", e.what());
    }

    return newRootLayer;
}

namespace {

// Resolve a layer-editor dccObjectPath (the UFE stage-path string) back to its stage.
pxr::UsdStageRefPtr StageFromDccObjectPath(const std::string& dccObjectPath)
{
    if (dccObjectPath.empty()) {
        return nullptr;
    }
    const pxr::UsdStageWeakPtr weak = MaxUsd::ufe::getStage(Ufe::PathString::path(dccObjectPath));
    if (!weak) {
        return nullptr;
    }
    auto* stageObj = StageObjectMap::GetInstance()->Get(weak);
    return stageObj ? stageObj->GetUSDStage() : nullptr;
}

// A component file-structure preview (nested dict = folder, bool leaf = file; true = conflict)
// as a JsValue tree.
pxr::JsValue componentPreviewToJs(const pxr::VtDictionary& dict)
{
    pxr::JsObject obj;
    for (const auto& [key, value] : dict) {
        if (value.IsHolding<pxr::VtDictionary>()) {
            obj[key] = componentPreviewToJs(value.UncheckedGet<pxr::VtDictionary>());
        } else {
            obj[key] = pxr::JsValue(value.IsHolding<bool>() && value.UncheckedGet<bool>());
        }
    }
    return pxr::JsValue(std::move(obj));
}

// Serialize the preview to the JSON string the layer editor's ComponentSaveWidget expects.
std::string componentPreviewToJson(const pxr::VtDictionary& dict)
{
    std::ostringstream out;
    pxr::JsWriteToStream(componentPreviewToJs(dict), out);
    return out.str();
}

} // namespace

void RegisterComponentHooks()
{
    // Mirror the layer editor's ComponentFns contract (dccObjectPath-keyed; JSON preview) so the
    // maxUsd registration is a drop-in when the editor ships as a consumed binary.
    UsdLayerEditor::ComponentFns fns;
    fns.saveComponent = [](const pxr::UsdStageRefPtr& stage, const std::string& /*dccObjectPath*/) {
        SaveAdskUsdComponent(stage);
    };
    fns.reloadComponent = [](const std::string& dccObjectPath) {
        const auto s = StageFromDccObjectPath(dccObjectPath);
        // Never-saved (temp-resident) components have nothing on disk to reload from.
        if (IsAdskUsdComponent(s) && !IsUnsavedAdskUsdComponent(s)) {
            ReloadAdskUsdComponent(s);
        }
    };
    fns.isStageAComponent = [](const std::string& dccObjectPath) {
        return IsAdskUsdComponent(StageFromDccObjectPath(dccObjectPath));
    };
    fns.isUnsavedComponent
        = [](const pxr::UsdStageRefPtr& stage) { return IsUnsavedAdskUsdComponent(stage); };
    fns.shouldDisplayComponentInitialSaveDialog
        = [](const pxr::UsdStageRefPtr& stage, const std::string& /*dccObjectPath*/) {
              return IsUnsavedAdskUsdComponent(stage);
          };
    fns.moveComponent = [](const std::string& saveLocation,
                           const std::string& componentName,
                           const std::string& dccObjectPath) -> std::string {
        const auto s = StageFromDccObjectPath(dccObjectPath);
        return s ? MoveComponentForSave(s, saveLocation, componentName) : std::string();
    };
    fns.previewComponentSave = [](const std::string& saveLocation,
                                  const std::string& componentName,
                                  const std::string& dccObjectPath) -> std::string {
        const auto s = StageFromDccObjectPath(dccObjectPath);
        if (!s || saveLocation.empty() || componentName.empty()) {
            return {};
        }
        auto desc = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(s);
        if (!desc) {
            return {};
        }
        try {
            const pxr::VtDictionary tree = AdskUsdComponentCreator::PreviewMoveComponentHierarchy(
                desc, saveLocation, componentName);
            return componentPreviewToJson(tree);
        } catch (...) {
            return {};
        }
    };
    fns.getComponentLayersToSave = [](const std::string& dccObjectPath) {
        return GetComponentLayersToSave(StageFromDccObjectPath(dccObjectPath));
    };
    UsdLayerEditor::setComponentFns(fns);

    // The save dialog's default name and location are not part of the ComponentFns contract.
    UsdLayerEditor::ComponentDefaultsFns defaults;
    defaults.defaultName = [](const std::string& dccObjectPath) -> std::string {
        const auto s = StageFromDccObjectPath(dccObjectPath);
        if (s) {
            auto d = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(s);
            if (d && !d->GetAssetName().empty()) {
                return d->GetAssetName();
            }
        }
        return "Component";
    };
    defaults.defaultLocation = [](const std::string& /*dccObjectPath*/) -> std::string {
        const MSTR dir = IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_SCENE_DIR);
        return MaxUsd::MaxStringToUsdString(dir);
    };
    UsdLayerEditor::setComponentDefaultsFns(defaults);
}

} // namespace MaxUsd::ComponentCreator
