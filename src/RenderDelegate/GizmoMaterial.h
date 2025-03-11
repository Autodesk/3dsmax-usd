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
#include <Graphics/HLSLMaterialHandle.h>

class GizmoMaterial
{
public:
    /// Used to retrieve the right gizmo material based on the situation.
    /// Note that there is no specific gizmo material for selected instances
    /// as we use the same material as regular selected geometry to display
    /// selection.
    enum GizmoMaterialType
    {
        Normal,
        Selected,
        Instanced
    };

    /**
     * Returns a material suitable for gizmos.
     * @param type The material type of the gizmo material we need (normal,
     * selected, instanced)
     * @return The gizmo material.
     */
    static MaxSDK::Graphics::BaseMaterialHandle Get(const GizmoMaterialType& type);

    /**
     * Checks if a given material is a gizmo material.
     * @param mtl The material to check.
     * @return True if the given material is a gizmo material, false otherwise.
     */
    static bool Check(const MaxSDK::Graphics::BaseMaterialHandle& mtl);

private:
    /// Materials used to draw gizmos in the viewport.
    static MaxSDK::Graphics::HLSLMaterialHandle gizmoMtl;
    static MaxSDK::Graphics::HLSLMaterialHandle gizmoMtlSelected;
    static MaxSDK::Graphics::HLSLMaterialHandle gizmoMtlInstanced;
};