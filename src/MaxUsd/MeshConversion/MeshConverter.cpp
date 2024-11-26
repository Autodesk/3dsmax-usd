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
#include "MeshConverter.h"

#include "PrimvarMappingOptions.h"

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/ChannelBuilder.h>
#include <MaxUsd/MappedAttributeBuilder.h>
#include <MaxUsd/MaxTokens.h>
#include <MaxUsd/Utilities/Logging.h>
#include <MaxUsd/Utilities/MaterialUtils.h>
#include <MaxUsd/Utilities/MathUtils.h>
#include <MaxUsd/Utilities/Meshutils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <iparamb2.h>
#include <mnmesh.h>
#include <numeric>
#include <plugapi.h>
#include <polyobj.h>
#include <stdmat.h>
#include <triobj.h>

namespace MAXUSD_NS_DEF {

static const float MAX2USD_CREASE = 10.f;
static const float USD2MAX_CREASE = 0.1f;

pxr::UsdGeomMesh MeshConverter::ConvertToUSDMesh(
    INode*                          node,
    const pxr::UsdStagePtr&         stage,
    const pxr::SdfPath&             path,
    const MaxMeshConversionOptions& options,
    bool                            applyOffsetTransform,
    bool                            animated,
    const ExportTime&               timeFrame)
{
    pxr::UsdGeomMesh usdMesh;

    const auto usdTimeCode = timeFrame.GetUsdTime();

    const auto& objectWS = node->EvalWorldState(timeFrame.GetMaxTime());
    const auto  obj = objectWS.obj;

    // The mesh we will effectively convert to a UsdGeomMesh.
    // Use a facade to transparently deal with MNMesh an Mesh classes.
    std::unique_ptr<MeshFacade> meshFacade = nullptr;

    // If the object we are exporting to a UsdGeomMesh needs to be converted to an intermediary
    // TriObject or PolyObject, that temporary will be held by this pointer. Needed so we can later
    // delete it.
    Object* temporaryObjectFromConvert = nullptr;

    // Utility lamba, builds the MeshFacade from a tri mesh, while considering the specified mesh
    // format. The passed mesh might get modified, so you should not pass the scene's mesh directly.
    auto getMeshFacadeFromTri
        = [](Mesh* mesh, bool ownMesh, bool convertToPoly) -> std::unique_ptr<MeshFacade> {
        if (convertToPoly) {
            MaxUsd::MeshUtils::SetupEdgeVisibility(*mesh, true);
            MNMesh* polyMesh = new MNMesh {};
            polyMesh->AddTri(*mesh);
            polyMesh->FillInMesh();
            polyMesh->EliminateBadVerts();
            polyMesh->MakePolyMesh(0, false);
            if (ownMesh) {
                delete mesh;
            }
            return std::make_unique<MeshFacade>(
                polyMesh, true); // Facade will be in charge of deleting the polyMesh.
        }
        return std::make_unique<MeshFacade>(mesh, ownMesh);
    };

    // Figure out if the object we are trying to convert to a mesh is already a TriObject or a
    // PolyObject...
    const auto originalTriObject = dynamic_cast<TriObject*>(obj);
    const auto originalPolyObject = dynamic_cast<PolyObject*>(obj);

    // We'll use the object channel validity intervals to limit what we export on the meshes.
    ObjectChannelIntervals channelIntervals;

    const auto meshFormat = options.GetMeshFormat();
    const bool convertToPoly = meshFormat == MaxMeshConversionOptions::MeshFormat::PolyMesh;

    // Lambda to get geom channels validity intervals for exported TriObjects.
    // If we are converting to poly at export time, we can only use instant validity intervals.
    // Indeed, the geom channels intervals can't be used, as the converted topology can vary frame
    // to frame independently from the source topology. Prior to conversion to poly, for the purpose
    // of export, we show edges between triangles that are not coplanar, this is to avoid bad
    // looking results with curved faces. The result of that operation can change as the mesh is
    // animated - making max's intervals meaningless.
    auto getIntervalsForTriObject = [&convertToPoly, &timeFrame](TriObject* triObject) {
        if (convertToPoly) {
            return GetInstantChannelIntervals(timeFrame.GetMaxTime());
        }
        return GetObjectChannelIntervals(triObject, timeFrame.GetMaxTime());
    };

    // If the object is a TriObject, build an MNMesh from its Mesh.
    if (originalTriObject) {
        meshFacade
            = getMeshFacadeFromTri(new Mesh { originalTriObject->GetMesh() }, true, convertToPoly);
        channelIntervals = getIntervalsForTriObject(originalTriObject);
    }
    // Some object types do not translate directly to PolyObjects. In those cases we just go through
    // TriObjects.
    else if (
        !obj->CanConvertToType(Class_ID(POLYOBJ_CLASS_ID, 0))
        && obj->CanConvertToType(Class_ID(TRIOBJ_CLASS_ID, 0))) {
        const auto triObjectCopy = static_cast<TriObject*>(
            obj->ConvertToType(timeFrame.GetMaxTime(), Class_ID(TRIOBJ_CLASS_ID, 0)));

        meshFacade = getMeshFacadeFromTri(&triObjectCopy->GetMesh(), false, convertToPoly);
        temporaryObjectFromConvert = triObjectCopy;
        channelIntervals = getIntervalsForTriObject(triObjectCopy);
    }
    // If the object is a PolyObject, make a copy of its MNMesh.
    else if (originalPolyObject) {
        channelIntervals = GetObjectChannelIntervals(originalPolyObject, timeFrame.GetMaxTime());
        meshFacade
            = std::make_unique<MeshFacade>(new MNMesh { originalPolyObject->GetMesh() }, true);
    }
    // Otherwise, check if the object can be converted to a PolyObject.
    else if (obj->CanConvertToType(Class_ID(POLYOBJ_CLASS_ID, 0))) {
        const auto polyObj = static_cast<PolyObject*>(
            obj->ConvertToType(timeFrame.GetMaxTime(), Class_ID(POLYOBJ_CLASS_ID, 0)));

        channelIntervals = GetObjectChannelIntervals(polyObj, timeFrame.GetMaxTime());
        meshFacade = std::make_unique<MeshFacade>(&polyObj->GetMesh(), false);
        temporaryObjectFromConvert = polyObj;
    }

    // Now ready to perform the actual conversion to USDGeomMesh.
    if (meshFacade != nullptr) {
        // Check if we need to triangulate if requested by the user.
        if (options.GetMeshFormat() == MaxMeshConversionOptions::MeshFormat::TriMesh) {
            meshFacade->Triangulate();
        }

        std::map<MtlID, pxr::VtIntArray> materialIdToFacesMap;
        if (applyOffsetTransform) {
            const bool wsmRequiresTransform
                = MaxUsd::WsmRequiresTransformToLocalSpace(node, timeFrame.GetMaxTime());

            // Bake the object offset if required. Note that if the object has a World Space
            // Modifier applied, its transform is the identity, as the total transform, including
            // the offset and the WSM, is already applied directly onto the geometry in world space.
            if (!wsmRequiresTransform && options.GetBakeObjectOffsetTransform()) {
                auto objectTransform = MaxUsd::GetMaxObjectOffsetTransform(node);
                meshFacade->Transform(objectTransform);
                ConvertToUSDMesh(
                    *meshFacade,
                    stage,
                    path,
                    options,
                    usdMesh,
                    usdTimeCode,
                    materialIdToFacesMap,
                    animated,
                    channelIntervals);
                pxr::UsdGeomXformable xformable(usdMesh.GetPrim());
            } else {
                // If a WSM is applied, move the geometry's points back into local space. So that
                // with the transforms inherited from its hierarchy, the object will end up in the
                // correct location on the USD side.
                if (wsmRequiresTransform) {
                    auto nodeTmInvert = node->GetNodeTM(timeFrame.GetMaxTime());
                    nodeTmInvert.Invert();
                    meshFacade->Transform(nodeTmInvert);
                }
                ConvertToUSDMesh(
                    *meshFacade,
                    stage,
                    path,
                    options,
                    usdMesh,
                    usdTimeCode,
                    materialIdToFacesMap,
                    animated,
                    channelIntervals);
            }
        } else {
            ConvertToUSDMesh(
                *meshFacade,
                stage,
                path,
                options,
                usdMesh,
                usdTimeCode,
                materialIdToFacesMap,
                animated,
                channelIntervals);
        }

        // Apply materialIds at the specificed timecode. materialIds are exported at the same frames
        // as topology. If topology was not exported at the current frame, no need to do anything.
        if (!materialIdToFacesMap.empty()) {
            ApplyMaxMaterialIDs(
                node->GetMtl(), materialIdToFacesMap, usdMesh.GetPrim(), usdTimeCode);
        }

        {
            // If the displayColor is not already authored, set it to the wireColor.
            if (!usdMesh.GetDisplayColorAttr().IsAuthored()) {
                Color             wireColor(node->GetWireColor());
                pxr::VtVec3fArray usdDisplayColor
                    = { pxr::GfVec3f(wireColor.r, wireColor.g, wireColor.b) };
                usdMesh.CreateDisplayColorAttr().Set(usdDisplayColor);
            }
        }

        // If the object is a temporary object from a conversion, need to delete it now.
        if (temporaryObjectFromConvert) {
            delete temporaryObjectFromConvert;
        }
    }

    // If we need to setup the object transform as an Xform op, only do so once, when exporting
    // the first frame of the mesh.
    if (timeFrame.IsFirstFrame()) {
        // Apply the offset transform as a Xform op on the mesh if not bakes.
        // The object offset is not animatable.
        if (applyOffsetTransform && !options.GetBakeObjectOffsetTransform()) {
            pxr::UsdGeomXformable xformable(usdMesh.GetPrim());
            MaxUsd::ApplyObjectOffsetTransform(node, xformable, timeFrame.GetMaxTime());
        }
    }
    return usdMesh;
}

namespace {
// Checks if a mesh attribute needs to be written out at a given timeCode, considering
// the object channels it depends on.
bool _checkWriteAttribute(
    const pxr::UsdTimeCode&                      timeCode,
    const std::vector<int>&&                     channels,
    const pxr::UsdAttribute&                     attribute,
    const MeshConverter::ObjectChannelIntervals& channelIntervals)
{
    // Intersect all the intervals of the channel dependencies.
    Interval intersect = FOREVER;
    for (const auto& channel : channels) {
        // Do we already have an authored frame the attribute that is within the channel's
        // validity interval?
        const auto it = channelIntervals.find(channel);
        if (it == channelIntervals.end()) {
            // Fallback to always writing out the attribute.
            return true;
        }
        intersect &= it->second;
    }

    // We always need to write the last sample of the interval, to make sure we interpolate
    // to the same value on the USD side for any time within the interval.
    if (MaxUsd::GetTimeValueFromFrame(timeCode.GetValue()) == intersect.End()) {
        return true;
    }

    // Finally, check if we already have a sample within the validity interval. If so, we
    // do not need to write a new sample..
    std::vector<double> timeSamples;
    const auto usdInterval = pxr::GfInterval { MaxUsd::GetFrameFromTimeValue(intersect.Start()),
                                               MaxUsd::GetFrameFromTimeValue(intersect.End()) };

    attribute.GetTimeSamplesInInterval(usdInterval, &timeSamples);

    // Have a sample already in the interval? No need to write!
    if (timeSamples.size() == 1) {
        return false;
    }
    // If we have more than one sample in this interval, it means that at some previous
    // frame, we decided to ignore the validity intervals (maybe we have to clean or sanitize
    // the topology for the purpose of export). If we did, we can't rely on the object channel
    // intervals any longer, need to write again.
    if (timeSamples.size() > 1) {
        return true;
    }
    return timeSamples.empty();
};

struct MeshInfo
{
    size_t              vertCount = 0;
    size_t              faceCount = 0;
    std::vector<size_t> mapVertCounts;
    bool                operator!=(const MeshInfo& other) const
    {
        return (
            vertCount != other.vertCount || faceCount != other.faceCount
            || mapVertCounts != other.mapVertCounts);
    }
};

MeshInfo _getMeshInfo(const MeshFacade& mesh)
{
    MeshInfo info;
    info.faceCount = mesh.FaceCount();
    info.vertCount = mesh.VertexCount();
    info.mapVertCounts.reserve(mesh.MapCount() + NUM_HIDDENMAPS);
    for (int map = -NUM_HIDDENMAPS; map < mesh.MapCount(); ++map) {
        info.mapVertCounts.push_back(mesh.MapDataCount(map));
    }
    return info;
}

} // namespace

void MeshConverter::ConvertToUSDMesh(
    MeshFacade&                       maxMesh,
    const pxr::UsdStagePtr&           stage,
    const pxr::SdfPath&               path,
    const MaxMeshConversionOptions&   options,
    pxr::UsdGeomMesh&                 usdMesh,
    const pxr::UsdTimeCode&           usdTime,
    std::map<MtlID, pxr::VtIntArray>& materialIdToFacesMap,
    bool                              animated,
    const ObjectChannelIntervals&     channelIntervals)
{
    usdMesh
        = MaxUsd::FetchOrCreatePrim<pxr::UsdGeomMesh>(stage, path, pxr::MaxUsdPrimTypeTokens->Mesh);

    // Local copy of the channel intervals.
    auto intervals = channelIntervals;

    // Some sanitization/cleanup of the polys.
    {
        // MakeConvex and MakePlanar can add new faces. If they do, and we are exporting an
        // animation, we need to override the geom validity intervals, indeed, we dont know if these
        // operations will behave the same at every frame.
        auto beforeSanitize = maxMesh.FaceCount();

        // The threshold here is defined as the cosinus of the max angle between planes.
        // Dot products are being compared to this value (lower than).
        // We want a tolerance of 0 degrees, cos(0) = 1.0.
        if (options.GetPreserveEdgeOrientation()) {
            maxMesh.MakePlanar(1.0f - FLT_EPSILON);
        }
        // Concave polys are a constant source of trouble. For tesselation, auto-computing normals,
        // and other things... not only in Max, but also in the USD viewer. For now only export
        // convex polys. This also prevents faces with holes.
        maxMesh.MakeConvex();

        auto afterSanitize = maxMesh.FaceCount();

        // Clean any dead structures.
        // This can impact the number of faces and the number of verts. If it actually does, we
        // can't rely on the geom validity intervals any longer (see comment above for MakeConvex()
        // / MakePlanar(), same idea).
        auto beforeCleanup = _getMeshInfo(maxMesh);
        maxMesh.Cleanup();
        auto afterCleanup = _getMeshInfo(maxMesh);

        // If the mesh was modified for export, we need to override the geom channel intervals.
        // If exporting a single frame, we dont care about intervals.
        if (animated) {
            if (afterSanitize != beforeSanitize || beforeCleanup != afterCleanup) {
                // Consider that all channels are only valid at the current time.
                const auto timeVal = MaxUsd::GetTimeValueFromFrame(usdTime.GetValue());
                const auto instantInterval = Interval(timeVal, timeVal);
                for (auto& channel : intervals) {
                    channel.second = instantInterval;
                }
            }
        }
    }

    // Extent attribute - depends on the topology and geom object channels.
    auto       extentAttr = usdMesh.CreateExtentAttr();
    const auto writeExtent = !animated
        || _checkWriteAttribute(usdTime, { TOPO_CHAN_NUM, GEOM_CHAN_NUM }, extentAttr, intervals);
    if (writeExtent) {
        const auto        bbox = maxMesh.BoundingBox();
        pxr::VtVec3fArray extent
            = { pxr::GfVec3f(ToUsd(bbox.Min())), pxr::GfVec3f(ToUsd(bbox.Max())) };
        extentAttr.Set(extent, usdTime);
    }

    // Points attribute - depends on the geom object channel.
    auto       pointAttr = usdMesh.CreatePointsAttr();
    const auto writePoints
        = !animated || _checkWriteAttribute(usdTime, { GEOM_CHAN_NUM }, pointAttr, intervals);
    if (writePoints) {
        // Points
        pxr::VtVec3fArray points;
        const auto        vertexCount = maxMesh.VertexCount();
        points.reserve(vertexCount);
        for (auto i = 0; i < vertexCount; ++i) {
            const auto& vertex = maxMesh.Vertex(i);
            points.push_back({ vertex.x, vertex.y, vertex.z });
        }
        usdMesh.CreatePointsAttr().Set(points, usdTime);
    }

    {
        const auto faceVertexCountsAttr = usdMesh.CreateFaceVertexCountsAttr();
        const auto faceVertexIndicesAttr = usdMesh.CreateFaceVertexIndicesAttr();

        // FaceVertexCountsAttr/FaceVertexIndicesAttr depend on the topology channel.
        // FaceVertexCountsAttr and FaceVertexIndicesAttr are always written out in pair. Only
        // look at one of them.
        const bool writeTopo = !animated
            || _checkWriteAttribute(usdTime, { TOPO_CHAN_NUM }, faceVertexCountsAttr, intervals);
        if (writeTopo) {
            // Polygons && material Ids
            const auto      faceCount = maxMesh.FaceCount();
            pxr::VtIntArray faceVertexCount;
            faceVertexCount.reserve(faceCount);

            pxr::VtIntArray faceVertexIndices;

            const auto faceVertexIndiceSize = maxMesh.FaceVertexIndicesCount();
            faceVertexIndices.reserve(faceVertexIndiceSize);

            for (auto i = 0; i < faceCount; ++i) {
                const auto faceDeg = maxMesh.FaceDegree(i);
                if (maxMesh.FaceIsDead(i) || faceDeg < 3) {
                    continue;
                }

                faceVertexCount.push_back(faceDeg);
                for (auto vi = 0; vi < faceDeg; ++vi) {
                    const auto vert = maxMesh.FaceVertex(i, vi);
                    faceVertexIndices.push_back(vert);
                }

                MtlID mtlId = maxMesh.FaceMaterial(i);
                materialIdToFacesMap[mtlId].push_back(i);
            }
            faceVertexCountsAttr.Set(faceVertexCount, usdTime);
            faceVertexIndicesAttr.Set(faceVertexIndices, usdTime);
        }
    }

    ApplyMaxNormals(maxMesh, usdMesh, options, intervals, usdTime, animated);
    ApplyMaxMapChannels(maxMesh, usdMesh, options, intervals, usdTime, animated);

    if (maxMesh.HasCreaseSupport()) {
        ApplyMaxVertCreases(maxMesh, usdMesh, usdTime);
        ApplyMaxEdgeCreases(maxMesh, usdMesh, usdTime);
    }
}

PolyObject* MeshConverter::ConvertToPolyObject(
    const pxr::UsdGeomMesh&      mesh,
    const PrimvarMappingOptions& options,
    std::map<int, std::string>&  channelNames,
    MultiMtl**                   geomSubsetsMaterial,
    pxr::UsdTimeCode             timeCode,
    bool                         cleanMesh)
{
    auto polyobj = static_cast<PolyObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, EPOLYOBJ_CLASS_ID));
    ConvertToMNMesh(
        mesh, polyobj->GetMesh(), options, channelNames, geomSubsetsMaterial, timeCode, cleanMesh);
    return polyobj;
}

void MeshConverter::ConvertToMNMesh(
    const pxr::UsdGeomMesh&      mesh,
    MNMesh&                      maxMesh,
    const PrimvarMappingOptions& options,
    std::map<int, std::string>&  channelNames,
    MultiMtl**                   geomsubSetsMaterial,
    pxr::UsdTimeCode             timeCode,
    bool                         cleanMesh)
{
    if (!mesh.GetPrim().IsValid())
        return;
    pxr::VtVec3fArray vertices;
    mesh.GetPointsAttr().Get(&vertices, timeCode);
    pxr::VtIntArray faceVertexCount;
    mesh.GetFaceVertexCountsAttr().Get(&faceVertexCount, timeCode);
    pxr::VtIntArray faceVertices;
    mesh.GetFaceVertexIndicesAttr().Get(&faceVertices, timeCode);

    if (vertices.empty() || faceVertexCount.empty() || faceVertices.empty()) {
        return;
    }

    maxMesh.setNumVerts(int(vertices.size()));

    // Do not consider faces with less than 3 vertices.
    const auto numFaces
        = std::accumulate(faceVertexCount.begin(), faceVertexCount.end(), 0, [](int total, int vc) {
              return total + (vc < 3 ? 0 : 1);
          });
    if (numFaces == 0) {
        return;
    }
    maxMesh.setNumFaces(numFaces);

    pxr::TfToken orientation;
    mesh.GetOrientationAttr().Get(&orientation, timeCode);

    std::unordered_set<int> vertexIndicesUsedSet;
    MNFace*                 faceIt = maxMesh.f;
    std::for_each(
        faceVertexCount.cbegin(),
        faceVertexCount.cend(),
        [&vertexIndicesUsedSet, &orientation, vertIt = faceVertices.cbegin(), faceIt](
            auto numVertices) mutable {
            if (numVertices < 3) {
                vertIt += numVertices;
                return;
            }
            auto& f(*faceIt++);
            f.SetDeg(numVertices);
            for (auto i = 0; i < numVertices; ++i) {
                vertexIndicesUsedSet.insert(*vertIt);
                f[i] = *vertIt++;
            }
            if (orientation == pxr::UsdGeomTokens->leftHanded) {
                f.Flip();
            }
        });

    for (int i = 0; i < vertices.size(); i++) {
        auto m = MNVert();
        m.p = Point3(vertices[i][0], vertices[i][1], vertices[i][2]);

        if (vertexIndicesUsedSet.find(i) == vertexIndicesUsedSet.end()) {
            m.SetFlag(MN_DEAD);
        }

        maxMesh.v[i] = m;
    };

    ApplyUSDNormals(mesh, maxMesh, timeCode);
    ApplyUSDPrimvars(mesh, maxMesh, options, channelNames, timeCode);
    ApplyUSDMaterialIDs(mesh.GetPrim(), maxMesh, timeCode, geomsubSetsMaterial);

    maxMesh.FillInMesh();

    ApplyUSDVertCreases(mesh, maxMesh, timeCode);
    ApplyUSDEdgeCreases(mesh, maxMesh, timeCode);

    const auto numUnusedVertices = maxMesh.VNum() - vertexIndicesUsedSet.size();
    if (cleanMesh && numUnusedVertices > 0) {
        maxMesh.CollapseDeadVerts();
        MaxUsd::Log::Warn(
            "{0} vertices were not imported from {1} because they were not part of any face.",
            std::to_string(numUnusedVertices),
            mesh.GetPrim().GetPath().GetString());
    }
}

