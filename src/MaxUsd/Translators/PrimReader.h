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
#pragma once

#include "ReadJobContext.h"

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>
#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

class MaxUsdPrimReader
{
public:
    MaxUSDAPI MaxUsdPrimReader(const UsdPrim&, MaxUsdReadJobContext&);
    virtual ~MaxUsdPrimReader() = default;

    /// The level of support a reader can offer for a given context
    ///
    /// A basic reader that gives correct results across most contexts should
    /// report `Fallback`, while a specialized reader that really shines in a
    /// given context should report `Supported` when the context is right and
    /// `Unsupported` if the context is not as expected.
    enum class ContextSupport
    {
        Supported,
        Fallback,
        Unsupported
    };

    /// This static function is expected for all prim readers and allows
    /// declaring how well this class can support the current context:
    MaxUSDAPI static ContextSupport
    CanImport(const MaxUsd::MaxSceneBuilderOptions& importArgs, const UsdPrim& importPrim);

    /// Reads the USD prim given by the prim reader args into a 3ds Max element
    /// The created element adds itself to the context
    /// Returns true if successful.
    MaxUSDAPI virtual bool Read() = 0;

    /// Whether this prim reader specifies a PostReadSubtree step.
    MaxUSDAPI virtual bool HasPostReadSubtree() const { return false; };

    /// An additional import step that runs after all descendants of this prim
    /// have been processed.
    /// For example, if we have prims /A, /A/B, and /C, then the import steps
    /// are run in the order:
    /// (1) Read A (2) Read B (3) PostReadSubtree B (4) PostReadSubtree A,
    /// (5) Read C (6) PostReadSubtree C
    MaxUSDAPI virtual void PostReadSubtree() {};

    /// Method called when a 3ds Max instance is created (cloned) from a Node which originally was
    /// created using this reader instance. Could be used to assign a material to this specific
    /// instance.
    /// \param prim Current instance prim
    /// \param instance Instanced 3ds Max Node
    MaxUSDAPI virtual void InstanceCreated(const UsdPrim& prim, INode* instance) {};

protected:
    /// The imported USD prim
    MaxUSDAPI const UsdPrim& GetUsdPrim() const;

    /// Import arguments
    MaxUSDAPI const MaxUsd::MaxSceneBuilderOptions& GetArgs() const;

    /// Get import job context
    MaxUSDAPI MaxUsdReadJobContext& GetJobContext() const;

private:
    const UsdPrim&        prim;
    MaxUsdReadJobContext& readJobCtx;
};

typedef std::shared_ptr<MaxUsdPrimReader> MaxUsdPrimReaderSharedPtr;

PXR_NAMESPACE_CLOSE_SCOPE
