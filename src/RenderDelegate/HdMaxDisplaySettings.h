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

/**
 * \brief Holds 3dsMax viewport display settings for a USD Stage object.
 * Setters take in a changeTracker object, so that the appropriate bit(s) can be dirtied.
 * for the next render call.
 */
class RenderDelegateAPI HdMaxDisplaySettings
{
public:
    enum DisplayMode
    {
        WireColor,
        USDDisplayColor,
        USDPreviewSurface
    };

    HdMaxDisplaySettings();

    void        SetDisplayMode(DisplayMode displayMode, pxr::HdChangeTracker& changeTracker);
    DisplayMode GetDisplayMode() const;

    const MaxSDK::Graphics::StandardMaterialHandle& GetWireColorMaterial(bool instanced) const;
    void SetWireColor(Color wireColor, pxr::HdChangeTracker& changeTracker);

    bool operator==(const HdMaxDisplaySettings& displaySettings) const;

private:
    DisplayMode displayMode = USDPreviewSurface;

    // Wire Color materials. Keep one for regular data and a separate one for instanced data,
    // workaround for an issue with the instancing API which can break the material of regular
    // data if shared...
    MaxSDK::Graphics::StandardMaterialHandle wireColorHandle;
    MaxSDK::Graphics::StandardMaterialHandle wireColorInstancedHandle;
    Color                                    wireColor;
};
