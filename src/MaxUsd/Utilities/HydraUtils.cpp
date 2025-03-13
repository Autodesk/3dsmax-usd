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

#include "HydraUtils.h"

// Hydra rendering is heavily multi-threaded. The Sync() method called in HdMaxMesh and
// HdMaxBasisCurves is called from many threads - some calls related to the 3dsMax SDK, and the
// Graphics APIs must be protected with mutexes. Note that the Sync() function is reentrant, i.e. in
// some scenarios we can reenter this function on the same thread before exiting the first
// invocation - therefore we must use recursive mutexes, which allow this.
namespace {
std::recursive_mutex maxSdkMutex;
}

namespace MAXUSD_NS_DEF {

#if PXR_VERSION >= 2311
pxr::HdMergingSceneIndexRefPtr
FindTopLevelMergingSceneIndex(const pxr::HdFilteringSceneIndexBaseRefPtr& base)
{
    if (!base) {
        return nullptr;
    }

    if (auto mergingSceneIndex = pxr::TfDynamic_cast<pxr::HdMergingSceneIndexRefPtr>(base)) {
        return mergingSceneIndex;
    }

    for (auto& inputScene : base->GetInputScenes()) {
        if (auto mergingSceneIndex
            = pxr::TfDynamic_cast<pxr::HdMergingSceneIndexRefPtr>(inputScene)) {
            return mergingSceneIndex;
        }
        if (auto fsi = pxr::TfDynamic_cast<pxr::HdFilteringSceneIndexBaseRefPtr>(inputScene)) {
            if (auto mergingSceneIndex = FindTopLevelMergingSceneIndex(fsi)) {
                return mergingSceneIndex;
            }
        }
    }
    return nullptr;
}
#endif

pxr::GfMatrix4d GetLightGizmoScaling(const double userScaling, const double stageMeterPerUnits)
{
    pxr::GfMatrix4d userScalingMat;
    userScalingMat.SetScale(userScaling);

    // Account for the units of gizmo meshes on disk - assumed to be in inches.
    pxr::GfMatrix4d gizmoUnitsScale;
    gizmoUnitsScale.SetScale(0.0254 / stageMeterPerUnits);
    return userScalingMat * gizmoUnitsScale;
}

std::recursive_mutex& GetMaxSdkMutex() { return maxSdkMutex; }

} // namespace MAXUSD_NS_DEF