void MeshConverter::ApplyUSDNormals(
    const pxr::UsdGeomMesh& mesh,
    MNMesh&                 maxMesh,
    pxr::UsdTimeCode        timeCode)
{
    const auto hasNormalsPrimvar = pxr::UsdGeomPrimvarsAPI(mesh.GetPrim())
                                       .HasPrimvar(pxr::UsdImagingTokens->primvarsNormals);
    // IsDefined() seems to always be true for that attribute. HasValue() is what we want instead.
    const bool hasNormalsAttribute = mesh.GetNormalsAttr().HasValue();
    if (!hasNormalsPrimvar && !hasNormalsAttribute) {
        return;
    }

    std::unique_ptr<pxr::UsdGeomPrimvar> primvar;
    pxr::TfToken                         interpolation;

    pxr::UsdAttribute attribute;
    // Primvar normals have precedence.
    if (hasNormalsPrimvar) {
        primvar = std::make_unique<pxr::UsdGeomPrimvar>(
            pxr::UsdGeomPrimvarsAPI(mesh).GetPrimvar(pxr::UsdImagingTokens->primvarsNormals));
        attribute = primvar->GetAttr();
        interpolation = primvar->GetInterpolation();
    }
    // Normal attribute, never indexed.
    else {
        attribute = mesh.GetNormalsAttr();
        interpolation = mesh.GetNormalsInterpolation();
    }

    pxr::TfToken orientation;
    mesh.GetOrientationAttr().Get(&orientation, timeCode);

    NormalsBuilder normalsBuilder(&maxMesh, orientation == pxr::UsdGeomTokens->leftHanded);
    normalsBuilder.Build(attribute, interpolation, primvar.get(), mesh, timeCode);
}
bool MeshConverter::ApplyMaxNormals(
    MeshFacade&                     maxMesh,
    pxr::UsdGeomMesh&               mesh,
    const MaxMeshConversionOptions& options,
    const ObjectChannelIntervals&   channelIntervals,
    pxr::UsdTimeCode                timeCode,
    bool                            animated)
{
    if (options.GetNormalMode() == MaxMeshConversionOptions::NormalsMode::None) {
        return false;
    }

    pxr::UsdAttribute                    normalsAttr;
    std::unique_ptr<pxr::UsdGeomPrimvar> primvar;
    if (options.GetNormalMode() == MaxMeshConversionOptions::NormalsMode::AsPrimvar) {
        pxr::UsdGeomPrimvarsAPI primVarApi(mesh.GetPrim());
        primvar = std::make_unique<pxr::UsdGeomPrimvar>(primVarApi.CreatePrimvar(
            pxr::UsdImagingTokens->primvarsNormals, pxr::SdfValueTypeNames->Float3Array));
        normalsAttr = primvar->GetAttr();
    } else {
        normalsAttr = mesh.GetNormalsAttr();
    }

    // Check if we need to write out normals at this time, given the concerned channels
    // and previously exported times. Normals mostly depend on the geom channel, but normals
    // have a complex history in 3dsMax, and so we "invalidate" them along with the topology
    // channel also, to be safe.
    if (animated
        && !_checkWriteAttribute(
            timeCode, { TOPO_CHAN_NUM, GEOM_CHAN_NUM }, normalsAttr, channelIntervals)) {
        return false;
    }

    maxMesh.LoadNormals();

    const auto normalCount = maxMesh.NormalCount();
    if (normalCount == 0) {
        return false;
    }

    // From the USD docs :
    // "Normals should not be authored on a subdivided mesh, since subdivision algorithms define
    // their own normals. They should only be authored for polygonal meshes."
    mesh.CreateSubdivisionSchemeAttr(pxr::VtValue(pxr::UsdGeomTokens->none));

    auto mappedData = std::make_shared<MappedAttributeBuilder::MappedData>(
        maxMesh.NormalData(), normalCount, maxMesh.NormalIndices());

    const MappedAttributeBuilder primvarConverter { maxMesh, mappedData };

    // Inferring the data layout is costly and the result could change over the course of an
    // animation. Always use faceVarying/indexed when exporting an animation.
    MappedAttributeBuilder::DataLayout dataLayout = options.GetPrimvarLayoutInference()
                == MaxMeshConversionOptions::PrimvarLayoutInference::Never
            || animated
        ? MappedAttributeBuilder::DataLayout(pxr::UsdGeomTokens->faceVarying, true)
        : primvarConverter.InferAttributeDataLayout();

    if (options.GetNormalMode() == MaxMeshConversionOptions::NormalsMode::AsPrimvar) {
        primvar->SetInterpolation(dataLayout.GetInterpolation());
    } else {
        mesh.SetNormalsInterpolation(dataLayout.GetInterpolation());
    }

    return primvarConverter.PopulateAttribute(normalsAttr, dataLayout, primvar.get(), timeCode);
}

bool MeshConverter::ChannelToPrimvar(
    MeshFacade&                           maxMesh,
    int                                   channel,
    pxr::UsdGeomMesh&                     mesh,
    const MappedAttributeBuilder::Config& primvarConfig,
    const ObjectChannelIntervals&         channelIntervals,
    const pxr::UsdTimeCode&               timeCode,
    bool                                  animated)
{
    // No target primvar set. Nothing to do.
    if (primvarConfig.GetPrimvarName().IsEmpty()) {
        return false;
    }

    const auto faceCount = maxMesh.MapFaceCount(channel);
    if (faceCount == 0) {
        return false;
    }

    // Check if we need to write the primvar at this time, given the concerned channels and
    // previously exported times. Depending on when modifiers/tools were written, they may only
    // deal one of the TEXMAP or VERT_COLOR channels - to be safe, we always intersect those two.
    const auto primvar = pxr::UsdGeomPrimvarsAPI(mesh).GetPrimvar(primvarConfig.GetPrimvarName());
    if (animated && primvar.IsDefined()
        && !_checkWriteAttribute(
            timeCode,
            { TOPO_CHAN_NUM, TEXMAP_CHAN_NUM, VERT_COLOR_CHAN_NUM },
            primvar.GetAttr(),
            channelIntervals)) {
        return false;
    }

    // flatten the face map indices.
    auto faceMapIndices = std::make_shared<std::vector<int>>();
    faceMapIndices->reserve(maxMesh.FaceVertexIndicesCount());

    for (int i = 0; i < faceCount; ++i) {
        const auto degree = maxMesh.MapFaceDegree(channel, i);
        for (int j = 0; j < degree; ++j) {
            faceMapIndices->push_back(maxMesh.MapFaceVertex(channel, i, j));
        }
    }

    auto mappedData = std::make_shared<MappedAttributeBuilder::MappedData>(
        maxMesh.MapData(channel), maxMesh.MapDataCount(channel), faceMapIndices);

    MappedAttributeBuilder primvarBuilder(maxMesh, mappedData);
    return primvarBuilder.BuildPrimvar(mesh, primvarConfig, timeCode, animated);
}

void MeshConverter::ApplyMaxMapChannels(
    MeshFacade&                     maxMesh,
    pxr::UsdGeomMesh&               mesh,
    const MaxMeshConversionOptions& options,
    const ObjectChannelIntervals&   channelIntervals,
    pxr::UsdTimeCode                timeCode,
    bool                            animated)
{
    for (int i = -NUM_HIDDENMAPS; i < maxMesh.MapCount(); ++i) {
        // Fetch the config for this primvar.
        const MappedAttributeBuilder::Config& primConfig = options.GetChannelPrimvarConfig(i);
        ChannelToPrimvar(maxMesh, i, mesh, primConfig, channelIntervals, timeCode, animated);
    }
}

void MeshConverter::ResolveChannelPrimvars(
    const pxr::UsdGeomMesh&             mesh,
    const PrimvarMappingOptions&        options,
    std::map<int, pxr::UsdGeomPrimvar>& channelPrimvars)
{
    channelPrimvars.clear();
    const auto&                     mappings = options.GetPrimvarMappings();
    std::unordered_set<std::string> processedPrimvars;

    // Note that the USD sdk returns the primvars in alphabetical order.
    const auto allPrimvars = pxr::UsdGeomPrimvarsAPI(mesh).GetPrimvars();

    // Start with the explicit mappings.
    for (const auto& primvar : allPrimvars) {
        const auto mappingIt = mappings.find(primvar.GetPrimvarName());
        if (mappingIt == mappings.end()) {
            continue;
        }
        auto channel = mappingIt->second.Get<int>();

        processedPrimvars.insert(primvar.GetName());

        // Check if the primvar was explicitely ignored for import.
        if (channel == PrimvarMappingOptions::invalidChannel) {
            continue;
        }

        // If the primvar is not defined for this mesh, or has no value, nothing to do.
        if (!primvar.IsDefined() || !primvar.HasValue()) {
            continue;
        }

        // Check that this Primvar can actually fit into a map channel.
        const auto dimension = MaxUsd::GetTypeDimension(primvar.GetTypeName());
        if (dimension > 4) {
            MaxUsd::Log::Warn(
                "{0} on {1} is of dimension {2} and cannot be imported to a 3dsMax channel.",
                primvar.GetName().GetString(),
                mesh.GetPath().GetString(),
                dimension);
            continue;
        }

        // We may get into a situation where we have a conflict. For example, by default, both map1
        // and st are mapped to the main uv channel, i.e. channel 1. It is reasonnable to expect
        // that these would be used on different meshes, but in cases they are both present on the
        // same mesh, log a warning.
        const auto it = channelPrimvars.find(channel);
        if (it != channelPrimvars.end()) {
            MaxUsd::Log::Warn(
                "Found a Primvar/Channel mapping conflict when importing {0}. Channel {1} is "
                "already "
                "used by {2}, {3} will be skipped.",
                mesh.GetPath().GetString(),
                it->first,
                it->second.GetPrimvarName().GetString(),
                primvar.GetPrimvarName().GetString());
            continue;
        }

        channelPrimvars[channel] = primvar;
    }

    if (!options.GetImportUnmappedPrimvars()) {
        return;
    }

    // Next Import all remaining unmapped primvars of dimensions 1,2 and 3 into max channels the
    // best we can. Channel 0 -> From Color3 primvars. Channel 1 -> From Texcoord primvars, or float
    // if none. channels 2+ -> Remaining primvars, starting with texcoord primvars.

    // Start by cleaning out some primvars we dont want to import, some well known primvars should
    // not be loaded blindly into channels, primvar:normals for example.
    std::vector<pxr::UsdGeomPrimvar> primvars;
    for (const auto& primvar : allPrimvars) {
        // Well known primvars, these are handled separately.
        if (primvar.GetPrimvarName() == pxr::MaxUsdPrimvarTokens->displayOpacity
            || primvar.GetPrimvarName() == pxr::MaxUsdPrimvarTokens->displayColor
            || primvar.GetName() == pxr::UsdImagingTokens->primvarsNormals) {
            continue;
        }

        // If a primvar can fit into a max channel, consider it. Note that primvars
        // of dimension 4 can still be imported, but it needs to be specified explicitely,
        // and it would trigger a warning because of the data-loss (4th dimension is lost).
        const auto attr = primvar.GetAttr();
        const auto dimension = MaxUsd::GetTypeDimension(attr.GetTypeName());
        if (dimension > 3) {
            continue;
        }
        pxr::VtValue values;
        attr.Get(&values);
        // Make sure the data can cast to floats, so that it can be loaded into max channels.
        if (!values.CanCast<pxr::VtVec3fArray>() && !values.CanCast<pxr::VtVec2fArray>()
            && !values.CanCast<pxr::VtFloatArray>()) {
            continue;
        }
        primvars.push_back(primvar);
    }

    // Sort the primvars by type. As a general rule, we want to load UV (texcoord) primvars
    // into the lower channels.
    const auto getTypeOrder = [](const pxr::UsdGeomPrimvar& primvar) {
        const auto typeName = primvar.GetTypeName();
        if (typeName == pxr::SdfValueTypeNames->TexCoord2fArray)
            return 1;
        if (typeName == pxr::SdfValueTypeNames->TexCoord2dArray)
            return 2;
        if (typeName == pxr::SdfValueTypeNames->TexCoord2hArray)
            return 3;
        if (typeName == pxr::SdfValueTypeNames->TexCoord3fArray)
            return 4;
        if (typeName == pxr::SdfValueTypeNames->TexCoord3dArray)
            return 5;
        if (typeName == pxr::SdfValueTypeNames->TexCoord3hArray)
            return 6;
        // For other types, the order does not matter.
        return 7;
    };
    std::sort(
        primvars.begin(),
        primvars.end(),
        [&getTypeOrder](
            const pxr::UsdGeomPrimvar& firstPrimvar, const pxr::UsdGeomPrimvar& secondPrimvar)
            -> bool { return getTypeOrder(firstPrimvar) < getTypeOrder(secondPrimvar); });

    // First look to infer the main UV channel, and the vertex color channel, from primvar types.
    // Main UV channel -> First texCoord primvar found. Or first float2Array/double2Array if none.
    // Vertex Color channel -> First color3 type primvar found.
    pxr::UsdGeomPrimvar inferredUvPrimvar;
    pxr::UsdGeomPrimvar inferredVertexColorPrimvar;
    bool                texCoordUvFound = false;

    const auto mainUvMapped = channelPrimvars.find(1) != channelPrimvars.end();
    const auto vcMapped = channelPrimvars.find(0) != channelPrimvars.end();
    if (!mainUvMapped || !vcMapped) {
        for (const auto& primvar : primvars) {
            // Can only use a primvars that were not already processed.
            if (processedPrimvars.find(primvar.GetName()) != processedPrimvars.end()) {
                continue;
            }

            if (!mainUvMapped && !texCoordUvFound) {
                // If we find a texcoord type primvar, use that.
                if (primvar.GetTypeName() == pxr::SdfValueTypeNames->TexCoord2fArray
                    || primvar.GetTypeName() == pxr::SdfValueTypeNames->TexCoord2dArray
                    || primvar.GetTypeName() == pxr::SdfValueTypeNames->TexCoord2hArray
                    || primvar.GetTypeName() == pxr::SdfValueTypeNames->TexCoord3fArray
                    || primvar.GetTypeName() == pxr::SdfValueTypeNames->TexCoord3dArray
                    || primvar.GetTypeName() == pxr::SdfValueTypeNames->TexCoord3hArray) {
                    inferredUvPrimvar = primvar;
                    texCoordUvFound = true;
                    // If we found both a texcoord UV primvar and the vertex color primvar we can
                    // stop looping.
                    if (inferredVertexColorPrimvar.IsDefined()) {
                        break;
                    }
                    continue;
                }
                // If we find float, double or half array primvar, we can use it, but continue
                // looping through the primvars in case a primvar of type texcoord exists.
                if ((primvar.GetTypeName() == pxr::SdfValueTypeNames->Float2Array
                     || primvar.GetTypeName() == pxr::SdfValueTypeNames->Double2Array
                     || primvar.GetTypeName() == pxr::SdfValueTypeNames->Half2Array)
                    && !inferredUvPrimvar.IsDefined()) {
                    inferredUvPrimvar = primvar;
                }
            }

            if (!vcMapped && !inferredVertexColorPrimvar.IsDefined()) {
                // If we find a Color3 array primvar, use that.
                if (primvar.GetTypeName() == pxr::SdfValueTypeNames->Color3fArray
                    || primvar.GetTypeName() == pxr::SdfValueTypeNames->Color3dArray
                    || primvar.GetTypeName() == pxr::SdfValueTypeNames->Color3hArray) {
                    inferredVertexColorPrimvar = primvar;
                    // If we found both a texcoord UV primvar and the vertex color primvar we can
                    // stop looping.
                    if (texCoordUvFound) {
                        break;
                    }
                }
            }
        }

        if (inferredUvPrimvar.IsDefined()) {
            channelPrimvars[1] = inferredUvPrimvar;
            processedPrimvars.insert(inferredUvPrimvar.GetName());
            MaxUsd::Log::Info(
                "No explicitly mapped primvar was found for the main UV channel (1) when importing "
                "{0}, falling back to {1} of type {2}",
                mesh.GetPath().GetString(),
                inferredUvPrimvar.GetName().GetString(),
                inferredUvPrimvar.GetTypeName().GetAsToken().GetString());
        }

        if (inferredVertexColorPrimvar.IsDefined()) {
            channelPrimvars[0] = inferredVertexColorPrimvar;
            processedPrimvars.insert(inferredVertexColorPrimvar.GetName());
            MaxUsd::Log::Info(
                "No explicitly mapped primvar was found for the main Vertex Color channel (0) when "
                "importing "
                "{0}, falling back to {1} of type {2}",
                mesh.GetPath().GetString(),
                inferredVertexColorPrimvar.GetName().GetString(),
                inferredVertexColorPrimvar.GetTypeName().GetAsToken().GetString());
        }
    }

    // Next, map any other primvars of dimensions 1,2 and 3 to the next available channels.
    // We dont want to load any primvar into the main UV channel or the Vertex Color channel. If not
    // mapped at this point, these will remain empty. Therefor, we start the search for the next
    // empty channel to be used at 2.
    int nextChannel = 2;
    for (const auto& primvar : primvars) {
        if (processedPrimvars.find(primvar.GetName()) != processedPrimvars.end()) {
            continue;
        }
        // Primvars of dimension > 3 do not fit in max channels. They could still be explicitely
        // mapped, but this causes warnings and data-loss.
        const auto dimension = MaxUsd::GetTypeDimension(primvar.GetTypeName());
        if (dimension > 3) {
            continue;
        }

        // Find the next suitable channel.
        while (channelPrimvars.find(nextChannel) != channelPrimvars.end()) {
            nextChannel++;
        }

        MaxUsd::Log::Info(
            "Importing unmapped primvar {0} on {1} to channel {2}.",
            primvar.GetName().GetString(),
            mesh.GetPath().GetString(),
            nextChannel);

        channelPrimvars[nextChannel++] = primvar;
    }
}

void MeshConverter::ApplyUSDPrimvars(
    const pxr::UsdGeomMesh&      usdMesh,
    MNMesh&                      maxMesh,
    const PrimvarMappingOptions& options,
    std::map<int, std::string>&  channelNames,
    const pxr::UsdTimeCode&      timeCode)
{
    // First figure out exactly how many channels we will be imported.
    std::map<int, pxr::UsdGeomPrimvar> channelPrimvars;
    ResolveChannelPrimvars(usdMesh, options, channelPrimvars);

    pxr::TfToken orientation;
    usdMesh.GetOrientationAttr().Get(&orientation, timeCode);
    const bool leftHandedOrientation = orientation == pxr::UsdGeomTokens->leftHanded;
    // Now ready to do the work!
    channelNames.clear();
    for (const auto& mapping : channelPrimvars) {
        MapBuilder  builder(&maxMesh, mapping.first, leftHandedOrientation);
        const auto& primvar = mapping.second;
        if (!builder.Build(
                primvar.GetAttr(), primvar.GetInterpolation(), &primvar, usdMesh, timeCode)) {
            MaxUsd::Log::Info(
                "Unable to import {0} into channel {1}.",
                primvar.GetName().GetString(),
                mapping.first);
            continue;
        }
        channelNames.insert({ mapping.first, mapping.second.GetPrimvarName() });
    }
}

void MeshConverter::ApplyMaxMaterialIDs(
    Mtl*                                    mtl,
    const std::map<MtlID, pxr::VtIntArray>& materialIdToFacesMap,
    const pxr::UsdPrim&                     usdPrim,
    const pxr::UsdTimeCode&                 timeCode)
{
    // if we only use one matId do not create subsets
    if (materialIdToFacesMap.size() == 1) {
        // offset the source mat id by one to match maxscript indexing and the max UI, and set is as
        // metatdata to the USD prim.
        int matId = materialIdToFacesMap.begin()->first + 1;
        usdPrim.SetCustomDataByKey(MaxUsd::MetaData::matId, pxr::VtValue(matId));
        return;
    }

    pxr::UsdShadeMaterialBindingAPI meshBindingAPI(usdPrim);
    const auto                      existingSubsets = meshBindingAPI.GetMaterialBindSubsets();
    bool                            createSubsets = existingSubsets.empty();
    if (createSubsets) {
        meshBindingAPI.SetMaterialBindSubsetsFamilyType(pxr::UsdGeomTokens->partition);
    }

    MaxUsd::UniqueNameGenerator subsetNameGenerator;

    auto subsetIdx = 0;
    for (const auto& it : materialIdToFacesMap) {
        pxr::UsdGeomSubset subset;
        // No subsets yet? Create them.
        if (createSubsets) {
            // Name it after the material.
            std::string subsetName = MaterialUtils::CreateSubsetName(mtl, it.first);
            subsetName = subsetNameGenerator.GetName(subsetName);

            // API forces us to specify indices at the default time, but we don't want to, pass in
            // an empty array, and then clear the attribute of it.
            subset = meshBindingAPI.CreateMaterialBindSubset(pxr::TfToken(subsetName), {});
            subset.GetIndicesAttr().Clear();

            // Set the source matId in the prim's metadata.
            // Offset the source mat id by one to match maxscript indexing and the max UI, and set
            // is as metatdata to the USD subset.
            subset.GetPrim().SetCustomDataByKey(
                MaxUsd::MetaData::matId, pxr::VtValue(int(it.first) + 1));
        } else {
            subset = existingSubsets[subsetIdx];
        }

        // Finall, set the subset face indices.
        subset.CreateIndicesAttr().Set(it.second, timeCode);
        subsetIdx++;
    }
}

void MeshConverter::ApplyMatIdToMesh(
    const pxr::UsdGeomSubset& subset,
    MNMesh&                   maxMesh,
    int                       matId,
    const pxr::UsdTimeCode&   timeCode)
{
    pxr::VtIntArray indices;
    if (subset.GetIndicesAttr().Get(&indices, timeCode)) {
        for (const auto it : indices) {
            // Safeguard against bad data.
            if (it <= maxMesh.FNum() - 1 && it >= 0) {
                maxMesh.F(it)->material = matId;
            }
        }
    }
}

int MeshConverter::GetMaterialIdFromCustomData(const pxr::UsdPrim& usdPrim)
{
    pxr::VtValue matIdVtValue = usdPrim.GetCustomDataByKey(MaxUsd::MetaData::matId);
    if (!matIdVtValue.IsEmpty()) {
        return matIdVtValue.Get<int>() - 1;
    }
    return -1;
}

void MeshConverter::ApplyUSDMaterialIDs(
    const pxr::UsdPrim&     usdPrim,
    MNMesh&                 maxMesh,
    const pxr::UsdTimeCode& timeCode,
    MultiMtl**              geomSubsetMaterial)
{
    // If the custom data for matId is on the prim itself it means all the faces have the same id.
    int matId = GetMaterialIdFromCustomData(usdPrim);
    if (matId >= 0) {
        for (int i = 0; i < maxMesh.numf; ++i) {
            maxMesh.F(i)->material = matId;
        }
        return;
    }

    pxr::UsdShadeMaterialBindingAPI meshBindingAPI(usdPrim);
    std::vector<pxr::UsdGeomSubset> mtlBindSubsets = meshBindingAPI.GetMaterialBindSubsets();

    const auto mtlCount = mtlBindSubsets.size();

    if (mtlCount > 0) {
        std::sort(
            mtlBindSubsets.begin(),
            mtlBindSubsets.end(),
            [](const pxr::UsdGeomSubset& s1, const pxr::UsdGeomSubset& s2) {
                return s1.GetPrim().GetName() < s2.GetPrim().GetName();
            });

        // Start by populating all the material ID for subset with custom data
        std::map<int, std::string> matIdToGeomSubsetPrimNameMap;
        std::vector<int>           subsetWithNoCustomDataIndexes;
        for (int i = 0; i < mtlCount; i++) {
            pxr::UsdGeomSubset subset = mtlBindSubsets[i];
            matId = GetMaterialIdFromCustomData(subset.GetPrim());

            if (matId >= 0) {
                ApplyMatIdToMesh(subset, maxMesh, matId, timeCode);
                matIdToGeomSubsetPrimNameMap[matId] = subset.GetPrim().GetName().GetString();
                continue;
            }

            subsetWithNoCustomDataIndexes.push_back(i);
        }

        // If we have subset without custom data generate a matId and assign it to the face.
        int matId = 0;
        for (auto i : subsetWithNoCustomDataIndexes) {
            pxr::UsdGeomSubset subset = mtlBindSubsets[i];
            while (matIdToGeomSubsetPrimNameMap.find(matId) != matIdToGeomSubsetPrimNameMap.end()) {
                matId++;
            }
            ApplyMatIdToMesh(subset, maxMesh, matId, timeCode);
            matIdToGeomSubsetPrimNameMap[matId] = subset.GetPrim().GetName().GetString();
        }

        // Setup the geomSubsetMaterial material to enable material binding.
        if (geomSubsetMaterial) {
            auto& multiMaterial = *geomSubsetMaterial;
            multiMaterial = NewDefaultMultiMtl();
            multiMaterial->SetNumSubMtls(static_cast<int>(matIdToGeomSubsetPrimNameMap.size()));

            IParamBlock2* mtlParamBlock2 = multiMaterial->GetParamBlockByID(0);
            short         paramId = MaxUsd::FindParamId(mtlParamBlock2, L"materialIDList");
            if (paramId < 0) {
                MaxUsd::Log::Error(
                    "Unable to find materialIDList param id on multiMaterial param block.");
            } else {
                int paramBlockTabIndex = 0;
                for (const auto& matIdToSubsetMapIt : matIdToGeomSubsetPrimNameMap) {
                    mtlParamBlock2->SetValue(
                        paramId, 0, matIdToSubsetMapIt.first, paramBlockTabIndex);
                    paramBlockTabIndex++;
                    WStr slotName = MaxUsd::UsdStringToMaxString(matIdToSubsetMapIt.second);
                    multiMaterial->SetSubMtlAndName(matIdToSubsetMapIt.first, nullptr, slotName);
                }
            }
        }
    }
}

void MeshConverter::ApplyMaxVertCreases(
    MeshFacade&       maxMesh,
    pxr::UsdGeomMesh& usdMesh,
    pxr::UsdTimeCode  timeCode)
{
    const float* vCreaseData = maxMesh.VertexCreaseData();
    if (!vCreaseData) {
        return;
    }

    pxr::VtIntArray   cornerIndices;
    pxr::VtFloatArray cornerSharpnesses;

    // Copy the vertex creases into the proper data structures for export
    for (int i = 0; i < maxMesh.VertexCount(); ++i) {
        // Only consider non-zero creases
        float creaseVal = MathUtils::clamp(vCreaseData[i], 0.f, 1.f);
        if (!MathUtils::IsAlmostZero(creaseVal)) {
            cornerIndices.push_back(i);
            cornerSharpnesses.push_back(creaseVal * MAX2USD_CREASE);
        }
    }

    if (!cornerIndices.empty()) {
        usdMesh.CreateCornerIndicesAttr().Set(cornerIndices, timeCode);
        usdMesh.CreateCornerSharpnessesAttr().Set(cornerSharpnesses, timeCode);
    }
}

void MeshConverter::ApplyUSDVertCreases(
    const pxr::UsdGeomMesh& usdMesh,
    MNMesh&                 maxMesh,
    pxr::UsdTimeCode        timeCode)
{
    pxr::VtIntArray cornerIndices;
    usdMesh.GetCornerIndicesAttr().Get(&cornerIndices, timeCode);
    pxr::VtFloatArray cornerSharpnesses;
    usdMesh.GetCornerSharpnessesAttr().Get(&cornerSharpnesses, timeCode);

    // Return if no crease
    if (cornerIndices.size() == 0 || cornerSharpnesses.size() == 0) {
        return;
    }

    // Make sure we can write crease data to proper vertex channel
    float* vCreaseData = maxMesh.vertexFloat(VDATA_CREASE);
    if (!vCreaseData) {
        maxMesh.setVDataSupport(VDATA_CREASE);
        vCreaseData = maxMesh.vertexFloat(VDATA_CREASE);
        assert(vCreaseData);
    }

    // Warn and return if inconsistent data
    if (cornerIndices.size() != cornerSharpnesses.size()) {
        MaxUsd::Log::Warn(
            "Vertex creasing data cannot be imported to 3ds Max because the data is inconsistent: "
            "the sizes of {0} and {1} should be equal.",
            usdMesh.GetCornerIndicesAttr().GetName().GetString(),
            usdMesh.GetCornerSharpnessesAttr().GetName().GetString());
        return;
    }

    // Apply creasing
    for (size_t i = 0; i < cornerIndices.size(); ++i) {
        int v = cornerIndices[i];
        if (maxMesh.v[v].GetFlag(MN_DEAD)) {
            continue;
        }

        // 3ds max only handles positive numbers between 0 and 1
        // USD creases range from 0 to 10, anything above being perfectly sharp.
        // So negative values with clamp to 0 and rescale all other values between 0 and 1
        vCreaseData[v] = MaxUsd::MathUtils::clamp(cornerSharpnesses[i] * USD2MAX_CREASE, 0.f, 1.f);
    }
}

void MeshConverter::ApplyMaxEdgeCreases(
    MeshFacade&       maxMesh,
    pxr::UsdGeomMesh& usdMesh,
    pxr::UsdTimeCode  timeCode)
{
    const float* eCreaseData = maxMesh.EdgeCreaseData();
    if (!eCreaseData) {
        return;
    }

    // Copy the crease groups into the proper USD data structures for export
    pxr::VtIntArray   creaseIndices;
    pxr::VtIntArray   creaseLengths;
    pxr::VtFloatArray creaseSharpnesses;

    // Copy the edge creases into the proper data structures for export
    for (int i = 0; i < maxMesh.EdgeCount(); ++i) {
        // Only consider non-zero creases
        float creaseVal = MathUtils::clamp(eCreaseData[i], 0.f, 1.f);
        if (!MathUtils::IsAlmostZero(creaseVal)) {
            creaseIndices.push_back(maxMesh.EdgeVertex(i, true));
            creaseIndices.push_back(maxMesh.EdgeVertex(i, false));
            creaseLengths.push_back(2);
            creaseSharpnesses.push_back(creaseVal * MAX2USD_CREASE);
        }
    }

    if (!creaseIndices.empty()) {
        usdMesh.CreateCreaseIndicesAttr().Set(creaseIndices, timeCode);
        usdMesh.CreateCreaseLengthsAttr().Set(creaseLengths, timeCode);
        usdMesh.CreateCreaseSharpnessesAttr().Set(creaseSharpnesses, timeCode);
    }
}

void MeshConverter::ApplyUSDEdgeCreases(
    const pxr::UsdGeomMesh& usdMesh,
    MNMesh&                 maxMesh,
    pxr::UsdTimeCode        timeCode)
{
    pxr::VtIntArray creaseIndices;
    usdMesh.GetCreaseIndicesAttr().Get(&creaseIndices, timeCode);
    pxr::VtIntArray creaseLengths;
    usdMesh.GetCreaseLengthsAttr().Get(&creaseLengths, timeCode);
    pxr::VtFloatArray creaseSharpnesses;
    usdMesh.GetCreaseSharpnessesAttr().Get(&creaseSharpnesses, timeCode);

    // Return if no crease
    if (creaseIndices.size() == 0 || creaseLengths.size() == 0 || creaseSharpnesses.size() == 0) {
        return;
    }

    // Make sure we can write crease data to proper edge channel
    float* eCreaseData = maxMesh.edgeFloat(EDATA_CREASE);
    if (!eCreaseData) {
        maxMesh.setEDataSupport(EDATA_CREASE);
        eCreaseData = maxMesh.edgeFloat(EDATA_CREASE);
        assert(eCreaseData);
    }

    // The sum of all crease groups should be the size of creaseIndices
    // and the number of crease groups should be equal to the number of sharpnesses
    // Warn and return if it isn't the case
    int nbIndices = std::accumulate(creaseLengths.cbegin(), creaseLengths.cend(), 0);
    if (creaseIndices.size() != nbIndices || creaseLengths.size() != creaseSharpnesses.size()) {
        MaxUsd::Log::Warn(
            "Edge creasing data cannot be imported to 3ds Max because the data is inconsistent: "
            "the size of {0} should be the sum of all {1} and the sizes of {1} and {2} should be "
            "equal.",
            usdMesh.GetCreaseIndicesAttr().GetName().GetString(),
            usdMesh.GetCreaseLengthsAttr().GetName().GetString(),
            usdMesh.GetCornerSharpnessesAttr().GetName().GetString());
        return;
    }

    // Apply creasing
    unsigned int creaseIndexBase = 0;
    for (size_t creaseGroup = 0; creaseGroup < creaseLengths.size();
         creaseIndexBase += creaseLengths[creaseGroup++]) {
        for (size_t i = 0; i < creaseLengths[creaseGroup] - 1; i++) {
            int eIndex = maxMesh.FindEdgeFromVertToVert(
                creaseIndices[creaseIndexBase + i], creaseIndices[creaseIndexBase + i + 1]);
            if (eIndex >= 0 && eIndex < maxMesh.nume) {
                if (maxMesh.e[eIndex].GetFlag(MN_DEAD)) {
                    continue;
                }

                // 3ds max only handles positive numbers between 0 and 1
                // USD creases range from 0 to 10, anything above being perfectly sharp.
                // So negative values with clamp to 0 and rescale all other values between 0 and 1
                eCreaseData[eIndex]
                    = MathUtils::clamp(creaseSharpnesses[creaseGroup] * USD2MAX_CREASE, 0.f, 1.f);
            }
        }
    }
}

} // namespace MAXUSD_NS_DEF
