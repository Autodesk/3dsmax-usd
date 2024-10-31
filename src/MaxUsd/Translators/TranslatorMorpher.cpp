//
// Copyright 2024 Autodesk
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
#include "TranslatorMorpher.h"

#include <MaxUsd/Translators/TranslatorUtils.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/usdSkel/animQuery.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/blendshape.h>
#include <pxr/usd/usdSkel/skinningQuery.h>

#include <maxscript/maxscript.h>

static constexpr auto  ClassID_Morpher = Class_ID(0x17bb6854, 0xa5cba2a3);
static constexpr float WEIGHT_ANIMATION_THRESHOLD = 0.001f;

PXR_NAMESPACE_OPEN_SCOPE

bool MaxUsdTranslatorMorpher::Read(
    const UsdPrim&              prim,
    INode*                      maxNode,
    const MaxUsdReadJobContext& context)
{
    if (!maxNode) {
        return false;
    }

    UsdSkelBindingAPI skelBinding = UsdSkelBindingAPI::Get(context.GetStage(), prim.GetPath());

    SdfPathVector blendShapeTargets;
    skelBinding.GetBlendShapeTargetsRel().GetTargets(&blendShapeTargets);

    // no blendshapes for this skel binding. Leave
    if (blendShapeTargets.empty()) {
        return false;
    }

    AddMorphTargets(maxNode, context, blendShapeTargets);

    return true;
}

bool MaxUsdTranslatorMorpher::ConfigureMorpherAnimations(
    const UsdSkelSkinningQuery& skinningQuery,
    const UsdSkelAnimQuery&     animQuery,
    INode*                      maxNode,
    const MaxUsdReadJobContext& context)
{
    if (!maxNode || !animQuery.IsValid()) {
        return false;
    }

    VtTokenArray skinblendShapeOrder;
    skinningQuery.GetBlendShapeOrder(&skinblendShapeOrder);

    // should only do morpher animations if there are blendshapes
    if (skinblendShapeOrder.empty()) {
        return false;
    }

    const auto allMods = MaxUsd::GetAllModifiers(maxNode);
    Modifier*  morpherMod = nullptr;
    for (Modifier* m : allMods) {
        if (m->ClassID() == ClassID_Morpher) {
            morpherMod = m;
            break;
        }
    }

    // leave can't find the morpher modifier
    if (!morpherMod) {
        return false;
    }

    SdfPathVector blendShapeTargets;
    skinningQuery.GetBlendShapeTargetsRel().GetTargets(&blendShapeTargets);

    // animation prim can hold blendshape information for multiple meshes
    // map the correct order which the blendshapes are in the animation prim
    VtTokenArray        animBlendShapeOrder = animQuery.GetBlendShapeOrder();
    std::vector<size_t> correctOrder;
    correctOrder.reserve(skinblendShapeOrder.size());
    for (const TfToken& token : skinblendShapeOrder) {
        const auto iter = std::find(animBlendShapeOrder.begin(), animBlendShapeOrder.end(), token);
        const auto idx = static_cast<int>(std::distance(animBlendShapeOrder.begin(), iter));
        if (idx >= 0) {
            correctOrder.emplace_back(idx);
        }
    }

    // Progressive morphers aren't supported for channels above 100 before 3dsMax 2025.1 (ver 27.1
    // below)
    const auto maxVersion = MaxSDKSupport::GetMaxVersion();
    const bool skipInBetweens
        = maxVersion.empty() || (maxVersion[0] < 27) || (maxVersion[0] == 27 && maxVersion[1] < 1);
    const UsdSkelAnimation animPrim(animQuery.GetPrim());
    MaxUsdTranslatorUtil::ReadUsdAttribute(
        animPrim.GetBlendShapeWeightsAttr(),
        [morpherMod, &correctOrder, &blendShapeTargets, &context, skipInBetweens](
            const VtValue& value, const UsdTimeCode& timeCode, const TimeValue& timeValue) {
            pxr::VtFloatArray shapeWeights = value.Get<pxr::VtFloatArray>();

            if (timeCode != UsdTimeCode::Default()) {
                bool isUsingLimits
                    = static_cast<IParamBlock*>(morpherMod->SubAnim(0))->GetInt(0, timeValue) != 0;
                for (size_t i = 0; i < correctOrder.size(); ++i) {
                    if (IParamBlock* animatablePB
                        = static_cast<IParamBlock*>(morpherMod->SubAnim(static_cast<int>(i) + 1))) {
                        UsdSkelBlendShape blendShape(
                            context.GetStage()->GetPrimAtPath(blendShapeTargets[i]));

                        const auto inbetweens = blendShape.GetInbetweens();

                        if (!inbetweens.empty() && i >= 100 && skipInBetweens) {
                            continue;
                        }

                        // if any of the inbetweens has a negative weight, the animation weights
                        // needs to be shifted to account for it
                        bool hasNegativeWeight = false;
                        for (auto& inbetween : inbetweens) {
                            if (inbetween.HasAuthoredWeight()) {
                                float inBetweenWeight;
                                inbetween.GetWeight(&inBetweenWeight);
                                if (inBetweenWeight < 0) {
                                    hasNegativeWeight = true;
                                    break;
                                }
                            }
                        }

                        float animationWeight = shapeWeights[correctOrder[i]];

                        // Disable UseLimits property on the Morpher if the animation weight is
                        // outside the default morpher limit range (0-1)
                        if (isUsingLimits && (animationWeight < 0 || animationWeight > 1)) {
                            if (Animatable* morpherControllers = morpherMod->SubAnim(0)) {
                                static_cast<IParamBlock*>(morpherControllers)
                                    ->SetValue(0, timeValue, 0);
                                isUsingLimits = false;
                            }
                        }

                        // the following logic should be applied when weight == 0 since it means no
                        // morpher should be applied
                        if (hasNegativeWeight) {
                            /*
                             * Max's progressive morpher doesn't support negative weights, so need
                             *to remap negative weights to 0-1. For example: def BlendShape "Box1"
                             * {
                             *		uniform point3f[] inbetweens:IBT_0 = [...] (
                             *			weight = -1
                             *		)
                             *		uniform point3f[] inbetweens:IBT_1 = [...] (
                             *			weight = -0.5
                             *		)
                             *		uniform point3f[] inbetweens:IBT_2 = [...] (
                             *			weight = 0.5 )
                             *		)
                             *		uniform vector3f[] offsets = [...]
                             * }
                             *
                             * The example above would be converted to the following:
                             * IBT_0: (-1 + 1) * 0.5 = 0.0
                             * IBT_1: (-0.5 + 1) * 0.5 = 0.25
                             * IBT_2: (0.5 + 1) * 0.5 = 0.75
                             */
                            animationWeight = std::max(
                                (animationWeight + 1) * 0.5f, WEIGHT_ANIMATION_THRESHOLD);
                        }
                        // Max's morpher weights are 0-100, so convert the weight from 0-1 to 0-100
                        animatablePB->SetValue(0, timeValue, animationWeight * 100.f);
                    }
                }
            }

            return true;
        },
        context);

    return true;
}

