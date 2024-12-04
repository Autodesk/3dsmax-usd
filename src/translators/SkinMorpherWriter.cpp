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
#include "SkinMorpherWriter.h"

#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/Translators/WriteJobContext.h>
#include <MaxUsd/Utilities/PluginUtils.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/UsdToolsUtils.h>

#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/blendShape.h>
#include <pxr/usd/usdSkel/cache.h>
#include <pxr/usd/usdSkel/root.h>
#include <pxr/usd/usdSkel/utils.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <maxscript/maxscript.h>

#include <IRefTargContainer.h>
#include <iskin.h> // ISkin

PXR_NAMESPACE_OPEN_SCOPE

/**
 * \brief Struct to cache progressive morpher data on a given morpher channel.
 * count: number of progressive morpher channels
 * nodes: array of nodes that make up the progressive morphers
 * weights: array of weights in which the progressive morphers are applied to 100%
 *
 * Note: the count can be different from the size of the nodes and weights arrays, for the cases in
 * which the progressive morpher nodes have been deleted from the scene.
 */
struct ProgressiveMorphersInfo
{
    int                 count = 0;
    std::vector<INode*> nodes;
    std::vector<float>  weights;
};

/**
 * \brief Extract normal points from the given prim.
 * \param meshPrim The mesh prim to extract the normals from.
 * \param outNormals The array to store the extracted normals.
 */
void _extractNormalsFromPrim(const UsdGeomMesh& meshPrim, VtVec3fArray& outNormals)
{
    // Extract normals from the prim
    const auto hasNormalsPrimvar
        = pxr::UsdGeomPrimvarsAPI(meshPrim).HasPrimvar(pxr::UsdImagingTokens->primvarsNormals);
    if (hasNormalsPrimvar) {
        pxr::UsdGeomPrimvarsAPI sourcePrimvarApi(meshPrim);
        sourcePrimvarApi.GetPrimvar(pxr::UsdImagingTokens->primvarsNormals)
            .GetAttr()
            .Get(&outNormals);
    } else {
        meshPrim.GetNormalsAttr().Get(&outNormals);
    }
}

bool _getProgressiveMorpherInfo(
    INode*                   sourceNode,
    int                      morpherIndex,
    ProgressiveMorphersInfo& outProgressiveMorphers)
{
    static const TSTR getProgressiveMorphersInfo = LR"(
		fn getProgressiveMorphersWeights nodeHandle idx =
		(
			local progressiveMorpherNodes = #()
			local progressiveMorpherWeights = #()
			local numberOfProgressiveMorphers = 0

			local node = maxOps.getNodeByHandle nodeHandle
			modi = (getModifierByClass node Morpher)
			if iskindof modi Modifier and IsValidMorpherMod modi do
			(
				numberOfProgressiveMorphers = (WM3_NumberOfProgressiveMorphs modi idx)
				for progressiveMorpher = 1 to numberOfProgressiveMorphers do
				(
					local progMorphNode = (WM3_GetProgressiveMorphNode modi idx progressiveMorpher)
					if progMorphNode != undefined do
					(
						append progressiveMorpherNodes progMorphNode
						append progressiveMorpherWeights (WM3_GetProgressiveMorphWeight modi idx progMorphNode)
					)
				)
			)
			return #(numberOfProgressiveMorphers, progressiveMorpherNodes, progressiveMorpherWeights)
		)
		getProgressiveMorphersWeights )";

    // The script will return a 3 element array with the following data:
    // 0 - number of progressive morphers
    // 1 - array of nodes that make up the progressive morphers
    // 2 - array of weights in which the progressive morphers are applied to 100%
    static constexpr int PROGRESSIVE_NUMBER_IDX = 0;
    static constexpr int PROGRESSIVE_NODES_IDX = 1;
    static constexpr int PROGRESSIVE_WEIGHTS_IDX = 2;

    FPValue            rvalue;
    std::wstringstream ss;
    ss << getModifierByClassScript << getProgressiveMorphersInfo << sourceNode->GetHandle() << L" "
       << (morpherIndex + 1) << L'\n' << L'\0';
    if (ExecuteMAXScriptScript(
            ss.str().c_str(), static_cast<MAXScript::ScriptSource>(3), false, &rvalue)
        && rvalue.type == TYPE_FPVALUE_TAB_BV) {
        // First the script returned a valid array
        if (Tab<FPValue*>* progressiveMorphersInfoArray = rvalue.fpv_tab) {
            outProgressiveMorphers.count
                = progressiveMorphersInfoArray->begin()[PROGRESSIVE_NUMBER_IDX]->i;
            Tab<INode*>* nodeHandlesTab
                = progressiveMorphersInfoArray->begin()[PROGRESSIVE_NODES_IDX]->n_tab;
            Tab<float>* weightsTab
                = progressiveMorphersInfoArray->begin()[PROGRESSIVE_WEIGHTS_IDX]->f_tab;

            // The outProgressiveMorphers.count can be greater than zero, while the arrays are empty
            // In that case, most likely the nodes from the scene were deleted and we can't retrieve
            // the data
            for (int i = 0; nodeHandlesTab && weightsTab && i < nodeHandlesTab->Count(); ++i) {
                outProgressiveMorphers.nodes.emplace_back(nodeHandlesTab->begin()[i]);
                outProgressiveMorphers.weights.emplace_back(weightsTab->begin()[i]);
            }
        }
    }

    return outProgressiveMorphers.count > 0;
}

MaxUsdSkinMorpherWriter::MaxUsdSkinMorpherWriter(const MaxUsdWriteJobContext& jobCtx, INode* node)
    : MaxUsdMeshWriter(jobCtx, node)
{
}

