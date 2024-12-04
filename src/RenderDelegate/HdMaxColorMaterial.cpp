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
#include "HdMaxColorMaterial.h"

#include <MaxUsd/Utilities/MathUtils.h>

std::map<std::tuple<double, double, double>, MaxSDK::Graphics::StandardMaterialHandle>
    HdMaxColorMaterial::cacheForSimpleGeometry;
std::map<std::tuple<double, double, double>, MaxSDK::Graphics::StandardMaterialHandle>
    HdMaxColorMaterial::cacheForInstances;

const float HdMaxColorMaterial::DiffuseFactor = 0.8f;
const float HdMaxColorMaterial::AmbientFactor = 0.2f;

const MaxSDK::Graphics::StandardMaterialHandle&
HdMaxColorMaterial::Get(const pxr::GfVec3f& color, bool forInstances)
{
    return Get(color[0], color[1], color[2], forInstances);
}

const MaxSDK::Graphics::StandardMaterialHandle&
HdMaxColorMaterial::Get(const Color& color, bool forInstances)
{
    return Get(color[0], color[1], color[2], forInstances);
}

const MaxSDK::Graphics::StandardMaterialHandle&
HdMaxColorMaterial::Get(float r, float g, float b, bool forInstances)
{
    // Round the RGB values to improve caching (dont want to miss out because of float
    // imprecision...)
    constexpr double precision = 0.0001;
    auto             red = MaxUsd::MathUtils::RoundToPrecision(r, precision);
    auto             green = MaxUsd::MathUtils::RoundToPrecision(g, precision);
    auto             blue = MaxUsd::MathUtils::RoundToPrecision(b, precision);

    auto colorKey = std::make_tuple(red, green, blue);

    // Get a reference to the cache we should use for materials.
    // We use two distinct caches for materials applied to instances VS simple geometry because of
    // an issue with the viewport instancing API. When building the instances, somehow this alters
    // the material in a way that breaks it for non-instanced geometry...For now we work around this
    // by making sure this doesn't happen...
    auto& colorMaterialCache = forInstances ? cacheForInstances : cacheForSimpleGeometry;

    auto it = colorMaterialCache.find(colorKey);
    if (it != colorMaterialCache.end()) {
        return it->second;
    }

    auto nitrousHandle = MaxSDK::Graphics::StandardMaterialHandle {};
    nitrousHandle.Initialize();
    nitrousHandle.SetDiffuse(GetDiffuseColor(r, g, b));
    nitrousHandle.SetAmbient(GetAmbientColor(r, g, b));
    const auto& inserted = colorMaterialCache.insert({ colorKey, nitrousHandle });
    return inserted.first->second;
}

Color HdMaxColorMaterial::GetDiffuseColor(const Color& source)
{
    return GetDiffuseColor(source.r, source.g, source.b);
}

Color HdMaxColorMaterial::GetDiffuseColor(double r, double g, double b)
{
    return Color(r * DiffuseFactor, g * DiffuseFactor, b * DiffuseFactor);
}

Color HdMaxColorMaterial::GetAmbientColor(const Color& source)
{
    return GetAmbientColor(source.r, source.g, source.b);
}

Color HdMaxColorMaterial::GetAmbientColor(double r, double g, double b)
{
    return Color(r * AmbientFactor, g * AmbientFactor, b * AmbientFactor);
}