INode* MaxUsdTranslatorMorpher::CloneMorpherNode(INode* morpherNode)
{
    INode* clone = nullptr;

    INodeTab lNodeToBeCloned;
    lNodeToBeCloned.AppendNode(morpherNode);
    INodeTab resultSource;
    INodeTab resultDest;

    Point3 pt(0, 0, 0);
    GetCOREInterface12()->CloneNodes(
        lNodeToBeCloned, pt, true, NODE_COPY, &resultSource, &resultDest);
    if (resultDest.Count() > 0) {
        clone = resultDest[0];
    }

    // disable morpher modifier on the clone so that it doesn't affect the original mesh
    const auto allMods = MaxUsd::GetAllModifiers(clone);
    for (Modifier* m : allMods) {
        if (m->ClassID() == ClassID_Morpher) {
            GetCOREInterface17()->DeleteModifier(*clone, *m);
            break;
        }
    }

    return clone;
}

bool MaxUsdTranslatorMorpher::AddMorphTargets(
    INode*                      morpherNode,
    const MaxUsdReadJobContext& context,
    const SdfPathVector&        blendShapeTargets)
{
    // cloneNode is used by the morpher API to build the morph targets.
    const auto  stage = context.GetStage();
    const float scaleFactor = static_cast<float>(MaxUsd::GetUsdToMaxScaleFactor(stage));

    // The node holding the morpher needs to account for scaling factor while performing the morpher
    // operations So, resize the node with the morpher before doing the operations, then revert back
    // when it's done
    INodeTab tab;
    tab.AppendNode(morpherNode);
    const auto scaleMorpherNodeScopeGuard = MaxUsd::MakeScopeGuard(
        [&tab, scaleFactor]() {
            GetCOREInterface()->ClearNodeSelection();
            GetCOREInterface()->SelectNodeTab(tab, TRUE, FALSE);
            GetCOREInterface()->RescaleWorldUnits(scaleFactor, true);
        },
        [scaleFactor]() {
            GetCOREInterface()->RescaleWorldUnits(1.f / scaleFactor, true);
            GetCOREInterface()->ClearNodeSelection();
        });

    const auto clonedNode = CloneMorpherNode(morpherNode);
    if (!clonedNode) {
        MaxUsd::Log::Error(
            L"Couldn't clone nodes for Morpher Modifier on \"{}\" .", morpherNode->GetName());
        return false;
    }

    // clonedObject is the mesh that will have its vertices modified by the blendshape offsets
    const auto originalScaledObject = morpherNode->GetObjectRef();
    const auto clonedObject = clonedNode->GetObjectRef();

    Modifier* morpherMod = static_cast<Modifier*>(CreateInstance(OSM_CLASS_ID, ClassID_Morpher));
    if (!morpherMod) {
        MaxUsd::Log::Error(
            L"Couldn't create a new morpher modifier for node \"{}\" .", morpherNode->GetName());
        return false;
    }

    GetCOREInterface12()->AddModifier(*morpherNode, *morpherMod);

    VtVec3fArray deltaPoints, deltaNormals;
    // cache all the created clones so that they can be deleted after
    std::vector<INode*> inBetweenClonedNodes;

    for (size_t channelIndex = 0; channelIndex < blendShapeTargets.size(); ++channelIndex) {
        const auto& blendShapePath = blendShapeTargets[channelIndex];
        const auto  bsPrim = stage->GetPrimAtPath(blendShapePath);
        if (!bsPrim.IsValid()) {
            TF_WARN(
                "Blendshape doesn't resolve to a valid prim path: %s", blendShapePath.GetText());
            continue;
        }
        UsdSkelBlendShape blendShapePrim(bsPrim);
        blendShapePrim.GetOffsetsAttr().Get(&deltaPoints);
        if (deltaPoints.empty()) {
            continue;
        }

        VtIntArray pointIndices;
        blendShapePrim.GetPointIndicesAttr().Get(&pointIndices);

        // pointIndices don't have to be authored in usd. If empty, assume indices are in order
        // 0..n-1
        if (pointIndices.empty()) {
            pointIndices.reserve(deltaPoints.size());
            for (size_t i = 0; i < deltaPoints.size(); ++i) {
                pointIndices.emplace_back(i);
            }
        }

        ApplyDeltasOffset(
            originalScaledObject, clonedObject, pointIndices, deltaPoints, scaleFactor);

        // set the cloned node name so that it builds to the proper name on the morpher channel
        const auto blendShapeName = blendShapePrim.GetPrim().GetName();
        clonedNode->SetName(MaxUsd::UsdStringToMaxString(blendShapeName).data());

        if (!AddMorpherTargetScript(morpherNode, clonedNode, channelIndex)) {
            MaxUsd::Log::Warn(
                L"Error running script to create Morpher \"{}\" names for Node \"{}\"",
                clonedNode->GetName(),
                morpherNode->GetName());
        }

        // revert offset changes done to create the morpher to the cloned node
        ApplyDeltasOffset(
            originalScaledObject, clonedObject, pointIndices, deltaPoints, scaleFactor, true);
        // one blendshape could have several inbetween weights defined
        bool hasInbetweens = AddAllInBetweens(
            morpherNode,
            originalScaledObject,
            clonedNode,
            blendShapePrim,
            pointIndices,
            channelIndex,
            inBetweenClonedNodes);
    }

    inBetweenClonedNodes.emplace_back(clonedNode);

    const auto coreInterface = GetCOREInterface12();
    for (const auto node : inBetweenClonedNodes) {
        coreInterface->DeleteNode(node);
    }
    return true;
}

bool MaxUsdTranslatorMorpher::AddAllInBetweens(
    INode*                   morpherNode,
    Object*                  originalScaledObject,
    INode*                   originalClonedNode,
    const UsdSkelBlendShape& blendShapePrim,
    const VtIntArray&        blendShapePointIndices,
    size_t                   morpherIndex,
    std::vector<INode*>&     inBetweenClonedNodes)
{
    const std::vector<UsdSkelInbetweenShape> inBetweens = blendShapePrim.GetInbetweens();
    if (inBetweens.empty()) {
        return false;
    }

    // There's a bug in the morpher modifier that it's not possible to set weights for progressive
    // morphers on channels above 100. So, if the morpherIndex is above 100, skip the inbetweens and
    // log an error. This issue has been fixed on 3ds Max 2025.1 (which translates to version 27.1
    // below)
    if (morpherIndex >= 100) {
        const auto maxVersion = MaxSDKSupport::GetMaxVersion();
        if (maxVersion.empty() || (maxVersion[0] < 27)
            || (maxVersion[0] == 27 && maxVersion[1] < 1)) {
            MaxUsd::Log::Error(
                L"Max morpher modifier only supports progressive morphers for 100 channels. "
                L"Skipping "
                L"inbetweens for Node \"{}\" and channel \"{}\"",
                morpherNode->GetName(),
                morpherIndex);
            return false;
        }
    }

    // Progressive morphers api requires the max nodes to work on it, so
    // pool the amount of nodes required to work based on the number of inbetweens.
    // Also, add an extra inbetween to account for the actual blendshape
    const float scaleFactor
        = static_cast<float>(MaxUsd::GetUsdToMaxScaleFactor(blendShapePrim.GetPrim().GetStage()));
    const size_t diff = (inBetweens.size() + 1) - inBetweenClonedNodes.size();
    for (size_t i = 0; i < diff; ++i) {
        inBetweenClonedNodes.emplace_back(CloneMorpherNode(morpherNode));
    }

    bool hasNegativeWeightInBetween = false;
    for (size_t i = 0; i < inBetweens.size(); ++i) {
        const auto inBetween = inBetweens[i];
        const auto inBetweenName
            = MaxUsd::UsdStringToMaxString(inBetween.GetAttr().GetName().GetString());

        VtVec3fArray inBetweenDeltaPoints, inBetweenDeltaNormals;
        inBetween.GetOffsets(&inBetweenDeltaPoints);
        if (inBetweenDeltaPoints.empty()) {
            MaxUsd::Log::Warn(
                L"Blendshape inbetween \"{}\" defined with no offset for Node \"{}\" on channel "
                L"\"{}\"",
                inBetweenName.data(),
                morpherNode->GetName(),
                morpherIndex);
            continue;
        }

        const auto clonedNode = inBetweenClonedNodes[i];
        // clonedObject is the mesh that will have its vertices offset by the blendshape offsets
        const auto clonedObject = clonedNode->GetObjectRef();

        ApplyDeltasOffset(
            originalScaledObject,
            clonedObject,
            blendShapePointIndices,
            inBetweenDeltaPoints,
            scaleFactor);

        // building a morpher channel uses the node name, properly set it before creating a new
        // channel
        clonedNode->SetName(inBetweenName);
        AddProgressiveMorpherScript(morpherNode, clonedNode, morpherIndex);

        ApplyDeltasOffset(
            originalScaledObject,
            clonedObject,
            blendShapePointIndices,
            inBetweenDeltaPoints,
            scaleFactor,
            true);

        float w;
        inBetween.GetWeight(&w);
        if (w < 0) {
            // Progressive morphers can't have negative weight in 3ds Max, so the weights are
            // remapped to 0-1. With this shift, the 0 weight would be shifted to 0.5. Because of
            // this, keep track if any of the inbetweens have negative weights, in order to add the
            // original mesh (unmodified) with 0.5 weight
            hasNegativeWeightInBetween = true;
        }
    }

    if (hasNegativeWeightInBetween) {
        // Since Max doesn't support progressive morphers with negative weight, we'll use weights
        // from 0-49 for the negative in-betweens and then 51-100 for the positive ones. In order to
        // do that, add a middle reference node of the original shape in the middle (50%) weight
        const auto middleReferenceNode = inBetweenClonedNodes[inBetweenClonedNodes.size() - 1];
        AddProgressiveMorpherScript(morpherNode, middleReferenceNode, morpherIndex);
    }

    // setting the progressive morpher weights last because each time a progressive morpher is
    // added, it resets the weights of all the previous progressive morphers to an average of 100%
    for (size_t i = 0; i < inBetweens.size(); ++i) {
        const auto inBetween = inBetweens[i];
        const auto clonedNode = inBetweenClonedNodes[i];

        if (inBetween.HasAuthoredWeight()) {
            float betweenWeight;
            inBetween.GetWeight(&betweenWeight);

            // Max's progressive morpher doesn't support negative weights, so need to remap them to
            // a value from 0-49
            if (hasNegativeWeightInBetween) {
                // In Max, a progressive morpher can't have a weight of 0.0f. Weight of 0.0f means
                // the original shape So, use a minimum threshold here to prevent that from
                // happening.
                betweenWeight = std::max((betweenWeight + 1.f) * 0.5f, WEIGHT_ANIMATION_THRESHOLD);
            }

            if (!SetProgressiveMorpherWeightScript(
                    morpherNode, clonedNode, morpherIndex, betweenWeight)) {
                MaxUsd::Log::Warn(
                    L"Error running script to set progressive morpher weight index \"{}\" for Node "
                    L"\"{}\"",
                    std::to_wstring(morpherIndex),
                    morpherNode->GetName());
            }
        }
    }

    // The weight of the original morpher shape has changed because progressive morphers were added.
    // Pass the node used for the original morpher shape so that the weight can be adjusted back to
    // 100%
    if (!SetProgressiveMorpherWeightScript(morpherNode, originalClonedNode, morpherIndex, 1.f)) {
        MaxUsd::Log::Warn(
            L"Error running script to set progressive morpher full weight index \"{}\" for Node "
            L"\"{}\"",
            std::to_wstring(morpherIndex),
            morpherNode->GetName());
    }

    if (hasNegativeWeightInBetween) {
        // Make sure that the middle reference added for the negative inbetweens has a weight of 50%
        const auto middleReferenceNode = inBetweenClonedNodes[inBetweenClonedNodes.size() - 1];
        if (!SetProgressiveMorpherWeightScript(
                morpherNode, middleReferenceNode, morpherIndex, 0.5f)) {
            MaxUsd::Log::Warn(
                L"Error running script to set progressive middle morpher weight index \"{}\" for "
                L"Node \"{}\"",
                std::to_wstring(morpherIndex),
                morpherNode->GetName());
        }
    }

    return true;
}

bool MaxUsdTranslatorMorpher::AddMorpherTargetScript(
    INode* maxNode,
    INode* clonedNode,
    size_t morpherIndex)
{
    static const TSTR buildMorphTargetScript = LR"(
		fn buildMorphTarget originalNodeHandle targetNodeHandle idx = (
			local success = false

			local originalNode = maxOps.getNodeByHandle originalNodeHandle
			local targetNode = maxOps.getNodeByHandle targetNodeHandle

			-- This fixes a bug in Max 2022 where we can't update the mesh offsets without it
			if classof targetNode != Editable_Poly do convertToPoly targetNode

			modi = (getModifierByClass originalNode Morpher)

			-- This fixes a bug in Max 2022 where the nodes wouldn't be added
			modi.Autoload_of_targets = 1
			if iskindof modi Modifier and IsValidMorpherMod modi do
			(
				success = WM3_MC_BuildFromNode modi idx targetNode
				if success do
				(
					WM3_SetProgressiveMorphTension modi idx 0.0
				)
			)

			-- Revert the fix above, otherwise morphers wouldn't disappear when we delete the duplicated nodes
			modi.Autoload_of_targets = 0
			return success
		)
		buildMorphTarget )";

    if (!maxNode || !clonedNode) {
        return false;
    }

    FPValue            rvalue;
    std::wstringstream ss;
    ss << getModifierByClassScript << buildMorphTargetScript << maxNode->GetHandle() << L" ";
    ss << clonedNode->GetHandle() << L" " << (morpherIndex + 1) << L'\n' << L'\0';
    return ExecuteMAXScriptScript(
               ss.str().c_str(), static_cast<MAXScript::ScriptSource>(3), false, &rvalue)
        && rvalue.type == TYPE_BOOL && rvalue.b;
}

bool MaxUsdTranslatorMorpher::AddProgressiveMorpherScript(
    INode* maxNode,
    INode* clonedNode,
    size_t morpherIndex)
{
    static const TSTR addProgressiveMorpherScript = LR"(
		fn addProgressiveMorpher originalNodeHandle targetNodeHandle idx = (
			local success = false

			local originalNode = maxOps.getNodeByHandle originalNodeHandle
			local targetNode = maxOps.getNodeByHandle targetNodeHandle

			-- This fixes a bug in Max 2022 where we can't update the mesh offsets without it
			if classof targetNode != Editable_Poly do convertToPoly targetNode

			modi = (getModifierByClass originalNode Morpher)

			-- This fixes a bug in Max 2022 where the nodes wouldn't be added
			modi.Autoload_of_targets = 1
			if iskindof modi Modifier and IsValidMorpherMod modi do
			(
				success = WM3_AddProgressiveMorphNode modi idx targetNode
			)

			-- Revert the fix above, otherwise morphers wouldn't disappear when we delete the duplicated nodes
			modi.Autoload_of_targets = 0
			return success
		)
		addProgressiveMorpher )";

    if (!maxNode || !clonedNode) {
        return false;
    }

    FPValue            rvalue;
    std::wstringstream ss;
    ss << getModifierByClassScript << addProgressiveMorpherScript << maxNode->GetHandle() << L" ";
    ss << clonedNode->GetHandle() << L" " << (morpherIndex + 1) << L'\n' << L'\0';
    return ExecuteMAXScriptScript(
               ss.str().c_str(), static_cast<MAXScript::ScriptSource>(3), false, &rvalue)
        && rvalue.type == TYPE_BOOL && rvalue.b;
}

bool MaxUsdTranslatorMorpher::SetProgressiveMorpherWeightScript(
    INode* maxNode,
    INode* clonedNode,
    size_t morpherIndex,
    float  weight)
{
    static const TSTR setProgressiveWeightMorpherScript = LR"(
		fn setProgressiveWeightMorpherScript originalNodeHandle targetNodeHandle idx weight = (
			local success = false

			local originalNode = maxOps.getNodeByHandle originalNodeHandle
			local targetNode = maxOps.getNodeByHandle targetNodeHandle

			modi = (getModifierByClass originalNode Morpher)

			if iskindof modi Modifier and IsValidMorpherMod modi do
			(
				success = WM3_SetProgressiveMorphWeight modi idx targetNode weight
			)

			return success
		)
		setProgressiveWeightMorpherScript )";

    if (!maxNode || !clonedNode) {
        return false;
    }

    FPValue            rvalue;
    std::wstringstream ss;
    ss << getModifierByClassScript << setProgressiveWeightMorpherScript << maxNode->GetHandle()
       << L" ";
    ss << clonedNode->GetHandle() << L" " << (morpherIndex + 1) << L" ";
    // maxscript requires a float for the weight, so need to have the decimal point even when its 0
    ss << (weight != 0.0 ? std::to_wstring(weight * 100.f) : std::to_wstring(0.0)) << L'\n'
       << L'\0';

    return ExecuteMAXScriptScript(
               ss.str().c_str(), static_cast<MAXScript::ScriptSource>(3), false, &rvalue)
        && rvalue.type == TYPE_BOOL && rvalue.b;
}

void MaxUsdTranslatorMorpher::ApplyDeltasOffset(
    Object*             originalScaledObject,
    Object*             clonedScaledObject,
    const VtIntArray&   blendShapePointIndices,
    const VtVec3fArray& blendShapeDeltaPoints,
    float               scaleFactor,
    bool                revertOffset)
{
    if (!originalScaledObject || !clonedScaledObject) {
        return;
    }

    const auto numPoints = blendShapePointIndices.size();
    if (numPoints != blendShapeDeltaPoints.size()) {
        return;
    }

    for (size_t i = 0; i < numPoints; ++i) {
        const int pointIdx = blendShapePointIndices[i];

        if (revertOffset) {
            clonedScaledObject->SetPoint(pointIdx, originalScaledObject->GetPoint(pointIdx));
        } else {
            const auto pointPos = originalScaledObject->GetPoint(pointIdx);
            const auto deltaPoint = blendShapeDeltaPoints[i] * scaleFactor;
            clonedScaledObject->SetPoint(
                pointIdx,
                Point3(
                    pointPos.x + deltaPoint[0],
                    pointPos.y + deltaPoint[1],
                    pointPos.z + deltaPoint[2]));
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE