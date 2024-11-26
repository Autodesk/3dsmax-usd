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
#include "TranslatorSkel.h"

#include "TranslatorPrim.h"

#include <MaxUsd/Utilities/MathUtils.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>

#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <pxr/usd/usdSkel/skeletonQuery.h>
#include <pxr/usd/usdSkel/skinningQuery.h>
#include <pxr/usd/usdSkel/topology.h>
#include <pxr/usd/usdSkel/utils.h>

#include <GetCOREInterface.h>
#include <inode.h>
#include <iskin.h>
#include <modstack.h>
#include <simpobj.h>

PXR_NAMESPACE_OPEN_SCOPE
bool MaxUsdTranslatorSkel::CreateJointHierarchy(
    const UsdSkelSkeletonQuery& skelQuery,
    INode*                      hierarchyRootNode,
    MaxUsdReadJobContext&       context,
    std::vector<INode*>&        joints)
{
    return CreateJointsNodes(skelQuery, hierarchyRootNode, context, joints)
        && SetJointProperties(skelQuery, context, joints)
        && CopyJointsAnimations(skelQuery, hierarchyRootNode, context, joints);
}

bool MaxUsdTranslatorSkel::GetJointsNodes(
    const UsdSkelSkeletonQuery& skelQuery,
    const MaxUsdReadJobContext& context,
    std::vector<INode*>&        joints)
{
    auto jointsOrder = skelQuery.GetJointOrder();

    joints.clear();
    joints.reserve(jointsOrder.size());

    for (const TfToken& joint : jointsOrder) {
        const SdfPath jointPath = SdfPath(joint);
        if (INode* jointNode = context.GetMaxNode(jointPath, false)) {
            joints.push_back(jointNode);
        }
    }

    return true;
}

bool MaxUsdTranslatorSkel::ConfigureSkinModifier(
    const UsdSkelSkinningQuery& skinQuery,
    INode*                      skinnedNode,
    const MaxUsdReadJobContext& context,
    std::vector<INode*>&        skinningJoints,
    const VtMatrix4dArray&      bindTransforms)
{
    if (!skinnedNode) {
        MaxUsd::Log::Error(
            "Skel reader found Null Max node for skinned prim at path \"{}\" .",
            skinQuery.GetPrim().GetPath().GetName());
        return false;
    }

    if (skinningJoints.empty()) {
        MaxUsd::Log::Warn(
            "No joints bound to prim \"{}\". Skipping the creation of the SkinModifer.",
            skinQuery.GetPrim().GetPath().GetName());
        return false;
    }

    Modifier* skinMod = (Modifier*)CreateInstance(OSM_CLASS_ID, SKIN_CLASSID);
    if (!skinMod) {
        MaxUsd::Log::Error(
            "Skel reader couldn't create a new skin modifier for prim \"{}\" .",
            skinQuery.GetPrim().GetPath().GetName());
        return false;
    }

    const auto skinnedObj = skinnedNode->GetObjectRef()->FindBaseObject();
    GetCOREInterface12()->AddModifier(*skinnedNode, *skinMod);

    ISkin*           iskin = static_cast<ISkin*>(skinMod->GetInterface(I_SKIN));
    ISkinImportData* iskinImport
        = static_cast<ISkinImportData*>(skinMod->GetInterface(I_SKINIMPORTDATA));
    if (!iskin || !iskinImport) {
        MaxUsd::Log::Error(
            "Failed to extract import interface for skin modifier on prim \"{}\" .",
            skinQuery.GetPrim().GetPath().GetName());
        return false;
    }

    const auto stage = context.GetStage();
    const auto timeConfig = context.GetArgs().GetResolvedTimeConfig(stage);
    const auto maxStartTime
        = MaxUsd::GetMaxTimeValueFromUsdTimeCode(stage, timeConfig.GetStartTimeCode());

    // Set skinned node bind transform
    const auto numJoints = skinningJoints.size();
    GfMatrix4d geomBindTransform = skinQuery.GetGeomBindTransform();
    if (MaxUsd::IsStageUsingYUpAxis(context.GetStage())) {
        MaxUsd::MathUtils::ModifyTransformYToZUp(geomBindTransform);
    }

    // evaluate the object to make sure the local mod data gets built
    skinnedNode->EvalWorldState(maxStartTime);

    Matrix3 skinNodeTm;
    MaxSDK::Graphics::Matrix44ToMaxWorldMatrix(skinNodeTm, MaxUsd::ToMax(geomBindTransform));
    iskinImport->SetSkinTm(skinnedNode, skinNodeTm, skinNodeTm);

    //  add bones to the skin modifier
    for (size_t i = 0; i < numJoints; i++) {
        const auto boneNode = skinningJoints[i];
        if (!boneNode) {
            continue;
        }

        if (i == numJoints - 1) {
            iskinImport->AddBoneEx(boneNode, TRUE);
        } else {
            iskinImport->AddBoneEx(boneNode, FALSE);
        }

        auto jointTM = bindTransforms[i];
        if (MaxUsd::IsStageUsingYUpAxis(context.GetStage())) {
            MaxUsd::MathUtils::ModifyTransformYToZUp(jointTM);
        }

        Matrix3 boneNodeTm;
        MaxSDK::Graphics::Matrix44ToMaxWorldMatrix(boneNodeTm, MaxUsd::ToMax(jointTM));

        iskinImport->SetBoneTm(boneNode, boneNodeTm, boneNodeTm);
    }

    if (const ISkinContextData* skinData = iskin->GetContextInterface(skinnedNode)) {
        const auto numPoints = skinData->GetNumPoints();

        VtIntArray   jointsIndicesArray;
        VtFloatArray jointsWeightsArray;
        if (skinQuery.ComputeVaryingJointInfluences(
                numPoints, &jointsIndicesArray, &jointsWeightsArray)) {
            const auto numOfInfluences = skinQuery.GetNumInfluencesPerComponent();

            for (int pt = 0; pt < numPoints; ++pt) {
                // using arrays for ease of use, needs to be converted to Max's Tab later
                std::vector<float>  weights;
                std::vector<INode*> bones;
                for (int j = 0; j < numOfInfluences; ++j) {
                    const int index = pt * numOfInfluences + j;
                    if (index >= jointsIndicesArray.size()) {
                        break;
                    }

                    const int boneIndex = jointsIndicesArray[index];
                    if (boneIndex >= 0
                        && static_cast<unsigned int>(boneIndex) < skinningJoints.size()) {
                        const float weight = jointsWeightsArray[index];
                        INode*      bone = skinningJoints[boneIndex];

                        if (weight != 0.0f && bone) {
                            weights.emplace_back(weight);
                            bones.emplace_back(bone);
                        }
                    }
                }

                Tab<float>  weightsTab;
                Tab<INode*> bonesTab;
                weightsTab.Append(static_cast<int>(weights.size()), weights.data());
                bonesTab.Append(static_cast<int>(bones.size()), bones.data());
                iskinImport->AddWeights(skinnedNode, pt, bonesTab, weightsTab);
            }
        }
    }
    skinnedNode->NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
    skinnedObj->NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
    skinMod->NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);

    skinnedNode->EvalWorldState(maxStartTime);

    return true;
}

bool MaxUsdTranslatorSkel::CreateJointsNodes(
    const UsdSkelSkeletonQuery& skelQuery,
    INode*                      skelContainer,
    MaxUsdReadJobContext&       context,
    std::vector<INode*>&        joints)
{
    const SdfPath& path = skelQuery.GetPrim().GetPath();
    const SdfPath  containerPath = skelQuery.GetPrim().GetPath().GetParentPath();

    const VtTokenArray jointTokens = skelQuery.GetJointOrder();
    const size_t       numJoints = jointTokens.size();
    joints.resize(numJoints);

    auto&      topo = skelQuery.GetTopology();
    const auto coreInterface = GetCOREInterface17();
    for (size_t i = 0; i < numJoints; ++i) {
        const SdfPath jointPath = SdfPath(jointTokens[i]);
        if (!jointPath.IsPrimPath()) {
            continue;
        }

        // can't use the Helper function to create and register node here because it would use the
        // same prim
        const auto boneObject = static_cast<Object*>(
            coreInterface->CreateInstance(GEOMOBJECT_CLASS_ID, Class_ID(BONE_OBJ_CLASSID)));
        const WStr nodeName = MaxUsd::UsdStringToMaxString(jointPath.GetElementString());
        INode*     jointNode = coreInterface->CreateObjectNode(boneObject, nodeName);
        context.RegisterNewMaxRefTargetHandle(jointPath, jointNode);
        MaxUsd::Log::Info(
            "Bone node created for skel {0} and joint {1} with name {2}.",
            path.GetString(),
            jointPath.GetString(),
            jointPath.GetElementString());

        const int parentId = topo.GetParent(i);
        if (parentId >= 0 && parentId < joints.size()) {
            if (INode* parentNode = joints[parentId]) {
                parentNode->AttachChild(jointNode);
            } else {
                MaxUsd::Log::Warn(
                    "Skeleton prim \"{0}\" has topology out of order. Parent joints should always "
                    "come "
                    "before children joints.",
                    skelQuery.GetPrim().GetName().GetString());
            }
        } else {
            // doesn't have a joint parent. we'll attach it to the skeleton container
            if (skelContainer) {
                skelContainer->AttachChild(jointNode);
            }
        }

        joints[i] = jointNode;
    }

    return true;
}

