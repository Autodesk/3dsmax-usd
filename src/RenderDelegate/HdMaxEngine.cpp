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
#include "HdMaxEngine.h"

#include "HdMaxChangeTracker.h"
#include "HdMaxConsolidator.h"
#include "Imaging/HdMaxRenderDelegate.h"

#include <MaxUsd/Utilities/MeshUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>

#include <pxr/base/gf/frustum.h>
#include <pxr/base/trace/trace.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hdx/renderTask.h>

#include <Graphics/CustomRenderItemHandle.h>
#include <Graphics/IVirtualDevice.h>
#include <maxscript/mxsplugin/mxsCustomAttributes.h>

#include <Rendering/IRenderMessageManager.h>
#include <mesh.h>
#include <stdmat.h>

HdMaxEngine::HdMaxEngine()
{
    renderDelegate = std::make_shared<pxr::HdMaxRenderDelegate>();
    consolidator = std::make_unique<HdMaxConsolidator>(renderDelegate);
}

HdMaxEngine::~HdMaxEngine()
{
    // Make sure the renderIndex is destroyed first, as it depends on other members
    // (renderDelegate/sceneDelegate)
    renderIndex.release();
}

void HdMaxEngine::HydraRender(
    const pxr::GfMatrix4d&    rootTransform,
    const pxr::UsdTimeCode&   timeCode,
    const pxr::TfTokenVector& renderTags,
    bool                      loadAllUnmappedPrimvars)
{
    TRACE_FUNCTION();

    // When rendering to render meshes, load all mapped primvars (even if not used by a USDPreview
    // surface material, perhaps a material override might use it).
    renderDelegate->SetRenderSetting(
        pxr::TfToken("loadAllMappedPrimvars"), pxr::VtValue(loadAllUnmappedPrimvars));

    this->sceneDelegate->SetRootTransform(rootTransform);

    // Perform the actual rendering.
    PrepareBatch(timeCode, renderTags);
    RenderBatch();

    // Deal with any render data that needs to be deleted for this mesh (for example,
    // if the topology has changed). We keep these around until now to make sure render
    // items get ref counted to 0 while on the main thread, it can cause issues otherwise.
    this->renderDelegate->GarbageCollect();
}

void HdMaxEngine::UpdateMaterialIdsList(
    const std::vector<HdMaxRenderData*>&     renderData,
    std::shared_ptr<HdMaxMaterialCollection> collection)
{
    // Keep track of the materials that have been converted. Generate an ID for each.
    materials.clear();
    auto nextMaterialId = 0;

    // Update material.
    for (const auto& primRenderData : renderData) {
        for (const HdMaxRenderData::SubsetRenderData& subGeom : primRenderData->shadedSubsets) {
            // Fallback to the displayColor for rendering, if no material is defined.
            auto renderMaterialKey
                = subGeom.materialData ? subGeom.materialData->GetId() : pxr::SdfPath {};
            auto renderMaterial = subGeom.materialData ? subGeom.materialData->GetMaxMaterial()
                                                       : collection->GetDisplayColorMaterial();
            const auto inserted
                = materials.insert({ renderMaterialKey, { nextMaterialId, renderMaterial } });
            if (inserted.second) {
                nextMaterialId++;
            }
        }
    }
}

void HdMaxEngine::UpdateMultiMaterial(MultiMtl* multiMat) const
{
    if (!multiMat) {
        return;
    }

    multiMat->SetNumSubMtls(int(materials.size()));
    for (const auto& matRef : materials) {
        auto material = matRef.second.second->GetAs<Mtl>();
        multiMat->SetSubMtlAndName(matRef.second.first, material, material->GetName());
    }
}

