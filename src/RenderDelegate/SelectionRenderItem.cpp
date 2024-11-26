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

#include "SelectionRenderItem.h"

#include "HdMaxDisplayPreferences.h"
#include "resource.h"

#include <Graphics/BaseMaterialHandle.h>
#include <Graphics/ICamera.h>
#include <Graphics/IVirtualDevice.h>
#include <Graphics/RenderStates.h>

#include <GraphicsWindow.h>
#include <dllutilities.h>
#include <maxapi.h>

MaxSDK::Graphics::EffectHandle         SelectionRenderItem::selectionEffect;
MaxSDK::Graphics::EffectInstanceHandle SelectionRenderItem::selectionEffectInstance;
std::once_flag                         initSelectionEffect;

SelectionRenderItem::SelectionRenderItem(
    const MaxSDK::Graphics::IRenderGeometryPtr& geom,
    bool                                        wireframe)
    : renderGeometry { geom }
    , isWireframe { wireframe }
{
    // Initialize the effect, when the first usd render item is created.
    std::call_once(initSelectionEffect, [this] {
        selectionEffect.InitializeWithResource(
            IDR_PRIM_SELECTION_SHADER, MaxSDK::GetHInstance(), L"SHADER");
        selectionEffectInstance = selectionEffect.CreateEffectInstance();
    });
}

SelectionRenderItem::~SelectionRenderItem() { }

void SelectionRenderItem::Display(MaxSDK::Graphics::DrawContext& drawContext)
{
    if (nullptr == renderGeometry) {
        return;
    }

    using namespace MaxSDK::Graphics;

    IVirtualDevice& vd = drawContext.GetVirtualDevice();

    // First, render the geometry with the assigned material.
    BaseMaterialHandle& mtl = const_cast<BaseMaterialHandle&>(drawContext.GetMaterial());
    mtl.Activate(drawContext);
    const auto passCount = mtl.GetPassCount(drawContext);
    for (unsigned pass = 0; pass < passCount; ++pass) {
        mtl.ActivatePass(drawContext, pass);
        renderGeometry->Display(
            drawContext, 0, static_cast<int>(renderGeometry->GetPrimitiveCount()), 0);
    }
    mtl.PassesFinished(drawContext);
    mtl.Terminate();

    // Next, render selection highlighting. Selection is displayed using wireframe.
    // Selection highlighting uses the vertex color channel. Selected geometry will
    // have vertex colors set to 1.f, unselected geometries will have 0.f

    // First, configure the blend state, the selection wire color is configurable, and we allow
    // alpha.

    // Copy the current blend state, so we can set it back after we are done.
    const auto previousBlendState = vd.GetBlendState();

    // Setup alpha blending..
    BlendState alphaBlend;
    alphaBlend.GetTargetBlendState(0).SetBlendEnabled(true);
    alphaBlend.GetTargetBlendState(0).SetSourceBlend(BlendSelectorSourceAlpha);
    alphaBlend.GetTargetBlendState(0).SetDestinationBlend(BlendSelectorInvSourceAlpha);
    alphaBlend.GetTargetBlendState(0).SetColorBlendOperation(BlendOperationAdd);
    alphaBlend.GetTargetBlendState(0).SetAlphaSourceBlend(BlendSelectorZero);
    alphaBlend.GetTargetBlendState(0).SetAlphaDestinationBlend(BlendSelectorInvSourceAlpha);
    alphaBlend.GetTargetBlendState(0).SetAlphaBlendOperation(BlendOperationAdd);
    vd.SetBlendState(alphaBlend);

    // Setup the selection effect..
    if (!isWireframe) {
        selectionEffect.SetActiveTechniqueByName(L"Shaded");
    } else {
        selectionEffect.SetActiveTechniqueByName(L"Wire");
    }
    const auto& selColor = HdMaxDisplayPreferences::GetInstance().GetSelectionColor();
    selectionEffectInstance.SetFloatParameter(
        L"LineColor", Point4(selColor.r, selColor.g, selColor.b, selColor.a));

    // Configure the ZBias. This is so that our selection wireframe displays on top of other things.
    // We need to mimic what max does for things to work correctly when mixed with regular render
    // items. Need to const cast, ViewExp not const-correct.
    const auto viewXp = const_cast<ViewExp*>(drawContext.GetViewExp());
    const auto scaledZbias = GetSelectionZBias(viewXp, isWireframe);
    selectionEffectInstance.SetFloatParameter(L"ZBias", scaledZbias);

    // Apply the effect and draw!
    selectionEffect.Activate(drawContext);
    selectionEffectInstance.Apply(drawContext);
    selectionEffect.ActivatePass(drawContext, 0); // We only have 1 pass.
    renderGeometry->Display(
        drawContext, 0, static_cast<int>(renderGeometry->GetPrimitiveCount()), 0);
    selectionEffect.PassesFinished(drawContext);
    selectionEffect.Terminate();

    // Set back the blend state to what it was.
    vd.SetBlendState(previousBlendState);
}

float SelectionRenderItem::GetSelectionZBias(ViewExp* viewExp, bool wireFrame)
{
    // To make sure that we display our selection highlighting wireframe over both
    // shaded and wireframe geometry, we need to configure a ZBias, and scale it like
    // 3dsMax does, based on view parameters.

    const auto baseBias = wireFrame ? wireZbias : shadedZbias;
    if (!viewExp) {
        return baseBias;
    }

    GraphicsWindow* gw = viewExp->getGW();

    using namespace MaxSDK::Graphics;
    Matrix3  viewMatrixInv;
    Matrix44 viewProjectionMatrix;
    int      perspective;
    float    hither, yon;
    gw->getCameraMatrix(viewProjectionMatrix.m, &viewMatrixInv, &perspective, &hither, &yon);

    Matrix44 viewMatrixInv44;
    MaxWorldMatrixToMatrix44(viewMatrixInv44, viewMatrixInv);
    Matrix44 projectionMatrix;
    Matrix44::Multiply(projectionMatrix, viewMatrixInv44, viewProjectionMatrix);

    const CameraPtr camera = ICamera::Create();
    camera->SetProjectionMatrix(projectionMatrix);

    float scaleFactor;
    if (camera->IsPerspective()) {
        scaleFactor = projectionMatrix.m[3][2] * 0.001f;
    } else {
        constexpr float viewDefaultWidth = 400.0f;
        auto            zoom = viewExp->GetScreenScaleFactor(Point3(0.0f, 0.0f, 0.0f));
        zoom *= static_cast<float>(gw->getWinSizeX())
            / (static_cast<float>(gw->getWinSizeY()) * viewDefaultWidth);
        scaleFactor = std::min(0.000075f, zoom / (yon - hither));
    }
    const float scaledBias = scaleFactor * baseBias;
    return scaledBias;
}