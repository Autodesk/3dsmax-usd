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

#include "MaxMeshConversionOptions.h"
#include "MeshFacade.h"
#include "PrimvarMappingOptions.h"

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/MappedAttributeBuilder.h>
#include <MaxUsd/MaxUSDAPI.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/subset.h>

#include <polyobj.h>
#include <stdmat.h>

namespace MAXUSD_NS_DEF {

/// MeshConverter Utility Class
///
/// Example of use:
/// ```cpp
/// spherePrim = maxUsd.MeshConverter.ConvertToUSDMesh(nodeHandle, stage, prim.GetPath(), opts,
/// applyOffset)
///	print("Write a Sphere as a USD Mesh prim!")
/// ```
class MaxUSDAPI MeshConverter
{
public:
    /**
     * \brief Geom channel intervals, contains the validity intervals of TOPO_CHAN_NUM, GEOM_CHAN_NUM,
     * TEXMAP_CHAN_NUM and VERT_COLOR_CHAN_NUM channels.
     */
    typedef std::unordered_map<int, Interval> ObjectChannelIntervals;

    /**
     * \brief Converts an INode carrying geometry to a UsdGeomMesh prim at a given time frame.
     * \param node The node to convert.
     * \param stage The stage in which the UsdGeomMesh will be created.
     * \param path The path to the output UsdGeomMesh.
     * \param options Mesh conversion options.
     * \param applyOffsetTransform Whether or not to apply the object offset transform onto the mesh.
     * \param animated True if the mesh is a being exported as part of an animation.
     * \param timeFrame The 3dsMax time to convert at, and associated target USD TimeCode.
     * \return The converted UsdGeomMesh.
     */
    pxr::UsdGeomMesh ConvertToUSDMesh(
        INode*                          node,
        const pxr::UsdStagePtr&         stage,
        const pxr::SdfPath&             path,
        const MaxMeshConversionOptions& options,
        bool                            applyOffsetTransform,
        bool                            animated,
        const MaxUsd::ExportTime&       timeFrame);

    /**
     * \brief Converts a MNMesh to a UsdGeomMesh prim.
     * \param maxMesh The mesh to convert.
     * \param stage The stage in which the UsdGeomMesh will be created.
     * \param path The path to the output UsdGeomMesh.
     * \param options Mesh conversion options.
     * \param usdMesh The output UsdGeomMesh.
     * \param timecode The USD timeCode at which the conversion happens.
     * \param materialIdToFacesMap Material mapping information.
     * \param animated Whether or not the conversion is part of an animation.
     * \param channelValidity The object channel validity intervals.
     */
    void ConvertToUSDMesh(
        MaxUsd::MeshFacade&               maxMesh,
        const pxr::UsdStagePtr&           stage,
        const pxr::SdfPath&               path,
        const MaxMeshConversionOptions&   options,
        pxr::UsdGeomMesh&                 usdMesh,
        const pxr::UsdTimeCode&           timecode,
        std::map<MtlID, pxr::VtIntArray>& materialIdToFacesMap,
        bool                              animated,
        const ObjectChannelIntervals&     channelValidity);
    /**
     * \brief Converts a MNMesh to a UsdGeomMesh prim. This overload was added to allow passing a temporary MeshFacade
     * as parameter.
     */
    void ConvertToUSDMesh(
        MaxUsd::MeshFacade&&              maxMesh,
        const pxr::UsdStagePtr&           stage,
        const pxr::SdfPath&               path,
        const MaxMeshConversionOptions&   options,
        pxr::UsdGeomMesh&                 usdMesh,
        const pxr::UsdTimeCode&           timecode,
        std::map<MtlID, pxr::VtIntArray>& materialIdToFacesMap,
        bool                              animated,
        const ObjectChannelIntervals&     channelValidity)
    {
        ConvertToUSDMesh(
            maxMesh,
            stage,
            path,
            options,
            usdMesh,
            timecode,
            materialIdToFacesMap,
            animated,
            channelValidity);
    }

    /**
     * \brief Converts a UsdGeomMesh to a 3dsMax PolyObject
     * \param mesh The USD mesh to be converted.
     * \param options Primvar mapping options.
     * \param channelNames The map channel names, generated from converted primvars.
     * \param geomSubsetsMaterial Generated multi-material, representing any found UsdGeomSubsets.
     * \param timeCode The USD timeCode at which the conversion happens.
     * \param cleanMesh Flag to remove vertices that aren't being used from the converted mesh.
     * \return The created PolyObject
     */
    PolyObject* ConvertToPolyObject(
        const pxr::UsdGeomMesh&      mesh,
        const PrimvarMappingOptions& options,
        std::map<int, std::string>&  channelNames,
        MultiMtl**                   geomSubsetsMaterial,
        pxr::UsdTimeCode             timeCode,
        bool                         cleanMesh = true);

    /**
     * \brief Converts a UsdGeomMesh to a MNesh.
     * \param mesh The USD mesh to convert.
     * \param maxMesh The output MNMesh.
     * \param options Primvar mapping options.
     * \param channelNames The map channel names, generated from converted primvars.
     * \param geomsubSetsMaterial Generated multi-material, representing any found UsdGeomSubsets.
     * \param timeCode The USD timeCode at which the conversion happens.
     * \param cleanMesh Flag to remove vertices that aren't being used from the converted mesh.
     */
    void ConvertToMNMesh(
        const pxr::UsdGeomMesh&      mesh,
        MNMesh&                      maxMesh,
        const PrimvarMappingOptions& options,
        std::map<int, std::string>&  channelNames,
        MultiMtl**                   geomsubSetsMaterial = nullptr,
        pxr::UsdTimeCode             timeCode = pxr::UsdTimeCode::Default(),
        bool                         cleanMesh = true);

    /**
     * \brief Get the material id from custom data on the given usd prim.
     * \param usdPrim The USD prim that contain the matId custom data.
     * \return the matId contained in the custom data of the prim. Return -1 if there is no custom data or if the data
     * is invalid.
     */
    static int GetMaterialIdFromCustomData(const pxr::UsdPrim& usdPrim);

protected:
    /**
     * \brief Applies the USD normals to the given MNmesh.
     * \param mesh The USD mesh from which to read the normals.
     * \param maxMesh The max mesh where to apply the normals.
     * \param timeCode The time code at which to get the normals.
     */
    static void ApplyUSDNormals(
        const pxr::UsdGeomMesh& mesh,
        MNMesh&                 maxMesh,
        pxr::UsdTimeCode        timeCode = pxr::UsdTimeCode::Default());

    /**
     * \brief Applies a MNmesh's normals to a usd mesh.
     * \param maxMesh The max mesh from which to fetch the normals.
     * \param mesh The usd mesh where to apply the normals.
     * \param options Mesh conversion options.
     * \param channelIntervals The object channel validity intervals.
     * \param timeCode The timecode at which to apply the normals.
     * \param animated Whether or not the normal conversion is part of an animation.
     * \return true if it was able to apply normals
     */
    static bool ApplyMaxNormals(
        MaxUsd::MeshFacade&             maxMesh,
        pxr::UsdGeomMesh&               mesh,
        const MaxMeshConversionOptions& options,
        const ObjectChannelIntervals&   channelIntervals,
        pxr::UsdTimeCode                timeCode,
        bool                            animated);

    /**
     * \brief Exports map channels to primvars on the target usd mesh.
     * \param maxMesh The Max mesh where to read the channels.
     * \param mesh The target USD mesh.
     * \param options Mesh conversion options.
     * \param channelIntervals The object channel validity intervals.
     * \param timeCode The timecode where to write the primvars.
     * \param animated Whether or not the conversion of the map channel is part of an animation.
     * \return
     */
    static void ApplyMaxMapChannels(
        MaxUsd::MeshFacade&             maxMesh,
        pxr::UsdGeomMesh&               mesh,
        const MaxMeshConversionOptions& options,
        const ObjectChannelIntervals&   channelIntervals,
        pxr::UsdTimeCode                timeCode,
        bool                            animated);