HdMaxConsolidator::OutputPtr HdMaxEngine::Consolidate(
    const std::vector<HdMaxRenderData*>&        renderData,
    const pxr::UsdTimeCode&                     lastTimeCode,
    const pxr::UsdTimeCode&                     timeCode,
    const HdMaxConsolidator::Config&            config,
    const MaxSDK::Graphics::BaseMaterialHandle& wireMaterial)
{
    HdMaxConsolidator::OutputPtr consolidation = nullptr;
    auto&                        currentConsolidationConfig = consolidator->GetConfig();

    // If the consolidation configuration has changed, we need to restart the consolidation from
    // scratch.
    if (!(currentConsolidationConfig == config)) {
        consolidator->Reset();
        consolidator->SetConfig(config);
    } else {
        // Attempt to update the consolidation...
        consolidator->UpdateConsolidation(renderData, lastTimeCode, timeCode);
    }

    // Figure out if we are in a static or dynamic context for the purpose of consolidation.
    // Static -> animation is stopped, the render timeCode is stable for more than "staticDelay"
    // miliseconds. Dynamic -> animating (either "play" or scrubbing the timeline)
    bool isStatic = false;
    if (config.staticDelay == 0) {
        isStatic = true;
    } else {
        if (lastTimeCode == timeCode) {
            if (!staticDelayStarted) {
                staticDelayStarted = true;
                staticDelayStartTime = std::chrono::system_clock::now();
            } else {
                const auto now = std::chrono::system_clock::now();
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - staticDelayStartTime);
                if (elapsed.count() >= config.staticDelay) {
                    isStatic = true;
                }
            }
        } else {
            staticDelayStarted = false;
        }
    }

    // Unless we are in "dynamic" mode, only consolidate if the time is stopped.
    if (config.strategy != HdMaxConsolidator::Strategy::Off
        && (isStatic || config.strategy == HdMaxConsolidator::Strategy::Dynamic)) {
        const auto existingConsolidation = consolidator->GetConsolidation(timeCode);
        // Check if the consolidation we already have was built from the same prims. If new prims
        // are added we need to build new consolidated geometries for those.
        auto isSameSourceData = [&existingConsolidation, &renderData]() {
            // existingConsolidation->sourceRenderData contains an entry for each subset.
            auto it = existingConsolidation->sourceRenderData.begin();
            for (const auto& rd : renderData) {
                for (int i = 0; i < rd->shadedSubsets.size(); ++i) {
                    if (it->primPath != rd->rPrimPath) {
                        return false;
                    }
                    ++it;
                }
            }
            return true;
        };

        if (existingConsolidation && isSameSourceData()) {
            consolidation = existingConsolidation;
        } else {
            consolidation = consolidator->BuildConsolidation(renderData, timeCode, wireMaterial);
        }
    }
    // However, if we have a still valid consolidation, we can use it.
    else if (config.strategy == HdMaxConsolidator::Strategy::Static && !isStatic) {
        consolidation = consolidator->GetConsolidation(timeCode);
    }

    // If we have a consolidation, update the dirty bits of whatever was consolidated.
    if (consolidation) {
        for (const auto& renderDataInfo : consolidation->consolidatedRenderData) {
            // SafeGetRenderData() allows us to pass the index of the render data held by the
            // delegate AND the prim path. The index almost never changes (it can only change when
            // things are removed, i.e. when a prim is deactivated) This allows us to avoid a map
            // lookup cost, in most cases. If the given id doesn't match the path, we fallback to
            // using the path to find the render data.
            auto& primRenderData
                = renderDelegate->SafeGetRenderData(renderDataInfo.index, renderDataInfo.primPath);
            if (primRenderData.rPrimPath.IsEmpty()
                || renderDataInfo.subsetIdx >= primRenderData.shadedSubsets.size()) {
                continue;
            }
            // Dirtiness is maintained for each shaded subset (i.e. subsets of the mesh requiring
            // different shading).
            HdMaxChangeTracker::ClearDirtyBits(
                primRenderData.shadedSubsets[renderDataInfo.subsetIdx].dirtyBits);
        }
    }
    return consolidation;
}

