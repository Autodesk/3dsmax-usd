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

#include "MaxSceneBuilderOptions.h"

#include <MaxUsd/Translators/PrimReader.h>
#include <MaxUsd/Translators/ReadJobContext.h>

#include <pxr/usd/usd/primRange.h>

#include <GetCOREInterface.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief 3ds Max Scene Builder.
 * \remarks This current implementation is a work-in-progress that will evolve as additional conversion operations
 * between USD and 3ds Max are supported. Performance of the import process is a design concern, and
 * while CRTP-type solutions are not (currently) implemented, future work should attempt to
 * improve/maintain run-time performance while maintaining a high level of flexibility.
 * \remarks This current implementation moves some of the import logic away from the USDSceneController where it was
 * previously located. In the process, the import still owns some of the UI/UX import process such
 * as the handling of 3ds Max's progress bar. Future work should abstract away this behavior, and
 * expose more control to the caller (e.g. through callbacks, or notifications about the current
 * state of the import process, etc.).
 */
class MaxUSDAPI MaxSceneBuilder
{
public:
    /**
     * \brief Constructor.
     */
    MaxSceneBuilder();

    /**
     * \brief Destructor.
     */
    virtual ~MaxSceneBuilder() = default;

    /**
     * \brief Start the scene building process.
     * \param node 3ds Max Node from which to start building the Scene.
     * \param prim USD Prim from which to start building the Scene.
     * \param buildOptions Options for the translation of USD content into 3ds Max content.
     * \param filename The filename of the USD file being used to build to the max scene.
     * \return IMPEXP_FAIL if failed, IMPEXP_SUCCESS if success and IMPEXP_CANCEL if canceled by user
     */
    int Build(
        INode*                        node,
        const pxr::UsdPrim&           prim,
        const MaxSceneBuilderOptions& buildOptions,
        const fs::path&               filename = "");

protected:
    typedef std::unordered_map<pxr::SdfPath, pxr::MaxUsdPrimReaderSharedPtr, pxr::SdfPath::Hash>
        PrimReaderMap;
    typedef std::unordered_map<INode*, pxr::SdfPath>
        NodeToPrimMap; // inverse lookup of PrimReaderMap
    // Structure used to correlate the prototype nodes and their clones to the original prototype
    // prim they originate from
    struct PrototypeLookupMaps
    {
        PrimReaderMap
            prototypeReaderMap; // mapping the prototype path to the reader used to import it
        NodeToPrimMap nodeToPrototypeMap; // mapping 3ds Max Node to the original prototype path it
                                          // got created from
    };

    /**
     * \brief Create the prim's 3ds Max Node.
     * \param primIt PrimRange iterator on the UsdPrim to import
     * \param readCtx The read job context being used in the current import job
     * \param primReaderMap The map between the imported prim path and its reader.
     */
    void DoImportPrimIt(
        pxr::UsdPrimRange::iterator& primIt,
        pxr::MaxUsdReadJobContext&   readCtx,
        PrimReaderMap&               primReaderMap);

    /**
     * \brief Creates the prim's 3ds Max Node instances upon creating first the associated prototype.
     * \param primIt PrimRange iterator on the UsdPrim to import
     * \param readCtx The read job context being used in the current import job
     * \param protypeLookupMaps Maps to correlate the prototype nodes and their clones
     * to the original prototype prim they originate from
     * \param insidePrototype Report if instancing from within a prototype
     */
    void DoImportInstanceIt(
        pxr::UsdPrimRange::iterator& primIt,
        pxr::MaxUsdReadJobContext&   readCtx,
        PrototypeLookupMaps&         prototypeLookupMaps,
        bool                         insidePrototype = false);

    /**
     * \brief Imports the prototype prim (and descendants) and adds it (them) to the read context
     * \param prototype The prim serving as prototype to import
     * \param readCtx The read job context being used in the current import job.
     * \param protypeLookupMaps Maps to correlate the prototype nodes and their clones
     * to the original prototype prim they originate from.
     */
    void ImportPrototype(
        const pxr::UsdPrim&        prototype,
        pxr::MaxUsdReadJobContext& readCtx,
        PrototypeLookupMaps&       protypeLookupMaps);

    /**
     * \brief Small helper to exclude some prim types from being handled by PrimReaders
     * \param primIt PrimRange iterator on the UsdPrim to import
     * \return True if the prim type is to be excluded, false otherwise
     */
    bool ExcludedPrimNode(pxr::UsdPrimRange::iterator& primIt);

    /// Reference to the Core Interface to use to interface with 3ds Max:
    Interface17* coreInterface { GetCOREInterface17() };
};

} // namespace MAXUSD_NS_DEF
