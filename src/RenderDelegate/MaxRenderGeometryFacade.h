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
#pragma once
#include <Graphics/SimpleRenderGeometry.h>
// For instancing. Instancing SDK changed in 2023.
#include <MaxUsd/Utilities/MaxRestrictedSupportUtils.h>

/**
 * \brief Simple facade to operate on SimpleRenderGeometry or InstanceRenderGeometry, transparently.
 */
class MaxRenderGeometryFacade
{
public:
    MaxRenderGeometryFacade(MaxSDK::Graphics::SimpleRenderGeometry* simpleRenderGeometry);
    MaxRenderGeometryFacade(
        MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceDisplayGeometry*
            instancedGeometry,
        MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceDisplayGeometry*
            instancedSelectionGeometry);

    ~MaxRenderGeometryFacade();

    MaxSDK::Graphics::SimpleRenderGeometry* GetSimpleRenderGeometry() const
    {
        return simpleRenderGeometry;
    }

    MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceDisplayGeometry*
    GetInstanceRenderGeometry() const
    {
        return instanceRenderGeometry;
    }

    MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceDisplayGeometry*
    GetInstanceSelectionRenderGeometry() const
    {
        return instanceSelectionRenderGeometry;
    }

    MaxSDK::Graphics::IndexBufferHandle GetIndexBuffer() const;
    void   SetIndexBuffer(const MaxSDK::Graphics::IndexBufferHandle& indexBuffer) const;
    void   AddVertexBuffer(const MaxSDK::Graphics::VertexBufferHandle& vertexBuffer) const;
    void   RemoveVertexBuffer(size_t index) const;
    size_t GetVertexBufferCount() const;
    MaxSDK::Graphics::VertexBufferHandle GetVertexBuffer(size_t index) const;
    void SetPrimitiveType(MaxSDK::Graphics::PrimitiveType type) const;
    void SetPrimitiveCount(size_t count) const;
    MaxSDK::Graphics::MaterialRequiredStreams& GetSteamRequirement() const;
    void SetStreamRequirement(const MaxSDK::Graphics::MaterialRequiredStreams& streamFormat) const;

    /**
     * \brief Rebuilds a new InstanceDisplayGeometry object. There is a very hard to reproduce issue in nitrous
     * where an instance geometry object can get corrupted when creating the instancing data
     * multiple times on the same InstanceGeometryObject. Recreating a new object works around this
     * issue. It is definitely a suspicious hack.
     * \param selectionInstances If true, rebuilds the selection instance's geometry object.
     */
    void RebuildInstanceGeom(bool selectionInstances);

private:
    MaxSDK::Graphics::SimpleRenderGeometry* simpleRenderGeometry = nullptr;
    // When instancing, we need two instance render items - one for the normal display, and one for
    // displaying selection. Both carry the exact same indices and vertex buffers, only instances
    // can differ (only a subset of all instances can be selected). The two render items are always
    // kept in sync in the MaxRenderGeometryFacade.
    MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceDisplayGeometry*
        instanceRenderGeometry
        = nullptr;
    MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceDisplayGeometry*
        instanceSelectionRenderGeometry
        = nullptr;
};
