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
#include "SkeletonWriter.h"

#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/Translators/WriteJobContext.h>
#include <MaxUsd/Utilities/MathUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>

#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/cache.h>
#include <pxr/usd/usdSkel/skeletonQuery.h>
#include <pxr/usd/usdSkel/utils.h>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdSkeletonWriter::MaxUsdSkeletonWriter(const MaxUsdWriteJobContext& jobCtx, INode* node)
    : MaxUsdPrimWriter(jobCtx, node)
{
}

bool MaxUsdSkeletonWriter::Write(
    UsdPrim&                  targetPrim,
    bool                      applyOffsetTransform,
    const MaxUsd::ExportTime& time)
{
    INode* sourceNode = GetNode();

    // Export the mesh itself and set it as guide. Only need to do so on the first frame we export.
    if (time.IsFirstFrame()) {
        // Export the bone geometry as a mesh (at the start time).
        // TODO: spline will be erroneously exported as meshes if they are being used as bone for a
        // skin modifier in the scene.
        if (GetExportArgs().GetTranslateMeshes()) {
            MaxUsd::MeshConverter meshConverter;
            UsdGeomMesh           prim = meshConverter.ConvertToUSDMesh(
                sourceNode,
                targetPrim.GetStage(),
                targetPrim.GetPrimPath(),
                GetExportArgs().GetMeshConversionOptions(),
                applyOffsetTransform,
                false,
                MaxUsd::ExportTime { time.GetMaxTime(), pxr::UsdTimeCode::Default(), true });
        }

        // Set the prim as purpose "guide". That way it can easily be hidden later.
        const auto imageable = pxr::UsdGeomImageable(targetPrim);
        imageable.CreatePurposeAttr().Set(pxr::UsdGeomTokens->guide);

        ReferenceTarget*             refTarget = static_cast<ReferenceTarget*>(sourceNode);
        MaxUsd::HasDependentSkinProc skinProc(refTarget);
        refTarget->DoEnumDependents(&skinProc);
        hasSkinModDependency = !skinProc.foundSkinsMod.empty();
    }

    // This Max node only depends on a Morpher modifier, no need to do anything else
    if (!hasSkinModDependency) {
        return true;
    }

    const UsdStagePtr                stage = targetPrim.GetStage();
    const SdfPath                    primPath = targetPrim.GetPath();
    const std::string                primName = targetPrim.GetName().GetString();
    const std::map<INode*, SdfPath>& nodesToPrims = GetJobContext().GetNodesToPrimsMap();

    auto nodesToPrimIt = nodesToPrims.find(sourceNode);
    if (nodesToPrimIt == nodesToPrims.end()) {
        MaxUsd::Log::Error(
            L"Node \"{}\" is required on export for SkelWriter for prim {} !",
            sourceNode->GetName(),
            MaxUsd::UsdStringToMaxString(primName).data());
        return false;
    }

    const SdfPath currentJointPath(nodesToPrims.at(sourceNode));
    const bool    isYUp = GetExportArgs().GetUpAxis() == MaxUsd::USDSceneBuilderOptions::UpAxis::Y;

    const SdfPath skelRootPath = MaxUsd::VerifyOrMakeSkelRoot(stage, primPath);
    if (skelRootPath.IsEmpty()) {
        MaxUsd::Log::Error("Couldn't Verify or Make SkelRoot path for prim {} !", primName);
        return false;
    }

    // get valid skel and animation prim by solving naming conflict if necessary
    const UsdSkelSkeleton skel(MaxUsd::VerifyOrMakePrimOfType<UsdSkelSkeleton>(
        stage, skelRootPath, GetExportArgs().GetBonesPrimName()));
    const SdfPath         skelPath = skel.GetPath();

    const UsdSkelAnimation anim(MaxUsd::VerifyOrMakePrimOfType<UsdSkelAnimation>(
        stage, skel.GetPath(), GetExportArgs().GetAnimationsPrimName()));

    const UsdSkelCache     skelCache;
    const UsdSkelAnimQuery animQuery = skelCache.GetAnimQuery(anim.GetPrim());
    const VtTokenArray     animJointsOrder = animQuery.GetJointOrder();

    // Remove the root "path" from the joints path to get the joint name
    const auto& jointSubPath = currentJointPath.MakeRelativePath(skelRootPath);
    if (jointSubPath.IsEmpty() || jointSubPath == SdfPath { "." }) {
        if (time.IsFirstFrame()) {
            MaxUsd::Log::Error(
                "Joint path {} is trying to use an invalid root path {}. Set a valid root prim to "
                "export UsdSkelRoot.",
                currentJointPath.GetAsString(),
                skelRootPath.GetAsString());
        }
        return false;
    }

    // Append the skel name to the beginning of each joint token for path reference when importing
    // This is necessary because the SkelAnimation prim can hold joint from several different
    // Skeleton prims and avoid naming collision.
    auto skelJointToken = skelPath.AppendPath(jointSubPath).GetAsToken();

    // When exporting the first frame, setup some time-independent properties.
    if (time.IsFirstFrame()) {
        currentSkelJointsOrder = skelCache.GetSkelQuery(skel).GetJointOrder();
        skel.CreatePurposeAttr().Set(pxr::UsdGeomTokens->guide);

        // Update the skel prim by adding then setting the newly added node.
        currentSkelJointsOrder.emplace_back(skelJointToken);
        if (!skel.GetJointsAttr().Set(currentSkelJointsOrder)) {
            MaxUsd::Log::Error("Error setting Skeleton joints attribute for {} !", primName);
            return false;
        }

        const UsdAttribute animJointAttribute = anim.GetJointsAttr();
        VtTokenArray       animTokensArray;
        animJointAttribute.Get(&animTokensArray);

        // Update anim prim by adding then setting the new joint.
        animTokensArray.emplace_back(skelJointToken);
        if (!anim.GetJointsAttr().Set(animTokensArray)) {
            MaxUsd::Log::Error("Error setting SkelAnimation joints attribute for {} !", primName);
            return false;
        }

        // Create relationship between animation and skel prim
        const UsdSkelBindingAPI binding = UsdSkelBindingAPI::Apply(skel.GetPrim());
        if (!binding.GetAnimationSourceRel().SetTargets({ anim.GetPath() })) {
            MaxUsd::Log::Error(
                "Couldn't set SkelAnimation {} relationship for {} !",
                anim.GetPath().GetAsString(),
                primName);
            return false;
        }

        // Invert the nodesToPrim map, as it will be useful later, do it here to only do it once on
        // the first frame.
        for (auto& e : nodesToPrims) {
            primsToNodes.insert({ e.second, e.first });
        }

        topo = { currentSkelJointsOrder };
    }

    // Next, write animatable properties.
    // If the joint has a parent, the joint transform is the relative transform from it.
    auto it
        = std::find(currentSkelJointsOrder.begin(), currentSkelJointsOrder.end(), skelJointToken);
    auto idx = std::distance(currentSkelJointsOrder.begin(), it);

    auto   parentIdx = topo.GetParent(idx);
    INode* parentNode = nullptr;
    if (parentIdx >= 0 && parentIdx < currentSkelJointsOrder.size()) {
        // need to rebuild the path to the parent node they aren't 1:1 anymore
        const auto parentJointSkel = pxr::SdfPath { currentSkelJointsOrder[parentIdx] };
        auto       parentJointSubPath = parentJointSkel.MakeRelativePath(skelPath);
        // remove the local reference to the skel that contains the joint then append the root node
        // to its path
        const auto& parentJointRootPath = skelRootPath.AppendPath(parentJointSubPath);
        auto        parentIt = primsToNodes.find(parentJointRootPath);
        if (parentIt != primsToNodes.end()) {
            parentNode = parentIt->second;
        } else {
            MaxUsd::Log::Error(
                "Unable to find associated 3dsMax node for {}", parentJointRootPath.GetString());
        }
    }

    const auto& timeVal = time.GetMaxTime();
    const auto& usdTimeCode = time.GetUsdTime();

    const auto        translationsAttr = anim.GetTranslationsAttr();
    pxr::VtVec3fArray translations;
    // Always get previous data when exporting to Usd Default time. When not exporting default time,
    // make sure it was authored on the given time. We don't want to get the interpolation from
    // previous frames.
    if (usdTimeCode == pxr::UsdTimeCode::Default()
        || MaxUsd::IsAttributeAuthored(translationsAttr, usdTimeCode)) {
        translationsAttr.Get(&translations, usdTimeCode);
    }

    auto              scalesAttr = anim.GetScalesAttr();
    pxr::VtVec3hArray scales;
    if (usdTimeCode == pxr::UsdTimeCode::Default()
        || MaxUsd::IsAttributeAuthored(scalesAttr, usdTimeCode)) {
        scalesAttr.Get(&scales, usdTimeCode);
    }

    auto              rotationsAttr = anim.GetRotationsAttr();
    pxr::VtQuatfArray rotations;
    if (usdTimeCode == pxr::UsdTimeCode::Default()
        || MaxUsd::IsAttributeAuthored(rotationsAttr, usdTimeCode)) {
        rotationsAttr.Get(&rotations, usdTimeCode);
    }

    auto nodeTransform = MaxUsd::GetNodeTransform(sourceNode, timeVal, isYUp);

    // Can't deal with non-invertible matrices.
    if (nodeTransform.GetDeterminant() == 0) {
        MaxUsd::Log::Warn(
            L"Node {0} has a non-invertible transform matrix, unable to properly use its transform "
            L"for UsdSkelAnimation joints. The identity transform will be used at UsdTimeCode {1}.",
            sourceNode->GetName(),
            std::to_wstring(usdTimeCode.GetValue()));
        nodeTransform.SetIdentity();
    }

    // We need to figure out the joint local transform.
    pxr::GfMatrix4d jointLocalTransform;
    jointLocalTransform.SetIdentity();

    if (parentNode) {
        pxr::GfMatrix4d parentTransform = MaxUsd::GetNodeTransform(parentNode, timeVal, isYUp);

        // UsdSkel join transforms do not deal with non-uniform scaling. Make sure, and enforce
        // that the parent transform we are using to compute the local joint transform, has uniform
        // scaling applied.
        if (MaxUsd::MathUtils::FixNonUniformScaling(parentTransform)) {
            MaxUsd::Log::Warn(
                L"Non-uniform scaling applied on parent bone {} at frame {}. A uniform "
                L"scaling (scaling average) will be used instead.",
                parentNode->GetName(),
                timeVal);
        }

        jointLocalTransform = nodeTransform * parentTransform.GetInverse();
    } else {
        // Otherwise, just use the transform as is.
        jointLocalTransform = nodeTransform;
    }

    // We will be using the mesh and bone positions on the first time as the rest pose.
    // Append the current rest pose to the end of the rest pose array.
    if (time.IsFirstFrame()) {
        // Setup the bind transform...

        MaxUsd::HasDependentSkinProc skinProc(sourceNode);
        sourceNode->DoEnumDependents(&skinProc);
        if (!skinProc.foundSkinsMod.empty()) {
            const auto objectTransform = MaxUsd::GetBindTransform(
                MaxUsd::BindTransformElement::Bone,
                sourceNode,
                skinProc.foundSkinsMod[0],
                isYUp,
                GetExportArgs().GetMeshConversionOptions().GetBakeObjectOffsetTransform());

            // Get the other bind transforms that were already there, in order to add the new one
            const UsdAttribute bindTransforms = skel.GetBindTransformsAttr();
            VtMatrix4dArray    bindTransformsArray;
            bindTransforms.Get(&bindTransformsArray);

            bindTransformsArray.emplace_back(objectTransform);
            if (!skel.GetBindTransformsAttr().Set(bindTransformsArray)) {
                MaxUsd::Log::Error(
                    "Couldn't set Skeleton bind transform attribute for {} !", primName);
                return false;
            }
        }

        // Setup the rest transform...

        const UsdAttribute restTransforms = skel.GetRestTransformsAttr();
        VtMatrix4dArray    restTransformsArray;
        restTransforms.Get(&restTransformsArray);
        restTransformsArray.emplace_back(jointLocalTransform);
        if (!skel.GetRestTransformsAttr().Set(restTransformsArray)) {
            MaxUsd::Log::Warn("Couldn't set Skeleton rest transform attribute for {} !", primName);
        }
    }

    // Also make sure we don't have non-uniform scaling in the joint transform itself.
    if (MaxUsd::MathUtils::FixNonUniformScaling(jointLocalTransform)) {
        MaxUsd::Log::Warn(
            "Non-uniform scaling applied on bone {} at frame {}. A uniform scaling "
            "(scaling average) will be used instead.",
            currentJointPath.GetString(),
            usdTimeCode.GetValue());
    }

    // Once we have, our joint local transform, decompose it to populate the UsdSkelAnimation
    // trans/scale/rotate attibutes.
    GfVec3f translation;
    GfQuatf rotation;
    GfVec3h scale;
    UsdSkelDecomposeTransform(jointLocalTransform, &translation, &rotation, &scale);

    scales.push_back(scale);
    translations.push_back(translation);
    rotations.push_back(rotation);

    scalesAttr.Set(scales, usdTimeCode);
    rotationsAttr.Set(rotations, usdTimeCode);
    translationsAttr.Set(translations, usdTimeCode);

    return true;
}

