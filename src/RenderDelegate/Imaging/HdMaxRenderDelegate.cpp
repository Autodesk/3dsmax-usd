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
#include "HdMaxRenderDelegate.h"

#include "HdMaxBasisCurves.h"
#include "HdMaxExtComputation.h"
#include "HdMaxInstancer.h"
#include "HdMaxMaterial.h"
#include "HdMaxMesh.h"
#include "HdMaxRenderPass.h"
#include "MaxUsd/MaxTokens.h"

#include <pxr/imaging/hd/extComputation.h>

using namespace MaxSDK::Graphics;

PXR_NAMESPACE_OPEN_SCOPE

const TfTokenVector HdMaxRenderDelegate::SUPPORTED_RPRIM_TYPES
    = { HdPrimTypeTokens->mesh, HdPrimTypeTokens->basisCurves };

const TfTokenVector HdMaxRenderDelegate::SUPPORTED_SPRIM_TYPES
    = { HdPrimTypeTokens->material,
        HdPrimTypeTokens->extComputation,
        pxr::HdPrimTypeTokens->light,
        pxr::HdPrimTypeTokens->cylinderLight,
        pxr::HdPrimTypeTokens->rectLight,
        pxr::HdPrimTypeTokens->distantLight,
        pxr::HdPrimTypeTokens->sphereLight,
        pxr::HdPrimTypeTokens->diskLight,
        pxr::HdPrimTypeTokens->domeLight,
        pxr::HdPrimTypeTokens->pluginLight,
        pxr::HdPrimTypeTokens->simpleLight
#if PXR_VERSION >= 2311
        ,
        pxr::HdPrimTypeTokens->meshLight
#endif
      };

const TfTokenVector HdMaxRenderDelegate::SUPPORTED_BPRIM_TYPES = {};

const TfTokenVector& HdMaxRenderDelegate::GetSupportedRprimTypes() const
{
    return SUPPORTED_RPRIM_TYPES;
}

const TfTokenVector& HdMaxRenderDelegate::GetSupportedSprimTypes() const
{
    return SUPPORTED_SPRIM_TYPES;
}

const TfTokenVector& HdMaxRenderDelegate::GetSupportedBprimTypes() const
{
    return SUPPORTED_BPRIM_TYPES;
}

HdMaxRenderDelegate::HdMaxRenderDelegate(HdRenderSettingsMap const& settings)
    : HdRenderDelegate(settings)
{
    materialCollection = std::make_shared<HdMaxMaterialCollection>();
    activeSelection.reset(new pxr::HdSelection);
}

HdMaxRenderDelegate::~HdMaxRenderDelegate() { }

HdMaxMeshRenderData& HdMaxRenderDelegate::GetMeshRenderData(size_t id)
{
    return meshRenderDataVector[id];
}

HdMaxMeshRenderData& HdMaxRenderDelegate::GetMeshRenderData(const pxr::SdfPath& primpath)
{
    return meshRenderDataVector[meshRenderDataIndexMap[primpath]];
}

HdMaxMeshRenderData&
HdMaxRenderDelegate::SafeGetMeshRenderData(size_t index, const pxr::SdfPath& primPath)
{
    static HdMaxMeshRenderData invalid { {} };

    auto findByKey = [this](const pxr::SdfPath& primPath) -> HdMaxMeshRenderData& {
        const auto it = meshRenderDataIndexMap.find(primPath);
        if (it == meshRenderDataIndexMap.end()) {
            return invalid;
        }
        return meshRenderDataVector[it->second];
    };

    if (index >= meshRenderDataVector.size()) {
        return findByKey(primPath);
    }

    // If the prim path of the data we retrieved is not the one we expect (index has changed since),
    // use the prim path instead.
    auto& renderData = meshRenderDataVector[index];
    if (renderData.rPrimPath == primPath) {
        return renderData;
    }
    return findByKey(primPath);
}

