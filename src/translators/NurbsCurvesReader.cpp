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
#include <MaxUsd/Translators/PrimReader.h>
#include <MaxUsd/Translators/PrimReaderRegistry.h>
#include <MaxUsd/Translators/TranslatorMorpher.h>
#include <MaxUsd/Translators/TranslatorPrim.h>
#include <MaxUsd/Translators/TranslatorSkel.h>
#include <MaxUsd/Translators/TranslatorUtils.h>
#include <MaxUsd/Translators/TranslatorXformable.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/usdGeom/nurbsCurves.h>

#include <surf_api.h>

PXR_NAMESPACE_OPEN_SCOPE

enum NURBSCurvesReaderCodes
{
    CurveVertexCountToPointsMismatch,
    CurveVertexCountsSizeToCurveOrderSizeMismatch,
    InsufficientKnotsDefined
};

TF_REGISTRY_FUNCTION(TfEnum)
{
    TF_ADD_ENUM_NAME(
        CurveVertexCountToPointsMismatch,
        "Total curve vertex count is not equal to number of points defined for BasisCurve.");
    TF_ADD_ENUM_NAME(
        CurveVertexCountsSizeToCurveOrderSizeMismatch,
        "Number of curves is not equal to number of curve orders.");
    TF_ADD_ENUM_NAME(
        InsufficientKnotsDefined,
        "Insufficient knots defined in the knots list. There must either 0 knots defined or "
        "exactly "
        "SumOfAllCurves(curveVertexCounts[i]+orders[i]) number of knots.");
};

class MaxUsdNurbsCurvesReader : public MaxUsdPrimReader
{
public:
    MaxUsdNurbsCurvesReader(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx)
        : MaxUsdPrimReader(prim, jobCtx)
    {
    }

    ~MaxUsdNurbsCurvesReader() = default;

    bool Read() override;
};

TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimReaderRegistry, UsdGeomNurbsCurves)
{
    MaxUsdPrimReaderRegistry::Register<UsdGeomNurbsCurves>(
        [](const UsdPrim& prim, MaxUsdReadJobContext& jobCtx) {
            return std::make_shared<MaxUsdNurbsCurvesReader>(prim, jobCtx);
        });
}

