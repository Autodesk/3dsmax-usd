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
#include "HdMaxInstanceGen.h"

#include "HdMaxDisplayPreferences.h"
#include "MaxRenderGeometryFacade.h"
#include "SelectionRenderItem.h"
#include "resource.h"

#include <MaxUsd/Utilities/TypeUtils.h>
#include <Maxusd/Utilities/TranslationUtils.h>

#include <Graphics/HLSLMaterialHandle.h>
#include <Graphics/StandardMaterialHandle.h>

#include <dllutilities.h>

using namespace MaxRestrictedSDKSupport::Graphics::ViewportInstancing;

namespace {
std::once_flag                       initSelectionMat;
MaxSDK::Graphics::HLSLMaterialHandle instanceSelectMaterial;
MaxSDK::Graphics::HLSLMaterialHandle instanceSelectWireMaterial;
} // namespace

HdMaxInstanceGen::HdMaxInstanceGen()
{
    // Initialize the selection effects, when the first usd render item is created.
    std::call_once(initSelectionMat, [this] {
        instanceSelectMaterial.InitializeWithResource(
            IDR_PRIM_SELECTION_SHADER, MaxSDK::GetHInstance(), L"SHADER");
        instanceSelectMaterial.SetActiveTechniqueName(L"Shaded_Instanced");
        instanceSelectWireMaterial.InitializeWithResource(
            IDR_PRIM_SELECTION_SHADER, MaxSDK::GetHInstance(), L"SHADER");
        instanceSelectWireMaterial.SetActiveTechniqueName(L"Wire_Instanced");
    });
}

void HdMaxInstanceGen::SetClean(bool wire)
{
    if (wire) {
        wireState = DirtyState::Clean;
        wireSelectionState = DirtyState::Clean;
        return;
    }
    shadedState = DirtyState::Clean;
    shadedSelectionState = DirtyState::Clean;
}

const std::vector<Matrix3>& HdMaxInstanceGen::GetTransforms() const { return transforms; }

void HdMaxInstanceGen::RequestUpdate(
    bool                        recreateInstances,
    const pxr::VtMatrix4dArray& usdTransforms)
{
    transforms.clear();
    transforms.reserve(usdTransforms.size());
    for (auto& tm : usdTransforms) {
        transforms.push_back(MaxUsd::ToMaxMatrix3(tm));
    }

    auto flag = [this](const DirtyState& state) {
        shadedState = std::max(state, shadedState);
        wireState = std::max(state, wireState);
        ;
    };

    // Typically, we need to regenerate the instances fully if things like the topology, the
    // materials, or the number of instances have changed.
    if (recreateInstances) {
        shadedData.numInstances = transforms.size();
        SetInstanceDataMatrices(shadedData, transforms);
        wireData.numInstances = transforms.size();
        SetInstanceDataMatrices(wireData, transforms);
        flag(DirtyState::NeedRecreate);
    }
    // Otherwise just update the transforms.
    else {
        flag(DirtyState::NeedUpdate);
        if (shadedState != DirtyState::NeedRecreate) {
            ClearInstanceData(shadedData);
        }
        if (wireState != DirtyState::NeedRecreate) {
            ClearInstanceData(wireData);
        }
        SetInstanceDataMatrices(shadedData, transforms);
        SetInstanceDataMatrices(wireData, transforms);

        shadedData.numInstances = transforms.size();
        wireData.numInstances = transforms.size();
    }

    RequestSelectionDisplayUpdate(recreateInstances);

    shadedData.bTransformationsAreInWorldSpace = false;
    wireData.bTransformationsAreInWorldSpace = false;
}

void HdMaxInstanceGen::RequestSelectionDisplayUpdate(bool recreate)
{
    auto flag = [this](const DirtyState& state) {
        shadedSelectionState = std::max(state, shadedSelectionState);
        wireSelectionState = std::max(state, wireSelectionState);
        ;
    };

    // Need to recreate when, for example, selection changes. A change in the number of instances
    // requires an full recreation. If only the transforms change, we can simply update, less
    // expensive.
    flag(recreate ? DirtyState::NeedRecreate : DirtyState::NeedUpdate);

    selectedTransforms.clear();
    for (int i = 0; i < selection.size(); ++i) {
        if (selection[i]) {
            selectedTransforms.push_back(transforms[i]);
        }
    }
    shadedSelectionData.numInstances = selectedTransforms.size();
    SetInstanceDataMatrices(shadedSelectionData, selectedTransforms);
    wireSelectionData.numInstances = selectedTransforms.size();
    SetInstanceDataMatrices(wireSelectionData, selectedTransforms);
}

void HdMaxInstanceGen::GenerateInstances(
    MaxRenderGeometryFacade*                      geom,
    MaxSDK::Graphics::BaseMaterialHandle*         material,
    MaxSDK::Graphics::IRenderItemContainer&       targetRenderItemContainer,
    const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
    MaxSDK::Graphics::UpdateNodeContext&          nodeContext,
    bool                                          wireframe,
    int                                           subset,
    ViewExp*                                      viewExp)
{
    if (!geom || !geom->GetInstanceRenderGeometry()) {
        return;
    }

    if (!wireframe && (subset < 0 && subset >= cachedShaded.size())) {
        DbgAssert(0 && _T("Invalid subset index"));
        return;
    }

    // Update and generate instances.

    // There is a very hard to reproduce issue in nitrous where an instance geometry object can get
    // corrupted when calling creating the instancing data multiple times on the same
    // InstanceGeometryObject. Recreating a new object works around this issue. It is definitely a
    // suspicious hack.
    const auto state = wireframe ? wireState : shadedState;
    if (state == DirtyState::NeedRecreate) {
        geom->RebuildInstanceGeom(false);
    }

    auto& data = wireframe ? wireData : shadedData;
    auto& selectionData = wireframe ? wireSelectionData : shadedSelectionData;

    // The given material can be null (in wireframe mode, this will let the system decide, and set
    // the correct wireframe material)
    data.numViewportMaterials = material ? 1 : 0;
    data.pViewportMaterials = material;

    selectionData.numViewportMaterials = 1;
    const auto selectionMaterial
        = wireframe ? &instanceSelectWireMaterial : &instanceSelectMaterial;
    const auto& selColor = HdMaxDisplayPreferences::GetInstance().GetSelectionColor();
    selectionMaterial->SetFloat4Parameter(
        L"LineColor", Point4(selColor.r, selColor.g, selColor.b, selColor.a));

    // Configure the ZBias. This is so that our selection wireframe displays on top of the geometry.
    if (viewExp) {
        auto bias = SelectionRenderItem::GetSelectionZBias(viewExp, wireframe);
        selectionMaterial->SetFloatParameter(L"ZBias", bias);
    }

    selectionData.pViewportMaterials = selectionMaterial;

    MaxSDK::Graphics::RenderItemHandleArray* cachedItems;
    MaxSDK::Graphics::RenderItemHandleArray* cachedSelectionItems;
    if (wireframe) {
        cachedItems = &cachedWire;
        cachedSelectionItems = &cachedSelectionWire;
    } else {
        cachedItems = &cachedShaded[subset];
        cachedSelectionItems = &cachedSelectionShaded[subset];
    }

    auto createOrUpdate = [this](
                              const DirtyState&                        state,
                              InstanceDisplayGeometry*                 geom,
                              const InstanceData&                      data,
                              MaxSDK::Graphics::RenderItemHandleArray* cached) {
        switch (state) {
        case DirtyState::NeedRecreate: {
            CreateInstanceData(geom, data);
            cached->ClearAllRenderItems();
            break;
        }
        case DirtyState::NeedUpdate: {
            UpdateInstanceData(geom, data);
            cached->ClearAllRenderItems();
            break;
        }
        case DirtyState::Clean:
        default:;
        }
    };

    // The instance geometry render items :

    const auto instanceGeom = geom->GetInstanceRenderGeometry();
    createOrUpdate(state, instanceGeom, data, cachedItems);

    // If we still have cached render items at this point, it means nothing has changed, and we can
    // use the render items we already have.
    if (cachedItems->GetNumberOfRenderItems() == 0) {
        instanceGeom->GenerateInstances(wireframe, updateDisplayContext, nodeContext, *cachedItems);
    }
    targetRenderItemContainer.AddRenderItems(*cachedItems);

    // Update and generate any required selection display instance render items.
    if (selectionData.numInstances) {
        // Even if the selection itself didnt change, we may need to update the selection render
        // items, for example if the geometry has changed.
        const auto effectiveSelectionState
            = std::max(state, wireframe ? wireSelectionState : shadedSelectionState);
        if (effectiveSelectionState == DirtyState::NeedRecreate) {
            geom->RebuildInstanceGeom(true);
        }
        const auto instanceSelectGeom = geom->GetInstanceSelectionRenderGeometry();
        createOrUpdate(
            effectiveSelectionState, instanceSelectGeom, selectionData, cachedSelectionItems);
        if (cachedSelectionItems->GetNumberOfRenderItems() == 0) {
            instanceSelectGeom->GenerateInstances(
                wireframe, updateDisplayContext, nodeContext, *cachedSelectionItems);
        }
        targetRenderItemContainer.AddRenderItems(*cachedSelectionItems);
    }
}

void HdMaxInstanceGen::Select(int instanceIdx)
{
    const bool inBounds = instanceIdx >= 0 && instanceIdx < selection.size();
    DbgAssert(inBounds);
    if (!inBounds) {
        return;
    }
    selection[instanceIdx] = true;
}

void HdMaxInstanceGen::ResetSelection()
{
    selection.resize(GetNumInstances());
    std::fill(selection.begin(), selection.end(), false);
}

const std::vector<bool>& HdMaxInstanceGen::GetSelection() { return selection; }

void HdMaxInstanceGen::SetSubsetCount(size_t subsetCount)
{
    cachedShaded.resize(subsetCount);
    cachedSelectionShaded.resize(subsetCount);
}

size_t HdMaxInstanceGen::GetSubsetCount() const { return cachedShaded.size(); }

const pxr::GfRange3d HdMaxInstanceGen::ComputeSelectionBoundingBox(pxr::GfRange3d& extent)
{
    pxr::GfRange3d totalExtent;
    if (extent.IsEmpty()) {
        return totalExtent;
    }

    // Convert selectedTransforms to pxr::VtMatrix4dArray
    pxr::VtMatrix4dArray pxrSelectedTransforms;
    for (const auto& selectedTransform : selectedTransforms) {
        pxrSelectedTransforms.push_back(MaxUsd::ToUsd(selectedTransform));
    }

    return MaxUsd::ComputeTotalExtent(extent, pxrSelectedTransforms);
}