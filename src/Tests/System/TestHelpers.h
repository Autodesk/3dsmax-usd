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
#include <RenderDelegate/HdMaxConsolidator.h>

#include <MaxUsd/Builders/MaxSceneBuilder.h>

#include <pxr/imaging/hd/tokens.h>

#include <Graphics/RenderItemHandleDecorator.h>
#include <Graphics/SimpleRenderGeometry.h>

class HdMaxEngine;

const Class_ID STAGE_CLASS_ID(0x24ce4724, 0x14d2486b);

// When comparing Point3 and Matrix3 values, use an epsilon a bit bigger than FLT_EPSILON,
// some float imprecision compounds..
const float MAX_FLOAT_EPSILON = 1E-4f;

// Used to expose protected methods from the MaxSceneBuilder, so they can be tested.
class MaxSceneBuilderTester : public MaxUsd::MaxSceneBuilder
{
public:
    MaxSceneBuilderTester()
        : MaxUsd::MaxSceneBuilder()
    {
    }
};

class MockUpdateDisplayContext : public MaxSDK::Graphics::UpdateDisplayContext
{
};
class MockUpdateNodeContext : public MaxSDK::Graphics::UpdateNodeContext
{
public:
    MockUpdateNodeContext() { this->GetRenderNode().Initialize(); }
};

class MockRenderItemDecoratorContainer : public MaxSDK::Graphics::IRenderItemContainer
{
public:
    size_t GetNumberOfRenderItems() const override { return renderItems.size(); }

    MaxSDK::Graphics::RenderItemHandle GetRenderItem(size_t i) const override
    {
        return renderItems[i];
    }

    const MaxSDK::Graphics::RenderItemHandleDecorator& At(size_t i) { return renderItems[i]; }

    void AddRenderItem(const MaxSDK::Graphics::RenderItemHandle& pRenderItem) override
    {
        const auto renderItem
            = dynamic_cast<const MaxSDK::Graphics::RenderItemHandleDecorator&>(pRenderItem);
        renderItems.push_back(renderItem);
    }

    void AddRenderItems(const IRenderItemContainer& renderItemContainer) override
    {
        for (int i = 0; i < renderItemContainer.GetNumberOfRenderItems(); ++i) {
            AddRenderItem(renderItemContainer.GetRenderItem(i));
        }
    }

    void RemoveRenderItem(size_t i) override { renderItems.erase(renderItems.begin() + i); }

    void ClearAllRenderItems() override { renderItems.clear(); }

private:
    std::vector<MaxSDK::Graphics::RenderItemHandleDecorator> renderItems;
};

class MockRenderItemContainer : public MaxSDK::Graphics::IRenderItemContainer
{
public:
    size_t GetNumberOfRenderItems() const override { return renderItems.size(); }

    MaxSDK::Graphics::RenderItemHandle GetRenderItem(size_t i) const override
    {
        return renderItems[i];
    }

    void AddRenderItem(const MaxSDK::Graphics::RenderItemHandle& renderItem) override
    {
        renderItems.push_back(renderItem);
    }

    void AddRenderItems(const IRenderItemContainer& renderItemContainer) override
    {
        for (int i = 0; i < renderItemContainer.GetNumberOfRenderItems(); ++i) {
            AddRenderItem(renderItemContainer.GetRenderItem(i));
        }
    }

    void RemoveRenderItem(size_t i) override { renderItems.erase(renderItems.begin() + i); }

    void ClearAllRenderItems() override { renderItems.clear(); }

private:
    std::vector<MaxSDK::Graphics::RenderItemHandle> renderItems;
};

class ViewMock : public View
{
public:
    Point2 ViewToScreen(Point3 p) override { return Point2 {}; }
};

fs::path GetTestDataPath();

void TestRender(
    pxr::UsdStageRefPtr                     stage,
    HdMaxEngine&                            engine,
    MaxSDK::Graphics::IRenderItemContainer& renderItems,
    const pxr::UsdTimeCode&                 timeCode,
    MultiMtl*                               multiMat = nullptr,
    const pxr::TfTokenVector&               reprs = { pxr::HdReprTokens->smoothHull },
    const HdMaxConsolidator::Config&        consolidationConfig = {});

/**
 * \brief Helper to fetch the render geometry from a render item. Used to validate render results.
 * Caller must specify if the render item is decorated (has an offset transform), and if it is a
 * render item also displaying selection (this uses a Custom render item internally)
 * \param renderItem The render item to fetch the geometry from.
 * \param decorated If the render item is decorated (with a transform)
 * \param selectionRenderItem If the render item displays selection highlighting
 * \return The render geometry (through a SimpleRenderGeometry).
 */
MaxSDK::Graphics::SimpleRenderGeometry* GetRenderItemGeometry(
    const MaxSDK::Graphics::RenderItemHandle& renderItem,
    bool                                      decorated = true,
    bool                                      selectionRenderItem = false);

bool Point3ArraysAreAlmostEqual(Point3* array1, int size1, Point3* array2, int size2);

int GetVertexCount(const MaxSDK::Graphics::RenderItemHandle& renderItem, bool decorated);

int GetTriCount(const MaxSDK::Graphics::RenderItemHandle& renderItem, bool decorated);

Box3 GetBoundingBox(
    const MaxSDK::Graphics::RenderItemHandle& renderItem,
    bool                                      decorated,
    Matrix3*                                  tm = nullptr);

bool BoundingBoxesAreEquivalent(const Box3& box1, const Box3& box2);