    /**
     * \brief Exports a channel to a primvar.
     * \param maxMesh The max mesh.
     * \param channel The channel to export to a primvar.
     * \param mesh The usd mesh where to create the primvar.
     * \param primvarConfig The configuration for the primvar.
     * \param channelIntervals The object channel validity intervals.
     * \param timeCode The timecode at which to output the data.
     * \param animated Whether or not the map channel conversion is part of an animation.
     * \return True if successful, false otherwise.
     */
    static bool ChannelToPrimvar(
        MaxUsd::MeshFacade&                           maxMesh,
        int                                           channel,
        pxr::UsdGeomMesh&                             mesh,
        const MaxUsd::MappedAttributeBuilder::Config& primvarConfig,
        const ObjectChannelIntervals&                 channelIntervals,
        const pxr::UsdTimeCode&                       timeCode,
        bool                                          animated);

    /**
     * \brief Resolves the target channels for the primvars of the given mesh. This takes care of incompatibilities
     * and potential conflicts. Following a call to this method, we have a clear idea of what
     * channel will host the data of what primvar for a specific mesh.
     * \param mesh The usd mesh, potentially hosting primvars that will need to be imported.
     * \param options The primvar to channel mapping options.
     * \param channelPrimvars The resolved association, max channel -> primvar.
     * \return The maximum channel used.
     */
    static void ResolveChannelPrimvars(
        const pxr::UsdGeomMesh&             mesh,
        const PrimvarMappingOptions&        options,
        std::map<int, pxr::UsdGeomPrimvar>& channelPrimvars);

    /**
     * \brief Applies USD primvars to the given Max mesh.
     * \param mesh  The usd mesh hosting the primvars to apply.
     * \param maxMesh The target max mesh.
     * \param options The primvar / channel mapping options.
     * \param channelNames Filled map of names associated with each imported channels. Typically from primvar names.
     * \param timeCode The USD time code at which to fetch the data.
     */
    static void ApplyUSDPrimvars(
        const pxr::UsdGeomMesh&      mesh,
        MNMesh&                      maxMesh,
        const PrimvarMappingOptions& options,
        std::map<int, std::string>&  channelNames,
        const pxr::UsdTimeCode&      timeCode);

    /**
     * \brief Create USD subsets on the given usdMesh for the materialID information provided in materialIdToFacesMap.
     * \param mtl The 3ds Max material associated to the mesh.
     * \param materialIdToFacesMap A map of faces by material id.
     * \param usdPrim The usd usdPrim on which to add the material binding subsets.
     */
    static void ApplyMaxMaterialIDs(
        Mtl*                                    mtl,
        const std::map<MtlID, pxr::VtIntArray>& materialIdToFacesMap,
        const pxr::UsdPrim&                     usdPrim,
        const pxr::UsdTimeCode&                 timeCode);

    /**
     * \brief Apply material id on the provide MNMesh from the provide usdMesh prim subsets
     * \param usdPrim UsdPrim prim from which to extract the material id subsets
     * \param maxMesh MNMesh on which to add the material id
     * \param timeCode The USD time code at which to fetch the data.
     * \param geomSubsetMaterial Optional, a multimaterial to be filled with geomSubset information
     * if any in case of multiple materials bound to the mesh.
     */
    static void ApplyUSDMaterialIDs(
        const pxr::UsdPrim&     usdPrim,
        MNMesh&                 maxMesh,
        const pxr::UsdTimeCode& timeCode,
        MultiMtl**              geomSubsetMaterial = nullptr);

