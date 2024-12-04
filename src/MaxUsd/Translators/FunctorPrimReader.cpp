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
// © 2022 Autodesk, Inc. All rights reserved.
//
#include "FunctorPrimReader.h"

PXR_NAMESPACE_OPEN_SCOPE

MaxUsd_FunctorPrimReader::MaxUsd_FunctorPrimReader(
    const UsdPrim&                     prim,
    MaxUsdReadJobContext&              jobCtx,
    MaxUsdPrimReaderRegistry::ReaderFn readerFn)
    : MaxUsdPrimReader(prim, jobCtx)
    , readerFn(readerFn)
{
}

bool MaxUsd_FunctorPrimReader::Read() { return readerFn(GetUsdPrim(), GetArgs(), GetJobContext()); }

/* static */
MaxUsdPrimReaderSharedPtr MaxUsd_FunctorPrimReader::Create(
    const UsdPrim&                     prim,
    MaxUsdReadJobContext&              jobCtx,
    MaxUsdPrimReaderRegistry::ReaderFn readerFn)
{
    return std::make_shared<MaxUsd_FunctorPrimReader>(prim, jobCtx, readerFn);
}

/* static */
MaxUsdPrimReaderRegistry::ReaderFactoryFn
MaxUsd_FunctorPrimReader::CreateFactory(MaxUsdPrimReaderRegistry::ReaderFn readerFn)
{
    return [=](const UsdPrim& prim, MaxUsdReadJobContext& jobCtx) {
        return Create(prim, jobCtx, readerFn);
    };
}

PXR_NAMESPACE_CLOSE_SCOPE
