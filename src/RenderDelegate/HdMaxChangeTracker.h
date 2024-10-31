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
class HdMaxChangeTracker
{
public:
    // Dirty bits for the nitrous representation of USD prims.
    // We need more granularity on the Max side than what is offered by HdChangeTracker dirty bits,
    // i.e. not only know that the data changed, but also whether the data size changed.
    enum : pxr::HdDirtyBits
    {
        Clean = 0,
        DirtyIndices = 1 << 0,
        DirtyIndicesSize = 1 << 1,
        DirtyPoints = 1 << 2,
        DirtyPointsSize = 1 << 3,
        DirtyNormals = 1 << 4,
        DirtyNormalsSize = 1 << 5,
        DirtyUvs = 1 << 6,
        DirtyUvsSize = 1 << 7,
        DirtyVertexColors = 1 << 8,
        DirtyVertexColorsSize = 1 << 9,
        DirtyTransforms = 1 << 10,
        DirtyTransformsSize = 1 << 11,
        DirtyVisibility = 1 << 12,
        DirtyMaterial = 1 << 13,
        DirtySelectionHighlight = 1 << 14,
        AllDirty = (1 << 15) - 1,
    };

    static bool CheckDirty(const pxr::HdDirtyBits& dirtyBits, const pxr::HdDirtyBits& dirtyFlag)
    {
        return (dirtyBits & dirtyFlag) != 0;
    }

    static void SetDirty(pxr::HdDirtyBits& dirtyBits, const pxr::HdDirtyBits& dirtyFlag)
    {
        dirtyBits |= dirtyFlag;
    }

    static void ClearDirtyBits(pxr::HdDirtyBits& dirtyBits)
    {
        dirtyBits = HdMaxChangeTracker::Clean;
    }
};