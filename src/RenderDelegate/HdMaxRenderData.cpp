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
#include "HdMaxRenderData.h"

#include "HdMaxDisplaySettings.h"

#include <MaxUsd/Utilities/VtUtils.h>

// Helper, sets a buffer in the geometry at the given index.
void _SetVertexBuffer(
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
HdMaxRenderData::SubsetRenderData::GetRenderItemDecorator(bool selected)
{
    if (selected) {
        return selectionRenderItem;
    }
    return renderItem;
}

bool HdMaxRenderData::SubsetRenderData::IsInstanced() const { return !renderItem.IsValid(); }

bool HdMaxRenderData::IsInstanced() const
{
    if (shadedSubsets.empty()) {
        return false;
    }

    return shadedSubsets[0].IsInstanced();
}

void HdMaxRenderData::UpdateRenderGeometry(bool fullReload)
{
    if (shadedSubsets.empty()) {
        return;
    }

    const bool updateIndices = fullReload
        || std::any_of(shadedSubsets.begin(),
                       shadedSubsets.end(),
                       [](const SubsetRenderData& subset) {
                           return HdMaxChangeTracker::CheckDirty(
                               subset.dirtyBits, HdMaxChangeTracker::DirtyIndices);
                       });

    // Update indices.
    if (updateIndices) {
        size_t totalWireSize = 0;
        for (const auto& subset : shadedSubsets) {
            const auto                          renderGeometry = subset.geometry;
            MaxSDK::Graphics::IndexBufferHandle indexBuffer;

            auto       indices = subset.indices;
            const auto newNumberOfIndices = indices.size() * 3;
            if (!indices.empty()) {
                const auto newData = MaxUsd::Vt::GetNoCopy<int, pxr::GfVec3i>(indices);
                indexBuffer.Initialize(MaxSDK::Graphics::IndexTypeInt, newNumberOfIndices, newData);
            }
            renderGeometry->SetIndexBuffer(indexBuffer);

            renderGeometry->SetPrimitiveCount(int(indices.size()));
            totalWireSize += subset.wireIndices.size();
        }

        // Wireframe
        if (totalWireSize) {
            MaxSDK::Graphics::IndexBufferHandle indexBuffer;

            indexBuffer.Initialize(MaxSDK::Graphics::IndexTypeInt, totalWireSize, nullptr);

            size_t nextStartIndex = 0;
            for (const auto& subset : shadedSubsets) {
                auto data = (int*)(indexBuffer.Lock(
                    nextStartIndex, subset.wireIndices.size(), MaxSDK::Graphics::WriteAcess));

                int* newData = const_cast<int*>(subset.wireIndices.cdata());
                std::copy(newData, newData + subset.wireIndices.size(), data);
                indexBuffer.Unlock();
                nextStartIndex += subset.wireIndices.size();
            }
            wireframe.geometry->SetIndexBuffer(indexBuffer);
        }
        wireframe.geometry->SetPrimitiveCount(int(totalWireSize) / 2);
    }

    // Update vertex buffers. All subsets share the same vertex buffers, so just look at the
    // geometry from the first subset.
    // const auto renderGeometry = shadedSubsets[0].geometry;

    bool cleanupBuffers = false;

    auto updateNitrousBuffer = [this, &fullReload, &cleanupBuffers](
                                   std::vector<SubsetRenderData>& subsetsToUpdate,
                                   int                            bufferIndex,
                                   const pxr::VtVec3fArray&       source,
                                   pxr::HdDirtyBits               dirtyFlag) {
        if (subsetsToUpdate.empty()) {
            return;
        }

        // All subsets share the same vertex buffers - we dont need to load the buffer unless at
        // least one is flagged dirty. In most cases, either all are dirty, or none. But a mix can
        // happen here, for example if only a few subsets end up consolidated - when we get here,
        // these will be clean, but the rest still marked dirty.
        const auto hasAtLeastOneDirtySubset = std::any_of(
            subsetsToUpdate.begin(),
            subsetsToUpdate.end(),
            [&dirtyFlag](const SubsetRenderData& subset) {
                return HdMaxChangeTracker::CheckDirty(subset.dirtyBits, dirtyFlag);
            });

        if (!hasAtLeastOneDirtySubset && !fullReload) {
            return;
        }

        // All the passed subsets will share the same vertex buffers.
        const auto& renderGeometry = subsetsToUpdate[0].geometry;

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

        // Make sure it is properly assigned to all subsets.
        if (vertexBuffer.IsValid()) {
            for (const auto& subsetItem : subsetsToUpdate) {
                const auto geometry = subsetItem.geometry;
                _SetVertexBuffer(geometry, bufferIndex, vertexBuffer);
            }
            // Wireframe doesnt need UVs.
            if (bufferIndex != UvsBuffer) {
                _SetVertexBuffer(wireframe.geometry, bufferIndex, vertexBuffer);
            }
        }
    };

    updateNitrousBuffer(shadedSubsets, PointsBuffer, points, HdMaxChangeTracker::DirtyPoints);
    updateNitrousBuffer(shadedSubsets, NormalsBuffer, normals, HdMaxChangeTracker::DirtyNormals);

    // For selection, we need to generate the data (ones or zeros). Only do so if we need to...
    const auto dirtySelection = std::any_of(
        shadedSubsets.begin(), shadedSubsets.end(), [](const SubsetRenderData& subset) {
            return HdMaxChangeTracker::CheckDirty(
                subset.dirtyBits, HdMaxChangeTracker::DirtySelectionHighlight);
        });
    if (dirtySelection) {
        const pxr::VtVec3fArray selectionBuff(
            points.size(), selected ? pxr::GfVec3f(1.0f) : pxr::GfVec3f(0.0f));
        updateNitrousBuffer(
            shadedSubsets,
            SelectionBuffer,
            selectionBuff,
            HdMaxChangeTracker::DirtySelectionHighlight);
    }

    if (uvs.empty()) {
        updateNitrousBuffer(shadedSubsets, UvsBuffer, {}, HdMaxChangeTracker::DirtyUvs);
    } else {
        std::unordered_map<int, std::vector<SubsetRenderData>> subsetSharingDiffuseColorUv;
        for (const auto& subset : shadedSubsets) {
            // For now, only support standard mode in the viewport, so only a single UV. Make sure
            // we use the UV used for the diffuseColor.

            // Default to the first uv in our list.
            auto uvIndex = 0;
            for (int i = 0; i < uvs.size(); ++i) {
                auto it = materialDiffuseColorUvPrimvars.find(subset.materiaId);
                if (it != materialDiffuseColorUvPrimvars.end()) {
                    if (it->second == uvs[i].varname) {
                        uvIndex = i;
                        break;
                    }
                }
            }
            subsetSharingDiffuseColorUv[uvIndex].push_back(subset);
        }
        for (auto entry : subsetSharingDiffuseColorUv) {
            updateNitrousBuffer(
                entry.second, UvsBuffer, uvs[entry.first].data, HdMaxChangeTracker::DirtyUvs);
        }
    }

    // Everything is clean now!
    for (auto& subset : shadedSubsets) {
        HdMaxChangeTracker::ClearDirtyBits(subset.dirtyBits);
    }
    if (cleanupBuffers) {
        // Make sure we don't have any invalid vertex buffers (this would typically happen when
        // switching to performance mode, which doesn't load UVs and Vertex Colors).
        auto geom = shadedSubsets[0].geometry;
        for (int i = int(geom->GetVertexBufferCount() - 1); i >= 0; i--) {
            auto vertexBuffer = geom->GetVertexBuffer(i);
            if (!vertexBuffer.IsValid() || vertexBuffer.GetNumberOfVertices() == 0) {
                geom->RemoveVertexBuffer(i);
            }
        }
    }
}

void HdMaxRenderData::SetAllSubsetRenderDataDirty(const pxr::HdDirtyBits& dirtyFlag)
{
    for (auto& subset : shadedSubsets) {
        HdMaxChangeTracker::SetDirty(subset.dirtyBits, dirtyFlag);
    }
}

MaxSDK::Graphics::MaterialRequiredStreams HdMaxRenderData::GetRequiredStreams(bool wire)
{
    MaxSDK::Graphics::MaterialRequiredStreams multiStreamRequirements;
    {
        MaxSDK::Graphics::MaterialRequiredStreamElement multiStreamElement;
        multiStreamElement.SetType(MaxSDK::Graphics::VertexFieldFloat3);
        multiStreamElement.SetChannelCategory(MaxSDK::Graphics::MeshChannelPosition);
        multiStreamElement.SetStreamIndex(PointsBuffer);
        multiStreamRequirements.AddStream(multiStreamElement);
    }
    // Need normals even in wireframe, as it can still be shaded.
    {
        MaxSDK::Graphics::MaterialRequiredStreamElement multiStreamElement;
        multiStreamElement.SetType(MaxSDK::Graphics::VertexFieldFloat3);
        multiStreamElement.SetChannelCategory(MaxSDK::Graphics::MeshChannelVertexNormal);
        multiStreamElement.SetStreamIndex(NormalsBuffer);
        multiStreamRequirements.AddStream(multiStreamElement);
    }
    // We use the vertex color buffer for selection highlighting in both shaded and wire items.
    {
        MaxSDK::Graphics::MaterialRequiredStreamElement multiStreamElement;
        multiStreamElement.SetType(MaxSDK::Graphics::VertexFieldFloat3);
        multiStreamElement.SetChannelCategory(MaxSDK::Graphics::MeshChannelVertexColor);
        multiStreamElement.SetStreamIndex(SelectionBuffer);
        multiStreamRequirements.AddStream(multiStreamElement);
    }

    if (!wire) {
        MaxSDK::Graphics::MaterialRequiredStreamElement multiStreamElement;
        multiStreamElement.SetType(MaxSDK::Graphics::VertexFieldFloat3);
        multiStreamElement.SetChannelCategory(MaxSDK::Graphics::MeshChannelTexcoord);
        multiStreamElement.SetStreamIndex(UvsBuffer);
        multiStreamRequirements.AddStream(multiStreamElement);
    }

    return multiStreamRequirements;
}

MaxSDK::Graphics::BaseMaterialHandle
HdMaxRenderData::GetDisplayColorNitrousHandle(bool instanced) const
{
    return instanced ? instanceDisplayColorNitrousHandle : displayColorNitrousHandle;
}

MaxSDK::Graphics::BaseMaterialHandle HdMaxRenderData::ResolveViewportMaterial(
    const SubsetRenderData&     subsetGeometry,
    const HdMaxDisplaySettings& displaySettings,
    bool                        instanced) const
{
    // Figure out the material we need to use in the viewport.
    switch (displaySettings.GetDisplayMode()) {
    case HdMaxDisplaySettings::USDDisplayColor: {

        return GetDisplayColorNitrousHandle(instanced);
    }
    case HdMaxDisplaySettings::USDPreviewSurface: {
        auto                                 materialData = subsetGeometry.materialData;
        MaxSDK::Graphics::BaseMaterialHandle nitrousMaterial;
        if (materialData) {
            nitrousMaterial = materialData->GetNitrousMaterial(instanced);
        }
        if (nitrousMaterial.IsValid()) {
            return nitrousMaterial;
        }
        // Fallback to display color.
        return GetDisplayColorNitrousHandle(instanced);
    }
    case HdMaxDisplaySettings::WireColor:
    default: {

        return displaySettings.GetWireColorMaterial(instanced);
    }
    }
}
