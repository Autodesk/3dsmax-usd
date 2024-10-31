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

#include <MaxUsd/Translators/primWriter.h>

#include <pxr/usd/usdSkel/topology.h>

PXR_NAMESPACE_OPEN_SCOPE

class UsdSkelSkeleton;
class UsdSkelAnimation;

class MaxUsdSkeletonWriter : public MaxUsdPrimWriter
{
public:
    static ContextSupport CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs);

    MaxUsdSkeletonWriter(const MaxUsdWriteJobContext& jobCtx, INode* node);

    bool
    Write(UsdPrim& targetPrim, bool applyOffsetTransform, const MaxUsd::ExportTime& time) override;

    MaxUsd::XformSplitRequirement RequiresXformPrim() override
    {
        return MaxUsd::XformSplitRequirement::Always;
    }

    MaxUsd::InstancingRequirement RequiresInstancing() override
    {
        // Temporarily disable instancing for prims used as bones.
        // Indeed, we need to call the Write() method on each bone instance to properly
        // configure UsdSkel Prims.
        return MaxUsd::InstancingRequirement::NoInstancing;
    }

    TfToken GetObjectPrimSuffix() override { return TfToken("Bone"); }

    TfToken GetPrimType() override { return MaxUsdPrimTypeTokens->Xform; }

    WStr GetWriterName() override { return L"Skeleton writer"; }

    Interval GetValidityInterval(const TimeValue& time) override;

private:
    // Inverse of GetJobContext().GetNodesToPrimsMap(), kept here to avoid recomputing it every
    // frame.
    std::unordered_map<pxr::SdfPath, INode*, pxr::SdfPath::Hash> primsToNodes;
    // Cache the joinOrder and topology.
    VtTokenArray    currentSkelJointsOrder;
    UsdSkelTopology topo;
    bool            hasSkinModDependency = false;
};

PXR_NAMESPACE_CLOSE_SCOPE
