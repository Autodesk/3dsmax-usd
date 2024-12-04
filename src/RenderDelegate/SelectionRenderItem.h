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
#include <Graphics/EffectHandle.h>
#include <Graphics/EffectInstanceHandle.h>
#include <Graphics/ICustomRenderItem.h>
#include <Graphics/IRenderGeometry.h>

class SelectionRenderItem : public MaxSDK::Graphics::ICustomRenderItem
{

public:
    /**
     * \brief Constructor
     * \param geom The geometry to be rendered in Display(). The selection highlight will be drawn using the same
     * geometry.
     * \param wireframe Whether this is a wireframe render item.
     */
    SelectionRenderItem(const MaxSDK::Graphics::IRenderGeometryPtr& geom, bool wireframe);

    /**
     * \brief Destructor
     */
    ~SelectionRenderItem() override;

    /**
     * \brief Draws the render item in the given context.
     * \param drawContext
     */
    void Display(MaxSDK::Graphics::DrawContext& drawContext) override;

    MaxSDK::Graphics::IRenderGeometryPtr GetRenderGeometry() const { return renderGeometry; }

    size_t GetPrimitiveCount() const override { return renderGeometry->GetPrimitiveCount(); }

    // Pure virtual. Needs an implementation.
    void Realize(MaxSDK::Graphics::DrawContext&) override { }

    /**
     * \brief Returns the Zbias used for selection highlighting. Zbias is useful to
     * make sure that the wireframe we display for selection, shows up over the geometry.
     * We need different biases for displaying selection over shaded or wireframe geometry.
     * Indeed, wireframe geometry has its own bias in 3dsmax..so we need to beat that.
     * \param viewExp View information. In 3dsMax, the bias is scaled from the view config, we need
     * to match that behavior.
     * \param wireFrame True if we are requesting the Zbias for wireframe geometry.
     * \return The ZBias to use.
     */
    static float GetSelectionZBias(ViewExp* viewExp, bool wireFrame);

private:
    MaxSDK::Graphics::IRenderGeometryPtr renderGeometry;
    bool                                 isWireframe = false;

    // Share a single selection effect accross all USD render items.
    static MaxSDK::Graphics::EffectHandle         selectionEffect;
    static MaxSDK::Graphics::EffectInstanceHandle selectionEffectInstance;

    static constexpr float wireZbias = 1.0;
    static constexpr float shadedZbias = 0.1f;
};