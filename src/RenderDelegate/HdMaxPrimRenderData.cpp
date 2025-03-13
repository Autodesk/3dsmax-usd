//
// Copyright 2024 Autodesk
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
#include "HdMaxPrimRenderData.h"

#include "GizmoMaterial.h"
#include "HdMaxDisplayPreferences.h"
#include "HdMaxDisplaySettings.h"

#include <MaxUsd/Utilities/VtUtils.h>

#include <Graphics/HLSLMaterialHandle.h>

// Helper, sets a buffer in the geometry at the given index.
void HdMaxPrimRenderData::SetVertexBuffer(
    std::shared_ptr<MaxRenderGeometryFacade> geometry,
    int                                      index,
    MaxSDK::Graphics::VertexBufferHandle     newBuffer)
{
    if (geometry->GetVertexBufferCount() == index) {
        geometry->AddVertexBuffer(newBuffer);
        return;
    }
    if (geometry->GetVertexBufferCount() < index) {
        return;
    }

    std::vector<MaxSDK::Graphics::VertexBufferHandle> buffers;
    const auto bufferCount = geometry->GetVertexBufferCount();
    for (int i = 0; i < bufferCount; i++) {
        if (i == index) {
            buffers.push_back(newBuffer);
            continue;
        }
        buffers.push_back(geometry->GetVertexBuffer(i));
    }

    for (int i = int(bufferCount) - 1; i >= 0; --i) {
        geometry->RemoveVertexBuffer(i);
    }

    for (int i = 0; i < bufferCount; i++) {
        geometry->AddVertexBuffer(buffers[i]);
    }
}

MaxSDK::Graphics::RenderItemHandleDecorator&
HdMaxPrimRenderData::SubsetRenderData::GetRenderItemDecorator(bool selected)
{
    if (selected) {
        return selectionRenderItem;
    }
    return renderItem;
}

bool HdMaxPrimRenderData::SubsetRenderData::IsInstanced() const { return !renderItem.IsValid(); }