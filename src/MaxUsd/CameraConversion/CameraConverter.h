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
#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/MaxUSDAPI.h>
#include <MaxUsd/Translators/ReadJobContext.h>

#include <pxr/usd/usdGeom/camera.h>

#include <Scene/IPhysicalCamera.h>

namespace MAXUSD_NS_DEF {

/// Camera Conversion utility Class
class MaxUSDAPI CameraConverter
{
public:
    static void ToPhysicalCamera(
        const pxr::UsdGeomCamera&        usdCamera,
        MaxSDK::IPhysicalCamera*         maxCamera,
        const pxr::MaxUsdReadJobContext& readContext);
};

} // namespace MAXUSD_NS_DEF