void HdMaxEngine::Render(
    const pxr::UsdPrim&                           rootPrim,
    const pxr::GfMatrix4d&                        rootTransform,
    MaxSDK::Graphics::IRenderItemContainer&       targetRenderItemContainer,
    const pxr::UsdTimeCode&                       timeCode,
    const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
    MaxSDK::Graphics::UpdateNodeContext&          nodeContext,
    const pxr::TfTokenVector&                     reprs,
    const pxr::TfTokenVector&                     renderTags,
    MultiMtl*                                     multiMat,
    const HdMaxConsolidator::Config&              consolidationConfig,
    ViewExp*                                      view,
    bool                                          buildOfflineRenderMaterial,
    const MaxUsd::ProgressReporter&               progressReporter)
{
    const auto maxNode = nodeContext.GetRenderNode().GetMaxNode();
    Mtl*       nodeMtl = maxNode ? maxNode->GetMtl() : nullptr;

    UpdateRootPrim(rootPrim, nodeMtl);
    HydraRender(rootTransform, timeCode, renderTags);

    std::vector<HdMaxRenderData*> renderData;
    renderDelegate->GetVisibleRenderData(renderTags, renderData);

    if (renderData.empty()) {
        return;
    }

    auto& displaySettings = renderDelegate->GetDisplaySettings();

    // Deal with the materials...
    const auto materialCollection = renderDelegate->GetMaterialCollection();
    // First, if we are requested to build the material for offline rendering, or if we are
    // displaying in the VP using UsdPreviewSurface, build the materials (convert the hydra material
    // networks to a 3dsMax materials).
    const auto displayIsUsdPreviewSurface
        = displaySettings.GetDisplayMode() == HdMaxDisplaySettings::USDPreviewSurface;
    if (buildOfflineRenderMaterial || displayIsUsdPreviewSurface) {
        // Build updated materials.
        materialCollection->BuildMaterials(
            progressReporter,
            GetRenderDelegate()->GetPrimvarMappingOptions(),
            displayIsUsdPreviewSurface,
            buildOfflineRenderMaterial);
    }

    // If we need to build the offline rendering material, also update the MultiMaterial we use for
    // that purpose, setting the right amount of sub-materials and assigning our materials in each
    // slot.
    if (buildOfflineRenderMaterial) {
        // Update the list of all material currently in use, and generate associated material Ids.
        // We need to associate material Ids to each used materials in use in case we need to render
        // the stage eventually. We need to do this regardless of whether the user has assigned the
        // UsdPreviewSurface materials to the node we always want usd meshes bound to different
        // materials, to be using different material ids.
        UpdateMaterialIdsList(renderData, materialCollection);
        UpdateMultiMaterial(multiMat);
    }

    const bool needShadedRepr
        = std::find(reprs.begin(), reprs.end(), pxr::HdReprTokens->smoothHull) != reprs.end();
    const bool needWireRepr
        = std::find(reprs.begin(), reprs.end(), pxr::HdReprTokens->wire) != reprs.end();

    // Attempt consolidation...
    // On the first render, assume no time change.
    const auto lastTime = this->lastVpRenderTime.IsDefault() ? timeCode : this->lastVpRenderTime;
    this->lastVpRenderTime = timeCode;
    const HdMaxConsolidator::OutputPtr consolidation = Consolidate(
        renderData,
        lastTime,
        timeCode,
        consolidationConfig,
        nodeContext.GetRenderNode().GetWireframeMaterial());
    if (consolidation) {
        for (const auto& consolidatedGeom : *consolidation->geoms) {
            if (needShadedRepr) {
                targetRenderItemContainer.AddRenderItem(consolidatedGeom->GetRenderItem(false));
            }
            if (needWireRepr) {
                auto& wireItem = consolidatedGeom->GetRenderItem(true);
                wireItem.SetCustomMaterial(nodeContext.GetRenderNode().GetWireframeMaterial());
                targetRenderItemContainer.AddRenderItem(wireItem);
            }
        }
    }

    // Finally add the render items for prims that were not consolidated.
    // Avoid looking at every prim if we can. If we have as many consolidated prim subsets as there
    // are subsets total, we can bail early.
    const int totalSubsets
        = std::accumulate(renderData.begin(), renderData.end(), 0, [](int total, const auto& data) {
              return total + static_cast<int>(data->shadedSubsets.size());
          });
    if (consolidation && totalSubsets == consolidation->primToGeom.size()) {
        return;
    }

    pxr::GfMatrix4d viewProjMatrixUsd;
    if (view) {
        MaxSDK::Graphics::Matrix44 viewProjectionMatrix;
        Matrix3                    viewMatrixInv;
        int                        perspective;
        float                      hither, yon;
        view->getGW()->getCameraMatrix(
            viewProjectionMatrix.m, &viewMatrixInv, &perspective, &hither, &yon);
        viewProjMatrixUsd = MaxUsd::ToUsd(viewProjectionMatrix);
    }

    const auto              objectTM = maxNode
                     ? MaxUsd::ToUsd(maxNode->GetObjectTM(updateDisplayContext.GetDisplayTime()))
                     : pxr::GfMatrix4d {}.SetIdentity();
    std::unordered_set<int> consolidatedSubsets;
    for (const auto& primRenderData : renderData) {
        // Are all the material subsets already part of some still valid consolidated mesh? If so,
        // we can skip this entire primRenderData..
        consolidatedSubsets.clear();
        for (int i = 0; i < primRenderData->shadedSubsets.size(); ++i) {
            if (primRenderData->shadedSubsets[i].inConsolidation) {
                consolidatedSubsets.insert(i);
            }
        }

        if (consolidatedSubsets.size() == primRenderData->shadedSubsets.size()) {
            continue;
        }

        // If a view information was given, perform frustum culling.
        if (view) {
            auto boundingBox = primRenderData->boundingBox;
            boundingBox.Transform(objectTM);
            auto worldSpaceBox = boundingBox.ComputeAlignedBox();
            if (!pxr::GfFrustum::IntersectsViewVolume(worldSpaceBox, viewProjMatrixUsd)) {
                continue;
            }
        }

        // Load the index and vertex buffers into the render item.
        primRenderData->UpdateRenderGeometry(false);

        // No geometry loaded -> nothing to do. Only points and normals are absolutely required.
        if (primRenderData->points.empty() || primRenderData->normals.empty()) {
            continue;
        }

        // Shaded render items (one for each UsdGeomSubset) :
        if (needShadedRepr) {
            for (int i = 0; i < primRenderData->shadedSubsets.size(); ++i) {
                // Is this shaded subset already consolidated?
                if (consolidatedSubsets.find(i) != consolidatedSubsets.end()) {
                    continue;
                }

                // Figure out the material we need to use in the viewport.
                HdMaxRenderData::SubsetRenderData& subsetGeometry
                    = primRenderData->shadedSubsets[i];
                const bool instanced = primRenderData->shadedSubsets[i].IsInstanced();
                MaxSDK::Graphics::BaseMaterialHandle materialToUse
                    = primRenderData->ResolveViewportMaterial(
                        subsetGeometry, displaySettings, instanced);
                // Basic geometry. In this case, we already created the renderItem.
                if (!instanced) {
                    auto& renderItem
                        = subsetGeometry.GetRenderItemDecorator(primRenderData->selected);
                    renderItem.SetOffsetMatrix(MaxUsd::ToMax(primRenderData->transform));
                    renderItem.SetCustomMaterial(materialToUse);
                    targetRenderItemContainer.AddRenderItem(renderItem);
                }
                // Instanced geometry. For instances, we only created the instance render
                // geometry. We only generate the render items now, as we need to display context
                // and the node context.
                else {
                    primRenderData->instancer->GenerateInstances(
                        subsetGeometry.geometry.get(),
                        &materialToUse,
                        targetRenderItemContainer,
                        updateDisplayContext,
                        nodeContext,
                        false,
                        i,
                        view);
                }
            }
            primRenderData->instancer->SetClean(false);
        }

        // Wireframe render item (only need one for the whole mesh, even if subsets exist).
        if (needWireRepr) {
            // Basic geometry
            if (!primRenderData->wireframe.IsInstanced()) {
                auto& wireRenderItem
                    = primRenderData->wireframe.GetRenderItemDecorator(primRenderData->selected);
                wireRenderItem.SetOffsetMatrix(MaxUsd::ToMax(primRenderData->transform));
                wireRenderItem.SetCustomMaterial(
                    nodeContext.GetRenderNode().GetWireframeMaterial());
                targetRenderItemContainer.AddRenderItem(wireRenderItem);
            }
            // Instanced geometry
            else {
                primRenderData->instancer->GenerateInstances(
                    primRenderData->wireframe.geometry.get(),
                    nullptr,
                    targetRenderItemContainer,
                    updateDisplayContext,
                    nodeContext,
                    true,
                    0,
                    view);
                primRenderData->instancer->SetClean(true);
            }
        }
    }
}

