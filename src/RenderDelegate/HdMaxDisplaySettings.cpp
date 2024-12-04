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
#include "HdMaxDisplaySettings.h"

#include "HdMaxColorMaterial.h"

HdMaxDisplaySettings::HdMaxDisplaySettings() { }

void HdMaxDisplaySettings::SetDisplayMode(
    DisplayMode           displayMode,
    pxr::HdChangeTracker& changeTracker)
{
    if (this->displayMode != displayMode) {
        // Dirty primvars, but also material assignment - both might be impacted (for example, no
        // need for uvs if using colors).
        changeTracker.MarkAllRprimsDirty(
            pxr::HdChangeTracker::DirtyPrimvar | pxr::HdChangeTracker::DirtyMaterialId);
    }
    this->displayMode = displayMode;
}

HdMaxDisplaySettings::DisplayMode HdMaxDisplaySettings::GetDisplayMode() const
{
    return displayMode;
}

const MaxSDK::Graphics::StandardMaterialHandle&
HdMaxDisplaySettings::GetWireColorMaterial(bool instanced) const
{
    return instanced ? wireColorInstancedHandle : wireColorHandle;
}

void HdMaxDisplaySettings::SetWireColor(Color wireColor, pxr::HdChangeTracker& changeTracker)
{
    if (this->wireColor == wireColor) {
        return;
    }
    this->wireColor = wireColor;
    changeTracker.MarkAllRprimsDirty(pxr::HdChangeTracker::DirtyMaterialId);
    wireColorHandle = HdMaxColorMaterial::Get(wireColor, false);
    wireColorInstancedHandle = HdMaxColorMaterial::Get(wireColor, true);
}

bool HdMaxDisplaySettings::operator==(const HdMaxDisplaySettings& settings) const
{
    return this->displayMode == settings.displayMode
        && this->wireColorHandle == settings.wireColorHandle
        && this->wireColorInstancedHandle == settings.wireColorInstancedHandle
        && this->wireColor == settings.wireColor;
}