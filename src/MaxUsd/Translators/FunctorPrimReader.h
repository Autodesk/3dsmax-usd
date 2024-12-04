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
#pragma once

#include "PrimReader.h"
#include "PrimReaderRegistry.h"
#include "ReadJobContext.h"

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>

#include <pxr/pxr.h>

#include <functional>

PXR_NAMESPACE_OPEN_SCOPE

/// \class MaxUsd_FunctorPrimReader
/// \brief This class is scaffolding to hold bare prim reader functions.
///
/// It is used by the PXR_MAXUSD_DEFINE_READER macro.
class MaxUsd_FunctorPrimReader final : public MaxUsdPrimReader
{
public:
    MaxUsd_FunctorPrimReader(
        const UsdPrim&,
        MaxUsdReadJobContext&,
        MaxUsdPrimReaderRegistry::ReaderFn);

    bool Read() override;

    static MaxUsdPrimReaderSharedPtr
    Create(const UsdPrim&, MaxUsdReadJobContext&, MaxUsdPrimReaderRegistry::ReaderFn readerFn);

    static MaxUsdPrimReaderRegistry::ReaderFactoryFn
    CreateFactory(MaxUsdPrimReaderRegistry::ReaderFn readerFn);

private:
    MaxUsdPrimReaderRegistry::ReaderFn readerFn;
};

PXR_NAMESPACE_CLOSE_SCOPE
