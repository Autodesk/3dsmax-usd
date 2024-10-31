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
#include "TestHelpers.h"

#include <RenderDelegate/HdMaxEngine.h>
#include <RenderDelegate/SelectionRenderItem.h>

#include <Graphics/GeometryRenderItemHandle.h>

fs::path GetTestDataPath()
{
    fs::path testDataPath;
    wchar_t  dllPath[MAX_PATH];
    HMODULE  hm = NULL;
    if (GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&GetTestDataPath,
            &hm)
        == 0) {
        return testDataPath;
    }
    if (GetModuleFileName(hm, dllPath, sizeof(dllPath)) == 0) {
        return testDataPath;
    }

    auto tmp = fs::path { dllPath };
    return tmp.parent_path().append(L"..\\data");
}

void TestRender(
    pxr::UsdStageRefPtr                     stage,
    HdMaxEngine&                            engine,
    MaxSDK::Graphics::IRenderItemContainer& renderItems,
    const pxr::UsdTimeCode&                 timeCode,
    MultiMtl*                               multiMat,
    const pxr::TfTokenVector&               reprs,
    const HdMaxConsolidator::Config&        consolidationConfig)
{
    pxr::GfMatrix4d rootTransform;
    rootTransform.SetIdentity();
    renderItems.ClearAllRenderItems();
    MockUpdateNodeContext nodeContext {};
    engine.Render(
        stage->GetPseudoRoot(),
        rootTransform,
        renderItems,
        timeCode,
        MockUpdateDisplayContext {},
        nodeContext,
        reprs,
        { pxr::HdTokens->geometry },
        multiMat,
        consolidationConfig);
}

MaxSDK::Graphics::SimpleRenderGeometry* GetRenderItemGeometry(
    const MaxSDK::Graphics::RenderItemHandle& renderItem,
    bool                                      decorated,
    bool                                      selectionRenderItem)
{
    MaxSDK::Graphics::IRenderGeometryPtr iRenderGeom;
    // Typically, USD render items are decorated render items (to support a transform offset)
    if (decorated) {
        const auto& decorator
            = static_cast<const MaxSDK::Graphics::RenderItemHandleDecorator&>(renderItem);

        if (selectionRenderItem) {
            const auto& customItemHandle
                = static_cast<const MaxSDK::Graphics::CustomRenderItemHandle&>(
                    decorator.GetDecoratedRenderItem());
            const auto usdRenderItem
                = static_cast<SelectionRenderItem*>(customItemHandle.GetCustomeImplementation());
            const auto renderGeom = usdRenderItem->GetRenderGeometry();
            iRenderGeom
                = dynamic_cast<MaxSDK::Graphics::SimpleRenderGeometry*>(renderGeom.GetPointer());
        } else {
            const auto& geometryRenderItem
                = static_cast<const MaxSDK::Graphics::GeometryRenderItemHandle&>(
                    decorator.GetDecoratedRenderItem());
            iRenderGeom = geometryRenderItem.GetRenderGeometry();
        }
    }
    // Consolidated/instanced render items are not decorated.
    else {
        if (selectionRenderItem) {
            const auto& customItem
                = static_cast<const MaxSDK::Graphics::CustomRenderItemHandle&>(renderItem);
            const auto usdRenderItem
                = static_cast<SelectionRenderItem*>(customItem.GetCustomeImplementation());
            if (!usdRenderItem) {
                return nullptr;
            }
            iRenderGeom = usdRenderItem->GetRenderGeometry();
        } else {
            const auto& item
                = static_cast<const MaxSDK::Graphics::GeometryRenderItemHandle&>(renderItem);
            if (!item.IsValid()) {
                return nullptr;
            }
            iRenderGeom = item.GetRenderGeometry();
        }
    }
    return static_cast<MaxSDK::Graphics::SimpleRenderGeometry*>(iRenderGeom.GetPointer());
}

bool Point3ArraysAreAlmostEqual(Point3* array1, int size1, Point3* array2, int size2)
{
    const float epsilon = 1.e-06F;

    if (size1 != size2) {
        return false;
    }
    for (int i = 0; i < size1; ++i) {
        if (!array1[i].Equals(array2[i])) {
            return false;
        }
    }
    return true;
}

int GetVertexCount(const MaxSDK::Graphics::RenderItemHandle& renderItem, bool decorated)
{
    const auto pointsBuffer = GetRenderItemGeometry(renderItem, decorated)
                                  ->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
    return static_cast<int>(pointsBuffer.GetNumberOfVertices());
}

int GetTriCount(const MaxSDK::Graphics::RenderItemHandle& renderItem, bool decorated)
{
    const auto indices = GetRenderItemGeometry(renderItem, decorated)->GetIndexBuffer();
    return static_cast<int>(indices.GetNumberOfIndices() / 3);
}

Box3 GetBoundingBox(
    const MaxSDK::Graphics::RenderItemHandle& renderItem,
    bool                                      decorated,
    Matrix3*                                  tm)
{
    auto points = GetRenderItemGeometry(renderItem, decorated)
                      ->GetVertexBuffer(HdMaxRenderData::PointsBuffer);
    auto rawPoints = reinterpret_cast<Point3*>(points.Lock(0, 0, MaxSDK::Graphics::ReadAcess));
    Box3 bbox;
    bbox.IncludePoints(rawPoints, static_cast<int>(points.GetNumberOfVertices()), tm);
    points.Unlock();
    return bbox;
}

// Custom box compare, math has changed across some usd versions (21.11 -> 22.11) and we cant be
// too precise in the comparison(epsilon 0.001).
bool BoundingBoxesAreEquivalent(const Box3& box1, const Box3& box2)
{
    const float epsilon = 0.001f;
    return abs(box1.pmax.x - box2.pmax.x) < epsilon && abs(box1.pmax.y - box2.pmax.y) < epsilon
        && abs(box1.pmax.z - box2.pmax.z) < epsilon && abs(box1.pmin.x - box2.pmin.x) < epsilon
        && abs(box1.pmin.y - box2.pmin.y) < epsilon && abs(box1.pmin.z - box2.pmin.z) < epsilon;
};