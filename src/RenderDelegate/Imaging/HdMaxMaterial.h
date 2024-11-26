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
#include "HdMaxRenderDelegate.h"

#include <pxr\imaging\hd\material.h>

PXR_NAMESPACE_OPEN_SCOPE

class HdMaxMaterial : public pxr::HdMaterial
{
public:
    /**
     * \brief Constructor.
     * \param delegate The render delegate.
     * \param id The material's path.
     */
    HdMaxMaterial(HdMaxRenderDelegate* delegate, const pxr::SdfPath& id)
        : HdMaterial(id)
        , renderDelegate { delegate }
    {
    }

    /**
     * Destructor.
     */
    ~HdMaxMaterial();

    /**
     * \brief Synchronize any changes to the material, with the 3dsMax representations. The actual 3dsMax
     * materials are built later, on the main thead.
     * \param sceneDelegate The scene delegate.
     * \param dirtyBits The material's dirty bits.
     */
    void Sync(
        pxr::HdSceneDelegate* sceneDelegate,
        pxr::HdRenderParam* /*renderParam*/,
        pxr::HdDirtyBits* dirtyBits) override;

    /**
     * \brief Returns the initial dirty bits.
     * \return The dirty bits.
     */
    pxr::HdDirtyBits GetInitialDirtyBitsMask() const override;

    /**
     * \brief Subscribes a rPrim so it can be notified when the material changes.
     * \param rprimId The path of the rPrim to subscribe.
     */
    void SubscribeForMaterialUpdates(const SdfPath& rprimId);

    /**
     * \brief Unsubscribes a rPrim to stop it being notified when the material changes.
     * \param rprimId The path of the rPrim to unsubscribe.
     */
    void UnsubscribeFromMaterialUpdates(const SdfPath& rprimId);

private:
    HdMaxRenderDelegate* renderDelegate;
    std::mutex           materialSubscriptionsMutex;
    std::set<SdfPath>    materialSubscriptions;
};

PXR_NAMESPACE_CLOSE_SCOPE