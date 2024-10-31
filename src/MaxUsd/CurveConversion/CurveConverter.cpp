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
#include "CurveConverter.h"

#include <pxr/base/tf/diagnostic.h>

PXR_NAMESPACE_USING_DIRECTIVE

enum BasisCurvesReaderCodes
{
    InsufficientCurveCount,
    CannotImportSingleKnotPeriodicCurve,
    InvalidCurveVertexCount,
    CurveVertexCountToPointsMismatch
};

TF_REGISTRY_FUNCTION(TfEnum)
{
    TF_ADD_ENUM_NAME(
        InsufficientCurveCount, "Insufficient curve vextex count for type of BasisCurve.");
    TF_ADD_ENUM_NAME(
        CannotImportSingleKnotPeriodicCurve,
        "Periodic curve with vertex count 3 cannot be imported.");
    TF_ADD_ENUM_NAME(
        InvalidCurveVertexCount, "Invalid curve vertex count value for type of BasisCurve.");
    TF_ADD_ENUM_NAME(
        CurveVertexCountToPointsMismatch,
        "Total curve vertex count is not equal to number of points defined for BasisCurve.");
};

namespace MAXUSD_NS_DEF {

size_t CurveConverter::ConvertToSplineShape(
    const pxr::UsdGeomBasisCurves& basisCurvesPrim,
    SplineShape&                   maxSpline,
    pxr::UsdTimeCode               timeCode)
{
    if (!basisCurvesPrim.GetPrim().IsValid())
        return 0;

    pxr::TfToken               curvesType, wrap;
    pxr::VtArray<pxr::GfVec3f> points;
    basisCurvesPrim.GetTypeAttr().Get(&curvesType, timeCode);
    basisCurvesPrim.GetWrapAttr().Get(&wrap, timeCode);
    basisCurvesPrim.GetPointsAttr().Get(&points, timeCode);
    pxr::VtArray<int> curveVertexCts;
    basisCurvesPrim.GetCurveVertexCountsAttr().Get(&curveVertexCts, timeCode);

    size_t vertCount = 0;
    for (const int& count : curveVertexCts) {
        vertCount += count;
    }

    // Check if vertCount is equal to the number of points
    if (vertCount != points.size()) {
        TF_ERROR(
            CurveVertexCountToPointsMismatch,
            "Total curveVertex count '%s' not equal to amount of points defined '%s'.",
            std::to_string(vertCount),
            std::to_string(points.size()));
        return 0;
    }

    size_t numberOfSplinesCreated = 0;
    if (curvesType == pxr::UsdGeomTokens->linear) {
        int pidx = 0;
        for (int cidx = 0; cidx < curveVertexCts.size(); cidx++) {
            auto count = curveVertexCts[cidx];
            if ((wrap == pxr::UsdGeomTokens->nonperiodic && count < 2)
                || (wrap == pxr::UsdGeomTokens->periodic && count < 3)) {
                TF_ERROR(
                    InsufficientCurveCount,
                    "Curve vertex count value is insufficient for linear BasisCurve. Skipping "
                    "import of curve "
                    "at "
                    "index '%s' with count '%s'.",
                    std::to_string(cidx),
                    std::to_string(count));
                continue;
            }

            Spline3D* spline = maxSpline.shape.NewSpline();
            for (int i = pidx; pidx < i + count; pidx++) {
                Point3 pt(points[pidx][0], points[pidx][1], points[pidx][2]);
                spline->AddKnot(SplineKnot(KTYPE_BEZIER, LTYPE_LINE, pt, pt, pt));
            }

            if (wrap == pxr::UsdGeomTokens->nonperiodic || wrap == pxr::UsdGeomTokens->pinned) {
                spline->SetClosed(0);
            } else {
                spline->SetClosed(1);
            }

            spline->ComputeBezPoints();
            numberOfSplinesCreated++;
        }
    } else if (curvesType == pxr::UsdGeomTokens->cubic) {
        int pidx = 0;
        for (int cidx = 0; cidx < curveVertexCts.size(); cidx++) {
            auto count = curveVertexCts[cidx];
            if (((wrap == pxr::UsdGeomTokens->nonperiodic || wrap == pxr::UsdGeomTokens->pinned)
                 && count < 4)
                || (wrap == pxr::UsdGeomTokens->periodic && count < 3)) {
                TF_ERROR(
                    InsufficientCurveCount,
                    "Curve vertex count value is insufficient for this type of cubic BasisCurve. "
                    "Skipping "
                    "import of "
                    "curve at "
                    "index '%s' with count '%s'.",
                    std::to_string(cidx),
                    std::to_string(count));
                continue;
            }

            if (wrap == pxr::UsdGeomTokens->periodic && count == 3) {
                TF_WARN(
                    CannotImportSingleKnotPeriodicCurve,
                    "Periodic cubic curve with vertex count 3 is valid but cannot be represented "
                    "in 3dsMax "
                    "with a single "
                    "knot as it is in USD. Skipping import of curve at index '%s' with count '%s'.",
                    std::to_string(cidx),
                    std::to_string(count));
                continue;
            }

            if (((wrap == pxr::UsdGeomTokens->nonperiodic || wrap == pxr::UsdGeomTokens->pinned)
                 && ((count - 4) % 3 != 0))
                || (wrap == pxr::UsdGeomTokens->periodic && ((count) % 3 != 0))) {
                TF_WARN(
                    InvalidCurveVertexCount,
                    "Curve vertex count value is invalid for this type of cubic BasisCurve - "
                    "Import may result "
                    "in unexpected results. Curve index '%s' with count "
                    "'%s.",
                    std::to_string(cidx),
                    std::to_string(count));
            }

            Spline3D* spline = maxSpline.shape.NewSpline();
            for (int i = pidx; pidx < i + count; pidx++) {
                if (pidx == i) // first point of a curve
                {
                    Point3 firstPt(points[pidx][0], points[pidx][1], points[pidx][2]);
                    Point3 secondPt(points[pidx + 1][0], points[pidx + 1][1], points[pidx + 1][2]);
                    spline->AddKnot(
                        SplineKnot(KTYPE_BEZIER, LTYPE_CURVE, firstPt, firstPt, secondPt));
                } else if (pidx == (i + count - 1)) // last point of a curve
                {
                    if ((pidx - i) % 3 == 0) // last point non-periodic
                    {
                        Point3 lastPt(points[pidx][0], points[pidx][1], points[pidx][2]);
                        Point3 beforeLastPt(
                            points[pidx - 1][0], points[pidx - 1][1], points[pidx - 1][2]);
                        spline->AddKnot(
                            SplineKnot(KTYPE_BEZIER, LTYPE_CURVE, lastPt, beforeLastPt, lastPt));
                    } else if (
                        (pidx - i) % 3 == 2
                        && wrap == pxr::UsdGeomTokens->periodic) // last point periodic
                    {
                        // in the periodic case, the last point is the first point. Change the
                        // in-vector to the last defined point
                        Point3 lastDefinedPoint(points[pidx][0], points[pidx][1], points[pidx][2]);
                        spline->SetInVec(0, lastDefinedPoint);
                        spline->SetClosed(1);
                    }
                } else if ((pidx - i) % 3 == 0) // control points
                {
                    Point3 currPt(points[pidx][0], points[pidx][1], points[pidx][2]);
                    Point3 prevPt(points[pidx - 1][0], points[pidx - 1][1], points[pidx - 1][2]);
                    Point3 nextPt(points[pidx + 1][0], points[pidx + 1][1], points[pidx + 1][2]);
                    spline->AddKnot(SplineKnot(KTYPE_BEZIER, LTYPE_CURVE, currPt, prevPt, nextPt));
                }
            }

            spline->ComputeBezPoints();
            numberOfSplinesCreated++;
        }
    }
    maxSpline.shape.UpdateSels();

    return numberOfSplinesCreated;
}

} // namespace MAXUSD_NS_DEF