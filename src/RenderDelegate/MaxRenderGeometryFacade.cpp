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
#include "MaxRenderGeometryFacade.h"

MaxRenderGeometryFacade::MaxRenderGeometryFacade(
    MaxSDK::Graphics::SimpleRenderGeometry* simpleRenderGeometry)
{
    this->simpleRenderGeometry = simpleRenderGeometry;
    this->simpleRenderGeometry->AddRef();
}

MaxRenderGeometryFacade::MaxRenderGeometryFacade(
    MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceDisplayGeometry*
        instanceGeometry,
    MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceDisplayGeometry*
        instancedSelectionGeometry)
{
    this->instanceRenderGeometry = instanceGeometry;
    this->instanceRenderGeometry->AddRef();
    this->instanceSelectionRenderGeometry = instancedSelectionGeometry;
    this->instanceSelectionRenderGeometry->AddRef();
}

MaxRenderGeometryFacade::~MaxRenderGeometryFacade()
{
    if (this->instanceRenderGeometry) {
        this->instanceRenderGeometry->Release();
        this->instanceSelectionRenderGeometry->Release();
        return;
    }
    this->simpleRenderGeometry->Release();
}

void MaxRenderGeometryFacade::SetIndexBuffer(
    const MaxSDK::Graphics::IndexBufferHandle& indexBuffer) const
{
    if (simpleRenderGeometry) {
        simpleRenderGeometry->SetIndexBuffer(indexBuffer);
        return;
    }
    instanceRenderGeometry->SetIndexBuffer(indexBuffer);
    instanceSelectionRenderGeometry->SetIndexBuffer(indexBuffer);
}

void MaxRenderGeometryFacade::AddVertexBuffer(
    const MaxSDK::Graphics::VertexBufferHandle& vertexBuffer) const
{
    if (simpleRenderGeometry) {
        simpleRenderGeometry->AddVertexBuffer(vertexBuffer);
        return;
    }
    instanceRenderGeometry->AddVertexBuffer(vertexBuffer);
    instanceSelectionRenderGeometry->AddVertexBuffer(vertexBuffer);
}

void MaxRenderGeometryFacade::RemoveVertexBuffer(size_t index) const
{
    if (simpleRenderGeometry) {
        simpleRenderGeometry->RemoveVertexBuffer(index);
        return;
    }
    instanceRenderGeometry->RemoveVertexBuffer(index);
    instanceSelectionRenderGeometry->RemoveVertexBuffer(index);
}

size_t MaxRenderGeometryFacade::GetVertexBufferCount() const
{
    if (simpleRenderGeometry) {
        return simpleRenderGeometry->GetVertexBufferCount();
    }
    return instanceRenderGeometry->GetVertexBufferCount();
}

MaxSDK::Graphics::VertexBufferHandle MaxRenderGeometryFacade::GetVertexBuffer(size_t index) const
{
    if (simpleRenderGeometry) {
        return simpleRenderGeometry->GetVertexBuffer(index);
    }
    return instanceRenderGeometry->GetVertexBuffer(index);
}

MaxSDK::Graphics::IndexBufferHandle MaxRenderGeometryFacade::GetIndexBuffer() const
{
    if (simpleRenderGeometry) {
        return simpleRenderGeometry->GetIndexBuffer();
    }
    return instanceRenderGeometry->GetIndexBuffer();
}

void MaxRenderGeometryFacade::SetPrimitiveType(MaxSDK::Graphics::PrimitiveType type) const
{
    if (simpleRenderGeometry) {
        simpleRenderGeometry->SetPrimitiveType(type);
        return;
    }
    instanceRenderGeometry->SetPrimitiveType(type);
    instanceSelectionRenderGeometry->SetPrimitiveType(type);
}

void MaxRenderGeometryFacade::SetPrimitiveCount(size_t count) const
{
    if (simpleRenderGeometry) {
        simpleRenderGeometry->SetPrimitiveCount(count);
        return;
    }
    instanceRenderGeometry->SetPrimitiveCount(count);
    instanceSelectionRenderGeometry->SetPrimitiveCount(count);
}

MaxSDK::Graphics::MaterialRequiredStreams& MaxRenderGeometryFacade::GetSteamRequirement() const
{
    if (simpleRenderGeometry) {
        return simpleRenderGeometry->GetSteamRequirement();
    }
    return instanceRenderGeometry->GetSteamRequirement();
}

void MaxRenderGeometryFacade::SetStreamRequirement(
    const MaxSDK::Graphics::MaterialRequiredStreams& streamFormat) const
{
    if (simpleRenderGeometry) {
        simpleRenderGeometry->SetSteamRequirement(streamFormat);
        return;
    }
    instanceRenderGeometry->SetStreamRequirement(streamFormat);
    instanceSelectionRenderGeometry->SetStreamRequirement(streamFormat);
}

void MaxRenderGeometryFacade::RebuildInstanceGeom(bool selectionInstances)
{
    auto& toRebuild = selectionInstances ? instanceSelectionRenderGeometry : instanceRenderGeometry;

    const auto tmp
        = new MaxRestrictedSDKSupport::Graphics::ViewportInstancing::InstanceDisplayGeometry {};
    tmp->SetIndexBuffer(toRebuild->GetIndexBuffer());
    for (int i = 0; i < toRebuild->GetVertexBufferCount(); ++i) {
        tmp->AddVertexBuffer(toRebuild->GetVertexBuffer(i));
    }
    tmp->SetPrimitiveType(toRebuild->GetPrimitiveType());
    tmp->SetPrimitiveCount(toRebuild->GetPrimitiveCount());
    tmp->SetStreamRequirement(toRebuild->GetSteamRequirement());
    tmp->AddRef();

    toRebuild->Release();
    toRebuild = tmp;
}
