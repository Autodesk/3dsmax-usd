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

#include "HdMaxExtComputation.h"
#include "HdMaxInstancer.h"
#include "HdMaxMaterial.h"
#include "HdMaxMesh.h"
#include "HdMaxRenderPass.h"

#include <pxr/imaging/hd/extComputation.h>

using namespace MaxSDK::Graphics;

PXR_NAMESPACE_OPEN_SCOPE

const TfTokenVector HdMaxRenderDelegate::SUPPORTED_RPRIM_TYPES = {
    HdPrimTypeTokens->mesh,
};

const TfTokenVector HdMaxRenderDelegate::SUPPORTED_SPRIM_TYPES
    = { HdPrimTypeTokens->material, HdPrimTypeTokens->extComputation };

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

HdMaxRenderData& HdMaxRenderDelegate::GetRenderData(size_t id) { return renderDataVector[id]; }

HdMaxRenderData& HdMaxRenderDelegate::GetRenderData(const pxr::SdfPath& primpath)
{
    return renderDataVector[renderDataIndexMap[primpath]];
}

HdMaxRenderData& HdMaxRenderDelegate::SafeGetRenderData(size_t index, const pxr::SdfPath& primPath)
{
    static HdMaxRenderData invalid { {} };

    auto findByKey = [this](const pxr::SdfPath& primPath) -> HdMaxRenderData& {
        const auto it = renderDataIndexMap.find(primPath);
        if (it == renderDataIndexMap.end()) {
            return invalid;
        }
        return renderDataVector[it->second];
    };

    if (index >= renderDataVector.size()) {
        return findByKey(primPath);
    }

    // If the prim path of the data we retrieved is not the one we expect (index has changed since),
    // use the prim path instead.
    auto& renderData = renderDataVector[index];
    if (renderData.rPrimPath == primPath) {
        return renderData;
    }
    return findByKey(primPath);
}

size_t HdMaxRenderDelegate::GetRenderDataIndex(const pxr::SdfPath& path) const
{
    return renderDataIndexMap.at(path);
}

const std::unordered_map<pxr::SdfPath, size_t, pxr::SdfPath::Hash>&
HdMaxRenderDelegate::GetRenderDataIdMap() const
{
    return renderDataIndexMap;
}

std::vector<HdMaxRenderData>& HdMaxRenderDelegate::GetAllRenderData() { return renderDataVector; }

void HdMaxRenderDelegate::GetVisibleRenderData(
    const TfTokenVector&           renderTags,
    std::vector<HdMaxRenderData*>& data)
{
    for (auto& primRenderData : renderDataVector) {
        // Only display the prim if visible and if its render tag is selected.
        if (!primRenderData.visible || !primRenderData.renderTagActive) {
            continue;
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
        renderDataVector.emplace_back(rPrimId);

        size_t renderDataIdx = renderDataVector.size() - 1;
        renderDataIndexMap.insert({ rPrimId, renderDataIdx });

        // We need to keep a reference to the hydra meshes we create, so that they can be deleted
        // properly.
        auto       mesh = std::make_unique<HdMaxMesh>(this, rPrimId, renderDataIdx);
        const auto meshPtr = mesh.get();
        meshes.insert({ rPrimId, (std::move(mesh)) });
        return meshPtr;
    }
    return nullptr;
}

void HdMaxRenderDelegate::DestroyRprim(HdRprim* rPrim)
{
    const auto& path = rPrim->GetId();
    auto        primIdx = renderDataIndexMap.find(path);
    if (primIdx == renderDataIndexMap.end()) {
        return;
    }

    // Update our data structures. For the vector, we want to avoid shifting
    // everything after the index, so to remove the prim's render data, we move
    // the last item in the vector in its place, and just pop the now emptied
    // last item.
    if (primIdx->second != renderDataVector.size() - 1) {
        auto& availableSlot = renderDataVector[primIdx->second];
        availableSlot = std::move(renderDataVector.back());
        // Make sure to update the index in the map.
        renderDataIndexMap[availableSlot.rPrimPath] = primIdx->second;
    }
    renderDataVector.pop_back();
    renderDataIndexMap.erase(path);
    meshes.erase(rPrim->GetId());
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
    renderDataIndexMap.clear();
    renderDataVector.clear();
    meshes.clear();
    materialCollection = std::make_shared<HdMaxMaterialCollection>();
}

void HdMaxRenderDelegate::GarbageCollect()
{
    if (!mustGc) {
        return;
    }
    for (auto& data : GetAllRenderData()) {
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