size_t HdMaxRenderDelegate::GetMeshRenderDataIndex(const pxr::SdfPath& path) const
{
    return meshRenderDataIndexMap.at(path);
}

const std::unordered_map<pxr::SdfPath, size_t, pxr::SdfPath::Hash>&
HdMaxRenderDelegate::GetMeshRenderDataIdMap() const
{
    return meshRenderDataIndexMap;
}

std::vector<HdMaxMeshRenderData>& HdMaxRenderDelegate::GetAllMeshRenderData()
{
    return meshRenderDataVector;
}

void HdMaxRenderDelegate::GetMeshRenderData(
    std::vector<HdMaxMeshRenderData*>& data,
    bool                               includeInvisible,
    bool                               includeGeomObjectSource)
{
    for (auto& primRenderData : meshRenderDataVector) {

        // Skip prims with inactive render tags.
        if (!primRenderData.renderTagActive) {
            continue;
        }

        // Skip invisible prims unless they were explicitly requested.
        if (!includeInvisible && !primRenderData.visible) {
            continue;
        }

        // Check if we should include prims used as USdGeomObject sources.
        // I.e. prims that were "promoted" to 3dsMax.
        if (!includeGeomObjectSource) {
            if (primRenderData.renderTag == MaxUsdPurposeTokens->geomObjectSource) {
                continue;
            }
        }

        // If using instancing, make sure we have at least one instance visible.
        if (primRenderData.shadedSubsets.empty()
            || (primRenderData.shadedSubsets[0].IsInstanced()
                && primRenderData.instancer->GetNumInstances() == 0)) {
            continue;
        }

        data.push_back(&primRenderData);
    }
}

HdMaxBasisCurvesRenderData& HdMaxRenderDelegate::GetBasisCurvesRenderData(size_t id)
{
    return basisCurvesRenderDataVector[id];
}

HdMaxBasisCurvesRenderData&
HdMaxRenderDelegate::GetBasisCurvesRenderData(const pxr::SdfPath& primpath)
{
    return basisCurvesRenderDataVector[basisCurvesRenderDataIndexMap[primpath]];
}

HdMaxBasisCurvesRenderData&
HdMaxRenderDelegate::SafeGetBasisCurvesRenderData(size_t index, const pxr::SdfPath& primPath)
{
    static HdMaxBasisCurvesRenderData invalid { {} };

    auto findByKey = [this](const pxr::SdfPath& primPath) -> HdMaxBasisCurvesRenderData& {
        const auto it = basisCurvesRenderDataIndexMap.find(primPath);
        if (it == basisCurvesRenderDataIndexMap.end()) {
            return invalid;
        }
        return basisCurvesRenderDataVector[it->second];
    };

    if (index >= basisCurvesRenderDataVector.size()) {
        return findByKey(primPath);
    }

    // If the prim path of the data we retrieved is not the one we expect (index has changed since),
    // use the prim path instead.
    auto& renderData = basisCurvesRenderDataVector[index];
    if (renderData.rPrimPath == primPath) {
        return renderData;
    }
    return findByKey(primPath);
}

size_t HdMaxRenderDelegate::GetBasisCurvesRenderDataIndex(const pxr::SdfPath& path) const
{
    return basisCurvesRenderDataIndexMap.at(path);
}

const std::unordered_map<pxr::SdfPath, size_t, pxr::SdfPath::Hash>&
HdMaxRenderDelegate::GetBasisCurvesRenderDataIdMap() const
{
    return basisCurvesRenderDataIndexMap;
}

std::vector<HdMaxBasisCurvesRenderData>& HdMaxRenderDelegate::GetAllBasisCurvesRenderData()
{
    return basisCurvesRenderDataVector;
}

void HdMaxRenderDelegate::GetBasisCurvesRenderData(
    std::vector<HdMaxBasisCurvesRenderData*>& data,
    bool                                      includeInvisible)
{
    for (auto& primRenderData : basisCurvesRenderDataVector) {

        // Skip prims with inactive render tags.
        if (!primRenderData.renderTagActive) {
            continue;
        }

        // Skip invisible prims unless they were explicitly requested.
        if (!includeInvisible && !primRenderData.visible) {
            continue;
        }

        // If using instancing, make sure we have at least one instance visible.
        if (primRenderData.shadedCurve.wireIndices.empty()
            || (primRenderData.shadedCurve.IsInstanced()
                && primRenderData.instancer->GetNumInstances() == 0)) {
            continue;
        }

        data.push_back(&primRenderData);
    }
}

HdRenderParam* HdMaxRenderDelegate::GetRenderParam() const { return nullptr; }

HdResourceRegistrySharedPtr HdMaxRenderDelegate::GetResourceRegistry() const { return nullptr; }

HdRenderPassSharedPtr
HdMaxRenderDelegate::CreateRenderPass(HdRenderIndex* index, HdRprimCollection const& collection)
{
    return HdRenderPassSharedPtr(new HdMaxRenderPass(index, collection));
}

HdInstancer* HdMaxRenderDelegate::CreateInstancer(HdSceneDelegate* delegate, SdfPath const& id)
{
    return new HdMaxInstancer(delegate, id);
}

void HdMaxRenderDelegate::DestroyInstancer(HdInstancer* instancer) { }

HdRprim* HdMaxRenderDelegate::CreateRprim(TfToken const& typeId, SdfPath const& rPrimId)
{
    if (typeId == HdPrimTypeTokens->mesh) {
        meshRenderDataVector.emplace_back(rPrimId);

        size_t renderDataIdx = meshRenderDataVector.size() - 1;
        meshRenderDataIndexMap.insert({ rPrimId, renderDataIdx });

        // We need to keep a reference to the hydra meshes we create, so that they can be deleted
        // properly.
        auto       mesh = std::make_unique<HdMaxMesh>(this, rPrimId, renderDataIdx);
        const auto meshPtr = mesh.get();
        meshes.insert({ rPrimId, (std::move(mesh)) });
        return meshPtr;
    }
    if (typeId == HdPrimTypeTokens->basisCurves) {
        basisCurvesRenderDataVector.emplace_back(rPrimId);

        size_t renderDataIdx = basisCurvesRenderDataVector.size() - 1;
        basisCurvesRenderDataIndexMap.insert({ rPrimId, renderDataIdx });

        auto       mesh = std::make_unique<HdMaxBasisCurves>(this, rPrimId, renderDataIdx);
        const auto meshPtr = mesh.get();
        basiscurves.insert({ rPrimId, (std::move(mesh)) });
        return meshPtr;
    }
    return nullptr;
}

void HdMaxRenderDelegate::DestroyRprim(HdRprim* rPrim)
{
    const auto& path = rPrim->GetId();

    auto meshPrimIdx = meshRenderDataIndexMap.find(path);
    auto basiscurvesPrimIdx = basisCurvesRenderDataIndexMap.find(path);
    if (meshPrimIdx == meshRenderDataIndexMap.end()
        && basiscurvesPrimIdx == basisCurvesRenderDataIndexMap.end()) {
        return;
    }

    if (basiscurvesPrimIdx == basisCurvesRenderDataIndexMap.end()) { // non-basiscurves case
        // Update our data structures. For the vector, we want to avoid shifting
        // everything after the index, so to remove the prim's render data, we move
        // the last item in the vector in its place, and just pop the now emptied
        // last item.
        if (meshPrimIdx->second != meshRenderDataVector.size() - 1) {
            auto& availableSlot = meshRenderDataVector[meshPrimIdx->second];
            availableSlot = std::move(meshRenderDataVector.back());
            // Make sure to update the index in the map.
            meshRenderDataIndexMap[availableSlot.rPrimPath] = meshPrimIdx->second;
        }
        meshRenderDataVector.pop_back();
        meshRenderDataIndexMap.erase(path);

        if (meshes.find(rPrim->GetId()) != meshes.end()) {
            meshes.erase(rPrim->GetId());
        }
    } else { // basiscurves case
        if (basiscurvesPrimIdx->second != basisCurvesRenderDataVector.size() - 1) {
            auto& availableSlot = basisCurvesRenderDataVector[basiscurvesPrimIdx->second];
            availableSlot = std::move(basisCurvesRenderDataVector.back());
            basisCurvesRenderDataIndexMap[availableSlot.rPrimPath] = basiscurvesPrimIdx->second;
        }
        basisCurvesRenderDataVector.pop_back();
        basisCurvesRenderDataIndexMap.erase(path);

        if (basiscurves.find(rPrim->GetId()) != basiscurves.end()) {
            basiscurves.erase(rPrim->GetId());
        }
    }
}

HdSprim* HdMaxRenderDelegate::CreateSprim(TfToken const& typeId, SdfPath const& sprimId)
{
    if (typeId == HdPrimTypeTokens->material) {
        return new HdMaxMaterial(this, sprimId);
    }
    if (typeId == HdPrimTypeTokens->extComputation) {
        return new HdMaxExtComputation(sprimId);
    }
    return nullptr;
}

HdSprim* HdMaxRenderDelegate::CreateFallbackSprim(TfToken const& typeId)
{
    if (typeId == HdPrimTypeTokens->extComputation) {
        return new HdMaxExtComputation(SdfPath::EmptyPath());
    }
    return nullptr;
}

void HdMaxRenderDelegate::DestroySprim(HdSprim* sprim) { delete sprim; }

HdBprim* HdMaxRenderDelegate::CreateBprim(TfToken const& typeId, SdfPath const& bprimId)
{
    return nullptr;
}

HdBprim* HdMaxRenderDelegate::CreateFallbackBprim(TfToken const& typeId) { return nullptr; }

void HdMaxRenderDelegate::DestroyBprim(HdBprim* bprim) { }

void HdMaxRenderDelegate::CommitResources(HdChangeTracker* tracker) { }

void HdMaxRenderDelegate::Clear()
{
    meshRenderDataIndexMap.clear();
    meshRenderDataVector.clear();
    basisCurvesRenderDataIndexMap.clear();
    basisCurvesRenderDataVector.clear();
    meshes.clear();
    basiscurves.clear();
    materialCollection = std::make_shared<HdMaxMaterialCollection>();
}

void HdMaxRenderDelegate::GarbageCollect()
{
    if (!mustGc) {
        return;
    }
    for (auto& data : GetAllMeshRenderData()) {
        data.toDelete.clear();
    }
    mustGc = false;
}

HdMaxDisplaySettings& HdMaxRenderDelegate::GetDisplaySettings() { return displaySettings; }

void HdMaxRenderDelegate::SetSelection(const pxr::HdSelectionSharedPtr& selection)
{
    activeSelection = selection;
}

const pxr::HdSelectionSharedPtr& HdMaxRenderDelegate::GetSelection() const
{
    return activeSelection;
}

const pxr::HdSelection::PrimSelectionState*
HdMaxRenderDelegate::GetSelectionStatus(const pxr::SdfPath& path) const
{
    if (!activeSelection) {
        return nullptr;
    }
    return activeSelection->GetPrimSelectionState(
        pxr::HdSelection::HighlightMode::HighlightModeSelect, path);
}

MaxUsd::PrimvarMappingOptions& HdMaxRenderDelegate::GetPrimvarMappingOptions()
{
    return primvarMappingOptions;
}

std::shared_ptr<HdMaxMaterialCollection> HdMaxRenderDelegate::GetMaterialCollection()
{
    return materialCollection;
}

PXR_NAMESPACE_CLOSE_SCOPE