// Checks if a modifier is allowed on top of the skin modifier for export to UsdSkel.
bool _IsModOkAfterSkin(Modifier* modifier)
{
    // Reject world space modifiers outright... on the USD side, the skinning
    // would be applied after the WSM, opposite to max...very little odds of
    // producing good results...
    if (modifier->SuperClassID() == WSM_CLASS_ID) {
        return false;
    }

    // We maintain a black list of object space modifiers which we know will cause
    // trouble if above the skin. More modifiers may alter the geometry, if the vert
    // counts end up different, we also generate warnings.
    static std::vector<Class_ID> bannedMods = {
        Class_ID(0x73ccf34a, 0x9abc45fc), // OpenSubdiv
        Class_ID(0x0d727b3e, 0x491d29a7), // TurboSmooth
        Class_ID(0x00000032, 0x00007f9e), // Meshsmooth
        Class_ID(0x332c9510, 0x38bb548c), // Chamfer,
        Class_ID(0xa3b26ff2, 0x00000000), // Tessellate
        Class_ID(0x10e36629, 0x0e54570e), // Subdivide
        Class_ID(0x8eb2b3f7, 0x57da4442), // ArrayModifier
        Class_ID(0x148132a1, 0x2ed9401c), // Lattice
        Class_ID(0x000c4d31, 0x00000000), // Optimize
        Class_ID(0x3ef24fe4, 0x5932330a), // ProOptimizer
        Class_ID(0x6a9e4c6b, 0x494744dd), // MultiRes
        Class_ID(0x6a2400ab, 0x5fd224da), // Welder
        Class_ID(0x709029e0, 0x2cfa07bd), // Vertex_Weld
        Class_ID(0x470a1d7a, 0x53955c31), // Cap_Holes
        Class_ID(0x000c3a32, 0x00000000), // Face_Extrude
        Class_ID(0x4bb0655a, 0x0e3e3a15), // Quadify_Mesh
        Class_ID(0x71d938ca, 0x90d1dca3), // RetopologyComponent
    };

    // Checks if the modifier is in the banned list.
    const auto it = std::find(bannedMods.begin(), bannedMods.end(), modifier->ClassID());
    return it == bannedMods.end();
}

