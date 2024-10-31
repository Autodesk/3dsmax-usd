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

#include "MeshWriter.h"

#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/skeleton.h>

PXR_NAMESPACE_OPEN_SCOPE

class UsdSkelBlendShape;
class UsdSkelInbetweenShape;

/*
 * MorpherProperties is a helper struct to cache morpher data.
 *
 * By default, Max's morpher weights are from 0 (minLimit) to 100 (maxLimit), while Usd's
 * Blendshapes are from 0 to 1. Thus, we need to remap Max values into Usd by dividing Max's values
 * by delta between the min and maximum weight (deltaLimit). Note: Both Max and Usd allows the
 * weight to go beyond the max limit (useLimit). Meaning that a weight of 115 in Max can be
 * translated to 1.15 in usd if the property 'Use Limits' is turned off in the morpher modifier.
 */
struct MorpherProperties
{
    Modifier* morpher = nullptr;
    float     minLimit = 0.f;
    float     maxLimit = 100.f;
    int       useLimits = 1;
};

class MaxUsdSkinMorpherWriter : public MaxUsdMeshWriter
{
public:
    static ContextSupport CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs);

    MaxUsdSkinMorpherWriter(const MaxUsdWriteJobContext& jobCtx, INode* node);

    bool
    Write(UsdPrim& targetPrim, bool applyOffsetTransform, const MaxUsd::ExportTime& time) override;

    bool PostExport(UsdPrim& targetPrim) override;

    TfToken GetPrimType() override { return MaxUsdPrimTypeTokens->Mesh; }

    WStr GetWriterName() override { return L"Skin and Morpher writer"; }

    Interval GetValidityInterval(const TimeValue& time) override;

private:
    // Auxiliar function that disables all modifiers on top of the skin modifer, then writes the
    // node mesh
    pxr::UsdGeomMesh DisabledModsAndWriteMeshData(
        INode*                    node,
        UsdStageWeakPtr           stage,
        const pxr::SdfPath&       primPath,
        bool                      applyOffsetTransform,
        const MaxUsd::ExportTime& time,
        bool                      writeWarning = true);

    static UsdSkelBlendShape CreateBlendShape(
        const UsdGeomMesh&  sourceMesh,
        const UsdGeomMesh&  targetMesh,
        const std::wstring& name);
    void CreateInBetweens(
        INode*                   sourceNode,
        const UsdGeomMesh&       sourceMeshPrim,
        int                      morpherIndex,
        const UsdSkelBlendShape& blendShape,
        TimeValue                startTime);

    // For each export time, it appends the animation prim the weight based on the ParamBlock*
    void
    WriteMorphWeightAnimations(const UsdPrim& targetPrim, const MaxUsd::ExportTime& time) const;

    // Auxiliar function get the morpher weight at the given time considering it's limit
    float GetMorpherWeightAtTime(IParamBlock* pb, const TimeValue& timeValue) const;

    static void GetMorpherNames(INode* node, std::vector<std::wstring>& morpherNames);

    MorpherProperties     morpherProperties;
    pxr::UsdGeomMesh      skinnedMesh;
    pxr::UsdSkelSkeleton  skeleton;
    pxr::UsdSkelAnimation skelAnimation;
};

PXR_NAMESPACE_CLOSE_SCOPE