bool MaxUsdTranslatorSkel::CopyJointsAnimations(
    const UsdSkelSkeletonQuery& skelQuery,
    INode*                      skelContainer,
    const MaxUsdReadJobContext& context,
    const std::vector<INode*>&  joints)
{
    if (joints.empty()) {
        return false;
    }

    const auto   stage = context.GetStage();
    const double timeCodePerSec = stage->GetTimeCodesPerSecond();

    // TODO: this is temporary until we add the sampling rate option to the import UI
    constexpr double samplingRate = 1.0;

    std::vector<double> usdTimeCodes;
    const auto          timeConfig = context.GetArgs().GetResolvedTimeConfig(stage);
    const double        startTimeCode = timeConfig.GetStartTimeCode();
    const double        endTimeCode = timeConfig.GetEndTimeCode();
    usdTimeCodes.reserve(
        static_cast<::size_t>(std::ceil((endTimeCode - startTimeCode) * timeCodePerSec)));
    for (double timeSample = startTimeCode; timeSample <= endTimeCode;) {
        usdTimeCodes.emplace_back(timeSample);
        timeSample += 1.0 / samplingRate;
    }

    // There's a bug in 3ds Max where the animation key is not created if the first time being
    // animated is 0. To go around that, have the time 0 being set later.
    if (startTimeCode == 0.0) {
        std::swap(*usdTimeCodes.begin(), *(usdTimeCodes.end() - 1));
    }

    // we aren't creating a node for the skeleton prim, so we need to cache any transform
    // applied to that specific prim we case we need to use it later on
    std::vector<GfMatrix4d>            skelLocalXforms(usdTimeCodes.size());
    const UsdGeomXformable::XformQuery xfQuery(skelQuery.GetSkeleton());
    for (size_t i = 0; i < usdTimeCodes.size(); ++i) {
        if (!xfQuery.GetLocalTransformation(&skelLocalXforms[i], usdTimeCodes[i])) {
            skelLocalXforms[i].SetIdentity();
        }
    }

    const auto& topo = skelQuery.GetTopology();

    // scope to disable auto key to prevent the creation of a "default" animated key
    {
        UsdGeomXformCache xformComputeCache;

        // only set keyframes if there is animation
        if (usdTimeCodes.size() > 1) {
            AnimateOn();
        }

        for (size_t i = 0; i < usdTimeCodes.size(); ++i) {
            VtMatrix4dArray xforms;
            const auto      maxTimeValue
                = MaxUsd::GetMaxTimeValueFromUsdTimeCode(stage, usdTimeCodes[i]);
            xformComputeCache.SetTime(usdTimeCodes[i]);
            if (!skelQuery.ComputeJointWorldTransforms(&xforms, &xformComputeCache)) {
                MaxUsd::Log::Error(
                    "Failed to calculate Joint transforms for Skeleton prim \"{}\" at USD timecode "
                    "{}",
                    skelQuery.GetPrim().GetName().GetString(),
                    usdTimeCodes[i]);
                return false;
            }

            for (size_t jointIdx = 0; jointIdx < joints.size(); ++jointIdx) {
                GfMatrix4d jointTransform = xforms[jointIdx];

                // if the joint has no parent, then we need to apply the skel prim transform
                if (topo.GetParent(jointIdx) < 0) {
                    jointTransform *= skelLocalXforms[i];
                }

                if (MaxUsd::IsStageUsingYUpAxis(context.GetStage())) {
                    MaxUsd::MathUtils::ModifyTransformYToZUp(jointTransform);
                }

                Matrix3 maxMatrix;
                MaxSDK::Graphics::Matrix44ToMaxWorldMatrix(
                    maxMatrix, MaxUsd::ToMax(jointTransform));
                joints[jointIdx]->SetNodeTM(maxTimeValue, maxMatrix);
            }

            for (size_t jointIdx = 0; jointIdx < joints.size(); ++jointIdx) {
                joints[jointIdx]->ResetBoneStretch(maxTimeValue);
            }
        }

        if (usdTimeCodes.size() > 1) {
            AnimateOff();
        }
    }

    return true;
}

bool MaxUsdTranslatorSkel::SetJointProperties(
    const UsdSkelSkeletonQuery& skelQuery,
    const MaxUsdReadJobContext& context,
    const std::vector<INode*>&  joints)
{
    VtMatrix4dArray bindTransforms;
    skelQuery.GetSkeleton().GetBindTransformsAttr().Get(&bindTransforms);

    if (bindTransforms.empty()) {
        return false;
    }

    const auto stage = context.GetStage();
    const auto timeConfig = context.GetArgs().GetResolvedTimeConfig(stage);
    const auto maxStartTime
        = MaxUsd::GetMaxTimeValueFromUsdTimeCode(stage, timeConfig.GetStartTimeCode());
    const float rescaleFactor = static_cast<float>(MaxUsd::GetUsdToMaxScaleFactor(stage));

    const size_t numJoints = joints.size();

    // First, find where the first child bone is, for each bone.
    // This will be used to later figure out the bone's direction.
    std::map<int, pxr::GfVec3d> firstChildPivots;
    const auto                  topo = skelQuery.GetTopology();
    for (size_t i = 0; i < numJoints; ++i) {
        const int  parent = topo.GetParent(i);
        const auto it = firstChildPivots.find(parent);
        if (it != firstChildPivots.end()) {
            continue;
        }
        const GfVec3d pivot = bindTransforms[i].ExtractTranslation();
        firstChildPivots.insert({ parent, pivot });
    }

    /// Return the index of the basis of mat that is best aligned with dir.
    /// "mat" must be orthogonal.
    auto findBestAlignedBasis = [](const GfMatrix4d& mat, const GfVec3d& dir) {
        const float pi4 = static_cast<float>(M_PI) / 4.0f;
        for (int i = 0; i < 2; ++i) {
            // If the transform is orthogonal, the best aligned axis
            // has an absolute dot product greater than PI/4.
            if (std::abs(GfDot(mat.GetRow3(i), dir)) > pi4) {
                return i;
            }
        }
        return 2;
    };

    // Default bone length.
    const double defaultLength = 2.0;

    for (size_t i = 0; i < numJoints; ++i) {
        auto       length = defaultLength;
        const auto node = joints[i];

        node->SetBoneNodeOnOff(TRUE, 0);
        node->SetBoneAutoAlign(TRUE);
        node->SetBoneFreezeLen(TRUE);
        node->SetBoneScaleType(BONE_SCALETYPE_NONE);
        node->SetBoneAxisFlip(FALSE);
        node->SetBoneAxis(BONE_AXIS_X);
        node->ShowBone(0);

        const GfVec3d pivot = bindTransforms[i].ExtractTranslation();

        // 3dsMax assumes X aligned bones when it draws bones, but bones can
        // be aligned differently (for example, coming from maya). We try to figure out
        // the alignment for each bone, and adjust the object accordingly.

        // X=0,Y=1,Z=2
        int axis = 0;

        // Figure out the bone's direction (toward its child) and length.
        auto it = firstChildPivots.find(static_cast<int>(i));
        if (it != firstChildPivots.end()) {
            auto boneDir = it->second - pivot;
            axis = findBestAlignedBasis(bindTransforms[i], boneDir.GetNormalized());
            length = (boneDir).GetLength();
        }
        // If no children, assume the same alignment as with the parent.
        else {
            const int parent = skelQuery.GetTopology().GetParent(i);
            if (parent >= 0 && static_cast<size_t>(parent) < numJoints) {
                GfVec3d parentPivot = bindTransforms[parent].ExtractTranslation();
                auto    boneDir = pivot - parentPivot;
                axis = findBestAlignedBasis(bindTransforms[parent], boneDir.GetNormalized());
            }
        }

        // Now we can offset the bone geometry to match the axis.
        switch (axis) {
        case 0:
            node->SetObjOffsetRot(IdentQuat());
            node->SetBoneAxis(BONE_AXIS_X);
            break;
        case 1:
            node->SetObjOffsetRot(Quat(0.0, 0.0, 1.0, -1.0));
            node->SetBoneAxis(BONE_AXIS_Y);
            break;
        case 2:
            node->SetObjOffsetRot(Quat(0.0, 1.0, 0.0, 1.0));
            node->SetBoneAxis(BONE_AXIS_Z);
            break;
        }

        IParamBlock2* boneParams = (static_cast<SimpleObject2*>(node->GetObjectRef()))
                                       ->GetParamBlockByID(boneobj_params);
        // The length doesn't get rescaled when the node is resvaled VS units... width and height
        // do.
        boneParams->SetValue(boneobj_length, maxStartTime, (float)length * rescaleFactor);

        // It doesnt look good if the width or height of the bone are bigger than the length
        // of the bone. If it is the case, adjust them.
        float    width;
        float    height;
        Interval inter;
        boneParams->GetValue(boneobj_width, maxStartTime, width, inter);
        boneParams->GetValue(boneobj_height, maxStartTime, height, inter);

        if (length < width) {
            boneParams->SetValue(boneobj_width, maxStartTime, (float)length);
        }
        if (length < height) {
            boneParams->SetValue(boneobj_height, maxStartTime, (float)length);
        }

        // Use the default 3dsMax bone color.
        node->SetWireColor(RGB(174, 186, 203));
        // Bones should not render.
        node->SetRenderable(FALSE);
    }

    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE