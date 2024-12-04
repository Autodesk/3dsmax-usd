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
#include <MaxUsd/Translators/PrimReader.h>
#include <MaxUsd/Translators/PrimReaderRegistry.h>
#include <MaxUsd/Translators/TranslatorMorpher.h>
#include <MaxUsd/Translators/TranslatorPrim.h>
#include <MaxUsd/Translators/TranslatorSkel.h>
#include <MaxUsd/Translators/TranslatorUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/usdSkel/cache.h>
#include <pxr/usd/usdSkel/root.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <pxr/usd/usdSkel/skeletonQuery.h>

#include <inode.h>

PXR_NAMESPACE_OPEN_SCOPE

class MaxUsdSkeletonReader : public MaxUsdPrimReader
{
public:
    MaxUsdSkeletonReader(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx)
        : MaxUsdPrimReader(prim, jobCtx)
    {
    }

    ~MaxUsdSkeletonReader() override { }

    bool Read() override;

private:
    UsdSkelCache skelCache;
};

TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimReaderRegistry, UsdSkelSkeleton)
{
    MaxUsdPrimReaderRegistry::Register<UsdSkelSkeleton>(
        [](const UsdPrim& prim, MaxUsdReadJobContext& jobCtx) {
            return std::make_shared<MaxUsdSkeletonReader>(prim, jobCtx);
        });
}

bool MaxUsdSkeletonReader::Read()
{
    const auto            prim = GetUsdPrim();
    const UsdSkelSkeleton skel(prim);

    if (const UsdSkelSkeletonQuery skeletonQuery = skelCache.GetSkelQuery(skel)) {
        // Nothing to do if there are no joints (could be just morpher)
        if (skeletonQuery.GetJointOrder().empty()) {
            return false;
        }

        MaxUsdTranslatorUtil::CreateDummyHelperNode(prim, prim.GetName(), GetJobContext());
        INode* parentNode = GetJobContext().GetMaxNode(prim.GetPath(), false);

        std::vector<INode*> jointsHierarchy;
        return MaxUsdTranslatorSkel::CreateJointHierarchy(
            skeletonQuery, parentNode, GetJobContext(), jointsHierarchy);
    }

    return false;
}

class MaxUsdSkelRootReader : public MaxUsdPrimReader
{
public:
    MaxUsdSkelRootReader(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx)
        : MaxUsdPrimReader(prim, jobCtx)
    {
    }

    ~MaxUsdSkelRootReader() override { }

    bool Read() override;

    bool HasPostReadSubtree() const override { return true; }

    void PostReadSubtree() override;

private:
    UsdSkelCache skelCache;
};

TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimReaderRegistry, UsdSkelRoot)
{
    MaxUsdPrimReaderRegistry::Register<UsdSkelRoot>(
        [](const UsdPrim& prim, MaxUsdReadJobContext& jobCtx) {
            return std::make_shared<MaxUsdSkelRootReader>(prim, jobCtx);
        });
}

bool MaxUsdSkelRootReader::Read()
{
    const auto prim = GetUsdPrim();

    // Using PointHelper since SkelRoot are just transforms
    return MaxUsdTranslatorUtil::CreateDummyHelperNode(prim, prim.GetName(), GetJobContext());
}

void MaxUsdSkelRootReader::PostReadSubtree()
{
    UsdSkelRoot skelRoot(GetUsdPrim());

    // After creating all skelroot children prims, we need to find skinned prims

    std::vector<UsdSkelBinding> bindings;
    skelCache.Populate(skelRoot, UsdTraverseInstanceProxies());
    if (!skelCache.ComputeSkelBindings(skelRoot, &bindings, UsdTraverseInstanceProxies())) {
        return;
    }

    for (const UsdSkelBinding& binding : bindings) {
        if (binding.GetSkinningTargets().empty()) {
            // no skinned target. go to next binding
            continue;
        }

        if (const UsdSkelSkeletonQuery skelQuery = skelCache.GetSkelQuery(binding.GetSkeleton())) {
            std::vector<INode*> joints;
            MaxUsdTranslatorSkel::GetJointsNodes(skelQuery, GetJobContext(), joints);

            // skinningQuery are used for joints animation
            for (const auto& skinningQuery : binding.GetSkinningTargets()) {
                // fetch the skinned node that should have been created by another importer 'read'
                const UsdPrim& skinnedPrim = skinningQuery.GetPrim();
                INode* skinnedNode = GetJobContext().GetMaxNode(skinnedPrim.GetPath(), false);
                if (!skinnedNode) {
                    // This shouldn't fail because the 3dsMax node should've been created.
                    MaxUsd::Log::Error(
                        "Couldn't find max node for \"{}\".",
                        skinningQuery.GetPrim().GetPath().GetName());
                    continue;
                }

                MaxUsdTranslatorMorpher::ConfigureMorpherAnimations(
                    skinningQuery, skelQuery.GetAnimQuery(), skinnedNode, GetJobContext());

                // nothing to skin if there are no joints on this skinQuery
                // skinQuery could exist if there are blendshapes bound to a mesh as well
                if (!skinningQuery.HasJointInfluences()) {
                    continue;
                }

                // In USD a skinned mesh can have a different joint order from what it was in the
                // skeleton prim If there's a different mapping, we want to take out that
                // information for bones and transforms

                std::vector<INode*> skinningJoints;
                const auto&         mapper = skinningQuery.GetMapper();
                if (!mapper || mapper->IsNull()) {
                    skinningJoints = joints;
                } else {
                    VtIntArray indices(joints.size());
                    for (size_t i = 0; i < joints.size(); ++i) {
                        indices[i] = static_cast<int>(i);
                    }

                    VtIntArray remappedIndices;
                    if (!mapper->Remap(indices, &remappedIndices)) {
                        MaxUsd::Log::Error(
                            "Error remapping joint indices for \"{}\".",
                            skinningQuery.GetPrim().GetPath().GetName());
                        continue;
                    }

                    skinningJoints.resize(remappedIndices.size());
                    for (size_t i = 0; i < remappedIndices.size(); ++i) {
                        int index = remappedIndices[i];
                        if (index >= 0 && static_cast<size_t>(index) < joints.size()) {
                            skinningJoints[i] = joints[index];
                        }
                    }
                }

                VtMatrix4dArray bindXforms, remappedBindXforms;
                if (!skelQuery.GetJointWorldBindTransforms(&bindXforms)) {
                    MaxUsd::Log::Error(
                        "Error acquiring bind transforms to configure Skin modifier for \"{}\".",
                        skinningQuery.GetPrim().GetPath().GetName());
                    continue;
                }

                if (mapper && !mapper->IsNull()) {
                    if (!mapper->IsSparse()) {
                        mapper->RemapTransforms(bindXforms, &remappedBindXforms);
                    }
                }

                if (joints.size() > bindXforms.size()) {
                    MaxUsd::Log::Error(
                        "Found incorrect number of bind transforms for joints of \"{}\" .",
                        skinningQuery.GetPrim().GetPath().GetName());
                    continue;
                }

                MaxUsdTranslatorSkel::ConfigureSkinModifier(
                    skinningQuery,
                    skinnedNode,
                    GetJobContext(),
                    skinningJoints,
                    remappedBindXforms.empty() ? bindXforms : remappedBindXforms);
            }
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE