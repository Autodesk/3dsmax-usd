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
#include "TranslatorXformable.h"

#include "TranslatorPrim.h"

#include <MaxUsd/Utilities/MathUtils.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>

#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xformCache.h>

#include <inode.h>

PXR_NAMESPACE_OPEN_SCOPE

void MaxUsdTranslatorXformable::Read(
    const UsdPrim&        prim,
    INode*                maxNode,
    MaxUsdReadJobContext& context,
    const Matrix3&        correction)
{
    // == Read attrs ==
    // Read parent class attrs
    MaxUsdTranslatorPrim::Read(prim, maxNode, context);

    if (!prim || !prim.IsA<UsdGeomXformable>()) {
        return;
    }

    // We will do many computations of prim transforms below, a UsdGeomXformCache will allow us
    // to avoid redoing the same computations multiple times for prims that share part of their
    // transforms.
    UsdGeomXformCache xformComputeCache;
    const auto        timeConfig = context.GetArgs().GetResolvedTimeConfig(prim.GetStage());
    const auto        startTimeCode = timeConfig.GetStartTimeCode();
    xformComputeCache.SetTime(startTimeCode);
    // transform
    pxr::UsdGeomXformable xformPrim(prim);
    pxr::GfMatrix4d       usdTransformMatrix = xformComputeCache.GetLocalToWorldTransform(prim);
    if (MaxUsd::IsStageUsingYUpAxis(prim.GetStage())) {
        MaxUsd::MathUtils::ModifyTransformYToZUp(usdTransformMatrix);
    }
    Matrix3 maxTransformMatrix;
    MaxSDK::Graphics::Matrix44ToMaxWorldMatrix(
        maxTransformMatrix, MaxUsd::ToMax(usdTransformMatrix));

    // apply correction
    maxTransformMatrix = correction * maxTransformMatrix;

    Matrix3 pivotMatrix = Matrix3::Identity;

    // Find the pivot if any.
    // We only consider simple pivots, more specialized pivots like in Maya are not imported as
    // such (scalePivot, rotatePivot, etc.).
    // Note that because translation matrices are commutative, we don't care if there are other
    // transform ops before / after the pivot translation.
    bool                             pivotFound = false;
    bool                             pivotInverseFound = false;
    bool                             resetsXformStack = false;
    std::vector<pxr::UsdGeomXformOp> xformops = xformPrim.GetOrderedXformOps(&resetsXformStack);
    bool                             pivotIsIdentity = false;
    const auto                       pivotToken = pxr::TfToken("xformOp:translate:pivot");
    const auto pivotInverseToken = pxr::TfToken("!invert!xformOp:translate:pivot");
    for (size_t i = 0; i < xformops.size(); i++) {
        const pxr::UsdGeomXformOp& xformop = xformops[i];
        if (xformop.GetOpName() == pivotToken) {
            pivotFound = true;
            auto pivotXform = xformop.GetOpTransform(startTimeCode);
            MaxSDK::Graphics::Matrix44ToMaxWorldMatrix(pivotMatrix, MaxUsd::ToMax(pivotXform));
            pivotIsIdentity = MaxUsd::MathUtils::IsIdentity(pivotMatrix);

            std::vector<double> usdSamples;
            xformop.GetTimeSamples(&usdSamples);
            if (timeConfig.IsAnimated() && usdSamples.size() > 1) {
                MaxUsd::Log::Warn(
                    "Prim '{0}' has an animated pivot transform, this operation is not "
                    "supported in 3ds Max",
                    prim.GetName().GetString());
            }

            if (pivotInverseFound || pivotIsIdentity) {
                break;
            }
        }
        // Make sure the pivot and its inverse are present.
        // Otherwise we cant use the pivot as an object offset, as that transform
        // should be inherited by any children the node may have.
        else if (xformop.GetOpName() == pivotInverseToken) {
            pivotInverseFound = true;
            if (pivotFound) {
                break;
            }
        }
    }

    // If a pivot (and its inverse) exists, make use it as object offset.
    // WARNING: This behavior will need to be disabled if/when we import animations. Indeed, object
    // offset transforms cannot be animated in 3dsMax, unlike in USD.
    const bool hasPivotOp = pivotFound && pivotInverseFound && !pivotIsIdentity;
    if (hasPivotOp) {
        // Remove it from the node's matrix.
        maxTransformMatrix = pivotMatrix * maxTransformMatrix;
        Point3 translation;
        Quat   rotation;
        Point3 scale;
        DecomposeMatrix(Inverse(pivotMatrix), translation, rotation, scale);
        maxNode->SetObjOffsetPos(translation);
    }

    auto         stage = context.GetStage();
    const double endTimeCode = timeConfig.GetEndTimeCode();
    // TODO: add an option to the UI to be able to change the sampling rate
    constexpr double samplingRate = 1.0;

    std::vector<double> usdTimeCodes;
    usdTimeCodes.reserve(static_cast<size_t>(
        std::ceil((endTimeCode - startTimeCode) * stage->GetTimeCodesPerSecond())));
    for (double timeSample = startTimeCode; timeSample <= endTimeCode;) {
        usdTimeCodes.emplace_back(timeSample);
        timeSample += 1.0 / samplingRate;
    }

    // There's a bug in 3ds Max where the animation key is not created if the first time being
    // animated is 0. To go around that, have the time 0 being set later.
    if (startTimeCode == 0.0) {
        std::swap(*usdTimeCodes.begin(), *(usdTimeCodes.end() - 1));
    }

    {
        // only set keyframes if there is animation
        if (usdTimeCodes.size() > 1 && !xformPrim.TransformMightBeTimeVarying()) {
            usdTimeCodes.erase(usdTimeCodes.begin() + 1, usdTimeCodes.end());
        }
        if (usdTimeCodes.size() > 1) {
            AnimateOn();
        }
        for (const auto& currentTimeCode : usdTimeCodes) {
            xformComputeCache.SetTime(currentTimeCode);
            // Compute the node's transform.
            pxr::GfMatrix4d usdMatrix = xformComputeCache.GetLocalToWorldTransform(prim);
            if (MaxUsd::IsStageUsingYUpAxis(prim.GetStage())) {
                MaxUsd::MathUtils::ModifyTransformYToZUp(usdMatrix);
            }

            Matrix3 maxMatrix;
            MaxSDK::Graphics::Matrix44ToMaxWorldMatrix(maxMatrix, MaxUsd::ToMax(usdMatrix));

            // apply correction
            maxMatrix = correction * maxMatrix;

            if (hasPivotOp) {
                maxMatrix = pivotMatrix * maxMatrix;
            }

            maxNode->SetNodeTM(
                MaxUsd::GetMaxTimeValueFromUsdTimeCode(stage, currentTimeCode), maxMatrix);
        }
        // only set keyframes if there is animation
        if (usdTimeCodes.size() > 1) {
            AnimateOff();
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE