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

#include "RenderDelegateAPI.h"

#include <Graphics/StandardMaterialHandle.h>

class RenderDelegateAPI HdMaxColorMaterial
{
public:
    // Multiplier for diffuse and ambient colors, when building a nitrous material from RGB values.
    static const float DiffuseFactor;
    static const float AmbientFactor;

    // Get a nitrous material representing a color. Will only create a material if none exist
    // already for this color. Different materials are required depending on whether the material is
    // meant for instances or not (different caches are kept).
    static const MaxSDK::Graphics::StandardMaterialHandle&
    Get(const pxr::GfVec3f& color, bool forInstances);
    static const MaxSDK::Graphics::StandardMaterialHandle&
    Get(const Color& color, bool forInstances);
    static const MaxSDK::Graphics::StandardMaterialHandle&
    Get(float r, float g, float b, bool forInstances);

    // Helpers :

    // Get adjusted diffuse and ambient colors, from a source color.
    static Color GetDiffuseColor(const Color& source);
    static Color GetDiffuseColor(double r, double g, double b);
    static Color GetAmbientColor(const Color& source);
    static Color GetAmbientColor(double r, double g, double b);

private:
    static std::map<std::tuple<double, double, double>, MaxSDK::Graphics::StandardMaterialHandle>
        cacheForSimpleGeometry;
    static std::map<std::tuple<double, double, double>, MaxSDK::Graphics::StandardMaterialHandle>
        cacheForInstances;
};
