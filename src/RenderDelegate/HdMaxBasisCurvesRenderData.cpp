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
#include "HdMaxBasisCurvesRenderData.h"

#include "HdMaxDisplaySettings.h"

#include <MaxUsd/Utilities/VtUtils.h>

void HdMaxBasisCurvesRenderData::UpdateRenderGeometry(bool fullReload)
{
    if (shadedCurve.wireIndices.empty()) {
        return;
    }

    const bool updateIndices = fullReload
        || HdMaxChangeTracker::CheckDirty(shadedCurve.dirtyBits, HdMaxChangeTracker::DirtyIndices);

    // Update indices.
    if (updateIndices) {
        {
            MaxSDK::Graphics::IndexBufferHandle indexBuffer;
            const auto newNumberOfIndices = wireframeCurve.wireIndices.size();
            if (!wireframeCurve.wireIndices.empty()) {
                int* newData = const_cast<int*>(wireframeCurve.wireIndices.cdata());
                indexBuffer.Initialize(MaxSDK::Graphics::IndexTypeInt, newNumberOfIndices, newData);
            }
            wireframeCurve.geometry->SetIndexBuffer(indexBuffer);
            wireframeCurve.geometry->SetPrimitiveCount(int(wireframeCurve.wireIndices.size() / 2));
        }

        const auto                          renderGeometry = shadedCurve.geometry;
        MaxSDK::Graphics::IndexBufferHandle indexBuffer;

        auto       indices = shadedCurve.wireIndices;
        const auto newNumberOfIndices = indices.size();
        if (!indices.empty()) {
            int* newData = const_cast<int*>(indices.cdata());
            indexBuffer.Initialize(MaxSDK::Graphics::IndexTypeInt, newNumberOfIndices, newData);
        }
        renderGeometry->SetIndexBuffer(indexBuffer);

        renderGeometry->SetPrimitiveCount(int(indices.size()) / 2);
    }

    bool cleanupBuffers = false;

    auto updateNitrousBuffer = [this, &fullReload, &cleanupBuffers](
                                   int                      bufferIndex,
                                   const pxr::VtVec3fArray& source,
                                   pxr::HdDirtyBits         dirtyFlag) {
        if (shadedCurve.wireIndices.empty()) {
            return;
        }

        if (!HdMaxChangeTracker::CheckDirty(shadedCurve.dirtyBits, dirtyFlag) && !fullReload) {
            return;
        }

        const auto& renderGeometry = shadedCurve.geometry;

        // Update the buffer.
        auto       vertexBuffer = renderGeometry->GetVertexBuffer(bufferIndex);
        const auto previousSize = vertexBuffer.IsValid() ? vertexBuffer.GetNumberOfVertices() : 0;
        const auto newSize = source.size();
        const auto newData = MaxUsd::Vt::GetNoCopy<pxr::GfVec3f, pxr::GfVec3f>(source);

        if (previousSize == newSize && vertexBuffer.IsValid()) {
            auto data = (pxr::GfVec3f*)(vertexBuffer.Lock(0, 0, MaxSDK::Graphics::WriteAcess));
            std::copy(newData, newData + source.size(), data);
            vertexBuffer.Unlock();
        }
        // Size changed, need to initialize a new buffer.
        else {
            if (newSize == 0) {
                if (vertexBuffer.IsValid()) {
                    // Buffers with no vertices will be removed from the geometry.
                    vertexBuffer.SetNumberOfVertices(0);
                    cleanupBuffers = true;
                }
            } else {
                vertexBuffer.Initialize(sizeof(float) * 3, newSize, newData);
            }
        }

        // Make sure it is properly assigned to both shaded and wireframe buffers.
        if (vertexBuffer.IsValid()) {
            SetVertexBuffer(shadedCurve.geometry, bufferIndex, vertexBuffer);
            SetVertexBuffer(wireframeCurve.geometry, bufferIndex, vertexBuffer);
        }
    };

    // Update vertex buffers
    updateNitrousBuffer(PointsBuffer, points, HdMaxChangeTracker::DirtyPoints);

    // For selection, we need to generate the data (ones or zeros). Only do so if we need to...
    const auto dirtySelection = HdMaxChangeTracker::CheckDirty(
        shadedCurve.dirtyBits, HdMaxChangeTracker::DirtySelectionHighlight);
    if (dirtySelection) {
        const pxr::VtVec3fArray selectionBuff(
            points.size(), selected ? pxr::GfVec3f(1.0f) : pxr::GfVec3f(0.0f));
        updateNitrousBuffer(
            SelectionBuffer, selectionBuff, HdMaxChangeTracker::DirtySelectionHighlight);
    }

    // Everything is clean now!
    HdMaxChangeTracker::ClearDirtyBits(shadedCurve.dirtyBits);
    if (cleanupBuffers) {
        // Make sure we don't have any invalid vertex buffers.
        auto geom = shadedCurve.geometry;
        for (int i = int(geom->GetVertexBufferCount() - 1); i >= 0; i--) {
            auto vertexBuffer = geom->GetVertexBuffer(i);
            if (!vertexBuffer.IsValid() || vertexBuffer.GetNumberOfVertices() == 0) {
                geom->RemoveVertexBuffer(i);
            }
        }
    }
}

MaxSDK::Graphics::MaterialRequiredStreams HdMaxBasisCurvesRenderData::GetRequiredStreams()
{
    MaxSDK::Graphics::MaterialRequiredStreams multiStreamRequirements;
    {
        MaxSDK::Graphics::MaterialRequiredStreamElement multiStreamElement;
        multiStreamElement.SetType(MaxSDK::Graphics::VertexFieldFloat3);
        multiStreamElement.SetChannelCategory(MaxSDK::Graphics::MeshChannelPosition);
        multiStreamElement.SetStreamIndex(PointsBuffer);
        multiStreamRequirements.AddStream(multiStreamElement);
    }
    {
        MaxSDK::Graphics::MaterialRequiredStreamElement multiStreamElement;
        multiStreamElement.SetType(MaxSDK::Graphics::VertexFieldFloat3);
        multiStreamElement.SetChannelCategory(MaxSDK::Graphics::MeshChannelVertexColor);
        multiStreamElement.SetStreamIndex(SelectionBuffer);
        multiStreamRequirements.AddStream(multiStreamElement);
    }
    return multiStreamRequirements;
}

bool HdMaxBasisCurvesRenderData::IsInstanced() const { return shadedCurve.IsInstanced(); }

void HdMaxBasisCurvesRenderData::SetAllSubsetRenderDataDirty(const pxr::HdDirtyBits& dirtyFlag)
{
    HdMaxChangeTracker::SetDirty(shadedCurve.dirtyBits, dirtyFlag);
}

MaxSDK::Graphics::BaseMaterialHandle HdMaxBasisCurvesRenderData::ResolveViewportMaterial(
    const HdMaxPrimRenderData&                renderData,
    const SubsetRenderData&                   subsetRenderData,
    const HdMaxDisplaySettings&               displaySettings,
    const MaxSDK::Graphics::RenderNodeHandle& renderNode,
    bool                                      instanced) const
{
    auto wireMat = renderNode.GetWireframeMaterial();
    if (wireMat.IsValid()) {
        return wireMat;
    }
    // Never gets here if if the renderNode is valid, but we
    // always want to return a valid material for testing purposes.
    static MaxSDK::Graphics::StandardMaterialHandle fallback;
    fallback.Initialize();
    return fallback;
}
