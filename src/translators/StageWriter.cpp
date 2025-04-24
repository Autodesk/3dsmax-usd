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
#include "StageWriter.h"

#include <MaxUsd/Interfaces/IUSDStageProvider.h>
#include <MaxUsd/Translators/primWriter.h>
#include <MaxUsd/Translators/writeJobContext.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/mathUtils.h>

#include <pxr/base/tf/pathUtils.h>
#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdUtils/pipeline.h>

#include <Shlwapi.h>
#include <linshape.h>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdStageWriter::MaxUsdStageWriter(const MaxUsdWriteJobContext& jobCtx, INode* node)
    : MaxUsdPrimWriter(jobCtx, node)
{
}

MaxUsdPrimWriter::ContextSupport
MaxUsdStageWriter::CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    if (!exportArgs.GetUsdStagesAsReferences()) {
        return ContextSupport::Unsupported;
    }
    Class_ID   USDSTAGEOBJECT_CLASS_ID(0x24ce4724, 0x14d2486b);
    const auto object = node->EvalWorldState(exportArgs.GetResolvedTimeConfig().GetStartTime()).obj;
    return object->ClassID() == USDSTAGEOBJECT_CLASS_ID ? ContextSupport::Fallback
                                                        : ContextSupport::Unsupported;
}

MaxUsd::XformSplitRequirement MaxUsdStageWriter::RequiresXformPrim()
{
    const auto object
        = GetNode()->EvalWorldState(GetExportArgs().GetResolvedTimeConfig().GetStartTime()).obj;
    auto usdProviderInterface = object->GetInterface(IUSDStageProvider_ID);
    if (!usdProviderInterface) {
        return MaxUsd::XformSplitRequirement::ForOffsetObjects;
    }
    auto stageProvider = static_cast<MaxUsd::IUSDStageProvider*>(usdProviderInterface);
    const pxr::UsdStageWeakPtr referencedStage = stageProvider->GetUSDStage();
    auto stageRootTransform = MaxUsd::GetStageAxisAndUnitRootTransform(referencedStage);
    return !MaxUsd::MathUtils::IsIdentity(stageRootTransform)
        ? MaxUsd::XformSplitRequirement::Always
        : MaxUsd::XformSplitRequirement::ForOffsetObjects;
}

MaxUsd::MaterialAssignRequirement MaxUsdStageWriter::RequiresMaterialAssignment()
{
    return MaxUsd::MaterialAssignRequirement::NoAssignment;
}

Interval MaxUsdStageWriter::GetValidityInterval(const TimeValue& time) { return FOREVER; }

bool MaxUsdStageWriter::Write(
    UsdPrim&                  targetPrim,
    bool                      applyOffsetTransform,
    const MaxUsd::ExportTime& time)
{
    INode* sourceNode = GetNode();
    // We need an Xformable Prim for any transform we might need apply before we reference in the
    // Stage's root layer, to account for different units or up-axis.
    pxr::UsdGeomXformable xformable { targetPrim };

    // Apply the offset transform if need be.

    // Get the stage from the USDStageObject
    auto stageProvider = static_cast<MaxUsd::IUSDStageProvider*>(
        sourceNode->GetObjectRef()->GetInterface(IUSDStageProvider_ID));
    pxr::UsdStageWeakPtr referencedStage = stageProvider->GetUSDStage();

    if (!referencedStage) {
        MaxUsd::Log::Warn(
            L"USD Stage Object {0} has no USD content loaded.", sourceNode->GetName());
        return true;
    }

    // If a population mask exists and contains valid paths (other than root '/'),
    // use the first path in the mask as the reference target, ignoring the default prim.
    // If the population mask contains only the root ('/'),
    // fallback to the default prim.
    // If no default prim is available, fallback to the first available root prim.
    auto       paths = referencedStage->GetPopulationMask().GetPaths();
    bool hasStageMask = false;
    auto       referencedPrim = referencedStage->GetDefaultPrim();
    const bool hasDefaultPrim = referencedPrim.IsValid();
    auto rootPrims = referencedStage->GetPrimAtPath(pxr::SdfPath::AbsoluteRootPath()).GetChildren();

    if (!paths.empty() && !paths[0].IsAbsoluteRootPath()) {
        bool hasRootPath = false;
        int  nonRootPathCount = 0;
        for (const auto& path : paths) {
            if (path.IsAbsoluteRootPath()) {
                hasRootPath = true;
                nonRootPathCount++;
                break;
            }
        }
        if (!hasRootPath && paths.size() == 1) {
            // If there is a stage mask with only one non-root path, use that prim path in the
            // reference.
            referencedPrim = referencedStage->GetPrimAtPath(paths[0]);
            hasStageMask = true;
        } else if (nonRootPathCount > 1) {
            // If there are multiple non-root paths, log a warning.
            MaxUsd::Log::Warn(L"Multiple non-root paths found in population mask, cannot "
                              L"choose between them.");
        }
    } else if (!hasDefaultPrim) {
        // Raise a warnings if no default prim is defined, we will fallback to the first
        // available
        // prim.
        MaxUsd::Log::Warn(
            L"No default Prim is defined on the root USD layer of {0}.", sourceNode->GetName());

        if (rootPrims.empty()) {
            MaxUsd::Log::Warn(
                L"Found no suitable Prim for referencing at the root layer of {0}. The USD "
                L"Stage "
                L"Object "
                L"will not be exported as a USD reference.",
                sourceNode->GetName());
            return true;
        }
        referencedPrim = rootPrims.front();
        MaxUsd::Log::Warn(
            "Using prim {0} as reference target.", referencedPrim.GetName().GetString());
    }
    // Raise a warning if more than one root primitives, or no default prim defined. Indeed, a
    // reference will only target one prim.
    const bool hasMultipleRootChildren
        = referencedStage->GetPrimAtPath(pxr::SdfPath::AbsoluteRootPath()).GetChildrenNames().size()
        > 1;
    if (hasMultipleRootChildren) {
        MaxUsd::Log::Warn(
            L"Multiple Prims exist at the root of the USD stage {0}. It is not possible to "
            L"properly create a "
            L"reference to this stage's root layer, as only a single Prim can be targeted ({1}).",
            sourceNode->GetName(),
            MaxUsd::UsdStringToMaxString(rootPrims.front().GetName().GetString()).data());
    }

    // Raise a warning if any in-memory changes on the stage. We do not support this yet.
    auto layers = referencedStage->GetLayerStack(true);
    for (const auto& layer : layers) {
        if (layer->IsDirty()) {
            MaxUsd::Log::Warn(
                L"The USD stage {0} had dirty layers. In memory changes will not be saved as part "
                L"of the "
                L"export of this USD Stage Object as a USD reference.",
                sourceNode->GetName());
            break;
        }
    }

    auto         pb = sourceNode->GetObjectRef()->GetParamBlock(0);
    const MCHAR* value = nullptr;
    Interval     valid;
    pb->GetValue(0 /*StageFile*/, GetCOREInterface()->GetTime(), value, valid);

    // For future safety, currently this should be caught upstream.
    if (!value) {
        MaxUsd::Log::Warn(L"The USD Stage {0} has no root layer defined.", sourceNode->GetName());
        return true;
    }
    // For future safety, currently this should be caught upstream.
    const std::string referenceFileName = MaxUsd::MaxStringToUsdString(value);
    if (MaxUsd::HasUnicodeCharacter(referenceFileName)) {
        MaxUsd::Log::Error(
            L"The root layer specified for USD Stage {0} contains illegal characters.",
            sourceNode->GetName());
        return true;
    }

    auto stage = targetPrim.GetStage();
    auto primPath = targetPrim.GetPath();
    // Create an override, which will hold a reference. Using an override enables us to target any
    // Prim type.
    pxr::UsdPrim overPrim = stage->OverridePrim(primPath.AppendChild(referencedPrim.GetName()));

    // Adjust for the referenced layer unit & up-axis setup.
    auto rootTransform = MaxUsd::GetStageAxisAndUnitRootTransform(referencedStage);
    xformable
        .AddXformOp(
            pxr::UsdGeomXformOp::TypeTransform,
            pxr::UsdGeomXformOp::PrecisionDouble,
            pxr::TfToken("axisAndUnitTransform"))
        .Set(rootTransform);

    auto       exportedFilename = this->GetJobContext().GetFilename();
    const bool isUsdzFormat = this->GetJobContext().IsUSDZFile();

    std::string referencePath = "";
    // Usdz should use absolute paths when exporting other usd references. There are scenarios in
    // which usdzip fails to resolve relative paths. To work around this, just use absolute path.
    // This doesn't change the end result, as those paths are retargeted to the usdz folder
    // structure
    if (isUsdzFormat) {
        MaxUsd::Log::Info(
            L"Exporting USDZ file, using an absolute path for the USD reference {0}",
            sourceNode->GetName());
        referencePath = referenceFileName;
    } else {
        wchar_t relativePath[MAX_PATH] = L"";
        if (PathRelativePathTo(
                relativePath,
                MaxUsd::UsdStringToMaxString(exportedFilename).data(),
                FILE_ATTRIBUTE_NORMAL,
                value,
                FILE_ATTRIBUTE_NORMAL)) {
            // Normalizing the path will strip the ./ from the relative path - as far as the OS is
            // concerned, that is fine. However, the PXR ArDefaultResolver, if not seeing a ./ will
            // also look into any defined search paths, having the ./ prefix will make it understand
            // that the relative path is anchored to the layer we are exporting. (see
            // ArDefaultResolver::SetDefaultSearchPath())
            referencePath = "./" + TfNormPath(MaxUsd::MaxStringToUsdString(relativePath));
        }
        // If building the relative path failed, use an absolute path (for example if the paths are
        // on different drives).
        else {
            MaxUsd::Log::Warn(
                L"Unable to create a relative path for the USD reference exported from {0}, using "
                L"an absolute path.",
                sourceNode->GetName());
            referencePath = TfNormPath(referenceFileName);
        }
    }

    if (hasDefaultPrim && !hasStageMask) {
        overPrim.GetReferences().AddReference(referencePath);
    } else {
        overPrim.GetReferences().AddReference(referencePath, referencedPrim.GetPath());
    }
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE