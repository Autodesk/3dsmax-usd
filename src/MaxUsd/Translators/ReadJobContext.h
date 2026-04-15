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

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>
#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/common.h>

#include <map>
#include <ref.h>
#include <set>

class Mtl;

PXR_NAMESPACE_OPEN_SCOPE

/// \class MaxUsdReadJobContext
/// \brief This class provides an interface for reader plugins to communicate
/// state back to the core usd 3ds Max logic as well as retrieve information set by
/// other plugins.
class MaxUsdReadJobContext
{
public:
    MaxUSDAPI
    MaxUsdReadJobContext(const MaxUsd::MaxSceneBuilderOptions& args, const UsdStageRefPtr& stage);

    MaxUSDAPI ~MaxUsdReadJobContext() = default;

    /**
     * \brief Returns the export arguments.
     * \return Export arguments
     */
    const MaxUsd::MaxSceneBuilderOptions& GetArgs() const { return args; }

    typedef std::map<SdfPath, RefTargetHandle> ReferenceTargetRegistry;
    MaxUSDAPI const ReferenceTargetRegistry&   GetReferenceTargetRegistry() const;

    /// \brief Returns the prim was registered at path.  If findAncestors
    /// is true and no object was found for path, this will return the object
    /// that corresponding to its nearest ancestor.
    ///
    /// Returns an invalid reference if no such object exists.
    MaxUSDAPI RefTargetHandle GetMaxRefTargetHandle(const SdfPath& path, bool findAncestors) const;
    MaxUSDAPI INode*          GetMaxNode(const SdfPath& path, bool findAncestors) const;

    /// \brief Record maxNode prim as being created path.
    MaxUSDAPI void RegisterNewMaxRefTargetHandle(const SdfPath& path, RefTargetHandle maxNode);

    /// \brief Retrieve all 3ds Max Node created during the import process
    MaxUSDAPI void GetAllCreatedNodes(std::vector<INode*>& nodes) const;

    /// \brief Remove a registered INode
    /// (to remove prototype Nodes that were created during the import process)
    MaxUSDAPI void RemoveNode(INode* node);

    /// \brief returns true if prim traversal of the children of the current
    /// node can be pruned.
    MaxUSDAPI bool GetPruneChildren() const;

    /// \brief If a reader takes care of reading all of its children, it
    /// should SetPruneChildren(true).
    MaxUSDAPI void SetPruneChildren(bool prune);

    MaxUSDAPI const UsdStageRefPtr GetStage() const { return stage; }

    /// \brief Rescale all nodes registered on the read job context.
    ///	The rescaling uses the stage units and the current max units to
    MaxUSDAPI void RescaleRegisteredNodes() const;

    /// \brief Register a 3ds Max material as being bound to geometry.
    /// This is used to efficiently determine which materials are unbound during import.
    /// \param maxMaterial The 3ds Max material that was created
    /// \param boundPrimPath The path of the prim the material is bound to
    MaxUSDAPI void RegisterBoundMaterial(Mtl* maxMaterial, const SdfPath& boundPrimPath);

    /// \brief Check if a 3ds Max material is bound to any geometry.
    /// \param maxMaterial The 3ds Max material to check
    /// \return true if the material is bound to geometry, false otherwise
    MaxUSDAPI bool IsMaterialBound(Mtl* maxMaterial) const;

    /// \brief Get the bound prim paths for a 3ds Max material.
    /// \param maxMaterial The 3ds Max material to check
    /// \return Reference to set of SdfPaths if found, to an empty set otherwise
    MaxUSDAPI const std::set<SdfPath>& GetBoundPrimPaths(Mtl* maxMaterial) const;

private:
    // Args for the import (any & all export options).
    const MaxUsd::MaxSceneBuilderOptions& args;

    // used to keep track of prims that are created.
    ReferenceTargetRegistry* referenceTargetMap;

    // is the current prim reader handling its descendants (pruning the scene tree)
    bool prune;

    // imported stage reference
    const UsdStageRefPtr stage;

    // Map of 3ds Max materials to their bound prim paths (one material can be bound to multiple
    // prims)
    std::map<Mtl*, std::set<SdfPath>> boundMaterials;
};

PXR_NAMESPACE_CLOSE_SCOPE
