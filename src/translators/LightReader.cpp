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
#include <MaxUsd/Translators/PrimReaderRegistry.h>
#include <MaxUsd/Translators/TranslatorLight.h>

#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/sphereLight.h>

PXR_NAMESPACE_OPEN_SCOPE

PXR_MAXUSD_DEFINE_READER(UsdLuxCylinderLight, usdPrim, args, context)
{
    return MaxUsdTranslatorLight::Read(usdPrim, args, context);
}

PXR_MAXUSD_DEFINE_READER(UsdLuxRectLight, usdPrim, args, context)
{
    return MaxUsdTranslatorLight::Read(usdPrim, args, context);
}

PXR_MAXUSD_DEFINE_READER(UsdLuxSphereLight, usdPrim, args, context)
{
    return MaxUsdTranslatorLight::Read(usdPrim, args, context);
}

PXR_MAXUSD_DEFINE_READER(UsdLuxDiskLight, usdPrim, args, context)
{
    return MaxUsdTranslatorLight::Read(usdPrim, args, context);
}

PXR_NAMESPACE_CLOSE_SCOPE