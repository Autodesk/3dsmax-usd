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
#include "HdMaxMaterial.h"

PXR_NAMESPACE_OPEN_SCOPE

HdMaxMaterial::~HdMaxMaterial()
{
    auto materialCollection = renderDelegate->GetMaterialCollection();
    // Hydra prims are destroyed on the main thread.
    materialCollection->RemoveMaterial(GetId());
}

void HdMaxMaterial::Sync(
    pxr::HdSceneDelegate* sceneDelegate,
    pxr::HdRenderParam*,
    pxr::HdDirtyBits* dirtyBits)
{
    if (*dirtyBits & (HdMaterial::DirtyResource | HdMaterial::DirtyParams)) {
        // Update the material.
        auto materialCollection = renderDelegate->GetMaterialCollection();
        materialCollection->UpdateMaterial(sceneDelegate, GetId());

        // Notify subscribed rPrims.
        std::lock_guard<std::mutex> lock(materialSubscriptionsMutex);
        HdChangeTracker& changeTracker = sceneDelegate->GetRenderIndex().GetChangeTracker();
        for (const SdfPath& rprimId : materialSubscriptions) {
            changeTracker.MarkRprimDirty(rprimId, HdChangeTracker::DirtyMaterialId);
        }
    }
    *dirtyBits = HdMaterial::Clean;
}

HdDirtyBits HdMaxMaterial::GetInitialDirtyBitsMask() const { return HdMaterial::AllDirty; }

void HdMaxMaterial::SubscribeForMaterialUpdates(const SdfPath& rprimId)
{
    std::lock_guard<std::mutex> lock(materialSubscriptionsMutex);
    materialSubscriptions.insert(rprimId);
}

void HdMaxMaterial::UnsubscribeFromMaterialUpdates(const SdfPath& rprimId)
{
    std::lock_guard<std::mutex> lock(materialSubscriptionsMutex);
    materialSubscriptions.erase(rprimId);
}
PXR_NAMESPACE_CLOSE_SCOPE