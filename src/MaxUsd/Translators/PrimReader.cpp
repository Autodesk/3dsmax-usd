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
#include "PrimReader.h"

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdPrimReader::MaxUsdPrimReader(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx)
    : prim(prim)
    , readJobCtx(jobCtx)
{
}

/* static */
MaxUsdPrimReader::ContextSupport
MaxUsdPrimReader::CanImport(const MaxUsd::MaxSceneBuilderOptions&, const UsdPrim&)
{
    // Default value for all readers is Fallback. More specialized writers can
    // override the base CanImport to report Supported/Unsupported as necessary.
    return ContextSupport::Fallback;
}

const UsdPrim& MaxUsdPrimReader::GetUsdPrim() const { return prim; }

const MaxUsd::MaxSceneBuilderOptions& MaxUsdPrimReader::GetArgs() const
{
    return readJobCtx.GetArgs();
}

MaxUsdReadJobContext& MaxUsdPrimReader::GetJobContext() const { return readJobCtx; }

PXR_NAMESPACE_CLOSE_SCOPE
