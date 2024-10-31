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

enum BasisCurvesReaderCodes
{
    InconsistentSplineTypesAndOrWarps
};

TF_REGISTRY_FUNCTION(TfEnum)
{
    TF_ADD_ENUM_NAME(
        InconsistentSplineTypesAndOrWarps,
        "Inconsistent spline type(linear/cubic) and/or wrap(closed/open) mode under same "
        "SplineShape detected.");
};

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

    // helper lambda to process linear shapes
    auto processLinearShape
        = [this](Spline3D* spline, pxr::VtIntArray& vertexCounts, pxr::VtVec3fArray& points) {
              int splKnotCount = spline->KnotCount();
              if (splKnotCount == 0) {
                  return;
              }

              points.reserve(splKnotCount);
              for (int j = 0; j < splKnotCount; ++j) {
                  SplineKnot kspl = spline->GetKnot(j);
                  Point3     knot = kspl.Knot();

                  points.emplace_back(knot.x, knot.y, knot.z);
              }
              vertexCounts.push_back(splKnotCount);
          };

    // helper lambda to process cubic shapes
    auto processCubicShape
        = [this](Spline3D* spline, pxr::VtIntArray& vertexCounts, pxr::VtVec3fArray& points) {
              int idxP = 0;
              int splKnotCount = spline->KnotCount();
              int isClosed = spline->Closed();
              if (splKnotCount == 0) {
                  return;
              }

              for (int j = 0; j < splKnotCount; ++j) {
                  SplineKnot kspl = spline->GetKnot(j);
                  Point3     inVec = kspl.InVec();
                  Point3     outVec = kspl.OutVec();
                  Point3     knot = kspl.Knot();

                  if (j == 0) { // first knot of the spline
                      points.emplace_back(knot.x, knot.y, knot.z);
                      points.emplace_back(outVec.x, outVec.y, outVec.z);
                      idxP += 2;
                  } else if (j == splKnotCount - 1) { // last knot of the spline
                      if (isClosed) {
                          // in case of a closed spline, in order for the curve shape to be
                          // preserved in the USD side, we need to add the first knot's inVec as the
                          // last actual point in order to have the correct tangent at the end of
                          // the curve
                          points.emplace_back(inVec.x, inVec.y, inVec.z);
                          points.emplace_back(knot.x, knot.y, knot.z);
                          points.emplace_back(outVec.x, outVec.y, outVec.z);
                          idxP += 3;

                          Point3 firstKnotInVec = spline->GetKnot(0).InVec();
                          points.emplace_back(firstKnotInVec.x, firstKnotInVec.y, firstKnotInVec.z);
                          idxP += 1;
                      } else {
                          points.emplace_back(inVec.x, inVec.y, inVec.z);
                          points.emplace_back(knot.x, knot.y, knot.z);
                          idxP += 2;
                      }
                  } else { // middle knot of the spline
                      points.emplace_back(inVec.x, inVec.y, inVec.z);
                      points.emplace_back(knot.x, knot.y, knot.z);
                      points.emplace_back(outVec.x, outVec.y, outVec.z);
                      idxP += 3;
                  }
              }
              vertexCounts.push_back(idxP);
          };

    pxr::UsdGeomBasisCurves usdCurve(targetPrim);

    // Setup some non-animatable attributes (only do it once, when we export the first frame).
    if (time.IsFirstFrame()) {
        // Use the wire color as USD display color.
        Color             wireColor(sourceNode->GetWireColor());
        pxr::VtVec3fArray usdDisplayColor = { pxr::GfVec3f(wireColor.r, wireColor.g, wireColor.b) };

        // default is to export "linear" type curves (i.e. cubic splines get interpolated as linear)
        usdCurve.CreateTypeAttr().Set(pxr::TfToken("linear"));
        usdCurve.CreateDisplayColorAttr().Set(usdDisplayColor);

        // In the case we have a data inconsistency when creating BasisCurves, preprocess the data
        // into the logical categories that we will export into due to BasisCurves limitations.
        if (dataInconsistency) {
            TF_WARN(
                InconsistentSplineTypesAndOrWarps,
                "Inconsistent wraps and/or types cannot be represented under a single BasisCurves "
                "prim. Multiple BasisCurves prims will created under a parent Xform prim, in order "
                "to accurately represent all combinations of type+wrap present in the exported "
                "SplineShape.");

            SplineShape* splineShape = dynamic_cast<SplineShape*>(
                sourceNode->EvalWorldState(timeConfig.GetStartTime()).obj);

            if (splineShape) {
                BezierShape* bezierShape = &splineShape->shape;
                int          numSplines = bezierShape->splineCount;

                if (numSplines > 0) {
                    // preprocess the splines into the categories that we will export into
                    for (int i = 0; i < numSplines; ++i) {
                        Spline3D* spl = bezierShape->splines[i];
                        bool      linearStatus = isSplineLinear(spl);
                        bool      closedStatus = spl->Closed() > 0;
                        if (linearStatus && closedStatus) {
                            closedLinearShapes.push_back(spl);
                        } else if (linearStatus && !closedStatus) {
                            openLinearShapes.push_back(spl);
                        } else if (!linearStatus && closedStatus) {
                            closedCubicShapes.push_back(spl);
                        } else if (!linearStatus && !closedStatus) {
                            openCubicShapes.push_back(spl);
                        }
                    }

                    // int to track the number of new prims created for the categories, used for
                    // naming
                    int numNewPrim = 1;

                    if (!openLinearShapes.empty()) {
                        // use the targetPrim as the container for this category
                        usdCurve.CreateTypeAttr().Set(pxr::TfToken("linear"));
                        usdCurve.CreateWrapAttr().Set(pxr::TfToken("nonperiodic"));
                        openLinearPrim = usdCurve;
                        numNewPrim++;
                    }
                    if (!closedLinearShapes.empty()) {
                        // use the targetPrim as the container for this category if this is the
                        // first category processed
                        if (numNewPrim == 1) {
                            usdCurve.CreateTypeAttr().Set(pxr::TfToken("linear"));
                            usdCurve.CreateWrapAttr().Set(pxr::TfToken("periodic"));
                            closedLinearPrim = usdCurve;
                        } else {
                            closedLinearPrim
                                = pxr::UsdGeomBasisCurves(targetPrim.GetStage()->DefinePrim(
                                    SdfPath(
                                        primPath.GetString() + "_" + std::to_string(numNewPrim)),
                                    pxr::TfToken("BasisCurves")));
                            closedLinearPrim.CreateTypeAttr().Set(pxr::TfToken("linear"));
                            closedLinearPrim.CreateWrapAttr().Set(pxr::TfToken("periodic"));
                            closedLinearPrim.CreateDisplayColorAttr().Set(usdDisplayColor);
                        }
                        numNewPrim++;
                    }
                    if (!openCubicShapes.empty()) {
                        // use the targetPrim as the container for this category if this is the
                        // first category processed
                        if (numNewPrim == 1) {
                            usdCurve.CreateTypeAttr().Set(pxr::TfToken("cubic"));
                            usdCurve.CreateWrapAttr().Set(pxr::TfToken("nonperiodic"));
                            openCubicPrim = usdCurve;
                        } else {
                            openCubicPrim
                                = pxr::UsdGeomBasisCurves(targetPrim.GetStage()->DefinePrim(
                                    SdfPath(
                                        primPath.GetString() + "_" + std::to_string(numNewPrim)),
                                    pxr::TfToken("BasisCurves")));
                            openCubicPrim.CreateTypeAttr().Set(pxr::TfToken("cubic"));
                            openCubicPrim.CreateWrapAttr().Set(pxr::TfToken("nonperiodic"));
                            openCubicPrim.CreateDisplayColorAttr().Set(usdDisplayColor);
                        }
                        numNewPrim++;
                    }
                    if (!closedCubicShapes.empty()) {
                        closedCubicPrim = pxr::UsdGeomBasisCurves(targetPrim.GetStage()->DefinePrim(
                            SdfPath(primPath.GetString() + "_" + std::to_string(numNewPrim)),
                            pxr::TfToken("BasisCurves")));
                        closedCubicPrim.CreateTypeAttr().Set(pxr::TfToken("cubic"));
                        closedCubicPrim.CreateWrapAttr().Set(pxr::TfToken("periodic"));
                        closedCubicPrim.CreateDisplayColorAttr().Set(usdDisplayColor);
                    }
                }
            }
        }
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

    SplineShape* splineShape = dynamic_cast<SplineShape*>(shapeObject);
    if (splineShape) {
        BezierShape* bezierShape = &splineShape->shape;
        int          numSplines = bezierShape->splineCount;

        if (numSplines > 0) {
            // special case of inconsistent basis and/or wrap data on the SplineShape
            if (dataInconsistency) {
                auto processShapes = [](const auto&      shapes,
                                        auto&            prim,
                                        auto             processShapeFunc,
                                        pxr::UsdTimeCode usdTimeCode) {
                    if (shapes.empty())
                        return;

                    pxr::VtIntArray   vertexCounts;
                    pxr::VtVec3fArray points;
                    const auto        vertexCountsAttr = prim.CreateCurveVertexCountsAttr();
                    const auto        pointsAttr = prim.CreatePointsAttr();

                    for (const auto& shape : shapes) {
                        processShapeFunc(shape, vertexCounts, points);
                    }

                    vertexCountsAttr.Set(vertexCounts, usdTimeCode);
                    pointsAttr.Set(points, usdTimeCode);
                };

                processShapes(openLinearShapes, openLinearPrim, processLinearShape, usdTimeCode);
                processShapes(
                    closedLinearShapes, closedLinearPrim, processLinearShape, usdTimeCode);
                processShapes(openCubicShapes, openCubicPrim, processCubicShape, usdTimeCode);
                processShapes(closedCubicShapes, closedCubicPrim, processCubicShape, usdTimeCode);
            } else {
                pxr::VtIntArray   vertexCounts;
                pxr::VtVec3fArray points;
                for (int i = 0; i < numSplines; ++i) {
                    int       idxP = 0;
                    Spline3D* spl = bezierShape->splines[i];
                    int       isClosed = spl->Closed();

                    if (isSplineLinear(spl)) { // linear logic
                        processLinearShape(spl, vertexCounts, points);

                        if (isClosed) {
                            usdCurve.CreateWrapAttr().Set(pxr::TfToken("periodic"));
                        }
                    } else { // cubic logic
                        if (time.IsFirstFrame()) {
                            usdCurve.CreateTypeAttr().Set(pxr::TfToken("cubic"));
                        }

                        processCubicShape(spl, vertexCounts, points);
                        if (isClosed) {
                            usdCurve.CreateWrapAttr().Set(pxr::TfToken("periodic"));
                        }
                    }
                }
                curvesVertexCountsAttr.Set(vertexCounts, usdTimeCode);
                curvesPointsAttr.Set(points, usdTimeCode);
            }
        }
    } else {
        // getPolyShape() returns a pair :
        // first -> the polyShape to export.
        // second -> whether or not that polyShape should be deleted.
        const auto polyShapePair = getPolyShape(
            shapeObject, sourceNode->GetName(), timeVal, displayTimeDependantWarnings);

        PolyShape* polyShape = polyShapePair.first;
        const bool destroyPolyShape = polyShapePair.second;

        const bool wsmRequiresTransform
            = MaxUsd::WsmRequiresTransformToLocalSpace(sourceNode, timeVal);
        // If a WSM is applied, move the geometry's points back into local space. So that with the
        // transforms inherited from its hierarchy, the object will end up in the correct location
        // on the USD side.
        if (wsmRequiresTransform) {
            auto nodeTmInvert = sourceNode->GetNodeTM(timeVal);
            nodeTmInvert.Invert();
            polyShape->Transform(nodeTmInvert);
        }

        pxr::VtIntArray   vertexCounts;
        pxr::VtVec3fArray points;

        for (int i = 0; i < polyShape->numLines; ++i) {
            // Not getting a const reference because in Max 2021, ::IsClosed() is not const
            // correct...
            auto& line = polyShape->lines[i];
            // If the curve is closed, repeat the first point at the end.
            const auto closed = line.IsClosed();
            int        numPoints = line.numPts;

            vertexCounts.push_back(numPoints);
            points.reserve(line.numPts);
            for (int j = 0; j < line.numPts; ++j) {
                const auto point = line.pts[j];
                points.emplace_back(point.p.x, point.p.y, point.p.z);
            }

            if (closed) {
                usdCurve.CreateWrapAttr().Set(pxr::TfToken("periodic"));
            }
        }

        curvesVertexCountsAttr.Set(vertexCounts, usdTimeCode);
        curvesPointsAttr.Set(points, usdTimeCode);

        if (destroyPolyShape) {
            delete polyShape;
        }
    }

    return true;
}

MaxUsd::XformSplitRequirement MaxUsdShapeWriter::RequiresXformPrim()
{
    INode*     sourceNode = GetNode();
    const auto timeConfig = GetExportArgs().GetResolvedTimeConfig();

    // In the case of SplineShape object, we need to check if the data is consistent with what
    // what the BasisCurves schema can handle. If not, we need to split the xform and create
    // multiple prims to handle the data correctly. The categories are: linear/open, linear/closed,
    // cubic/open, and cubic/closed.
    SplineShape* splineShape
        = dynamic_cast<SplineShape*>(sourceNode->EvalWorldState(timeConfig.GetStartTime()).obj);

    if (splineShape) {
        BezierShape* bezierShape = &splineShape->shape;
        int          numSplines = bezierShape->splineCount;

        if (numSplines > 0) {
            bool firstSplineClosedStatus = bezierShape->splines[0]->Closed() > 0;
            bool firstSplineLinearStatus = isSplineLinear(bezierShape->splines[0]);

            // check for data inconsistency (all splines must have the same closed and linear
            // status)
            for (int i = 0; i < numSplines; ++i) {
                Spline3D* spl = bezierShape->splines[i];

                if ((spl->Closed() > 0) != firstSplineClosedStatus
                    || isSplineLinear(spl) != firstSplineLinearStatus) {
                    dataInconsistency = true;
                    break;
                }
            }
        }
    }

    if (dataInconsistency
        || (!GetExportArgs().GetAllowNestedGprims() && sourceNode->NumberOfChildren() > 0)) {
        return MaxUsd::XformSplitRequirement::Always;
    }
    return MaxUsd::XformSplitRequirement::ForOffsetObjects;
}

bool MaxUsdShapeWriter::isSplineLinear(Spline3D* spline)
{
    for (int j = 0; j < spline->Segments(); ++j) {
#if MAX_VERSION_MAJOR >= 27
        if (!IsSplineSegmentEffectivelyLinear(spline, j, 0.0)) {
#else
        if (!IsSplineSegmentLinear(spline, j)) {
#endif
            return false;
        }
    }
    return true;
}

bool MaxUsdShapeWriter::IsSplineSegmentLinear(Spline3D* spline, int segment)
{
    if (spline == nullptr || segment < 0 || segment >= spline->Segments())
        return false;

    SplineKnot k1 = spline->GetKnot(segment);
    if (k1.Ltype() == LTYPE_LINE)
        return true;

    int        nextKnot = (segment + 1) % spline->KnotCount();
    SplineKnot k2 = spline->GetKnot(nextKnot);

    int k1Type = k1.Ktype(), k2Type = k2.Ktype();
    if (k1Type == KTYPE_CORNER && k2Type == KTYPE_CORNER)
        return true;
    if (k1Type == KTYPE_AUTO || k2Type == KTYPE_AUTO)
        return false;

    auto isLinearKnot = [](const SplineKnot& k, const SplineKnot& other, bool isCorner) {
        if (isCorner)
            return true;
        if (k.Ktype() & KTYPE_BEZIER) {
            Point3 normalizedOut = Normalize(k.OutVec() - k.Knot());
            Point3 normalizedDir = Normalize(other.Knot() - k.Knot());
            return k.OutVec() == k.Knot() || Length(normalizedOut - normalizedDir) < 0.0f;
        }
        return false;
    };

    return isLinearKnot(k1, k2, k1Type == KTYPE_CORNER)
        && isLinearKnot(k2, k1, k2Type == KTYPE_CORNER);
}
PXR_NAMESPACE_CLOSE_SCOPE