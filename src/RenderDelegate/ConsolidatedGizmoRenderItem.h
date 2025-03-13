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

#pragma once
#include <Graphics/EffectHandle.h>
#include <Graphics/ICustomRenderItem.h>
#include <Graphics/IRenderGeometry.h>

class ConsolidatedGizmoRenderItem : public MaxSDK::Graphics::ICustomRenderItem
{

public:
    /**
     * \brief Constructor
     * \param geom The geometry of the consolidated gizmo - to be rendered in Display().
     */
    ConsolidatedGizmoRenderItem(const MaxSDK::Graphics::IRenderGeometryPtr& geom);

    /**
     * \brief Destructor
     */
    ~ConsolidatedGizmoRenderItem() override;

    /**
     * \brief Draws the render item in the given context.
     * \param drawContext
     */
    void Display(MaxSDK::Graphics::DrawContext& drawContext) override;

    MaxSDK::Graphics::IRenderGeometryPtr GetRenderGeometry() const { return renderGeometry; }

    size_t GetPrimitiveCount() const override { return renderGeometry->GetPrimitiveCount(); }

    // Pure virtual. Needs an implementation.
    void Realize(MaxSDK::Graphics::DrawContext&) override { }

private:
    MaxSDK::Graphics::IRenderGeometryPtr renderGeometry = nullptr;
};