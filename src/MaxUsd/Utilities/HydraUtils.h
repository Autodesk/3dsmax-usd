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

#include <MaxUsd.h>
#include <MaxUsd/MaxUSDAPI.h>

namespace MAXUSD_NS_DEF {
#if PXR_VERSION >= 2311

/**
 * Find the top-most "merging scene index" in scene index hierarchy.
 * @param base The filtering scene index to use as a starting point.
 * @return The merging scene index.
 */
MaxUSDAPI pxr::HdMergingSceneIndexRefPtr
          FindTopLevelMergingSceneIndex(const pxr::HdFilteringSceneIndexBaseRefPtr& base);
#endif

/**
 * Gets the effective scaling to apply on light gizmos to account for user configured scaling
 * and stage/scene units.
 * @param stageMeterPerUnits The stage units, in meters per unit.
 * @return The scaling to apply on gizmo meshes.
 */
MaxUSDAPI pxr::GfMatrix4d
          GetLightGizmoScaling(const double userScaling, const double stageMeterPerUnits);

MaxUSDAPI std::recursive_mutex& GetMaxSdkMutex();
} // namespace MAXUSD_NS_DEF