void HdMaxEngine::SetSelection(
    const std::unordered_map<pxr::SdfPath, pxr::VtIntArray, pxr::SdfPath::Hash>& newSelection) const
{
    if (!sceneDelegate || !renderDelegate) {
        return;
    }

    auto selection = renderDelegate->GetSelection();
    auto selectedPaths = selection->GetAllSelectedPrimPaths();

    // Can we know for sure the selection hasn't changed?
    const auto checkSelectionChanged = [&newSelection, &selection, &selectedPaths]() {
        // Check new items in selection.
        for (const auto& sel : newSelection) {
            if (!selection->GetPrimSelectionState(
                    pxr::HdSelection::HighlightModeSelect, sel.first)) {
                return true;
            }
        }

        for (const auto& selPath : selectedPaths) {
            // Something removed from selection?
            const auto it = newSelection.find(selPath);
            if (it == newSelection.end()) {
                return true;
            }

            // Not the same instances selected?
            auto state
                = selection->GetPrimSelectionState(pxr::HdSelection::HighlightModeSelect, selPath);
            size_t curIdx = 0;
            for (const auto& indexArray : state->instanceIndices) {
                for (const auto index : indexArray) {
                    if (it->second[curIdx++] != index) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    if (!checkSelectionChanged()) {
        return;
    }

    // If we had a selection previously, we need to flag those rprims dirty for selection,
    // as they may not be selected any more.
    pxr::HdChangeTracker& changeTracker = sceneDelegate->GetRenderIndex().GetChangeTracker();
    const auto&           renderIndex = sceneDelegate->GetRenderIndex();
    for (const auto& path : selectedPaths) {
        // Only care for rPrims, other prims have no selection display in vp (materials, skelroot,
        // etc.)
        if (!renderIndex.GetRprim(path)) {
            continue;
        }
        if (selection->GetPrimSelectionState(pxr::HdSelection::HighlightModeSelect, path)) {
            changeTracker.MarkRprimDirty(path, pxr::HdMaxMesh::DirtySelectionHighlight);
        }
    }

    // Rebuild the new selection..
    selection.reset(new pxr::HdSelection);
    for (const auto& sel : newSelection) {
        // Point instances.
        if (!sel.second.empty()) {
            for (const auto& instanceIdx : sel.second) {
                sceneDelegate->PopulateSelection(
                    pxr::HdSelection::HighlightModeSelect, sel.first, instanceIdx, selection);
            }
        }
        // Prims.
        else {
            sceneDelegate->PopulateSelection(
                pxr::HdSelection::HighlightModeSelect, sel.first, -1, selection);
        }
    }
    renderDelegate->SetSelection(selection);

    // Mark all selected paths dirty...
    for (const auto& path : selection->GetAllSelectedPrimPaths()) {
        // Only care for rPrims, other prims have no selection display in vp (materials, skelroot,
        // etc.)
        if (!renderIndex.GetRprim(path)) {
            continue;
        }
        changeTracker.MarkRprimDirty(path, pxr::HdMaxMesh::DirtySelectionHighlight);
    }
}

void HdMaxEngine::RenderToMeshes(
    INode*                              node,
    const pxr::UsdPrim&                 rootPrim,
    const pxr::GfMatrix4d&              rootTransform,
    std::vector<std::shared_ptr<Mesh>>& outputMeshes,
    std::vector<Matrix3>&               meshTransforms,
    const pxr::UsdTimeCode&             timeCode,
    const pxr::TfTokenVector&           renderTags)
{
    // Raise a warning if no material is applied to the UsdStage object. The user may need to
    // explicitly apply the UsdPreviewSurface materials.
    if (!node->GetMtl()) {
        IRenderMessageManager* pRenderMessageManager = GetRenderMessageManager();
        const std::wstring     warningNoMtl
            = std::wstring(L"Warning : No material applied to ") + node->GetName()
            + std::wstring(
                  L". If you want to use the UsdPreviewSurface materials from the USD Stage, use "
                  L"the \"Assign "
                  L"UsdPreviewSurface material\" command from the \"Rendering settings\" rollup.");

        pRenderMessageManager->LogMessage(
            IRenderMessageManager::kSource_ProductionRenderer,
            IRenderMessageManager::kType_Warning,
            0,
            warningNoMtl.c_str());
    }

    outputMeshes.clear();
    meshTransforms.clear();

    UpdateRootPrim(rootPrim, node->GetMtl());
    HydraRender(rootTransform, timeCode, renderTags, true);

    std::vector<HdMaxRenderData*> renderData;
    renderDelegate->GetVisibleRenderData(renderTags, renderData);

    // Then, update a lists of all materials currently in use, and generate associated material Ids.
    const auto materialCollection = renderDelegate->GetMaterialCollection();
    UpdateMaterialIdsList(renderData, materialCollection);

    pxr::TfHashSet<pxr::TfToken, pxr::TfToken::HashFunctor> unmappedPrimvars;
    for (const auto& primRenderData : renderData) {
        // Create a 3dsMax mesh for this USD prim's data.
        auto primMesh = std::make_shared<Mesh>();

        MaxUsd::MeshUtils::UsdRenderGeometry usdRenderGeom;
        // VtArray have copy-on-write semantics, so these assignments are not copying the data.
        usdRenderGeom.points = primRenderData->points;
        usdRenderGeom.uvs = primRenderData->uvs;
        usdRenderGeom.normals = primRenderData->normals;
        usdRenderGeom.colors = primRenderData->colors;

        // We are converting the geometry used in the viewport, to geometry usable for rendering.
        // If any mapped data (uvs, normals, colors, etc.) cannot be shared, then we also needed to
        // "unshare" the vertices themselves to satisfy nitrous (vertex buffers must all be the same
        // size). When rendering, this is not great because the meshes dont appear "welded" and for
        // some materials this is an issue (think displacement for example). So for vertices we must
        // make sure to share again the vertices that were shared in the source geometry. For mapped
        // data (primvar data) it does not matter.
        usdRenderGeom.subsetTopoIndices.resize(primRenderData->shadedSubsets.size());
        usdRenderGeom.subsetPrimvarIndices.resize(primRenderData->shadedSubsets.size());

        // Did we "unshare" the vertices to satisfy Nitrous?
        const bool unsharedPoints
            = usdRenderGeom.points.size() != primRenderData->sourceTopology.GetNumPoints();

        if (unsharedPoints) {
            // If the points were unshared for Nitrous, we need to make sure to share them again.
            // We don't keep the original vertex buffers around, to avoid using more memory, but we
            // can simply use the index of one of the "unshared points". For example, the corner of
            // a welded box is one vertex in the source data. For nitrous, because the normals and
            // uvs of each face of that box can't be shared at that vertex, we split the corner into
            // 3 vertices (A, B, C). Here, what we do is remap the indices pointing to B and C to A.
            // So that all indices for that corner of the box point to A. This, obviously, will
            // result in some unused vertices - these will be cleaned out as part of the conversion
            // to the 3dsMax mesh.

            // When we unshare vertices, the new indices are a natural sequence [0-N], so we can
            // easily map render indices to scene indices by just looking at the scene indices.
            const auto& renderIdxToSceneIdx = primRenderData->sourceTopology.GetFaceVertexIndices();

            // Find the reverse mapping. scene -> render is a 1 - N mapping. For the reverse we just
            // use the first index we find that "works".
            std::vector<int> sceneIdxToRenderIdx;
            constexpr int    uninitializedValue = -1;
            sceneIdxToRenderIdx.resize(
                primRenderData->sourceTopology.GetNumPoints(), uninitializedValue);

            for (size_t i = 0; i < renderIdxToSceneIdx.size(); ++i) {
                const int sceneFaceVtxId = renderIdxToSceneIdx[i];
                // Use the first vertex we find.
                if (sceneIdxToRenderIdx[sceneFaceVtxId] == uninitializedValue) {
                    sceneIdxToRenderIdx[sceneFaceVtxId] = static_cast<int>(i);
                }
            }

            for (int i = 0; i < primRenderData->shadedSubsets.size(); ++i) {
                // Remap the topology indices.
                auto& subset = primRenderData->shadedSubsets[i];
                for (int j = 0; j < subset.indices.size(); ++j) {
                    auto sceneIndex1 = renderIdxToSceneIdx[subset.indices[j][0]];
                    auto sceneIndex2 = renderIdxToSceneIdx[subset.indices[j][1]];
                    auto sceneIndex3 = renderIdxToSceneIdx[subset.indices[j][2]];

                    usdRenderGeom.subsetTopoIndices[i].emplace_back(
                        sceneIdxToRenderIdx[sceneIndex1],
                        sceneIdxToRenderIdx[sceneIndex2],
                        sceneIdxToRenderIdx[sceneIndex3]);
                }
                // Mapped data indices are kept as is.
                usdRenderGeom.subsetPrimvarIndices[i] = subset.indices;
            }
        } else {
            // Otherwise, we can use the indices as is for points & data.
            for (int i = 0; i < primRenderData->shadedSubsets.size(); ++i) {
                auto& subset = primRenderData->shadedSubsets[i];
                usdRenderGeom.subsetTopoIndices[i] = subset.indices;
                usdRenderGeom.subsetPrimvarIndices[i] = subset.indices;
            }
        }

        // Figure out the material ids associated with each subset.
        const auto materialCollection = renderDelegate->GetMaterialCollection();
        for (const auto& subsetGeometry : primRenderData->shadedSubsets) {
            int  matId = 0;
            auto renderMaterial = subsetGeometry.materialData ? subsetGeometry.materialData->GetId()
                                                              : pxr::SdfPath {};
            auto it = materials.find(renderMaterial);
            if (it != materials.end()) {
                matId = it->second.first;
            }
            usdRenderGeom.materialIds.push_back(matId);
        }

        if (!ToRenderMesh(
                usdRenderGeom,
                *primMesh,
                GetRenderDelegate()->GetPrimvarMappingOptions(),
                unmappedPrimvars)) {
            GetCOREInterface()->Log()->LogEntry(
                SYSLOG_ERROR,
                NO_DIALOG,
                NULL,
                _T("Failed to convert %s's geometry for rendering."),
                primRenderData->rPrimPath.GetString());
        }

        // If instanced, use all the instance transforms, otherwise, just the transform defined on
        // the prim.
        auto transforms = primRenderData->instancer->GetTransforms();
        if (transforms.empty()) {
            Matrix3 primTransform = MaxUsd::ToMaxMatrix3(primRenderData->transform);
            transforms.push_back(primTransform);
        }

        for (auto& transform : transforms) {
            outputMeshes.push_back(primMesh);
            meshTransforms.push_back(transform);
        }
    }

    // If we have unmapped primvars (primvars that are in use by some material, but not mapped to
    // any 3dsMax channel), the render result may not be as we would expect. Warn the user.
    if (!unmappedPrimvars.empty()) {
        IRenderMessageManager* pRenderMessageManager = GetRenderMessageManager();
        std::wstring warningUnmapped = std::wstring(L"Warning : The Usd Stage ") + node->GetName()
            + std::wstring(L", contains materials using primvars that are not mapped to any 3dsMax "
                           L"map channel : ");

        bool first = true;
        for (const auto& pv : unmappedPrimvars) {
            if (!first) {
                warningUnmapped.append(L", ");
            }
            warningUnmapped.append(
                std::wstring(MaxUsd::UsdStringToMaxString(pv.GetString()).data()));
            first = false;
        }
        warningUnmapped.append(
            L". Primvar/channel mappings can be set using the maxscript function "
            L"\"SetPrimvarChannelMapping(...) available on the Stage object.\"");
        pRenderMessageManager->LogMessage(
            IRenderMessageManager::kSource_ProductionRenderer,
            IRenderMessageManager::kType_Warning,
            0,
            warningUnmapped.c_str());
    }
}

bool HdMaxEngine::UpdateRootPrim(const pxr::UsdPrim& rootPrim, Mtl* nodeMaterial)
{
    if (this->rootPrim == rootPrim) {
        return false;
    }

    // On first initialization or whenever we are changing to an entirely new stage, setup
    // a new render index (and associated task controller). Also clear anything cached in our
    // render delegate - indeed that data is no longer important.
    if ((!this->rootPrim.IsValid() || !rootPrim.IsValid())
        || (this->rootPrim.GetStage() != rootPrim.GetStage())) {
        renderDelegate->Clear();

        // Release in order considering dependencies.
        renderIndex.release();
        sceneDelegate.release();

        renderIndex.reset(pxr::HdRenderIndex::New(renderDelegate.get(), {}));
        taskController = std::make_unique<pxr::HdMaxTaskController>(
            renderIndex.get(),
            pxr::SdfPath::AbsoluteRootPath().AppendChild(pxr::TfToken("hydraMaxTaskController")));
    }

    // If the root primitive to render from changes, we need to create a new scene delegate for it.
    this->sceneDelegate = std::make_unique<pxr::UsdImagingDelegate>(
        renderIndex.get(), pxr::SdfPath::AbsoluteRootPath());
    sceneDelegate->Populate(rootPrim, {});

    this->rootPrim = rootPrim;

    if (nodeMaterial) {
        // If the Root prim has changed, initialize the material collection - needed if we are
        // loading the USD Stage from a .max file - to avoid rebuilding new materials. Instead
        // this reconnects the materials to the USD source...
        InitializeMaterialCollection(rootPrim.GetStage(), nodeMaterial);
    }

    return true;
}

size_t HdMaxEngine::GetNumRenderPrim(const pxr::TfTokenVector& renderTags) const
{
    // The render primitives are initialized when the scene delegate is created, depending on the
    // situation, they may not be filled up with actual geometry when we get here, but the count is
    // already correct.
    size_t numRenderPrim = 0;

    std::vector<HdMaxRenderData*> renderData;
    renderDelegate->GetVisibleRenderData(renderTags, renderData);
    for (const auto& data : renderData) {
        if (!data->visible || !data->renderTagActive) {
            continue;
        }

        numRenderPrim += std::max(size_t(1), data->instancer->GetNumInstances());
    }
    return numRenderPrim;
}

void HdMaxEngine::PrepareBatch(
    const pxr::UsdTimeCode&   timeCode,
    const pxr::TfTokenVector& renderTags)
{
    TRACE_FUNCTION();
    if (taskController->GetRenderTags() != renderTags) {
        consolidator->Reset();
    }
    taskController->SetRenderTags(renderTags);
    sceneDelegate->SetTime(timeCode);
    sceneDelegate->ApplyPendingUpdates();
}

void HdMaxEngine::InitializeMaterialCollection(pxr::UsdStageWeakPtr stage, Mtl* material)
{
    // Look at the currently set material on the node, if it is a material we built, expect a
    // MultiMtl* carrying MaxUsdPreviewSurfaceMaterials.
    const auto multiMtl = dynamic_cast<MultiMtl*>(material);
    if (!multiMtl) {
        // Nothing to do - the material is not one that we built.
        return;
    }

    const auto materialCollection = GetRenderDelegate()->GetMaterialCollection();

    // Go through the sub-materials, looking for 3dsMax UsdPreviewSurface materials that we might be
    // able to connect to the Stage materials.
    for (int i = 0; i < multiMtl->NumSubMtls(); ++i) {
        const auto subMaterial = multiMtl->GetSubMtl(i);
        if (!subMaterial
            || subMaterial->ClassID()
                != HdMaxMaterialCollection::MaxUsdPreviewSurfaceMaterialClassID) {
            continue;
        }

        // When we build materials for the stage, we assign the source material prim path as name.
        // Can we find a material prim on the current stage using this path?
        const auto materialPrimPath
            = pxr::SdfPath(MaxUsd::MaxStringToUsdString(subMaterial->GetName()));
        auto materialPrim = stage->GetPrimAtPath(materialPrimPath);
        if (!materialPrim.IsValid() || !materialPrim.IsA<pxr::UsdShadeMaterial>()) {
            continue;
        }

        // Looks good... we register the existing material in the collection. This material will now
        // be updated if its USD source material changes.
        materialCollection->RegisterMaxMaterial(materialPrimPath, subMaterial);

        // Also look at any bitmaps this material is using, use the same process to match bitmaps
        // with source USD texture nodes. If we find a match, add the bitmap to the bitmap cache we
        // maintain.
        for (int i = 0; i < subMaterial->NumSubTexmaps(); ++i) {
            const auto texMap = subMaterial->GetSubTexmap(i);
            const auto bitmapTex = dynamic_cast<BitmapTex*>(texMap);

            if (!bitmapTex) {
                continue;
            }

            // Is this from a texture prim in the stage?
            auto texturePrimPath = pxr::SdfPath(MaxUsd::MaxStringToUsdString(bitmapTex->GetName()));
            auto texturePrim = stage->GetPrimAtPath(texturePrimPath);

            if (!texturePrim.IsValid() || !texturePrim.IsA<pxr::UsdShadeShader>()) {
                continue;
            }
            materialCollection->RegisterMaxBitmap(bitmapTex);
        }
    }
}

pxr::HdChangeTracker& HdMaxEngine::GetChangeTracker()
{
    if (!renderIndex) {
        static pxr::HdChangeTracker emptyTracker;
        return emptyTracker;
    }
    return renderIndex->GetChangeTracker();
}

void HdMaxEngine::RenderBatch()
{
    TRACE_FUNCTION();

    pxr::VtValue renderTags({});
    engine.SetTaskContextData(pxr::HdTokens->renderTags, renderTags);

    const pxr::HdReprSelector reprSelector = pxr::HdReprSelector(pxr::HdReprTokens->smoothHull);
    const pxr::TfToken        collectionName = pxr::HdTokens->geometry;

    renderCollection = pxr::HdRprimCollection(collectionName, reprSelector);
    renderCollection.SetRootPath(pxr::SdfPath::AbsoluteRootPath());

    taskController->SetCollection(renderCollection);
    auto renderingTasks = taskController->GetRenderingTasks();

    engine.Execute(renderIndex.get(), &renderingTasks);
}