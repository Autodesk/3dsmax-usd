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

#include "pxr/pxr.h" // PXR_VERSION

// Light gizmos are only supported in version with USD 23.11+
#if PXR_VERSION >= 2311

#include "HdLightGizmoSceneIndexFilter.h"
#include "HdMaxLightGizmoMeshAccess.h"

#include <MaxUsd/Utilities/UsdToolsUtils.h>

#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/usd/usdGeom/mesh.h>

#if _MSVC_LANG > 201402L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
const std::string SPHERE_SHAPE = "sphereLight";
const std::string DISK_SHAPE = "diskLight";
const std::string RECT_SHAPE = "rectLight";
const std::string CYLINDER_SHAPE = "cylinderLight";
const std::string BASE_SHAPE = "baseLight";
} // namespace

std::unordered_map<std::string, pxr::HdLightGizmoSceneIndexFilter::GizmoMesh>
    HdMaxLightGizmoMeshAccess::gizmoMeshes;
std::unordered_map<pxr::TfToken, std::string, pxr::TfToken::HashFunctor>
    HdMaxLightGizmoMeshAccess::typeGizmos;

HdMaxLightGizmoMeshAccess::HdMaxLightGizmoMeshAccess()
{
    // Load gizmo meshes once per 3dsMax session.
    if (gizmoMeshes.empty()) {

        std::wstring gizmoDir;
        MaxUsd::UsdToolsUtils::GetPluginDirectory(gizmoDir);
        gizmoDir = gizmoDir.append(L"/../lightGizmos");

        for (const auto& entry : fs::directory_iterator(gizmoDir)) {
            if (entry.path().extension() == ".usda" || entry.path().extension() == ".usd") {

                auto stage = pxr::UsdStage::Open(entry.path().string());
                if (!stage) {
                    DbgAssert("Invalid Light gizmo USD source file." && 0);
                    continue;
                }

                auto mesh = pxr::UsdGeomMesh::Get(stage, pxr::SdfPath { "/root/gizmo" });
                if (!mesh.GetPrim().IsValid()) {
                    DbgAssert("Light gizmo USD source file has no /root/gizmo mesh prim." && 0);
                    continue;
                }
                VtIntArray vertCounts;
                mesh.GetFaceVertexCountsAttr().Get(&vertCounts);
                VtIntArray indices;
                mesh.GetFaceVertexIndicesAttr().Get(&indices);
                VtVec3fArray points;
                mesh.GetPointsAttr().Get(&points);

                pxr::VtVec3fArray extent;
                mesh.GetExtentAttr().Get(&extent);

                gizmoMeshes[entry.path().filename().replace_extension().string()]
                    = { vertCounts, indices, points, extent };
            }
        }

        // Mapping light type to source usd file names.
        typeGizmos[HdPrimTypeTokens->diskLight] = DISK_SHAPE;
        typeGizmos[HdPrimTypeTokens->cylinderLight] = CYLINDER_SHAPE;
        typeGizmos[HdPrimTypeTokens->rectLight] = RECT_SHAPE;
        typeGizmos[HdPrimTypeTokens->sphereLight] = SPHERE_SHAPE;

        // For all other light types, fallback to a sphere.
        typeGizmos[HdPrimTypeTokens->light] = BASE_SHAPE;
        typeGizmos[HdPrimTypeTokens->distantLight] = BASE_SHAPE;
        typeGizmos[HdPrimTypeTokens->domeLight] = BASE_SHAPE;
        typeGizmos[HdPrimTypeTokens->meshLight] = BASE_SHAPE;
        typeGizmos[HdPrimTypeTokens->pluginLight] = BASE_SHAPE;
        typeGizmos[HdPrimTypeTokens->simpleLight] = BASE_SHAPE;
    }
}

GfMatrix4d HdMaxLightGizmoMeshAccess::GetLightScalingMatrix(
    const TfToken&                     type,
    const SdfPath&                     path,
    const HdContainerDataSourceHandle& source)
{
    GfMatrix4d scaling;

    // To read light parameters, we need to use the scene delegate that is passed in the data
    // source.
    auto sceneDelSrc = HdRetainedTypedSampledDataSource<HdSceneDelegate*>::Cast(
        source->Get(pxr::TfToken("sceneDelegate")));
    if (!sceneDelSrc) {

        scaling.SetScale(1);
        return scaling;
    }

    auto sceneDel = sceneDelSrc->GetValue(0).Get<HdSceneDelegate*>();
    if (!sceneDel) {

        scaling.SetScale(1);
        return scaling;
    }

    static const TfToken radiusToken = pxr::TfToken("radius");
    static const TfToken widthToken = pxr::TfToken("width");
    static const TfToken heightToken = pxr::TfToken("height");
    static const TfToken lengthToken = pxr::TfToken("length");

    // The scaling is calculated relative to the default values for the parameters.

    if (type == HdPrimTypeTokens->sphereLight) {
        const auto radius = sceneDel->GetLightParamValue(path, radiusToken).Get<float>() / 0.5f;
        return scaling.SetScale(radius);
    }

    if (type == HdPrimTypeTokens->diskLight) {
        const auto radius = sceneDel->GetLightParamValue(path, radiusToken).Get<float>() / 0.5f;
        return scaling.SetScale(radius);
    }

    if (type == HdPrimTypeTokens->rectLight) {
        const auto width = sceneDel->GetLightParamValue(path, widthToken).Get<float>();
        const auto height = sceneDel->GetLightParamValue(path, heightToken).Get<float>();
        return scaling.SetScale({ width, height, 1 });
    }

    if (type == HdPrimTypeTokens->cylinderLight) {
        const auto radius = sceneDel->GetLightParamValue(path, radiusToken).Get<float>() / 0.5f;
        const auto length = sceneDel->GetLightParamValue(path, lengthToken).Get<float>();
        return scaling.SetScale({ length, radius, radius });
    }

    scaling.SetScale(1);
    return scaling;
}

pxr::HdRetainedContainerDataSourceHandle HdMaxLightGizmoMeshAccess::GetGizmoSource(
    const HdSceneIndexPrim& sourceLight,
    const SdfPath&          lightPath) const
{
    const auto it = typeGizmos.find(sourceLight.primType);
    if (it == typeGizmos.end()) {
        DbgAssert("Unsupported light type gizmo." && 0);
        return {};
    }

    const auto& baseGizmoIt = gizmoMeshes.find(BASE_SHAPE);
    if (baseGizmoIt == gizmoMeshes.end()) {
        DbgAssert("Base light gizmo not found." && 0);
        return {};
    }

    const auto& baseGizmo = baseGizmoIt->second;

    const auto& lightDataSrc = sourceLight.dataSource;

    pxr::GfBBox3d baseGizmoBB = { { baseGizmo.extent[0], baseGizmo.extent[1] } };

    VtIntArray       vertexCounts;
    VtIntArray       indices;
    VtArray<GfVec3f> points;
    GfVec3d          extMin;
    GfVec3d          extMax;

    // Some lights require special gizmos to show the light shape. Those shapes are scaled according
    // to the light parameters.
    if (it->second != BASE_SHAPE) {

        // Get the type specific shape and scale it.
        const auto meshIt = gizmoMeshes.find(it->second);
        if (meshIt == gizmoMeshes.end()) {
            DbgAssert("Unmapped light gizmo mesh." && 0);
            return {};
        }
        const auto& gizmoMesh = &meshIt->second;

        // VtArrays are copy on write.
        vertexCounts = gizmoMesh->vertexCounts;
        indices = gizmoMesh->indices;
        points = gizmoMesh->points;

        GfMatrix4d scaling = GetLightScalingMatrix(sourceLight.primType, lightPath, lightDataSrc);

        for (auto& p : points) {
            p = scaling.Transform(p);
        }

        // Append the base gizmo mesh.
        auto initVertexCountsSize = vertexCounts.size();
        vertexCounts.resize(initVertexCountsSize + baseGizmo.vertexCounts.size());
        for (int i = 0; i < baseGizmo.vertexCounts.size(); ++i) {
            vertexCounts[initVertexCountsSize + i] = baseGizmo.vertexCounts[i];
        }

        auto initPointsSize = points.size();
        points.resize(initPointsSize + baseGizmo.points.size());
        for (int i = 0; i < baseGizmo.points.size(); ++i) {
            points[initPointsSize + i] = baseGizmo.points[i];
        }

        auto initIndicesCount = indices.size();
        indices.resize(initIndicesCount + baseGizmo.indices.size());
        for (int i = 0; i < baseGizmo.indices.size(); ++i) {
            indices[initIndicesCount + i] = baseGizmo.indices[i] + static_cast<int>(initPointsSize);
        }

        pxr::GfBBox3d shapeGizmoBB { { scaling.Transform(gizmoMesh->extent[0]),
                                       scaling.Transform(gizmoMesh->extent[1]) } };

        const auto boundingBox = GfBBox3d::Combine(shapeGizmoBB, baseGizmoBB);

        extMin = boundingBox.GetRange().GetMin();
        extMax = boundingBox.GetRange().GetMax();

    } else {
        vertexCounts = baseGizmo.vertexCounts;
        indices = baseGizmo.indices;
        points = baseGizmo.points;
        extMin = baseGizmo.extent[0];
        extMax = baseGizmo.extent[1];
    }

    using PointArrayDs = HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>;
    using IntArrayDs = HdRetainedTypedSampledDataSource<VtIntArray>;

    const auto meshSrc = HdMeshSchema::Builder()
                             .SetTopology(HdMeshTopologySchema::Builder()
                                              .SetFaceVertexCounts(IntArrayDs::New(vertexCounts))
                                              .SetFaceVertexIndices(IntArrayDs::New(indices))
                                              .Build())
                             .Build();

    const auto primvarsSrc = HdRetainedContainerDataSource::New(
        // Create the vertex positions primvar.
        HdPrimvarsSchemaTokens->points,
        HdPrimvarSchema::Builder()
            .SetPrimvarValue(PointArrayDs::New(points))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(HdPrimvarSchemaTokens->vertex))
            .SetRole(HdPrimvarSchema::BuildRoleDataSource(HdPrimvarSchemaTokens->point))
            .Build(),
        // Create the isGizmo primvar.
        TfToken("isGizmo"),
        HdPrimvarSchema::Builder()
            .SetPrimvarValue(HdRetainedTypedSampledDataSource<bool>::New(true))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(HdPrimvarSchemaTokens->constant))
            .Build());

    auto extSrc = HdExtentSchema::Builder()
                      .SetMin(HdRetainedTypedSampledDataSource<GfVec3d>::New(extMin))
                      .SetMax(HdRetainedTypedSampledDataSource<GfVec3d>::New(extMax))
                      .Build();

    // Override the display style to wireframe.
    const auto displayStyleSrc = HdRetainedContainerDataSource::New(
        HdLegacyDisplayStyleSchemaTokens->reprSelector,
        HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
            { HdReprTokens->refinedWire, TfToken(), TfToken() }));

    return HdRetainedContainerDataSource::New(
        // Xform and instanceBy as-is from the light.
        HdXformSchemaTokens->xform,
        lightDataSrc->Get(HdXformSchemaTokens->xform),
        HdInstancedBySchemaTokens->instancedBy,
        lightDataSrc->Get(HdInstancedBySchemaTokens->instancedBy),
        // The rest is specific to the gizmo.
        HdMeshSchemaTokens->mesh,
        meshSrc,
        HdPrimvarsSchemaTokens->primvars,
        primvarsSrc,
        HdExtentSchemaTokens->extent,
        extSrc,
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        displayStyleSrc);
}

#endif