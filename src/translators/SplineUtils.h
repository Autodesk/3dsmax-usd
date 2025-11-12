//
// Copyright 2025 Autodesk
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

#ifdef USD_CURVES_SUPPORTED
#include <MaxUsd/Utilities/TimeUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/base/ts/knot.h>
#include <pxr/base/ts/spline.h>

#include <control.h>

PXR_NAMESPACE_OPEN_SCOPE
namespace MaxSDKSupport {

template <typename KNOT_TYPE>
static TsKnotMap GetKnotsFromController(
    UsdStageWeakPtr                     stage,
    Control*                            controller,
    std::function<KNOT_TYPE(KNOT_TYPE)> transformerFunction = [](KNOT_TYPE value) { return value; })
{
    TsKnotMap knots;

    if (!controller) {
        return knots;
    }

    auto      knotType = TfType::Find<KNOT_TYPE>();
    const int numKeys = controller->NumKeys();
    for (int k = 0; k < numKeys; ++k) {
        Interval invalid;

        float keyValue = 0;
        auto  keyTime = controller->GetKeyTime(k);
        controller->GetValue(keyTime, &keyValue, invalid);

        TsKnot knot(knotType);
        knot.SetValue(transformerFunction(keyValue));
        auto maxFrame = MaxUsd::GetFrameFromTimeValue(keyTime);
        knot.SetTime(MaxUsd::GetUsdTimeCodeFromMaxFrame(stage, maxFrame).GetValue());

        if (auto tangents = GetTangentInterface(controller)) {
            if (k < numKeys - 1) {
                // not the last key
                knot.SetNextInterpolation(TsInterpCurve);
            } else {
                knot.SetNextInterpolation(TsInterpHeld);
            }

            float curInAngle = 0.0f;
            float curOutAngle = 0.0f;
            float curInLength = 0.0f;
            float curOutLength = 0.0f;
            DWORD flags = 0;
            DWORD kflags = 0;

            tangents->GetTangents(
                keyTime, 0, HITKEY_INTAN, nullptr, &curInAngle, &curInLength, &flags, &kflags);
            knot.SetPreTanWidth(TsTime(curOutLength));
            knot.SetPreTanSlope(KNOT_TYPE(curInAngle * DEG_TO_RAD));

            tangents->GetTangents(
                keyTime, 0, HITKEY_OUTTAN, nullptr, &curOutAngle, &curOutLength, &flags, &kflags);
            knot.SetPostTanWidth(TsTime(curOutLength));
            knot.SetPostTanSlope(KNOT_TYPE(curOutAngle * DEG_TO_RAD));
        }

        knots.insert(knot);
    }

    return knots;
}

template <typename KNOT_TYPE>
static TsSpline CreateSplineFromControl(
    UsdStageWeakPtr                     stage,
    Control*                            controller,
    std::function<KNOT_TYPE(KNOT_TYPE)> transformerFunction = [](KNOT_TYPE value) { return value; })
{
    auto     knotType = TfType::Find<KNOT_TYPE>();
    TsSpline spline(knotType);

    if (!controller) {
        return spline;
    }

    spline.SetKnots(GetKnotsFromController<KNOT_TYPE>(stage, controller, transformerFunction));
    return spline;
}

template <typename KNOT_TYPE>
static TsSpline CombineSplines(
    const TsSpline&                                      spline1,
    const TsSpline&                                      spline2,
    std::function<KNOT_TYPE(KNOT_TYPE v1, KNOT_TYPE v2)> combineFunction)
{
    TsSpline combinedSplines(TfType::Find<KNOT_TYPE>());

    if (spline1.GetValueType() != combinedSplines.GetValueType()
        || spline2.GetValueType() != combinedSplines.GetValueType()) {
        return combinedSplines;
    }

    auto knots1 = spline1.GetKnots();
    auto knots2 = spline2.GetKnots();
    if (knots1.empty() && knots2.empty()) {
        return combinedSplines;
    }

    if (spline1.GetKnots().empty()) {
        return spline2;
    }

    if (spline2.GetKnots().empty()) {
        return spline1;
    }

    TsSpline secondarySplines(TfType::Find<KNOT_TYPE>());
    bool     lambdaOrder = true;
    if (knots1.size() < knots2.size()) {
        combinedSplines.SetKnots(knots2);
        secondarySplines.SetKnots(knots1);
        lambdaOrder = false;
    } else {
        combinedSplines.SetKnots(knots1);
        secondarySplines.SetKnots(knots2);
    }

    auto combinedKnots = combinedSplines.GetKnots();
    // Combine the knots from both splines
    for (auto& knot : combinedKnots) {
        KNOT_TYPE knotValue = KNOT_TYPE();
        KNOT_TYPE secKnotValue = KNOT_TYPE();

        knot.GetValue<KNOT_TYPE>(&knotValue);
        secondarySplines.Eval(knot.GetTime(), &secKnotValue);

        if (lambdaOrder) {
            knot.SetValue(combineFunction(knotValue, secKnotValue));
        }
    }

    combinedSplines.SetKnots(combinedKnots);
    return combinedSplines;
}

template <typename KNOT_TYPE>
static bool WriteSplineAttribute(
    UsdStageWeakPtr                     stage,
    Control*                            controller,
    const UsdPrim&                      prim,
    UsdAttribute                        attribute,
    std::function<KNOT_TYPE(KNOT_TYPE)> transformerFunction = [](KNOT_TYPE value) { return value; })
{
    if (!controller || !prim.IsValid() || !attribute.IsValid()) {
        return false;
    }

    TsSpline spline = CreateSplineFromControl<KNOT_TYPE>(stage, controller, transformerFunction);
    if (!spline.GetKnots().empty()) {
        if (!attribute.SetSpline(spline)) {
            MaxUsd::Log::Warn(
                "Failed to set spline attribute '{0}' on prim '{1}'.",
                attribute.GetName().GetString(),
                prim.GetPath().GetString());
        }
        return false;
    }

    // If the spline has no knots, we still need to set a default value.
    KNOT_TYPE value {};
    controller->GetValue(0, &value, FOREVER);
    return attribute.Set(transformerFunction(value));
}

} // namespace MaxSDKSupport
PXR_NAMESPACE_CLOSE_SCOPE
#endif