//
// Copyright 2018 Pixar
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
// © 2023 Autodesk, Inc. All rights reserved.
//
#include "FallbackPrimReader.h"

#include "TranslatorUtils.h"

#include <MaxUsd/Utilities/TranslationUtils.h>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsd_FallbackPrimReader::MaxUsd_FallbackPrimReader(
    const UsdPrim&        prim,
    MaxUsdReadJobContext& jobCtx)
    : MaxUsdPrimReader(prim, jobCtx)
{
}

bool MaxUsd_FallbackPrimReader::Read()
{
    const UsdPrim& usdPrim = GetUsdPrim();
    if (usdPrim.HasAuthoredTypeName() && !usdPrim.IsA<UsdGeomImageable>()) {
        // Only create fallback 3ds Max nodes for untyped prims or imageable prims
        // that have no prim reader.
        return false;
    }

    return MaxUsdTranslatorUtil::CreateDummyHelperNode(usdPrim, usdPrim.GetName(), GetJobContext());
}

/* static */
MaxUsdPrimReaderRegistry::ReaderFactoryFn MaxUsd_FallbackPrimReader::CreateFactory()
{
    return [](const UsdPrim& prim, MaxUsdReadJobContext& jobCtx) {
        return std::make_shared<MaxUsd_FallbackPrimReader>(prim, jobCtx);
    };
}

PXR_NAMESPACE_CLOSE_SCOPE
