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
#include "ReadJobContext.h"

#include <MaxUsd/Utilities/TranslationUtils.h>

#include <inode.h>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdReadJobContext::MaxUsdReadJobContext(
    const MaxUsd::MaxSceneBuilderOptions& args,
    const UsdStageRefPtr&                 stage)
    : args(args)
    , referenceTargetMap(new ReferenceTargetRegistry)
    , prune(false)
    , stage(stage)
{
}

const MaxUsdReadJobContext::ReferenceTargetRegistry&
MaxUsdReadJobContext::GetReferenceTargetRegistry() const
{
    return *referenceTargetMap;
}

RefTargetHandle
MaxUsdReadJobContext::GetMaxRefTargetHandle(const SdfPath& path, bool findAncestors) const
{
    // Get Node parent
    for (SdfPath parentPath = path; !parentPath.IsEmpty();
         parentPath = parentPath.GetParentPath()) {
        // retrieve from a registry if it exists
        ReferenceTargetRegistry::const_iterator it = referenceTargetMap->find(parentPath);
        if (it != referenceTargetMap->end()) {
            return it->second;
        }

        if (!findAncestors) {
            break;
        }
    }
    return nullptr;
}

INode* MaxUsdReadJobContext::GetMaxNode(const SdfPath& path, bool findAncestors) const
{
    return dynamic_cast<INode*>(GetMaxRefTargetHandle(path, findAncestors));
}

void MaxUsdReadJobContext::RegisterNewMaxRefTargetHandle(
    const SdfPath&  path,
    RefTargetHandle maxNode)
{
    referenceTargetMap->insert({ path, maxNode });
}

void MaxUsdReadJobContext::RemoveNode(INode* node)
{
    ReferenceTargetRegistry::iterator iter = referenceTargetMap->begin();
    ReferenceTargetRegistry::iterator iterEnd = referenceTargetMap->end();
    for (; iter != iterEnd; ++iter) {
        if (iter->second == node) {
            break;
        }
    }
    if (iter != iterEnd) {
        referenceTargetMap->erase(iter);
    }
}

void MaxUsdReadJobContext::GetAllCreatedNodes(std::vector<INode*>& nodes) const
{
    for (const auto& item : *referenceTargetMap) {
        if (INode* node = dynamic_cast<INode*>(item.second)) {
            nodes.push_back(node);
        }
    }
}

bool MaxUsdReadJobContext::GetPruneChildren() const { return prune; }

/// Sets whether traversal should automatically continue into this prim's
/// children. This only has an effect if set during the
/// MaxUsdPrimReader::Read() step, and not in the
/// MaxUsdPrimReader::PostReadSubtree() step, since in the latter, the
/// children have already been processed.
void MaxUsdReadJobContext::SetPruneChildren(bool prune) { this->prune = prune; }

void MaxUsdReadJobContext::RescaleRegisteredNodes() const
{
    INodeTab tab;

    // Workaround : rescaling the world units of skin/bones is broken.
    // To work around the issue, save the bone properties toggle off the
    // bone-ness and reapply them once we are done.

    struct NodeBone
    {
        INode* node;
        BOOL   autoAlign = FALSE;
        BOOL   freezeLen = FALSE;
        INT    scaleType = BONE_SCALETYPE_NONE;
        BOOL   axisFlip = FALSE;
        INT    boneAxis = BONE_AXIS_Z;
        BOOL   showBone = FALSE;

        Quat offsetRot;
    };

    std::vector<NodeBone> boneNodes;
    for (auto it = referenceTargetMap->begin(); it != referenceTargetMap->end(); it++) {
        if (INode* node = dynamic_cast<INode*>(it->second)) {
            tab.Append(1, &node);

            if (node->GetBoneNodeOnOff()) {
                boneNodes.emplace_back(NodeBone { node,
                                                  node->GetBoneAutoAlign(),
                                                  node->GetBoneFreezeLen(),
                                                  node->GetBoneScaleType(),
                                                  node->GetBoneAxisFlip(),
                                                  node->GetBoneAxis(),
                                                  node->IsBoneShowing(),
                                                  node->GetObjOffsetRot() });

                node->SetBoneNodeOnOff(FALSE, 0);
            }
        }
    }

    GetCOREInterface()->ClearNodeSelection();
    GetCOREInterface()->SelectNodeTab(tab, TRUE, FALSE);
    GetCOREInterface()->RescaleWorldUnits(
        static_cast<float>(MaxUsd::GetUsdToMaxScaleFactor(stage)), true);

    for (auto boneNode : boneNodes) {
        const auto node = boneNode.node;
        node->SetBoneNodeOnOff(TRUE, 0);
        node->SetBoneAutoAlign(boneNode.autoAlign);
        node->SetBoneFreezeLen(boneNode.freezeLen);
        node->SetBoneScaleType(boneNode.scaleType);
        node->SetBoneAxisFlip(boneNode.axisFlip);
        node->SetBoneAxis(boneNode.boneAxis);
        node->ShowBone(boneNode.showBone);
        node->SetObjOffsetRot(boneNode.offsetRot);
    }

    for (const auto bone : boneNodes) {
        bone.node->ResetBoneStretch(0);
    }

    for (const auto bone : boneNodes) {
        bone.node->SetObjOffsetRot(bone.offsetRot);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