    /**
     * \brief Applies the MNMesh vert creases to the given USD mesh.
     * \param usdMesh The USD mesh where to apply the vert creases.
     * \param maxMesh The max mesh from which to read the vert creases.
     * \param timeCode The time code at which to get the vert creases.
     */
    static void ApplyMaxVertCreases(
        MaxUsd::MeshFacade& maxMesh,
        pxr::UsdGeomMesh&   usdMesh,
        pxr::UsdTimeCode    timeCode = pxr::UsdTimeCode::Default());

    /**
     * \brief Applies the USD vert creases to the given MNmesh.
     * \param usdMesh The USD mesh from which to read the vert creases.
     * \param maxMesh The max mesh where to apply the vert creases.
     * \param timeCode The time code at which to get the vert creases.
     */
    static void ApplyUSDVertCreases(
        const pxr::UsdGeomMesh& usdMesh,
        MNMesh&                 maxMesh,
        pxr::UsdTimeCode        timeCode = pxr::UsdTimeCode::Default());

    /**
     * \brief Applies the MNMesh edge creases to the given USD mesh.
     * \param usdMesh The USD mesh where to apply the edge creases.
     * \param maxMesh The max mesh from which to read the edge creases.
     * \param timeCode The time code at which to get the edge creases.
     */
    static void ApplyMaxEdgeCreases(
        MaxUsd::MeshFacade& maxMesh,
        pxr::UsdGeomMesh&   usdMesh,
        pxr::UsdTimeCode    timeCode = pxr::UsdTimeCode::Default());

    /**
     * \brief Applies the USD edge creases to the given MNmesh.
     * \param usdMesh The USD mesh from which to read the edge creases.
     * \param maxMesh The max mesh where to apply the edge creases.
     * \param timeCode The time code at which to get the edge creases.
     */
    static void ApplyUSDEdgeCreases(
        const pxr::UsdGeomMesh& usdMesh,
        MNMesh&                 maxMesh,
        pxr::UsdTimeCode        timeCode = pxr::UsdTimeCode::Default());

    /**
     * \brief Applies the given matId on the given mesh for the face found in the given geom Subset
     * \param subset The USD geom subset in which to find the face indices
     * \param maxMesh The max mesh where to apply the matId
     * \param matId The matId to apply to the faces found in the geom subset
     * \param timeCode The time code at which to get the faces.
     */
    static void ApplyMatIdToMesh(
        const pxr::UsdGeomSubset& subset,
        MNMesh&                   maxMesh,
        int                       matId,
        const pxr::UsdTimeCode&   timeCode);

    /**
     * \brief Gets the validity intervals of the object channels of the passed mesh object (poly or tri).
     * \param object The object to get channel validities from.
     * \param time The time value at which to get the validity intervals.
     * \return Map of unordered object channel ids to validity intervals.
     */
    template <class MeshObjectType>
    static ObjectChannelIntervals
    GetObjectChannelIntervals(const MeshObjectType& object, const TimeValue& time)
    {
        return { { TOPO_CHAN_NUM, object->ChannelValidity(time, TOPO_CHAN_NUM) },
                 { GEOM_CHAN_NUM, object->ChannelValidity(time, GEOM_CHAN_NUM) },
                 { TEXMAP_CHAN_NUM, object->ChannelValidity(time, TEXMAP_CHAN_NUM) },
                 { VERT_COLOR_CHAN_NUM, object->ChannelValidity(time, VERT_COLOR_CHAN_NUM) } };
    }

    /**
     * \brief Returns "instance" validity intervals for geom channels.
     * \param time The time at which channels are valid
     * \return Channel validity intervals.
     */
    static ObjectChannelIntervals GetInstantChannelIntervals(const TimeValue& time)
    {
        const auto instantInterval = Interval(time, time);
        return { { TOPO_CHAN_NUM, instantInterval },
                 { GEOM_CHAN_NUM, instantInterval },
                 { TEXMAP_CHAN_NUM, instantInterval },
                 { VERT_COLOR_CHAN_NUM, instantInterval } };
    }
};

} // namespace MAXUSD_NS_DEF
