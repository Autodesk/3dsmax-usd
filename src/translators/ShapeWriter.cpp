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
#include "ShapeWriter.h"

#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/Translators/primWriter.h>
#include <MaxUsd/Translators/writeJobContext.h>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>

#include <linshape.h>
#include <simpshp.h>
#include <simpspl.h>
#include <splshape.h>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdShapeWriter::MaxUsdShapeWriter(const MaxUsdWriteJobContext& jobCtx, INode* node)
    : MaxUsdPrimWriter(jobCtx, node)
{
}

MaxUsdPrimWriter::ContextSupport
MaxUsdShapeWriter::CanExport(INode* node, const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    if (!exportArgs.GetTranslateShapes()) {
        return ContextSupport::Unsupported;
    }

    const auto object = node->EvalWorldState(exportArgs.GetResolvedTimeConfig().GetStartTime()).obj;
    if (object->SuperClassID() != SHAPE_CLASS_ID) {
        return ContextSupport::Unsupported;
    }

    ShapeObject* shapeObject = dynamic_cast<ShapeObject*>(object);

    // First make sure that we can indeed export this shape.
    if (!shapeObject) {
        return ContextSupport::Unsupported;
    }
    return ContextSupport::Fallback;
}

bool MaxUsdShapeWriter::Write(
    UsdPrim&                  targetPrim,
    bool                      applyOffsetTransform,
    const MaxUsd::ExportTime& time)
{
    INode* sourceNode = GetNode();

    ShapeObject* shapeObject
        = dynamic_cast<ShapeObject*>(sourceNode->EvalWorldState(time.GetMaxTime()).obj);

    // First make sure that we can indeed export this shape.
    if (!shapeObject) {
        // Should not happen, SHAPE type objects should at the very least cast to ShapeObjects just
        // above - might still happen from faulty programming?
        MSTR className;
        sourceNode->GetClassName(className);
        if (time.IsFirstFrame()) {
            // On the first frame only, log this error, it is time independent.
            MaxUsd::Log::Error(
                L"The \"{}\" shape class used by node \"{}\" is not supported and could "
                L"not be properly exported to a UsdGeomBasisCurves USD prim.",
                className.data(),
                sourceNode->GetName());
        }
        return false;
    }

    auto stage = targetPrim.GetStage();
    auto primPath = targetPrim.GetPath();
    auto timeConfig = GetExportArgs().GetResolvedTimeConfig();

    // If the shape is displayed as a mesh in the viewport, we need to export it as such also.
    if (shapeObject->GetDispRenderMesh() == TRUE) {
        MaxUsd::MeshConverter meshConverter;
        meshConverter.ConvertToUSDMesh(
            sourceNode,
            stage,
            primPath,
            GetExportArgs().GetMeshConversionOptions(),
            applyOffsetTransform,
            timeConfig.IsAnimated(),
            time);
        return true;
    }

    pxr::UsdGeomBasisCurves usdCurve(targetPrim);

    // Setup some non-animatable attributes (only do it once, when we export the first frame).
    if (time.IsFirstFrame()) {
        // Currently exported "linear" type curves, as we export the interpolated splines.
        usdCurve.CreateTypeAttr().Set(pxr::TfToken("linear"));

        // Use the wire color as USD display color.
        Color             wireColor(sourceNode->GetWireColor());
        pxr::VtVec3fArray usdDisplayColor = { pxr::GfVec3f(wireColor.r, wireColor.g, wireColor.b) };
        usdCurve.CreateDisplayColorAttr().Set(usdDisplayColor);
    }

    const auto& timeVal = time.GetMaxTime();
    const auto& usdTimeCode = time.GetUsdTime();

    // Utility lambda to get the PolyShape that we will end up exporting.
    // We export the interpolation of the shapes, so the exported curve will match what is actually
    // seen in the Max viewport - all shapes get "baked" to a PolyShape representation. In some
    // cases, a PolyShape is already the internal representation, while in others, we need to build
    // it using the interpolation parameters. This lambda returns, for a given object, the PolyShape
    // that we need to export, and whether that PolyShape needs to be destroyed (meaning it had to
    // be created for our purpose).
    auto getPolyShape = [](Object*   object,
                           WStr      nodeName,
                           TimeValue timeVal,
                           bool      displayWarnings) -> std::pair<PolyShape*, bool> {
        // In the case of bezier shapes, we need to interpolate the curve.
        BezierShape* bezierShape = nullptr;
        SplineShape* splineShape = dynamic_cast<SplineShape*>(object);
        if (splineShape) {
            bezierShape = &splineShape->shape;
        } else {
            SimpleSpline* simpleSpline = dynamic_cast<SimpleSpline*>(object);
            if (simpleSpline != nullptr) {
                bezierShape = &simpleSpline->shape;
            }
        }
        if (bezierShape) {
            auto polyShape = new PolyShape();
            bezierShape->MakePolyShape(*polyShape, bezierShape->steps, bezierShape->optimize);
            return { polyShape, true }; // Will need to be deleted by caller.
        }

        SimpleShape* simpleShape = dynamic_cast<SimpleShape*>(object);
        if (simpleShape != nullptr) {
            return { &simpleShape->shape, false };
        }

        LinearShape* linearShape = dynamic_cast<LinearShape*>(object);
        if (linearShape != nullptr) {
            return { &linearShape->shape, false };
        }

        // Unknown shape type, try and convert it to a LinearShape.
        const auto linearShapeClassId = Class_ID { LINEARSHAPE_CLASS_ID, 0 };
        if (object->CanConvertToType(linearShapeClassId)) {
            LinearShape* linearShape
                = static_cast<LinearShape*>(object->ConvertToType(timeVal, linearShapeClassId));
            // We could avoid this copy at the cost of code complexity. As this is mostly fallback
            // code, I dont think it is worth it at this time.
            auto polyShape = new PolyShape { linearShape->shape };
            linearShape->MaybeAutoDelete();
            return { polyShape, true };
        }

        MSTR className;
        object->GetClassName(className);

        // At this point, we already made sure that the object can cast to ShapeObject.
        ShapeObject* shapeObject = static_cast<ShapeObject*>(object);

        if (displayWarnings) {
            MaxUsd::Log::Warn(
                L"The \"{}\" shape class used by node \"{}\" is not fully supported for node, "
                L"using "
                L"adaptive interpolation.",
                className.data(),
                nodeName.data());
        }
        auto polyShape = new PolyShape();
        shapeObject->MakePolyShape(timeVal, *polyShape, PSHAPE_ADAPTIVE_STEPS, TRUE);
        return { polyShape, true };
    };

    const auto curvesVertexCountsAttr = usdCurve.CreateCurveVertexCountsAttr();
    const auto curvesPointsAttr = usdCurve.CreatePointsAttr();

    // Flag used to make sure we only raise warnings once, and not for every frame.
    const bool displayTimeDependantWarnings = time.IsFirstFrame();

    // getPolyShape() returns a pair :
    // first -> the polyShape to export.
    // second -> whether or not that polyShape should be deleted.
    const auto polyShapePair
        = getPolyShape(shapeObject, sourceNode->GetName(), timeVal, displayTimeDependantWarnings);

    PolyShape* polyShape = polyShapePair.first;
    const bool destroyPolyShape = polyShapePair.second;

    const bool wsmRequiresTransform = MaxUsd::WsmRequiresTransformToLocalSpace(sourceNode, timeVal);
    // If a WSM is applied, move the geometry's points back into local space. So that with the
    // transforms inherited from its hierarchy, the object will end up in the correct location on
    // the USD side.
    if (wsmRequiresTransform) {
        auto nodeTmInvert = sourceNode->GetNodeTM(timeVal);
        nodeTmInvert.Invert();
        polyShape->Transform(nodeTmInvert);
    }

    pxr::VtIntArray   vertexCounts;
    pxr::VtVec3fArray points;

    for (int i = 0; i < polyShape->numLines; ++i) {
        // Not getting a const reference because in Max 2021, ::IsClosed() is not const correct...
        auto& line = polyShape->lines[i];
        // If the curve is closed, repeat the first point at the end.
        const auto closed = line.IsClosed();
        int        numPoints = line.numPts;
        if (closed) {
            numPoints++;
        }

        vertexCounts.push_back(numPoints);
        for (int j = 0; j < line.numPts; ++j) {
            const auto point = line.pts[j];
            points.emplace_back(point.p.x, point.p.y, point.p.z);
        }

        if (closed) {
            const auto firstPoint = line.pts[0];
            points.emplace_back(firstPoint.p.x, firstPoint.p.y, firstPoint.p.z);
        }
    }

    curvesVertexCountsAttr.Set(vertexCounts, usdTimeCode);
    curvesPointsAttr.Set(points, usdTimeCode);

    if (destroyPolyShape) {
        delete polyShape;
    }

    return true;
}

MaxUsd::XformSplitRequirement MaxUsdShapeWriter::RequiresXformPrim()
{
    if (!GetExportArgs().GetAllowNestedGprims() && GetNode()->NumberOfChildren() > 0) {
        return MaxUsd::XformSplitRequirement::Always;
    }
    return MaxUsd::XformSplitRequirement::ForOffsetObjects;
}

PXR_NAMESPACE_CLOSE_SCOPE