MaxUsdPrimWriter::ContextSupport
MaxUsdSkeletonWriter::CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    if (!exportArgs.GetTranslateSkin() && !exportArgs.GetTranslateMorpher()) {
        return ContextSupport::Unsupported;
    }

    const bool isBakedOffset = exportArgs.GetMeshConversionOptions().GetBakeObjectOffsetTransform();
    ReferenceTarget* refTarget = static_cast<ReferenceTarget*>(node);

    // any node can be used as a bone in skin modifier
    // check if the node is being used by any skin modifier in the scene
    MaxUsd::HasDependentSkinProc skinProc(refTarget);
    refTarget->DoEnumDependents(&skinProc);

    // if there's a dependent skin modifier node, then we should use this skel writer
    if (!skinProc.foundSkinsMod.empty()) {
        if (skinProc.foundSkinsMod.size() > 1) {
            const auto bindTm = MaxUsd::GetBindTransform(
                MaxUsd::BindTransformElement::Bone,
                node,
                skinProc.foundSkinsMod[0],
                exportArgs.GetUpAxis() == MaxUsd::USDSceneBuilderOptions::UpAxis::Y,
                isBakedOffset);
            for (ISkin* s : skinProc.foundSkinsMod) {
                if (bindTm
                    != MaxUsd::GetBindTransform(
                        MaxUsd::BindTransformElement::Bone,
                        node,
                        s,
                        exportArgs.GetUpAxis() == MaxUsd::USDSceneBuilderOptions::UpAxis::Y,
                        isBakedOffset)) {
                    MaxUsd::Log::Error(
                        L"Bone node {} has different bind transforms on the skin modifiers that is "
                        L"being "
                        L"used. This is not supported in USD and may produced undesired results",
                        node->GetName());
                    break;
                }
            }
        }

        return ContextSupport::Fallback;
    }

    MaxUsd::HasDependentMorpherProc morpherProc(node);
    refTarget->DoEnumDependents(&morpherProc);
    if (morpherProc.hasDependentMorpher) {
        return ContextSupport::Fallback;
    }

    return ContextSupport::Unsupported;
}

Interval MaxUsdSkeletonWriter::GetValidityInterval(const TimeValue& time)
{
    // Declare the export valid at this exact time only. We want the writer to be called into at
    // every frame, whatever the object's validity interval, as we are also working with transforms.
    return Interval(time, time);
}

PXR_NAMESPACE_CLOSE_SCOPE
