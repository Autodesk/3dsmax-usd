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
#pragma once

#include "ReadJobContext.h"

#include <MaxUsd/MaxUSDAPI.h>

PXR_NAMESPACE_OPEN_SCOPE
class UsdSkelAnimQuery;
class UsdSkelBlendShape;
class UsdSkelSkinningQuery;

/// \brief Provides helper functions for reading UsdSkel BlendShapes related prims.
struct MaxUsdTranslatorMorpher
{
    /**
     * \brief Import a UsdSkelBlendShape schema as a corresponding 3ds Max morpher modifier.
     * \param prim The UsdSkelBlendShape prim to import
     * \param maxNode The 3ds Max node to import the UsdSkelBlendShape prim to
     * \param context The context for the import job
     * \return true if the 3ds Max morpher modifier was properly created and imported
     */
    MaxUSDAPI static bool
    Read(const UsdPrim& prim, INode* maxNode, const MaxUsdReadJobContext& context);

    /**
     * \brief Configure the animation of the morpher weights based on the skinning and animation prims
     * \param skinningQuery The UsdSkelSkinningQuery for the skinning prim
     * \param animQuery The UsdSkelAnimQuery for the animation prim
     * \param maxNode The 3ds Max node that holds the Morpher that will have the animation configured
     * \param context The context for the import job
     * \return true if the animation was properly configured
     */
    MaxUSDAPI static bool ConfigureMorpherAnimations(
        const UsdSkelSkinningQuery& skinningQuery,
        const UsdSkelAnimQuery&     animQuery,
        INode*                      maxNode,
        const MaxUsdReadJobContext& context);

private:
    /**
     * \brief Helper function to clone a max node and removes the morpher modifier from the cloned node
     * \param morpherNode The node to be cloned
     * \return The new cloned node
     */
    static INode* CloneMorpherNode(INode* morpherNode);

    /**
     * \brief Helper function to add new morph targets to a morpher modifier based on the the blendShapeTargets passed.
     * \param morpherNode The node that has a morpher modifier
     * \param context The context for the import job
     * \param blendShapeTargets The list of blendShapeTargets to converted into new morpher channels
     * \return true if the morpher channels were properly added to the morpher modifier
     */
    static bool AddMorphTargets(
        INode*                      morpherNode,
        const MaxUsdReadJobContext& context,
        const SdfPathVector&        blendShapeTargets);

    /**
     * \brief Helper function to add new progressive morph targets
     * \param morpherNode The node that has a morpher modifier
     * \param originalScaledObject The original object that will be used to compare the new in betweens
     * \param originalClonedNode the node used to create the original morpher, it will be used to adjust the weights if necessary
     * \param blendShapePrim The UsdSkelBlendShape that also has the in-betweens
     * \param blendShapePointIndices The array of indices that will be offset on the mesh
     * \param morpherIndex The index of the morpher channel that will have the in-betweens added to it
     * \param inBetweenClonedNodes Array to cache temporary nodes used for the in-betweens, to be deleted after the process
     * \return true if the in-betweens were properly added to the morpher modifier; false otherwise
     */
    static bool AddAllInBetweens(
        INode*                   morpherNode,
        Object*                  originalScaledObject,
        INode*                   originalClonedNode,
        const UsdSkelBlendShape& blendShapePrim,
        const VtIntArray&        blendShapePointIndices,
        size_t                   morpherIndex,
        std::vector<INode*>&     inBetweenClonedNodes);

    // ============================================================================
    // Helper functions to call maxscript functions by passing c++ parameters.
    // The morpher API is only available through maxscript until 3ds Max 2025.
    // So, it's necessary to have several methods to configure the morpher modifier
    // ============================================================================

    /**
     * \brief Helper function to add a buid morph targets based on the cloned node given as parameters modifier to a
     * node.
     * \param maxNode The node to that will have a new morpher channel added to it's morpher modifier
     * \param clonedNode The node that will be used to build the new morpher channel to be added to the maxNode
     * \param morpherIndex The index of the morpher channel to be added to the maxNode
     * \return true if the morpher channel was properly added to the maxNode
     */
    static bool AddMorpherTargetScript(INode* maxNode, INode* clonedNode, size_t morpherIndex);

    /**
     * \brief Helper function to add a progressive morpher based on a node to a an already built morpher channel.
     * \param maxNode The node that has a morpher modifier that will have a progressive morpher added to it
     * \param clonedNode The node that will be used to build the progressive morpher to be added to the morpher
     * \param morpherIndex The index of the morpher channel that will have the progressive morpher added to it
     * \return true if the progressive morpher was properly added to the morpher channel
     */
    static bool AddProgressiveMorpherScript(INode* maxNode, INode* clonedNode, size_t morpherIndex);

    /**
     *
     * \brief Helper function to set the weight of a morpher channel.
     * \param maxNode The node that has a morpher modifier
     * \param weight The new weight of the morpher channel
     * \param morpherIndex The index of the morpher channel that will have the weight changed
     * \return true if the weight of the morpher channel was properly changed
     */
    static bool SetProgressiveMorpherWeightScript(
        INode* maxNode,
        INode* clonedNode,
        size_t morpherIndex,
        float  weight);

    /**
     * \brief Helper function to apply deltas offset to a max object.
     * \param originalScaledObject The original scaled object that will be used to get the initial position of the
     * vertices
     * \param clonedScaledObject The cloned object that will have the deltas applied to it
     * \param blendShapePointIndices The array of indices that will be offset on the mesh
     * \param blendShapeDeltaPoints The array of blendShapeDeltaPoints to converted into new morpher channels
     * \param scaleFactor scale factor to be applied to the vertices positions to account for usd's units difference
     * \param revertOffset If true, the deltas will be reverted instead of applied
     */
    static void ApplyDeltasOffset(
        Object*             originalScaledObject,
        Object*             clonedScaledObject,
        const VtIntArray&   blendShapePointIndices,
        const VtVec3fArray& blendShapeDeltaPoints,
        float               scaleFactor,
        bool                revertOffset = false);
};

PXR_NAMESPACE_CLOSE_SCOPE
