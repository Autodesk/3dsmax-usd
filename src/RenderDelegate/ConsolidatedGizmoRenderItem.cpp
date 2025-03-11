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

#include "ConsolidatedGizmoRenderItem.h"

#include <Graphics/BaseMaterialHandle.h>
#include <Graphics/ICamera.h>

ConsolidatedGizmoRenderItem::ConsolidatedGizmoRenderItem(
    const MaxSDK::Graphics::IRenderGeometryPtr& geom)
    : renderGeometry { geom }
{
}

ConsolidatedGizmoRenderItem::~ConsolidatedGizmoRenderItem() { }

void ConsolidatedGizmoRenderItem::Display(MaxSDK::Graphics::DrawContext& drawContext)
{
    if (nullptr == renderGeometry) {
        return;
    }

    using namespace MaxSDK::Graphics;

    IVirtualDevice& vd = drawContext.GetVirtualDevice();

    // Simply render the geometry with the assigned material. We expect the gizmo
    // material to be set on render items meant to display gizmos. The only reason
    // we use this custom render item is to avoid issues with combining the HLSLMaterial
    // that we use for gizmos with GeometryRenderitem - in this case Nitrous forces
    // triangle primitives - which breaks display.
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
}