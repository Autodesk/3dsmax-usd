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

#include "GizmoMaterial.h"

#include "HdMaxDisplayPreferences.h"
#include "resource.h"

#include <dllutilities.h>
#include <gfx.h>

// Gizmos when instanced, or selected, need to be rendered with different materials.
// These materials are initialized on first usage and remain valid for the entire
// 3dsmax session.
MaxSDK::Graphics::HLSLMaterialHandle GizmoMaterial::gizmoMtl;
MaxSDK::Graphics::HLSLMaterialHandle GizmoMaterial::gizmoMtlSelected;
MaxSDK::Graphics::HLSLMaterialHandle GizmoMaterial::gizmoMtlInstanced;

MaxSDK::Graphics::BaseMaterialHandle GizmoMaterial::Get(const GizmoMaterialType& type)
{
    // When running in network render mode, there is no graphics driver setup,
    // initializing shaders would crash, and we do not need them.
    if (GetCOREInterface()->IsNetworkRenderServer()) {
        return {};
    }

    if (type == Normal) {
        if (!gizmoMtl.IsValid()) {
            gizmoMtl.InitializeWithResource(
                IDR_GIZMO_SELECTION_SHADER, MaxSDK::GetHInstance(), L"SHADER");
            gizmoMtl.SetActiveTechniqueName(L"Gizmo");
            Point3 defLightColor = GetUIColor(COLOR_LIGHT_OBJ);
            Color  lightColor(defLightColor);
            gizmoMtl.SetFloat4Parameter(
                L"LineColor", { lightColor.r, lightColor.g, lightColor.b, 1.0f });
        }
        return gizmoMtl;
    }
    if (type == Selected) {
        if (!gizmoMtlSelected.IsValid()) {
            gizmoMtlSelected.InitializeWithResource(
                IDR_GIZMO_SELECTION_SHADER, MaxSDK::GetHInstance(), L"SHADER");
            gizmoMtl.SetActiveTechniqueName(L"Gizmo");
        }
        const auto& selColor = HdMaxDisplayPreferences::GetInstance().GetSelectionColor();
        gizmoMtlSelected.SetFloat4Parameter(
            L"LineColor", { selColor.r, selColor.g, selColor.b, selColor.a });

        return gizmoMtlSelected;
    }

    if (type == Instanced) {
        if (!gizmoMtlInstanced.IsValid()) {

            gizmoMtlInstanced.InitializeWithResource(
                IDR_GIZMO_SELECTION_SHADER, MaxSDK::GetHInstance(), L"SHADER");
            gizmoMtlInstanced.SetActiveTechniqueName(L"Gizmo_Instanced");
            Point3 defLightColor = GetUIColor(COLOR_LIGHT_OBJ);
            Color  lightColor(defLightColor);
            gizmoMtlInstanced.SetFloat4Parameter(
                L"LineColor", { lightColor.r, lightColor.g, lightColor.b, 1.0f });
        }
        return gizmoMtlInstanced;
    }
    DbgAssert(0 && L"Invalid gizmo material type.");
    return {}; // Invalid.
}

bool GizmoMaterial::Check(const MaxSDK::Graphics::BaseMaterialHandle& mtl)
{
    return mtl == Get(Normal) || mtl == Get(Selected) || mtl == Get(Instanced);
}