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
#include "MeshWriter.h"

#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/Translators/primWriter.h>
#include <MaxUsd/Translators/writeJobContext.h>

#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdMeshWriter::MaxUsdMeshWriter(const MaxUsdWriteJobContext& jobCtx, INode* node)
    : MaxUsdPrimWriter(jobCtx, node)
{
}

MaxUsdPrimWriter::ContextSupport
MaxUsdMeshWriter::CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    if (!exportArgs.GetTranslateMeshes()) {
        return ContextSupport::Unsupported;
    }
    const auto object = node->EvalWorldState(exportArgs.GetResolvedTimeConfig().GetStartTime()).obj;
    if (object->CanConvertToType({ TRIOBJ_CLASS_ID, 0 })
        && object->SuperClassID() != SHAPE_CLASS_ID) {
        return ContextSupport::Fallback;
    }
    return ContextSupport::Unsupported;
}

MaxUsd::XformSplitRequirement MaxUsdMeshWriter::RequiresXformPrim()
{
    if (!GetExportArgs().GetAllowNestedGprims() && GetNode()->NumberOfChildren() > 0) {
        return MaxUsd::XformSplitRequirement::Always;
    }
    return !GetExportArgs().GetMeshConversionOptions().GetBakeObjectOffsetTransform()
        ? MaxUsd::XformSplitRequirement::ForOffsetObjects
        : MaxUsd::XformSplitRequirement::Never;
}

bool MaxUsdMeshWriter::Write(
    UsdPrim&                  targetPrim,
    bool                      applyOffsetTransform,
    const MaxUsd::ExportTime& time)
{
    INode* sourceNode = GetNode();

    const auto timeConfig = GetExportArgs().GetResolvedTimeConfig();

    // Currently, 3dsMax Shapes (for example, splines), are converted to poly prior to export. This
    // may not always give the best results. Until we can provide smarter results, log a warning
    // (only once, on the first frame).
    if (time.IsFirstFrame()) {
        const auto object = sourceNode->EvalWorldState(time.GetMaxTime()).obj;
        if (object->SuperClassID() == SHAPE_CLASS_ID) {
            MaxUsd::Log::Warn(
                L"{0} is a Shape, it will be converted to Poly prior to export.",
                sourceNode->GetName());
        }
    }
    MaxUsd::MeshConverter meshConverter;
    pxr::UsdGeomMesh      prim = meshConverter.ConvertToUSDMesh(
        sourceNode,
        targetPrim.GetStage(),
        targetPrim.GetPrimPath(),
        GetExportArgs().GetMeshConversionOptions(),
        applyOffsetTransform,
        timeConfig.IsAnimated(),
        time);
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE