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
#include <MaxUsd/CameraConversion/CameraConverter.h>
#include <MaxUsd/Translators/PrimReader.h>
#include <MaxUsd/Translators/PrimReaderRegistry.h>
#include <MaxUsd/Translators/ReadJobContext.h>
#include <MaxUsd/Translators/TranslatorMaterial.h>
#include <MaxUsd/Translators/TranslatorPrim.h>
#include <MaxUsd/Translators/TranslatorXformable.h>

#include <pxr/usd/usdGeom/camera.h>

#include <Scene/IPhysicalCamera.h>

PXR_NAMESPACE_OPEN_SCOPE

// prim reader for mesh
class MaxUsdPrimReaderCamera final : public MaxUsdPrimReader
{
public:
    MaxUsdPrimReaderCamera(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx)
        : MaxUsdPrimReader(prim, jobCtx)
    {
    }

    ~MaxUsdPrimReaderCamera() override { }

    bool Read() override;
};

TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimReaderRegistry, UsdGeomCamera)
{
    MaxUsdPrimReaderRegistry::Register<UsdGeomCamera>(
        [](const UsdPrim& prim, MaxUsdReadJobContext& jobCtx) {
            return std::make_shared<MaxUsdPrimReaderCamera>(prim, jobCtx);
        });
}

bool MaxUsdPrimReaderCamera::Read()
{
    auto prim = GetUsdPrim();
    auto usdCamera = UsdGeomCamera(prim);
    if (!usdCamera) {
        return false;
    }

    MaxSDK::IPhysicalCamera* maxCamera
        = static_cast<MaxSDK::IPhysicalCamera*>(GetCOREInterface17()->CreateInstance(
            CAMERA_CLASS_ID, MaxSDK::IPhysicalCamera::GetClassID()));
    if (maxCamera == nullptr) {
        return false;
    }

    MaxUsd::CameraConverter::ToPhysicalCamera(usdCamera, maxCamera, GetJobContext());

    auto createdNode = MaxUsdTranslatorPrim::CreateAndRegisterNode(
        prim, maxCamera, prim.GetName(), GetJobContext());

    // position the node
    MaxUsdTranslatorXformable::Read(prim, createdNode, GetJobContext());

    // At the time of writing, "usdview" uses "main_cam" as the name of the default camera to use
    // when opening a USD file. For convenience, this uses the same convention in 3ds Max.
    //
    // In addition, the "usdview" documentation states that if multiple cameras are named
    // "main_cam", the one that will be used will effectively be random. In the case of the
    // importer, since we always explore the scene in a depth-first manner, the last camera with the
    // name "main_cam" will always be used.
    if (prim.GetName().GetString() == "main_cam") {
        GetCOREInterface17()->GetCurrentRenderView()->SetViewCamera(createdNode);
    }

    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE