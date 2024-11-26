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
#include "SpherePrimWriter.h"

#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/Translators/PrimWriterRegistry.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdShade/shader.h>

#include <iparamb2.h>
#include <notify.h>
#include <object.h>

// Macro for the pixar namespace "pxr::"
// This is putting all of the code, until the close macro, into the pixar namespace.
// This is needed for the Macros to compile and is required for all the Prim/Shader Writers
PXR_NAMESPACE_OPEN_SCOPE

// This macro registers the Prim Writer, it's adding the SpherePrimWriter class as a candidate when
// trying to export an object. Unlike for shader writer, this doesn't contain information about the
// supported classes. The CanExport() method is responsible for defining what can be exported or
// not. It is also really important to set the project option "Remove unreferenced code and data" to
// NO, this could cause the Macro to be optimized out and the Writer to never be properly
// registered.
PXR_MAXUSD_REGISTER_WRITER(SpherePrimWriter);

PXR_NAMESPACE_CLOSE_SCOPE

// For demonstration purposes, provide an example to export using a native usd sphere, or as a usd
// mesh.
static bool EXPORT_AS_NATIVE_SPHERE = true;

/*
 * This method is responsible of telling the export process if it can export the current node's
 * object. In the case of this sample, the only object type we want to handle is the Sphere.
 */
pxr::MaxUsdPrimWriter::ContextSupport
SpherePrimWriter::CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions&)
{
    // We simply pass TimeValue "0" here to EvalWorldStage() as it is not important at what time we
    // evaluate the object, we are only looking at the object's type and we assume that it will not
    // change over time.
    const auto object = node->EvalWorldState(0).obj;
    if (object->ClassID() == Class_ID(SPHERE_CLASS_ID, 0)) {
        return ContextSupport::Supported;
    }
    return ContextSupport::Unsupported;
}

/*
 * This Writer only deals with Sphere objects - we return a "Sphere" token if we want to convert the
 * node to a native USD Sphere, or mesh, if not. For performance reasons, the export is done in two
 * passes. The first pass creates all the prims inside a single SdfChangeBlock, the second pass
 * populate each prim's attributes. It is not mandatory to implement this function, it is also
 * possible to define the prim from the Write() method (which would override the prim's type created
 * in the first pass), but you would lose the performance benefit. If not implemented, the base
 * implementation returns Xform.
 */
pxr::TfToken SpherePrimWriter::GetPrimType()
{
    return EXPORT_AS_NATIVE_SPHERE ? pxr::TfToken("Sphere") : pxr::TfToken("Mesh");
}

SpherePrimWriter::SpherePrimWriter(const pxr::MaxUsdWriteJobContext& jobCtx, INode* node)
    : MaxUsdPrimWriter(jobCtx, node)
{
}

/*
 * For this sample, we will demonstrate how to export the Radius and DisplayColor attributes.
 * We'll also demonstrate one way to handle animation.
 */
bool SpherePrimWriter::Write(
    pxr::UsdPrim&             targetPrim,
    bool                      applyOffsetTransform,
    const MaxUsd::ExportTime& timeFrame)
{
    auto sourceNode = GetNode();

    if (EXPORT_AS_NATIVE_SPHERE) {
        // targetPrim is already a Sphere, it was created for us from the type returned in
        // GetPrimType()
        pxr::UsdGeomSphere spherePrim(targetPrim);
        pxr::UsdAttribute  radiusAttr = spherePrim.CreateRadiusAttr();
        pxr::UsdAttribute  extentAttr = spherePrim.CreateExtentAttr();
        const auto         spherePb
            = sourceNode->EvalWorldState(timeFrame.GetMaxTime()).obj->GetParamBlock(0);
        float    radius = 0;
        Interval inter = FOREVER;

        // Get the value in Max time.
        // We already know we're dealing with a Sphere object at this point, because we wouldn't be
        // here otherwise. CanConvert must have passed for Write to be called.
        spherePb->GetValueByName(_T("Radius"), timeFrame.GetMaxTime(), radius, inter);
        // Set it at the equivalent usd time.
        // When doing set on Attribute, the type must match exactly, otherwise it'll do nothing.
        radiusAttr.Set(static_cast<double>(radius), timeFrame.GetUsdTime());
        // Set the extent Attribute, this is usd to compute the bounding box in USD
        pxr::VtVec3fArray extent
            = { pxr::GfVec3f(-radius, -radius, -radius), pxr::GfVec3f(radius, radius, radius) };
        extentAttr.Set(extent, timeFrame.GetUsdTime());

        if (timeFrame.IsFirstFrame()) {
            pxr::UsdAttribute displayColorAttr = spherePrim.CreateDisplayColorAttr();
            const COLORREF    wireCol = sourceNode->GetWireColor();
            pxr::VtVec3fArray colVec;
            colVec.push_back(pxr::GfVec3f(
                static_cast<int>(GetRValue(wireCol)) / 255.f,
                static_cast<int>(GetGValue(wireCol)) / 255.f,
                static_cast<int>(GetBValue(wireCol)) / 255.f));
            // When doing set on Attribute, the type must match exactly, otherwise it'll do nothing.
            displayColorAttr.Set(colVec);
        }
    }
    // Alternatively, we could export the Sphere as a mesh, using the MeshConverter utility :
    else {
        MaxUsd::MeshConverter converter;
        converter.ConvertToUSDMesh(
            sourceNode,
            targetPrim.GetStage(),
            targetPrim.GetPath(),
            GetExportArgs().GetMeshConversionOptions(),
            applyOffsetTransform,
            GetExportArgs().GetResolvedTimeConfig().IsAnimated(),
            timeFrame);
    }
    return true;
}

Interval SpherePrimWriter::GetValidityInterval(const TimeValue& time)
{
    // The base implementation of GetValidityInterval() will return the object's validity interval.
    // In the sphere writer, we only export the radius, for demonstration purposes, lets make sure
    // we only export the frames we really need, by telling the exporter that the exported sphere
    // is valid as long as the radius doesnt change (i.e. we dont care about other properties that
    // may change on the 3dsmax sphere)
    const auto node = GetNode();
    const auto spherePb = node->EvalWorldState(time).obj->GetParamBlock(0);

    Interval radiusInterval = FOREVER;
    float    radius = 0;
    spherePb->GetValueByName(_T("Radius"), time, radius, radiusInterval);

    return radiusInterval;
}
