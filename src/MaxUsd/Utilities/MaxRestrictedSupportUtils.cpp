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
#include "MaxRestrictedSupportUtils.h"

#ifdef IS_MAX2023_OR_GREATER
#include <Graphics/InstanceDisplayGeometry.h>
#endif

#ifdef IS_MAX2025_OR_GREATER
#include <Geom/matrix3.h>
#else
#include <matrix3.h>
#endif
#ifdef IS_MAX2022
#include <Graphics/InstanceRenderGeometry.h>
#endif

namespace MaxRestrictedSDKSupport {

#ifdef IS_MAX2023_OR_GREATER

void Graphics::ViewportInstancing::CreateInstanceData(
    InstanceDisplayGeometry* instanceGeometry,
    const InstanceData&      data)
{
    instanceGeometry->CreateInstanceData(data);
}

void Graphics::ViewportInstancing::UpdateInstanceData(
    InstanceDisplayGeometry* instanceGeometry,
    const InstanceData&      data)
{
    instanceGeometry->UpdateInstanceData(data);
}

void Graphics::ViewportInstancing::SetInstanceDataMatrices(
    InstanceData&         data,
    std::vector<Matrix3>& matrices)
{
    data.pMatrices = matrices.data();
}

void Graphics::ViewportInstancing::ClearInstanceData(InstanceData& data)
{
    data.numInstances = 0;
    data.bTransformationsAreInWorldSpace = false;
    data.pMatrices = nullptr;
    data.pPositions = nullptr;
    data.pOrientationsAsPoint4 = nullptr;
    data.numOrientationsAsPoint4 = 0;
    data.pOrientationsAsQuat = nullptr;
    data.numOrientationsAsQuat = 0;
    data.pScales = nullptr;
    data.numScales = 0;
    data.pViewportMaterials = nullptr;
    data.numViewportMaterials = 0;
    data.pUVWMapChannel1 = nullptr;
    data.numUVWMapChannel1 = 0;
    data.pUVWMapChannel2 = nullptr;
    data.numUVWMapChannel2 = 0;
    data.pUVWMapChannel3 = nullptr;
    data.numUVWMapChannel3 = 0;
    data.pUVWMapChannel4 = nullptr;
    data.numUVWMapChannel4 = 0;
    data.pUVWMapChannel5 = nullptr;
    data.numUVWMapChannel5 = 0;
    data.pUVWMapChannel6 = nullptr;
    data.numUVWMapChannel6 = 0;
    data.pUVWMapChannel7 = nullptr;
    data.numUVWMapChannel7 = 0;
    data.pUVWMapChannel8 = nullptr;
    data.numUVWMapChannel8 = 0;
    data.pColors = nullptr;
    data.numColors = 0;
    data.pVertexColorsAsColor = nullptr;
    data.numVertexColorsAsColor = 0;
    data.pVertexColorsAsDWORD = nullptr;
    data.numVertexColorsAsDWORD = 0;
}

#endif

#ifdef IS_MAX2022
void Graphics::ViewportInstancing::CreateInstanceData(
    InstanceDisplayGeometry* instanceGeometry,
    const InstanceData&      data)
{
    instanceGeometry->CreateInstanceVertexBuffer(data);
}

void Graphics::ViewportInstancing::UpdateInstanceData(
    InstanceDisplayGeometry* instanceGeometry,
    const InstanceData&      data)
{
    instanceGeometry->UpdateInstanceVertexBuffer(data);
}

void Graphics::ViewportInstancing::SetInstanceDataMatrices(
    InstanceData&         data,
    std::vector<Matrix3>& matrices)
{
    data.pMatrices = matrices.data();
    data.numMatrices = matrices.size();
}

void Graphics::ViewportInstancing::ClearInstanceData(InstanceData& data)
{
    data.numInstances = 0;
    data.bTransformationsAreInWorldSpace = false;
    data.pMatrices = nullptr;
    data.numMatrices = 0;
    data.pPositions = nullptr;
    data.numPositions = 0;
    data.pOrientationsAsPoint4 = nullptr;
    data.numOrientationsAsPoint4 = 0;
    data.pOrientationsAsQuat = nullptr;
    data.numOrientationsAsQuat = 0;
    data.pScales = nullptr;
    data.numScales = 0;
    data.pViewportMaterials = nullptr;
    data.materialStyle
        = MaxSDK::Graphics::MaterialConversionHelper::MaterialStyles::MaterialStyle_Simple;
    data.numViewportMaterials = 0;
    data.pUVWMapChannel1 = nullptr;
    data.numUVWMapChannel1 = 0;
    data.pUVWMapChannel2 = nullptr;
    data.numUVWMapChannel2 = 0;
    data.pUVWMapChannel3 = nullptr;
    data.numUVWMapChannel3 = 0;
    data.pUVWMapChannel4 = nullptr;
    data.numUVWMapChannel4 = 0;
    data.pUVWMapChannel5 = nullptr;
    data.numUVWMapChannel5 = 0;
    data.pUVWMapChannel6 = nullptr;
    data.numUVWMapChannel6 = 0;
    data.pUVWMapChannel7 = nullptr;
    data.numUVWMapChannel7 = 0;
    data.pUVWMapChannel8 = nullptr;
    data.numUVWMapChannel8 = 0;
    data.pColors = nullptr;
    data.numColors = 0;
    data.pVertexColorsAsColor = nullptr;
    data.numVertexColorsAsColor = 0;
    data.pVertexColorsAsDWORD = nullptr;
    data.numVertexColorsAsDWORD = 0;
}
#endif
} // namespace MaxRestrictedSDKSupport