// Write() will only get called once, as we are returning FOREVER from GetValidityInterval().
bool MaxUsdSkinMorpherWriter::Write(
    UsdPrim&                  targetPrim,
    bool                      applyOffsetTransform,
    const MaxUsd::ExportTime& time)
{
    INode* sourceNode = GetNode();

    if (time.IsFirstFrame()) {
        const SdfPath      primPath = targetPrim.GetPath();
        const std::string& targetPrimName = primPath.GetString();
        const auto         stage = targetPrim.GetStage();

        const SdfPath skelRootPath = MaxUsd::VerifyOrMakeSkelRoot(stage, primPath);
        if (skelRootPath.IsEmpty()) {
            MaxUsd::Log::Error(
                "Couldn't Verify or Make SkelRoot path for prim {} !", targetPrimName);
            return false;
        }

        skeleton = MaxUsd::VerifyOrMakePrimOfType<UsdSkelSkeleton>(
            stage, skelRootPath, GetExportArgs().GetBonesPrimName());
        if (!skeleton) {
            MaxUsd::Log::Error("Couldn't create Skeleton prim for {} !", targetPrimName);
            return false;
        }

        skelAnimation = MaxUsd::VerifyOrMakePrimOfType<UsdSkelAnimation>(
            stage, skeleton.GetPath(), GetExportArgs().GetAnimationsPrimName());
        if (!skelAnimation) {
            MaxUsd::Log::Error("Couldn't create SkelAnimation prim for {} !", targetPrimName);
            return false;
        }

        const std::map<INode*, SdfPath>& nodesToPrims = GetJobContext().GetNodesToPrimsMap();
        const MaxUsd::TimeConfig         timeConfig = GetExportArgs().GetResolvedTimeConfig();
        const auto                       startTime = timeConfig.GetStartTime();

        const pxr::SdfPath skelPath = skeleton.GetPath();
        UsdSkelBindingAPI  binding = UsdSkelBindingAPI::Apply(targetPrim);
        if (!binding.CreateSkeletonRel().SetTargets(SdfPathVector({ skelPath }))) {
            MaxUsd::Log::Error(
                "Couldn't create Skeleton binding for prim {} with path {} !",
                targetPrimName,
                skelPath.GetAsString());
            return false;
        }

        // Create relationship between animation and skel prim
        // this could be redundant with SkelWriter, but for cases with no skeleton, the morpher also
        // needs this
        const UsdSkelBindingAPI skelBinding = UsdSkelBindingAPI::Apply(skeleton.GetPrim());
        if (!skelBinding.GetAnimationSourceRel().SetTargets({ skelAnimation.GetPath() })) {
            MaxUsd::Log::Error(
                "Couldn't set SkelAnimation {} relationship for {} !",
                skelAnimation.GetPath().GetAsString(),
                skelPath.GetName());
            return false;
        }

        auto meshConvertOptions = GetExportArgs().GetMeshConversionOptions();
        meshConvertOptions.SetPrimvarLayoutInference(
            MaxUsd::MaxMeshConversionOptions::PrimvarLayoutInference::Never);

        const std::vector<ISkin*>    skinModifiers = MaxUsd::GetMaxSkinModifiers(sourceNode);
        const std::vector<Modifier*> morphers = MaxUsd::GetMaxMorpherModifiers(sourceNode);
        if (!morphers.empty()) {
            morpherProperties.morpher = morphers[0];
        }

        // if there's a morpher, we need to remove all it's influence before exporting the original
        // mesh
        {
            const auto disableMorpher = MaxUsd::MakeScopeGuard(
                [this]() {
                    if (morpherProperties.morpher) {
                        morpherProperties.morpher->DisableMod();
                    }
                },
                [this]() {
                    if (morpherProperties.morpher) {
                        morpherProperties.morpher->EnableMod();
                    }
                });

            // if translating skin, we need to disable certain modifiers
            if (!skinModifiers.empty() && GetExportArgs().GetTranslateSkin()) {
                skinnedMesh = DisabledModsAndWriteMeshData(
                    sourceNode,
                    stage,
                    primPath,
                    applyOffsetTransform,
                    MaxUsd::ExportTime { time.GetMaxTime(), pxr::UsdTimeCode::Default(), true });
            } else {
                MaxUsd::MeshConverter meshConverter;
                skinnedMesh = meshConverter.ConvertToUSDMesh(
                    sourceNode,
                    stage,
                    primPath,
                    meshConvertOptions,
                    applyOffsetTransform,
                    false,
                    MaxUsd::ExportTime { time.GetMaxTime(), pxr::UsdTimeCode::Default(), true });
            }
        }

        if (morpherProperties.morpher && GetExportArgs().GetTranslateMorpher()) {
            if (morphers.size() > 1) {
                MaxUsd::Log::Warn(
                    "Only one Morpher modifier per object is supported on export. Node {} contains "
                    "more than one !",
                    MaxUsd::MaxStringToUsdString(sourceNode->GetName()));
            }

            UsdAttribute      shapeWeightAttribute = skelAnimation.GetBlendShapeWeightsAttr();
            pxr::VtFloatArray defaultShapeWeights;
            shapeWeightAttribute.Get(&defaultShapeWeights);

            /**
             * SubAnim(0) is the overall UI morpher ui.
             * That UI has the following things - Taken from max's source code (wm3_main.cpp):
             static ParamBlockDescID GlobalParams[] = {
                            { TYPE_INT, NULL, FALSE, 0 },	// overrides: Use Limits
                            { TYPE_FLOAT, NULL, FALSE, 1 },	// overrides: Spinner Min
                            { TYPE_FLOAT, NULL, FALSE, 2 },	// overrides: Spinner Max
                            { TYPE_INT, NULL, FALSE, 3 },	// overrides: Use Selection
                            { TYPE_INT, NULL, FALSE, 4 },	// advanced:  Value increments
                            { TYPE_INT, NULL, FALSE, 5 },	// clist:	  Auto load
             };
             */
            if (Animatable* morpherUIControls = morpherProperties.morpher->SubAnim(0)) {
                VtArray<TfToken> morphTokens;
                SdfPathVector    morphPaths;

                morpherProperties.useLimits
                    = static_cast<IParamBlock*>(morpherUIControls)->GetInt(0, startTime);
                morpherProperties.minLimit
                    = static_cast<IParamBlock*>(morpherUIControls)->GetFloat(1, startTime);
                morpherProperties.maxLimit
                    = static_cast<IParamBlock*>(morpherUIControls)->GetFloat(2, startTime);

                /*
                 * In order to export morphers. We'll need to change the weight for each morph
                 * target. First, we set the weight of all channels to 0. Then, one by one we set
                 * them to max weight. However, it's possible to assign controllers to the spinners
                 * (morph targets). The controller makes it so that it's no longer possible to set
                 * values direct on the channels. Because of this, before exporting, we remove the
                 * controllers from the channels while we operate changing the weights as we need
                 * and, at the end, we re-assign the controllers back.
                 */

                // The controllers reference would get cleaned up and made null when we removed it,
                // so we use this container to make sure that the controllers still have a valid
                // reference
                SingleRefMaker   containerHolder;
                ReferenceTarget* container = (ReferenceTarget*)GetCOREInterface()->CreateInstance(
                    REF_TARGET_CLASS_ID, REFTARG_CONTAINER_CLASS_ID);
                containerHolder.SetRef(container);
                IRefTargContainer* containerRef = static_cast<IRefTargContainer*>(container);

                std::vector<unsigned int> usedChannels;
                // need to modify the morpher weights. scope guarding to make sure to reset it
                // afterwards
                const auto removeControllers = MaxUsd::MakeScopeGuard(
                    [this, &containerRef, &usedChannels, &defaultShapeWeights, &startTime]() {
                        auto morpher = morpherProperties.morpher;
                        for (int i = 1; i < morpher->NumSubs(); ++i) {
                            if (Animatable* animatable = morpher->SubAnim(i)) {
                                auto animatablePB = static_cast<IParamBlock*>(animatable);

                                // cache the controller previously set for this morph target
                                containerRef->SetItem(i, animatablePB->GetController(0));

                                const float morpherWeight
                                    = GetMorpherWeightAtTime(animatablePB, startTime);

                                // map Max's weight (0-100) to usd weight values (0-1). See
                                // MorpherProperties
                                defaultShapeWeights.emplace_back(morpherWeight / 100.f);
                                usedChannels.emplace_back(i);

                                // removing the controller to prevent the weight value being locked
                                animatablePB->RemoveController(0);

                                // need to set all weights to 0 before exporting morph targets
                                animatablePB->SetValue(0, startTime, 0);
                            }
                        }
                    },
                    [this, &containerRef, &usedChannels, &defaultShapeWeights, &startTime]() {
                        for (int i = 0; i < usedChannels.size(); ++i) {
                            const int c = usedChannels[i];
                            if (Animatable* animatable = morpherProperties.morpher->SubAnim(c)) {
                                auto animatablePB = static_cast<IParamBlock*>(animatable);

                                // remap the weight from usd to max values. See MorpherProperties
                                // comments
                                animatablePB->SetValue(
                                    0, startTime, defaultShapeWeights[i] * 100.f);

                                // re-assigning the controller the we had previously removed
                                animatablePB->SetController(
                                    0, static_cast<Control*>(containerRef->GetItem(c)), FALSE);
                            }
                        }
                    });

                {
                    // we can't access the morpher names from c++, so we need a maxscript call to
                    // get them
                    std::vector<std::wstring> morpherNames;
                    GetMorpherNames(sourceNode, morpherNames);
                    if (morpherNames.size() != usedChannels.size()) {
                        MaxUsd::Log::Warn(
                            "Different amount of Morpher names ({}) and used morpher channels ({}) "
                            "!",
                            morpherNames.size(),
                            usedChannels.size());

                        // if we got less names than expected, we fill the array with preset names
                        // {NodeName}_BlendShape
                        MaxUsd::UniqueNameGenerator nameGenerator;
                        const std::string           newBlendShapeName
                            = primPath.GetElementString() + "_BlendShape";
                        while (morpherNames.size() < usedChannels.size()) {
                            morpherNames.emplace_back(MaxUsd::UsdStringToMaxString(
                                                          nameGenerator.GetName(newBlendShapeName))
                                                          .data());
                        }
                    }

                    for (size_t i = 0; i < usedChannels.size(); ++i) {
                        auto animatablePB = static_cast<IParamBlock*>(
                            morpherProperties.morpher->SubAnim(usedChannels[i]));

                        // creates an extra mesh in the stage to calculate deltas. this mesh will be
                        // removed later
                        UsdGeomMesh           targetMeshPrim;
                        UsdSkelBlendShape     blendShape;
                        MaxUsd::MeshConverter targetMeshConverter;
                        {
                            // sets each morph channel to max weight, to export the equivalent
                            // blendshape then bring it back to 0 weight once it's done exporting,
                            // to export the next channel
                            const auto resetChannelWeightScopeGuard = MaxUsd::MakeScopeGuard(
                                [animatablePB, this, &startTime]() {
                                    animatablePB->SetValue(
                                        0, startTime, morpherProperties.maxLimit);
                                },
                                [animatablePB, &startTime]() {
                                    animatablePB->SetValue(0, startTime, 0);
                                });

                            if (!skinModifiers.empty() && GetExportArgs().GetTranslateSkin()) {
                                targetMeshPrim = DisabledModsAndWriteMeshData(
                                    sourceNode,
                                    stage,
                                    primPath.ReplaceName(TfToken(
                                        primPath.GetElementString()
                                        + pxr::TfMakeValidIdentifier(MaxUsd::GenerateGUID()))),
                                    applyOffsetTransform,
                                    MaxUsd::ExportTime {
                                        startTime, pxr::UsdTimeCode::Default(), true },
                                    false);
                            } else {
                                // if not exporting skin, we can take the mesh as is to create
                                // blendshapes
                                targetMeshPrim = targetMeshConverter.ConvertToUSDMesh(
                                    sourceNode,
                                    stage,
                                    primPath.ReplaceName(TfToken(
                                        primPath.GetElementString()
                                        + pxr::TfMakeValidIdentifier(MaxUsd::GenerateGUID()))),
                                    meshConvertOptions,
                                    applyOffsetTransform,
                                    false,
                                    MaxUsd::ExportTime {
                                        startTime, pxr::UsdTimeCode::Default(), true });
                            }
                            blendShape
                                = CreateBlendShape(skinnedMesh, targetMeshPrim, morpherNames[i]);
                        }

                        CreateInBetweens(
                            sourceNode, skinnedMesh, static_cast<int>(i), blendShape, startTime);
                        morphPaths.emplace_back(blendShape.GetPath());
                        std::string blendShapePathString = blendShape.GetPath().GetString();
                        std::replace(
                            blendShapePathString.begin(), blendShapePathString.end(), '/', '_');
                        morphTokens.emplace_back(blendShapePathString);

                        // clean up the extra mesh created to calculate blendshapes delta
                        stage->RemovePrim(targetMeshPrim.GetPath());
                    }
                }

                if (!binding.GetBlendShapesAttr().Set(morphTokens)) {
                    MaxUsd::Log::Error(
                        "Couldn't create BlendShape attribute for {} !", targetPrimName);
                }

                if (!binding.GetBlendShapeTargetsRel().SetTargets(morphPaths)) {
                    MaxUsd::Log::Error(
                        "Couldn't create BlendShape Target relationship for {} !", targetPrimName);
                }

                const UsdAttribute blendShapeAnimAttr = skelAnimation.GetBlendShapesAttr();
                VtTokenArray       existingBlendShapes;
                blendShapeAnimAttr.Get(&existingBlendShapes);
                for (const auto& bsToken : morphTokens) {
                    existingBlendShapes.emplace_back(bsToken);
                }

                if (!skelAnimation.GetBlendShapesAttr().Set(existingBlendShapes)) {
                    MaxUsd::Log::Error(
                        "Error setting BlendShapes to the animation token {} !",
                        skelAnimation.GetPath().GetString());
                }
            }
        }

        if (!skinModifiers.empty() && GetExportArgs().GetTranslateSkin()
            && (skinModifiers[0]->GetNumBones() > 0)) {
            if (skinModifiers.size() > 1) {
                MaxUsd::Log::Warn(
                    "Only one skin modifier per object is supported on export. Node {} contains "
                    "more than one !",
                    MaxUsd::MaxStringToUsdString(sourceNode->GetName()));
            }

            ISkin* skin = skinModifiers[0];
            auto   skinBindTransform = MaxUsd::GetBindTransform(
                MaxUsd::BindTransformElement::Mesh,
                sourceNode,
                skin,
                GetExportArgs().GetUpAxis() == MaxUsd::USDSceneBuilderOptions::UpAxis::Y,
                meshConvertOptions.GetBakeObjectOffsetTransform());
            binding.GetGeomBindTransformAttr().Set(skinBindTransform);

            VtTokenArray jointsPaths;
            for (int i = 0; i < skin->GetNumBones(); ++i) {
                // append bones as they are listed on the skin modifier
                INode* boneNode = skin->GetBone(i);
                if (boneNode) {
                    if (nodesToPrims.find(boneNode) != nodesToPrims.end()) {
                        const auto& jointPath = nodesToPrims.at(boneNode);
                        if (jointPath.GetCommonPrefix(skelRootPath) == SdfPath { "/" }) {
                            MaxUsd::Log::Error(
                                "Max Node {} is trying to use an invalid root path {} for UsdSkel "
                                "data. "
                                "Set a valid root prim to export UsdSkelRoot.",
                                MaxUsd::MaxStringToUsdString(sourceNode->GetName()),
                                skelRootPath.GetString());
                            return false;
                        }
                        const auto jointSubPath = jointPath.MakeRelativePath(skelRootPath);
                        jointsPaths.emplace_back(skelPath.AppendPath(jointSubPath).GetAsToken());
                    } else {
                        MaxUsd::Log::Warn(
                            "Prim {} relies on bone {} which is not being exported. Results might "
                            "not be correct!",
                            targetPrimName,
                            MaxUsd::MaxStringToUsdString(boneNode->GetName()));
                    }
                }
            }

            VtIntArray        jointsIndicesArray;
            VtFloatArray      jointsWeightsArray;
            ISkinContextData* skinData = skin->GetContextInterface(sourceNode);
            if (skinData) {
                const unsigned long long numOfPoints = skinData->GetNumPoints();
                const int                numOfJoints = skin->GetNumBones();
                jointsIndicesArray.resize(numOfPoints * numOfJoints);
                jointsWeightsArray.resize(numOfPoints * numOfJoints);
                for (unsigned int i = 0; i < numOfPoints; ++i) {
                    int numOfAssignedB = skinData->GetNumAssignedBones(i);
                    for (int j = 0; j < numOfJoints; ++j) {
                        jointsIndicesArray[i * numOfJoints + j] = j; // boneIndex;
                    }

                    for (int b = 0; b < numOfAssignedB; ++b) {
                        const int   boneIndex = skinData->GetAssignedBone(i, b);
                        const float boneWeight = skinData->GetBoneWeight(i, b);
                        if (boneIndex >= 0) {
                            jointsWeightsArray[i * numOfJoints + boneIndex] = boneWeight;
                        }
                    }
                }
            }

            pxr::VtVec3fArray vertices;
            skinnedMesh.GetPointsAttr().Get(&vertices);
            const unsigned long long numberOfMeshVertices = vertices.size();
            const int                numberOfSkinVertices = skinData->GetNumPoints();
            if (numberOfMeshVertices != numberOfSkinVertices) {
                MaxUsd::Log::Error(
                    "The number of vertices on the exported mesh differs from the vertices on the "
                    "skin "
                    "modifier for "
                    "node {}! This could be caused by a modifier higher on the stack and may cause "
                    "unexpected results.",
                    MaxUsd::MaxStringToUsdString(sourceNode->GetName()));

                const int diff
                    = std::abs(static_cast<int>(numberOfMeshVertices) - numberOfSkinVertices);
                if (numberOfMeshVertices > numberOfSkinVertices) {
                    // adding a default weight for the padded vertices
                    const float defaultWeight = 1.0f / (float)skin->GetNumBones();
                    for (unsigned long long i = 0; i < diff; ++i) {
                        for (int j = 0; j < skin->GetNumBones(); ++j) {
                            jointsIndicesArray.emplace_back(0);
                            jointsWeightsArray.emplace_back(defaultWeight);
                        }
                    }
                } else {
                    const auto deltaSize = diff * skin->GetNumBones();
                    jointsIndicesArray.resize(jointsIndicesArray.size() - deltaSize);
                    jointsWeightsArray.resize(jointsWeightsArray.size() - deltaSize);
                }
            }

            if (!UsdSkelSortInfluences(
                    &jointsIndicesArray, &jointsWeightsArray, skin->GetNumBones())) {
                MaxUsd::Log::Warn("Couldn't sort influences for {} !", targetPrimName);
            }

            if (!binding.GetJointsAttr().Set(jointsPaths)) {
                MaxUsd::Log::Error("Couldn't set joints attribute for {} !", targetPrimName);
            }

            UsdGeomPrimvar jointsIndexAttribute
                = binding.CreateJointIndicesPrimvar(false, skin->GetNumBones());
            if (!jointsIndexAttribute.Set(jointsIndicesArray)) {
                MaxUsd::Log::Error(
                    "Couldn't set indices attribute for {} !", targetPrim.GetName().GetString());
            }

            UsdGeomPrimvar jointsWeightAttribute
                = binding.CreateJointWeightsPrimvar(false, skin->GetNumBones());
            pxr::UsdSkelNormalizeWeights(jointsWeightsArray, skin->GetNumBones());
            if (!jointsWeightAttribute.Set(jointsWeightsArray)) {
                MaxUsd::Log::Error(
                    "Couldn't set joints weights attribute for {} !",
                    targetPrim.GetName().GetString());
            }
        } // end if(!skinModifiers.empty())
    } // end if (time.IsFirstFrame())

    // if morpher is valid, we need to write the weights changes over time
    if (morpherProperties.morpher) {
        WriteMorphWeightAnimations(targetPrim, time);
    }

    return true;
}

bool MaxUsdSkinMorpherWriter::PostExport(UsdPrim& targetPrim)
{
    if (!skinnedMesh.GetPrim().IsValid()) {
        MaxUsd::Log::Error(
            "PostExport didn't have a valid mesh for prim {} !", targetPrim.GetName().GetString());
        return false;
    }

    if (!skeleton.GetPrim().IsValid()) {
        MaxUsd::Log::Error(
            "PostExport didn't have a valid UsdSkeleton for prim {} !",
            targetPrim.GetName().GetString());
        return false;
    }

    if (!skelAnimation.GetPrim().IsValid()) {
        MaxUsd::Log::Error(
            "PostExport didn't have a valid UsdSkelAnimation for prim {} !",
            targetPrim.GetName().GetString());
        return false;
    }

    // USD skinned meshes can't have extent attribute
    skinnedMesh.GetExtentAttr().Clear();

    const UsdStagePtr stage = targetPrim.GetStage();
    const SdfPath     targetPath = targetPrim.GetPath();
    const SdfPath     skelRootPath = MaxUsd::VerifyOrMakeSkelRoot(stage, targetPath);
    const UsdSkelRoot skelRoot = UsdSkelRoot(stage->GetPrimAtPath(skelRootPath));

    auto         predicates = !pxr::UsdPrimIsAbstract && pxr::UsdPrimIsDefined;
    UsdPrimRange primRange(
        stage->GetPrimAtPath(skelRootPath), pxr::UsdTraverseInstanceProxies(predicates));

    std::vector<GfRange3d>              cachedRanges;
    const std::vector<pxr::UsdTimeCode> usdTimeSamples
        = MaxUsd::GetUsdTimeSamplesForExport(stage, GetExportArgs().GetResolvedTimeConfig());
    cachedRanges.reserve(usdTimeSamples.size());

    UsdAttribute skelRootExtents;
    // only calculate bounding box for all prims once
    if (!skelRoot.GetExtentAttr().IsAuthored() && !usdTimeSamples.empty()) {
        skelRootExtents = skelRoot.CreateExtentAttr();
        static const auto includePurposes
            = { UsdGeomTokens->default_, UsdGeomTokens->renderVisibility, UsdGeomTokens->proxy };
        for (size_t i = 0; i < usdTimeSamples.size(); ++i) {
            const pxr::UsdTimeCode& timeCode = usdTimeSamples[i];
            UsdGeomBBoxCache        tmpCache { timeCode, includePurposes, true };

            // calculate the total cached extent on the current usdTimeCode
            GfRange3d extentAtTime {};
            for (const pxr::UsdPrim& prim : primRange) {
                if (prim.IsA<pxr::UsdGeomImageable>()) {
                    auto bbox = tmpCache.ComputeRelativeBound(prim, skelRoot.GetPrim());
                    extentAtTime.UnionWith(bbox.ComputeAlignedRange());
                }
            }

            VtVec3fArray skelExtents(2);
            if (UsdGeomBoundable::ComputeExtentFromPlugins(skelRoot, timeCode, &skelExtents)) {
                GfRange3d skelRange = { skelExtents[0], skelExtents[1] };
                extentAtTime.UnionWith(skelRange);
            }

            // convert back from range to extent (array) to set  the attribute
            VtVec3fArray totalExtents
                = { GfVec3f(extentAtTime.GetMin()), GfVec3f(extentAtTime.GetMax()) };
            skelRootExtents.Set(totalExtents, timeCode);
        }
    }

    return true;
}

Interval MaxUsdSkinMorpherWriter::GetValidityInterval(const TimeValue& time)
{
    return Interval(time, time);
}

MaxUsdSkinMorpherWriter::ContextSupport
MaxUsdSkinMorpherWriter::CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    if (!exportArgs.GetTranslateMeshes()) {
        return ContextSupport::Unsupported;
    }

    if (!exportArgs.GetTranslateSkin() && !exportArgs.GetTranslateMorpher()) {
        return ContextSupport::Unsupported;
    }

    // For now, only support skinning of object that can/should be converted to meshes, exclude
    // shapes.
    const auto obj = node->EvalWorldState(exportArgs.GetResolvedTimeConfig().GetStartTime()).obj;
    const bool meshConvertible = obj->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0))
        || obj->CanConvertToType(Class_ID(POLYOBJ_CLASS_ID, 0));
    if (!meshConvertible || obj->SuperClassID() == SHAPE_CLASS_ID) {
        return ContextSupport::Unsupported;
    }

    const auto allEnabledMods = MaxUsd::GetAllModifiers(node);
    ISkin*     firstSkinFound = nullptr;
    Modifier*  firstMorpherFound = nullptr;

    for (const auto& mod : allEnabledMods) {
        if (mod->ClassID() == Class_ID(0x17bb6854, 0xa5cba2a3) && !firstMorpherFound) {
            // only cache the first morpher on the stack
            firstMorpherFound = mod;
        } else if (auto* skin = static_cast<ISkin*>(mod->GetInterface(I_SKIN))) {
            firstSkinFound = skin;

            // We may not be able to match the results in USD if a morpher is on top the of the skin
            // modifier in max
            if (firstMorpherFound) {
                MaxUsd::Log::Warn(
                    L"The node {} has a {} modifier on top of the {} modifier. USD results may not "
                    L"match "
                    "3ds Max scene!",
                    node->GetName(),
                    firstMorpherFound->GetName(true).data(),
                    mod->GetName(true).data());
            }
        }
    }

    // we only check the first found skin modifier
    if (firstSkinFound) {
        if (firstSkinFound->GetNumBones() > 0) {
            return ContextSupport::Fallback;
        }

        // don't exit here because there could still be valid morphers
        MaxUsd::Log::Warn(
            "The node {} has a skin modifier, but no bones on it!",
            MaxUsd::MaxStringToUsdString(node->GetName()));
    }

    return firstMorpherFound ? ContextSupport::Fallback : ContextSupport::Unsupported;
}

pxr::UsdGeomMesh MaxUsdSkinMorpherWriter::DisabledModsAndWriteMeshData(
    INode*                    node,
    UsdStageWeakPtr           stage,
    const pxr::SdfPath&       primPath,
    bool                      applyOffsetTransform,
    const MaxUsd::ExportTime& time,
    bool                      writeWarning)
{
    // Some modifiers that might be found higher on the stack may alter the geometry,
    // which will render our skinning data useless.
    // Get rid of some of them that will always create issues (temporarily disable).
    // Other mods may still alter the geometry, in those cases, we warn the user and
    // produce a "best effort" result which may or not be correct.
    const auto             allEnabledMods = MaxUsd::GetAllModifiers(node);
    std::vector<Modifier*> disabledMods;

    const auto scopeGuard = MaxUsd::MakeScopeGuard(
        [&allEnabledMods, node, &disabledMods, writeWarning]() {
            // Mods are ordered, starting with the top of the stack (WSMs come first).
            for (const auto& mod : allEnabledMods) {
                // When we get to the skin modifier, we can stop. Note that we do not handle cases
                // with multiple skin modifiers, in that case a warning is raised earlier.
                if (static_cast<ISkin*>(mod->GetInterface(I_SKIN))) {
                    // we need to export the mesh without the skin modifier as well.
                    // this removes any initial deformations to the meshes when binding to the skin
                    // modifier
                    mod->DisableMod();
                    disabledMods.push_back(mod);

                    break;
                }

                if (_IsModOkAfterSkin(mod)) {
                    continue;
                }

                // Disable the modifier, and keep track of it so we can re-enable it after
                // the mesh export. Warn the user.
                mod->DisableMod();
                disabledMods.push_back(mod);

                if (writeWarning) {
                    MaxUsd::Log::Warn(
                        L"Node {} has a {} modifier on top of the Skin modifier. This would alter "
                        L"the "
                        L"geometry and make the "
                        L"skinning points invalid when translated to USDSkel. The modifier will be "
                        L"disabled temporarily.",
                        node->GetName(),
                        mod->GetName(false).data());
                }
            }
        },
        [&disabledMods]() {
            // Reenable any modifiers we disabled for the export.
            for (const auto& mod : disabledMods) {
                mod->EnableMod();
            }
        });

    auto meshConvertOptions = GetExportArgs().GetMeshConversionOptions();
    meshConvertOptions.SetPrimvarLayoutInference(
        MaxUsd::MaxMeshConversionOptions::PrimvarLayoutInference::Never);
    MaxUsd::MeshConverter meshConverter;
    UsdGeomMesh           prim = meshConverter.ConvertToUSDMesh(
        node,
        stage,
        primPath,
        meshConvertOptions,
        applyOffsetTransform,
        false,
        MaxUsd::ExportTime { time.GetMaxTime(), pxr::UsdTimeCode::Default(), true });

    return prim;
}

float MaxUsdSkinMorpherWriter::GetMorpherWeightAtTime(IParamBlock* pb, const TimeValue& timeValue)
    const
{
    if (!pb) {
        return 0.f;
    }

    const float pbWeight = pb->GetFloat(0, timeValue);
    if (morpherProperties.useLimits) {
        if (pbWeight >= morpherProperties.minLimit && pbWeight <= morpherProperties.maxLimit) {
            return pbWeight;
        }

        if (pbWeight > morpherProperties.maxLimit) {
            return morpherProperties.maxLimit;
        }

        if (pbWeight < morpherProperties.minLimit) {
            return morpherProperties.minLimit;
        }
    }

    return pbWeight;
}

void MaxUsdSkinMorpherWriter::GetMorpherNames(INode* node, std::vector<std::wstring>& morpherNames)
{
    static const TSTR getMorpherNamesScript = LR"(
	fn extractMorpherNames nodeHandle =
	(
	    local node = maxOps.getNodeByHandle nodeHandle
	    local morphNames = #()
	    modi = (getModifierByClass node Morpher)
	    if iskindof modi Modifier and IsValidMorpherMod modi do
	    (
	        -- each morpher has 100 channels (possible morph targets), iterate over all those and check which has a valid data/morph target
	        for channelNumber = 1 to (WM3_NumberOfChannels modi) where WM3_MC_IsValid modi channelNumber and WM3_MC_HasData modi channelNumber do
	        (
	            -- cache the channel's name to use as the USD Blendshape name
	            append morphNames ( WM3_MC_GetName modi channelNumber)
	        )
	    )
	    return morphNames
	)
	extractMorpherNames )";

    std::wstringstream ss;
    ss << getModifierByClassScript << getMorpherNamesScript << node->GetHandle() << L'\n' << L'\0';

    FPValue rvalue;
    // MAXScript::ScriptSource::Dynamic doesn't exist for max 2022, so using
    // static_cast<MAXScript::ScriptSource>(3) as hack for now
    if (bool executeReturn
        = ExecuteMAXScriptScript(
              ss.str().c_str(), static_cast<MAXScript::ScriptSource>(3), false, &rvalue)
            && rvalue.type == TYPE_STRING_TAB) {
        Tab<const wchar_t*>* morphersNamesArray = rvalue.s_tab;
        for (int i = 0; morphersNamesArray && i < morphersNamesArray->Count(); ++i) {
            morpherNames.emplace_back(morphersNamesArray->begin()[i]);
        }
    } else {
        MaxUsd::Log::Error(
            L"Error running script to acquiring Morpher channel names for Node {}",
            node->GetName());
    }
}

UsdSkelBlendShape MaxUsdSkinMorpherWriter::CreateBlendShape(
    const UsdGeomMesh&  sourceMesh,
    const UsdGeomMesh&  targetMesh,
    const std::wstring& name)
{
    const auto nodePrim = sourceMesh.GetPrim();
    auto       stage = nodePrim.GetStage();

    VtVec3fArray sourceMeshPoints, targetMeshPoints, sourceNormals, targetNormals, deltaPoints,
        deltaNormals;

    sourceMesh.GetPointsAttr().Get(&sourceMeshPoints);
    targetMesh.GetPointsAttr().Get(&targetMeshPoints);

    const auto hasNormalsPrimvar
        = pxr::UsdGeomPrimvarsAPI(nodePrim).HasPrimvar(pxr::UsdImagingTokens->primvarsNormals);
    if (hasNormalsPrimvar) {
        pxr::UsdGeomPrimvarsAPI sourcePrimvarApi(nodePrim);
        pxr::UsdGeomPrimvarsAPI targetPrimvarApi(targetMesh);
        sourcePrimvarApi.GetPrimvar(pxr::UsdImagingTokens->primvarsNormals)
            .GetAttr()
            .Get(&sourceNormals);
        targetPrimvarApi.GetPrimvar(pxr::UsdImagingTokens->primvarsNormals)
            .GetAttr()
            .Get(&targetNormals);
    } else {
        sourceMesh.GetNormalsAttr().Get(&sourceNormals);
        targetMesh.GetNormalsAttr().Get(&targetNormals);
    }

    const size_t numDeltaPoints = sourceMeshPoints.size();
    deltaPoints.resize(numDeltaPoints);
    deltaNormals.resize(sourceNormals.size());

    for (size_t i = 0; i < numDeltaPoints; ++i) {
        deltaPoints[i] = targetMeshPoints[i] - sourceMeshPoints[i];
    }

    for (size_t i = 0; i < sourceNormals.size(); ++i) {
        deltaNormals[i] = targetNormals[i] - sourceNormals[i];
    }

    // new blendshape prim to represent the morph target
    UsdSkelBlendShape bs = MaxUsd::VerifyOrMakePrimOfType<UsdSkelBlendShape>(
        stage,
        nodePrim.GetPath(),
        TfToken(TfMakeValidIdentifier(MaxUsd::MaxStringToUsdString(name.c_str()))));

    const auto         blendShapePath = bs.GetPath();
    const std::string& blendShapeName = blendShapePath.GetName();

    if (!bs.CreateOffsetsAttr().Set(deltaPoints)) {
        MaxUsd::Log::Error(
            "Couldn't create offset points attribute for BlendShape {} !", blendShapeName);
    }

    if (!bs.CreateNormalOffsetsAttr().Set(deltaNormals)) {
        MaxUsd::Log::Error(
            "Couldn't create offset normals attribute for BlendShape {} !", blendShapeName);
    }

    return bs;
}

void MaxUsdSkinMorpherWriter::CreateInBetweens(
    INode*                   sourceNode,
    const UsdGeomMesh&       sourceMeshPrim,
    int                      morpherIndex,
    const UsdSkelBlendShape& blendShape,
    TimeValue                startTime)
{
    const auto              nodePrim = sourceMeshPrim.GetPrim();
    const auto              primPath = nodePrim.GetPath();
    auto                    stage = nodePrim.GetStage();
    ProgressiveMorphersInfo progressiveMorpherInfo;
    _getProgressiveMorpherInfo(sourceNode, morpherIndex, progressiveMorpherInfo);

    // only has the main target mesh, no progressive morphers
    if (progressiveMorpherInfo.count <= 1) {
        return;
    }

    if (progressiveMorpherInfo.count != progressiveMorpherInfo.nodes.size()) {
        MaxUsd::Log::Error(
            "Can't convert some progressive morphers in channel {} for Node "
            "\"{}\" to USD. Most likely the nodes used for the progressive "
            "morphers have been deleted from the scene.",
            morpherIndex + 1,
            MaxUsd::MaxStringToUsdString(sourceNode->GetName()));
        return;
    }

    VtVec3fArray sourceMeshPoints, sourceNormals;
    sourceMeshPrim.GetPointsAttr().Get(&sourceMeshPoints);
    const auto hasNormalsPrimvar
        = pxr::UsdGeomPrimvarsAPI(nodePrim).HasPrimvar(pxr::UsdImagingTokens->primvarsNormals);
    _extractNormalsFromPrim(sourceMeshPrim, sourceNormals);

    for (size_t idx = 0; idx < progressiveMorpherInfo.nodes.size(); ++idx) {
        // export the progressive morpher node as a temporary usd mesh so that we cache the vertices
        // positions to calculate the deltas comparing with the original mesh
        const auto progressiveMorpherNode = progressiveMorpherInfo.nodes[idx];
        auto       meshConvertOptions = GetExportArgs().GetMeshConversionOptions();
        meshConvertOptions.SetPrimvarLayoutInference(
            MaxUsd::MaxMeshConversionOptions::PrimvarLayoutInference::Never);
        MaxUsd::MeshConverter meshConverter;
        UsdGeomMesh           progMorpherMesh = meshConverter.ConvertToUSDMesh(
            progressiveMorpherNode,
            stage,
            primPath.ReplaceName(TfToken(
                primPath.GetElementString() + pxr::TfMakeValidIdentifier(MaxUsd::GenerateGUID()))),
            meshConvertOptions,
            false,
            false,
            MaxUsd::ExportTime { startTime, pxr::UsdTimeCode::Default(), true });

        VtVec3fArray targetMeshPoints, targetNormals, deltaPoints, deltaNormals;
        progMorpherMesh.GetPointsAttr().Get(&targetMeshPoints);
        _extractNormalsFromPrim(progMorpherMesh, targetNormals);

        stage->RemovePrim(progMorpherMesh.GetPath());

        const size_t numDeltaPoints = sourceMeshPoints.size();
        deltaPoints.resize(numDeltaPoints);
        deltaNormals.resize(sourceNormals.size());

        for (size_t i = 0; i < numDeltaPoints; ++i) {
            deltaPoints[i] = targetMeshPoints[i] - sourceMeshPoints[i];
        }

        for (size_t i = 0; i < sourceNormals.size(); ++i) {
            deltaNormals[i] = targetNormals[i] - sourceNormals[i];
        }

        const float weight = progressiveMorpherInfo.weights[idx];
        if (weight < 100.f) {
            std::string nodeName = MaxUsd::MaxStringToUsdString(progressiveMorpherNode->GetName());

            // pxr::TfMakeValidIdentifier doesn't like numbers as the first character and will
            // replace those numbers with "_". This can completely change the order and weights of
            // inbetweens in the USD file. To avoid this, add an underscore to the name if the first
            // character is a number before calling TfMakeValidIdentifier.
            if (!nodeName.empty() && std::isdigit(nodeName[0])) {
                nodeName = "_" + nodeName;
            }

            UsdSkelInbetweenShape ib
                = blendShape.CreateInbetween(TfToken(TfMakeValidIdentifier(nodeName)));
            ib.SetOffsets(deltaPoints);
            ib.SetNormalOffsets(deltaNormals);
            ib.SetWeight(weight / 100.f);
        } else {
            blendShape.GetOffsetsAttr().Set(deltaPoints);
            blendShape.GetNormalOffsetsAttr().Set(deltaNormals);
        }
    }
}

void MaxUsdSkinMorpherWriter::WriteMorphWeightAnimations(
    const UsdPrim&            targetPrim,
    const MaxUsd::ExportTime& time) const
{
    const auto& timeVal = time.GetMaxTime();
    const auto& usdTimeCode = time.GetUsdTime();

    const SdfPath          primPath = targetPrim.GetPath();
    const auto             stage = targetPrim.GetStage();
    const SdfPath          skelRootPath = MaxUsd::VerifyOrMakeSkelRoot(stage, primPath);
    const UsdSkelSkeleton  skel(MaxUsd::VerifyOrMakePrimOfType<UsdSkelSkeleton>(
        stage, skelRootPath, GetExportArgs().GetBonesPrimName()));
    const UsdSkelAnimation anim(MaxUsd::VerifyOrMakePrimOfType<UsdSkelAnimation>(
        stage, skel.GetPath(), GetExportArgs().GetAnimationsPrimName()));

    pxr::VtFloatArray  shapeWeights;
    const UsdAttribute shapeWeightAttribute = anim.GetBlendShapeWeightsAttr();

    // If time is not default, need to check if the timecode already exists in the
    // shapeWeightAttribute
    if (usdTimeCode != UsdTimeCode::Default()) {
        std::vector<double> weightSamples;
        shapeWeightAttribute.GetTimeSamples(&weightSamples);

        const bool hasWeightsOnUsdTime
            = std::find(weightSamples.begin(), weightSamples.end(), usdTimeCode)
            != weightSamples.end();
        if (hasWeightsOnUsdTime) {
            shapeWeightAttribute.Get(&shapeWeights, usdTimeCode);
        }
    } else {
        shapeWeightAttribute.Get(&shapeWeights);
    }

    auto morpher = morpherProperties.morpher;
    for (int i = 1; i < morpher->NumSubs(); i++) {
        // animatable is only valid for valid morpher channels
        if (IParamBlock* animatablePB = static_cast<IParamBlock*>(morpher->SubAnim(i))) {
            // remap spinner values from Max weights to Usd equivalent. See MorpherProperties
            // comments
            shapeWeights.emplace_back(GetMorpherWeightAtTime(animatablePB, timeVal) / 100.f);
        }
    }

    shapeWeightAttribute.Set(shapeWeights, usdTimeCode);
}

PXR_NAMESPACE_CLOSE_SCOPE
