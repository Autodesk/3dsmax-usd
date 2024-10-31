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
#pragma once

#include "ReadJobContext.h"

#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/usd/usdSkel/skeletonQuery.h>

PXR_NAMESPACE_OPEN_SCOPE
class UsdSkelSkeletonQuery;
class UsdSkelSkinningQuery;

/// \brief Provides helper functions for reading UsdSkel related prims.
struct MaxUsdTranslatorSkel
{
    /** \brief Creates 3ds Max bone nodes based on the skel joints given as parameters.
     * \param skelQuery reading interface for the usd skel information
     * \param hierarchyRootNode parent node where the new joints will be children of
     * \param context The current MaxUsdReadJobContext to register the 3ds Node to
     * \param joints the created joints will be added to this vector
     */
    MaxUSDAPI static bool CreateJointHierarchy(
        const UsdSkelSkeletonQuery& skelQuery,
        INode*                      hierarchyRootNode,
        MaxUsdReadJobContext&       context,
        std::vector<INode*>&        joints);

    /** \brief Gets the joints nodes that were created for the given skelQuery.
     * \param skelQuery reading interface for the usd skel information
     * \param context The current MaxUsdReadJobContext to register the 3ds Node to
     * \param joints the created joints will be added to this vector
     */
    MaxUSDAPI static bool GetJointsNodes(
        const UsdSkelSkeletonQuery& skelQuery,
        const MaxUsdReadJobContext& context,
        std::vector<INode*>&        joints);

    /** \brief Configure the skin modifier on the given skinnedNode.
     * \param skinQuery reading interface for the usd skel information
     * \param skinnedNode the 3ds Max node to have a skin modifier applied to
     * \param context The current MaxUsdReadJobContext
     * \param skinningJoints the 3ds max bone nodes that will be used and added to the skih modifier
     * \param bindTransforms Usd joints bind transforms
     */
    MaxUSDAPI static bool ConfigureSkinModifier(
        const UsdSkelSkinningQuery& skinQuery,
        INode*                      skinnedNode,
        const MaxUsdReadJobContext& context,
        std::vector<INode*>&        skinningJoints,
        const VtMatrix4dArray&      bindTransforms);

private:
    /**
     * \brief Internal helper method to created the 3ds Max bone nodes based on the skel joints given as parameters.
     * \param skelQuery reading interface for the usd skel information
     * \param skelContainer 3ds Max node that will have the newly created joints
     * \param context The current MaxUsdReadJobContext to register the 3ds Node to
     * \param joints array to hold a reference to all the newly created 3ds Max nodes
     * \return true if it succeed creating the joints
     */
    static bool CreateJointsNodes(
        const UsdSkelSkeletonQuery& skelQuery,
        INode*                      skelContainer,
        MaxUsdReadJobContext&       context,
        std::vector<INode*>&        joints);

    /**
     * \brief Copy the animations from the usd skel to the joint max bones
     * \param skelQuery reading interface for the usd skel information
     * \param skelContainer 3ds Max node that will have the newly created joints
     * \param context the current MaxUsdReadJobContext to register the 3ds Node to
     * \param joints 3ds Max nodes that will receive the animations from the skelQuery
     * \return true if it succeeded copying the animations from usd to the max joints; false otherwise
     */
    static bool CopyJointsAnimations(
        const UsdSkelSkeletonQuery& skelQuery,
        INode*                      skelContainer,
        const MaxUsdReadJobContext& context,
        const std::vector<INode*>&  joints);

    /** \brief Helper function to set the joint properties on the given skinnedNode.
     * Properties are size of the joints based on the distance from it's parents and the amount of
     * children.
     * \param skelQuery reading interface for the usd skel information
     * \param context The current MaxUsdReadJobContext
     * \param joints array of joints that will have the properties update.
     */
    static bool SetJointProperties(
        const UsdSkelSkeletonQuery& skelQuery,
        const MaxUsdReadJobContext& context,
        const std::vector<INode*>&  joints);
};

PXR_NAMESPACE_CLOSE_SCOPE