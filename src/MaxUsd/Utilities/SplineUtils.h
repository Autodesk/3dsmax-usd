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
#include <pxr/base/ts/tangentConversions.h>

#include <cmath>
#include <control.h>
#include <cs/BIPEXP.H>
#include <plugapi.h>

namespace MAXUSD_NS_DEF {

/// Enum to specify which component of scale to extract
enum class ScaleComponent
{
    X,
    Y,
    Z
};

/// Check if a controller is valid to convert to USD splines.
static bool IsValidController(Control* transformController)
{
    if (!transformController) {
        return false;
    }

    // By default, when a controller is not animated or created, it will be null.
    // Thus assume they are valid by default.
    bool isValidPosController = true;
    bool isValidRotController = true;
    bool isValidScaleController = true;

    if (Control* posCtrl = transformController->GetPositionController()) {
        isValidPosController = posCtrl->SuperClassID() == CTRL_POINT3_CLASS_ID
            || posCtrl->SuperClassID() == CTRL_POSITION_CLASS_ID;
    }

    if (Control* rotCtrl = transformController->GetRotationController()) {
        isValidRotController = rotCtrl->SuperClassID() == CTRL_POINT3_CLASS_ID
            || rotCtrl->SuperClassID() == CTRL_POSITION_CLASS_ID
            || rotCtrl->SuperClassID() == CTRL_POINT4_CLASS_ID
            || rotCtrl->SuperClassID() == CTRL_ROTATION_CLASS_ID;
    }

    if (Control* scaleCtrl = transformController->GetScaleController()) {
        isValidScaleController = scaleCtrl->SuperClassID() == CTRL_SCALE_CLASS_ID;
    }

    return isValidPosController && isValidRotController && isValidScaleController;
}

namespace detail {

/// Determine interpolation type based on key index and outgoing tangent flags.
static PXR_NS::TsInterpMode DetermineInterpolation(int keyIndex, int numKeys, DWORD outKflags)
{
    if (keyIndex >= numKeys - 1) {
        // Last key - held
        return PXR_NS::TsInterpHeld;
    } else if (outKflags == BEZKEY_STEP) {
        return PXR_NS::TsInterpHeld;
    } else {
        // Bezier (default), flat, auto, or other curve types
        return PXR_NS::TsInterpCurve;
    }
}

/// Process a single tangent (in or out) for a knot.
/// Converts Max angle/length tangent to USD width/slope format and applies it to the knot.
/// Returns the kflags for use in determining interpolation.
template <typename KNOT_TYPE>
static DWORD ProcessTangent(
    IAdjustTangents* tangents,
    TimeValue        keyTime,
    TimeValue        adjacentKeyTime,
    int              componentIndex,
    bool             isInTangent,
    PXR_NS::TsKnot&  knot)
{
    float angle = 0.0f;
    float length = 0.0f;
    DWORD kflags = 0;

    if (!tangents->GetTangents(
            keyTime,
            componentIndex,
            isInTangent ? HITKEY_INTAN : HITKEY_OUTTAN,
            nullptr,
            &angle,
            &length,
            nullptr,
            &kflags)) {
        return 0;
    }

    const float timeDist = isInTangent ? (float)(keyTime - adjacentKeyTime) / GetTicksPerFrame()
                                       : (float)(adjacentKeyTime - keyTime) / GetTicksPerFrame();
    const float tangentDistance = timeDist * length;
    const float tanHeight = tangentDistance * tan(angle) * GetTicksPerFrame();

    PXR_NS::TsTime outWidth {};
    KNOT_TYPE      outSlope {};
    PXR_NS::TsConvertToStandardTangent(
        PXR_NS::TsTime(tangentDistance),
        KNOT_TYPE(tanHeight),
        true,  // convertHeightToSlope - input is height, convert to slope
        false, // divideValuesByThree - Bezier tangent convention
        isInTangent,
        &outWidth,
        &outSlope);

    if (std::isnan(outSlope)) {
        outSlope = KNOT_TYPE(0);
    }

    if (isInTangent) {
        knot.SetPreTanWidth(outWidth);
        knot.SetPreTanSlope(outSlope);
    } else {
        knot.SetPostTanWidth(outWidth);
        knot.SetPostTanSlope(outSlope);
    }

    return kflags;
}

/// Apply tangent data to a knot from a controller's tangent interface.
template <typename KNOT_TYPE>
static void ApplyTangentsToKnot(
    IAdjustTangents* tangents,
    TimeValue        keyTime,
    TimeValue        prevKeyTime,
    TimeValue        nextKeyTime,
    int              componentIndex,
    int              keyIndex,
    int              numKeys,
    PXR_NS::TsKnot&  knot)
{
    ProcessTangent<KNOT_TYPE>(tangents, keyTime, prevKeyTime, componentIndex, true, knot);

    DWORD outKflags
        = ProcessTangent<KNOT_TYPE>(tangents, keyTime, nextKeyTime, componentIndex, false, knot);

    knot.SetNextInterpolation(DetermineInterpolation(keyIndex, numKeys, outKflags));
}

/// Extract a float value from ScaleValue based on component.
static float ExtractScaleComponent(const ScaleValue& scaleValue, ScaleComponent component)
{
    switch (component) {
    case ScaleComponent::X: return scaleValue.s.x;
    case ScaleComponent::Y: return scaleValue.s.y;
    case ScaleComponent::Z: return scaleValue.s.z;
    default: return 0.0f;
    }
}

/// Generic knot extraction that works with different value types.
/// ValueExtractor: (Control*, TimeValue, Interval&) -> float
template <typename KNOT_TYPE, typename ValueExtractor>
static PXR_NS::TsKnotMap GetKnotsFromControllerImpl(
    PXR_NS::UsdStageWeakPtr             stage,
    Control*                            controller,
    int                                 componentIndex,
    ValueExtractor                      valueExtractor,
    std::function<KNOT_TYPE(KNOT_TYPE)> transformerFunction)
{
    PXR_NS::TsKnotMap knots;

    if (!controller) {
        return knots;
    }

    auto      knotType = PXR_NS::TfType::Find<KNOT_TYPE>();
    const int numKeys = controller->NumKeys();
    for (int k = 0; k < numKeys; ++k) {
        Interval invalid;

        auto  keyTime = controller->GetKeyTime(k);
        auto  prevKeyTime = controller->GetKeyTime(k > 0 ? k - 1 : 0);
        auto  nextKeyTime = controller->GetKeyTime(k < numKeys - 1 ? k + 1 : k);
        float keyValue = valueExtractor(controller, keyTime, invalid);

        PXR_NS::TsKnot knot(knotType);
        knot.SetValue(transformerFunction(static_cast<KNOT_TYPE>(keyValue)));
        auto maxFrame = MaxUsd::GetFrameFromTimeValue(keyTime);
        knot.SetTime(MaxUsd::GetUsdTimeCodeFromMaxFrame(stage, maxFrame).GetValue());

        if (auto tangents = GetTangentInterface(controller)) {
            ApplyTangentsToKnot<KNOT_TYPE>(
                tangents, keyTime, prevKeyTime, nextKeyTime, componentIndex, k, numKeys, knot);
        }

        knots.insert(knot);
    }

    return knots;
}

} // namespace detail

template <typename KNOT_TYPE>
static PXR_NS::TsKnotMap GetKnotsFromController(
    PXR_NS::UsdStageWeakPtr             stage,
    Control*                            controller,
    std::function<KNOT_TYPE(KNOT_TYPE)> transformerFunction = [](KNOT_TYPE value) { return value; })
{
    auto floatExtractor = [](Control* ctrl, TimeValue keyTime, Interval& invalid) -> float {
        float value = 0.0f;
        ctrl->GetValue(keyTime, &value, invalid);
        return value;
    };
    return detail::GetKnotsFromControllerImpl<KNOT_TYPE>(
        stage, controller, 0, floatExtractor, transformerFunction);
}

template <typename KNOT_TYPE>
static PXR_NS::TsSpline CreateSplineFromControl(
    PXR_NS::UsdStageWeakPtr             stage,
    Control*                            controller,
    std::function<KNOT_TYPE(KNOT_TYPE)> transformerFunction = [](KNOT_TYPE value) { return value; })
{
    auto knotType = PXR_NS::TfType::Find<KNOT_TYPE>();
    if (!controller) {
        return PXR_NS::TsSpline(knotType);
    }
    auto knots = GetKnotsFromController<KNOT_TYPE>(stage, controller, transformerFunction);
    PXR_NS::TsSpline spline(knotType);
    spline.SetKnots(knots);
    return spline;
}

template <typename KNOT_TYPE>
static PXR_NS::TsSpline CombineSplines(
    const PXR_NS::TsSpline&                              spline1,
    const PXR_NS::TsSpline&                              spline2,
    std::function<KNOT_TYPE(KNOT_TYPE v1, KNOT_TYPE v2)> combineFunction)
{
    PXR_NS::TsSpline combinedSplines(PXR_NS::TfType::Find<KNOT_TYPE>());

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

    PXR_NS::TsSpline secondarySplines(PXR_NS::TfType::Find<KNOT_TYPE>());
    bool             lambdaOrder = true;
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

        // Apply combine function with arguments in correct order based on which spline was primary
        if (lambdaOrder) {
            knot.SetValue(combineFunction(knotValue, secKnotValue));
        } else {
            knot.SetValue(combineFunction(secKnotValue, knotValue));
        }
    }

    combinedSplines.SetKnots(combinedKnots);
    return combinedSplines;
}

namespace detail {

/// Generic implementation for writing a spline attribute.
/// DefaultValueGetter: (Control*) -> KNOT_TYPE - extracts the default value when no keyframes exist
template <typename KNOT_TYPE, typename DefaultValueGetter>
static bool WriteSplineAttributeImpl(
    const PXR_NS::TsSpline&             spline,
    Control*                            controller,
    const PXR_NS::UsdPrim&              prim,
    PXR_NS::UsdAttribute                attribute,
    DefaultValueGetter                  defaultValueGetter,
    std::function<KNOT_TYPE(KNOT_TYPE)> transformerFunction)
{
    if (!controller || !prim.IsValid() || !attribute.IsValid()) {
        return false;
    }

    if (!spline.GetKnots().empty()) {
        if (!attribute.SetSpline(spline)) {
            MaxUsd::Log::Warn(
                "Failed to set spline attribute '{0}' on prim '{1}'.",
                attribute.GetName().GetString(),
                prim.GetPath().GetString());
            return false;
        }
        return true;
    }

    // If the spline has no knots, we still need to set a default value.
    KNOT_TYPE value = defaultValueGetter(controller);
    return attribute.Set(transformerFunction(value));
}

} // namespace detail

template <typename KNOT_TYPE>
static bool WriteSplineAttribute(
    PXR_NS::UsdStageWeakPtr             stage,
    Control*                            controller,
    const PXR_NS::UsdPrim&              prim,
    PXR_NS::UsdAttribute                attribute,
    std::function<KNOT_TYPE(KNOT_TYPE)> transformerFunction = [](KNOT_TYPE value) { return value; })
{
    PXR_NS::TsSpline spline
        = CreateSplineFromControl<KNOT_TYPE>(stage, controller, transformerFunction);

    // Always read as float from Max controllers, then cast to target type.
    // Max float controllers expect float* pointers - passing double* would leave
    // upper bytes uninitialized since float is 4 bytes and double is 8 bytes.
    auto defaultValueGetter = [](Control* ctrl) -> KNOT_TYPE {
        float value = 0.0f;
        ctrl->GetValue(0, &value, FOREVER);
        return static_cast<KNOT_TYPE>(value);
    };

    return detail::WriteSplineAttributeImpl<KNOT_TYPE>(
        spline, controller, prim, attribute, defaultValueGetter, transformerFunction);
}

/// Get knots from a scale controller for a specific axis component.
/// Scale controllers in 3ds Max return ScaleValue structs, not simple floats.
template <typename KNOT_TYPE>
static PXR_NS::TsKnotMap GetKnotsFromScaleController(
    PXR_NS::UsdStageWeakPtr             stage,
    Control*                            controller,
    ScaleComponent                      component,
    std::function<KNOT_TYPE(KNOT_TYPE)> transformerFunction = [](KNOT_TYPE value) { return value; })
{
    auto scaleExtractor
        = [component](Control* ctrl, TimeValue keyTime, Interval& invalid) -> float {
        ScaleValue scaleValue;
        ctrl->GetValue(keyTime, &scaleValue, invalid);
        return detail::ExtractScaleComponent(scaleValue, component);
    };
    return detail::GetKnotsFromControllerImpl<KNOT_TYPE>(
        stage, controller, static_cast<int>(component), scaleExtractor, transformerFunction);
}

/// Create a spline from a scale controller for a specific axis component.
template <typename KNOT_TYPE>
static PXR_NS::TsSpline CreateSplineFromScaleControl(
    PXR_NS::UsdStageWeakPtr             stage,
    Control*                            controller,
    ScaleComponent                      component,
    std::function<KNOT_TYPE(KNOT_TYPE)> transformerFunction = [](KNOT_TYPE value) { return value; })
{
    auto knotType = PXR_NS::TfType::Find<KNOT_TYPE>();
    if (!controller) {
        return PXR_NS::TsSpline(knotType);
    }
    auto knots
        = GetKnotsFromScaleController<KNOT_TYPE>(stage, controller, component, transformerFunction);
    PXR_NS::TsSpline spline(knotType);
    spline.SetKnots(knots);
    return spline;
}

/// Write a spline attribute from a scale controller for a specific axis component.
template <typename KNOT_TYPE>
static bool WriteScaleSplineAttribute(
    PXR_NS::UsdStageWeakPtr             stage,
    Control*                            controller,
    ScaleComponent                      component,
    const PXR_NS::UsdPrim&              prim,
    PXR_NS::UsdAttribute                attribute,
    std::function<KNOT_TYPE(KNOT_TYPE)> transformerFunction = [](KNOT_TYPE value) { return value; })
{
    PXR_NS::TsSpline spline = CreateSplineFromScaleControl<KNOT_TYPE>(
        stage, controller, component, transformerFunction);

    auto defaultValueGetter = [component](Control* ctrl) -> KNOT_TYPE {
        ScaleValue scaleValue;
        ctrl->GetValue(0, &scaleValue, FOREVER);
        return static_cast<KNOT_TYPE>(detail::ExtractScaleComponent(scaleValue, component));
    };

    return detail::WriteSplineAttributeImpl<KNOT_TYPE>(
        spline, controller, prim, attribute, defaultValueGetter, transformerFunction);
}

} // namespace MAXUSD_NS_DEF
#endif