bool MaxUsdNurbsCurvesReader::Read()
{
    const auto               prim = GetUsdPrim();
    const UsdGeomNurbsCurves nurbsCurvesPrim(prim);

    const auto timeConfig = GetArgs().GetResolvedTimeConfig(prim.GetStage());
    const auto startTimeCode = timeConfig.GetStartTimeCode();

    VtArray<int>    curveOrders;
    VtArray<double> curveKnots;
    nurbsCurvesPrim.GetOrderAttr().Get(&curveOrders, startTimeCode);
    nurbsCurvesPrim.GetKnotsAttr().Get(&curveKnots, startTimeCode);
    VtArray<int> curveVertexCts;
    nurbsCurvesPrim.GetCurveVertexCountsAttr().Get(&curveVertexCts, startTimeCode);
    VtArray<GfVec3f> points;
    nurbsCurvesPrim.GetPointsAttr().Get(&points, startTimeCode);

    size_t vertCount = 0;
    // Validate the curve count sizes and accumulate vertCount
    for (const int& count : curveVertexCts) {
        vertCount += count;
    }

    // Check if there are a matching number of curves and orders defined (i.e need an order for each
    // curve)
    if (curveVertexCts.size() != curveOrders.size()) {
        TF_ERROR(
            CurveVertexCountsSizeToCurveOrderSizeMismatch,
            "Number of elements in curveVertexCounts '%s' not equal to amount of elements in "
            "orders '%s'.",
            std::to_string(curveVertexCts.size()),
            std::to_string(curveOrders.size()));
        return false;
    }

    size_t   numKnotsRequired = 0;
    unsigned cidx = 0;
    for (const int& count : curveVertexCts) {
        numKnotsRequired += count + curveOrders[cidx];
        cidx++;
    }

    // If a knots attribute is defined, check to make sure that it has the correct number of knots
    // defined based on the curve sizes and the curve order
    if (curveKnots.size() > 0 && curveKnots.size() != numKnotsRequired) {
        TF_ERROR(
            InsufficientKnotsDefined,
            "There are '%s' knots defined but there should be '%s' knots defined (i.e for each "
            "curve defined, there needs to be 'numPoints + order' knots).",
            std::to_string(curveKnots.size()),
            std::to_string(numKnotsRequired));
        return false;
    }

    // Check if vertCount is equal to the number of points
    if (vertCount != points.size()) {
        TF_ERROR(
            CurveVertexCountToPointsMismatch,
            "Total curveVertex count '%s' not equal to amount of points defined '%s'.",
            std::to_string(vertCount),
            std::to_string(points.size()));
        return false;
    }

    NURBSSet nurbsSet;
    int      cvidx = 0;
    int      ckidx = 0;
    for (int idx = 0; idx < curveVertexCts.size(); idx++) {
        NURBSCVCurve* nurbsCurve = new NURBSCVCurve();
        nurbsCurve->SetNumCVs(curveVertexCts[idx]);
        if (idx < curveOrders.size()) {
            nurbsCurve->SetOrder(curveOrders[idx]);
        }

        int numKnots = (curveVertexCts[idx] + curveOrders[idx]);
        nurbsCurve->SetNumKnots(numKnots);
        if (curveKnots.size() >= ckidx + numKnots) {
            for (int i = ckidx; ckidx < i + numKnots; ckidx++) {
                nurbsCurve->SetKnot(ckidx - i, curveKnots[ckidx]);
            }
        } else {
            // Generate uniform knot vector if none is provided.
            // The vector will be normalized to the range [0, 1]. The first and last knots will be
            // repeated based on the curve order. Example: for a curve with 8 CVs and order 4, the
            // knot vector will be [0, 0, 0, 0.1428, 0.2857, 0.4285, 0.5714, 0.7142, 0.8571, 1, 1,
            // 1]
            double repeatedVal;
            int    beginUniform = 0;
            int    endUniform = numKnots;

            int numRepeated = nurbsCurve->GetOrder() - 2;
            if (numRepeated < 0) {
                MaxUsd::Log::Warn(
                    "The curve order is smaller than 2. Setting number of repeated knots to 0.");
                numRepeated = 0;
            }
            beginUniform += numRepeated;
            endUniform -= numRepeated;

            // Generate uniform portion of range
            int   numUniform = endUniform - beginUniform;
            float invRange = 1.0f;
            if (numUniform > 1) {
                invRange = 1.0f / ((float)(numUniform - 1));
            }

            for (int knotIndex = beginUniform; knotIndex < endUniform; ++knotIndex) {
                float knotVal = invRange * ((float)(knotIndex - beginUniform));
                nurbsCurve->SetKnot(knotIndex, knotVal);
            }

            // Fill out repeated knots
            repeatedVal = nurbsCurve->GetKnot(beginUniform);
            for (int knotIndex = 0; knotIndex < beginUniform; ++knotIndex) {
                nurbsCurve->SetKnot(knotIndex, repeatedVal);
            }

            repeatedVal = nurbsCurve->GetKnot(endUniform - 1);
            for (int knotIndex = endUniform; knotIndex < numKnots; ++knotIndex) {
                nurbsCurve->SetKnot(knotIndex, repeatedVal);
            }
        }

        NURBSControlVertex cv;
        for (int i = cvidx; cvidx < i + curveVertexCts[idx]; cvidx++) {
            cv.SetPosition(cvidx, Point3(points[cvidx][0], points[cvidx][1], points[cvidx][2]));
            nurbsCurve->SetCV(cvidx - i, cv);
        }

        // Add the NURBSCVCurve object to the set
        nurbsSet.AppendObject(nurbsCurve);
    }

    // Create the NURBS object from the NURBSSet
    Matrix3 mat;
    Object* obj = CreateNURBSObject((IObjParam*)GetCOREInterface(), &nurbsSet, mat);

    auto createdNode
        = MaxUsdTranslatorPrim::CreateAndRegisterNode(prim, obj, prim.GetName(), GetJobContext());

    // position the node
    MaxUsdTranslatorXformable::Read(prim, createdNode, GetJobContext());

    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE