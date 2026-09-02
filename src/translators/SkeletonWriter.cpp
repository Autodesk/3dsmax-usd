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
        ReferenceTarget*             refTarget = static_cast<ReferenceTarget*>(sourceNode);
        MaxUsd::HasDependentSkinProc skinProc(refTarget);
        refTarget->DoEnumDependents(&skinProc);
        MaxUsd::HasDependentMorpherProc morpherProc(sourceNode);
        refTarget->DoEnumDependents(&morpherProc);

        // Export the bone geometry as a mesh (at the start time).
        // Use the TranslateMeshes option to decide when the node is a morpher and the preserve bone
        // meshes when the node is a bone node
        // TODO: spline will be erroneously exported as meshes if they are being used as bone for a
        // skin modifier in the scene.
        if ((GetExportArgs().GetTranslateMeshes() && morpherProc.hasDependentMorpher)
            || GetExportArgs().GetPreserveBoneMeshes()) {
            MaxUsd::MeshConverter meshConverter;
            UsdGeomMesh           prim = meshConverter.ConvertToUSDMesh(
                sourceNode,
                targetPrim.GetStage(),
                targetPrim.GetPrimPath(),
                GetExportArgs().GetMeshConversionOptions(),
                applyOffsetTransform,
                false,
                MaxUsd::ExportTime { time.GetMaxTime(), pxr::UsdTimeCode::Default(), true },
                GetExportArgs().GetTransformFormat());
        }

        // Set the prim as purpose "guide". That way it can easily be hidden later.
        const auto imageable = pxr::UsdGeomImageable(targetPrim);
        imageable.CreatePurposeAttr().Set(pxr::UsdGeomTokens->guide);

        // A node is considered a "morpher node" if it has a morpher modifier and has no dependent
        // skin modifier.
        isMorpherNode = morpherProc.hasDependentMorpher && skinProc.foundSkinsMod.empty();
    }

    // This Max node only depends on a Morpher modifier, no need to do anything else
    if (isMorpherNode) {
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

    // 3ds Max allows for mesh nodes to be used as bones. To improve the round-tripping when
    // exporting and reimporting USD data from 3ds Max, we export the joint name with the full path
    // from the skel root. When reimporting, it's possible to check if there's a mesh on that path
    // and use that mesh as the original bone.
    // If the user doesn't care about round-tripping, they can choose to simplify the joint paths
    // to only use the bone name.
    auto skelJointToken = GetExportArgs().GetSimplifyBonePaths()
        ? jointSubPath.GetAsToken()
        : skelPath.AppendPath(jointSubPath).GetAsToken();

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
        // Get the other bind transforms that were already there, in order to add the new one
        const UsdAttribute bindTransforms = skel.GetBindTransformsAttr();
        VtMatrix4dArray    bindTransformsArray;
        bindTransforms.Get(&bindTransformsArray);

        MaxUsd::HasDependentSkinProc skinProc(sourceNode);
        sourceNode->DoEnumDependents(&skinProc);

        // The Skeleton prim needs a binding transform for each bone in it.
        // Add the identity matrix for the cases where the bone is not referenced by a skin modifier
        const auto objectTransform = skinProc.foundSkinsMod.empty()
            ? GfMatrix4d(1)
            : MaxUsd::GetBindTransform(
                  MaxUsd::BindTransformElement::Bone,
                  sourceNode,
                  skinProc.foundSkinsMod[0],
                  isYUp,
                  GetExportArgs().GetMeshConversionOptions().GetBakeObjectOffsetTransform());
        bindTransformsArray.emplace_back(objectTransform);

        if (!skel.GetBindTransformsAttr().Set(bindTransformsArray)) {
            MaxUsd::Log::Error("Couldn't set Skeleton bind transform attribute for {} !", primName);
            return false;
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

    // Always export any bone when the include all bones option is on. Look past any modifiers
    // applied on the bone to find the base object, so that bones with modifiers are still
    // recognized as bones.
    if (exportArgs.GetIncludeAllBones()
        && MaxUsd::IsBoneObject(node->GetObjectRef()->FindBaseObject())) {
        return ContextSupport::Fallback;
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

bool MaxUsdSkeletonWriter::PostExport(UsdPrim& targetPrim)
{
    //The writers can't prevent the prims to be created on the stage.
    //The GetPreserveBoneMeshes option allows the user to not export the bone geometry.
    //We do that by removing the prims that were unnecessarily created.
    //The geometry for the bones are generally created with an xform + a mesh prim under it.
    //This method is called for the bone mesh prim, so we check if the parent is an xform with the
    //same name minus the suffix we are using for the bone prims. Then remove both prims.
    if (!GetExportArgs().GetPreserveBoneMeshes() && targetPrim.IsValid()) {
        if (auto stage = targetPrim.GetStage()) {
            auto targetPrimPath = targetPrim.GetPath();
            auto targetElementString = targetPrimPath.GetElementString();
            auto boneSuffix = "_" + GetObjectPrimSuffix().GetString();
            auto parentXformElement
                = targetElementString.substr(0, targetElementString.size() - boneSuffix.size());

            SdfPath parentPath;
            if (parentXformElement == targetPrim.GetParent().GetPath().GetElementString()) {
                parentPath = targetPrim.GetParent().GetPath();
            }

            return stage->RemovePrim(targetPrim.GetPath()) && stage->RemovePrim(parentPath);
        }
    }

    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
