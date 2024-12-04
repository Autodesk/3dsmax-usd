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
#include "TranslatorUtils.h"

#include "TranslatorPrim.h"
#include "TranslatorXformable.h"

#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <istdplug.h>

PXR_NAMESPACE_OPEN_SCOPE

bool MaxUsdTranslatorUtil::CreateDummyHelperNode(
    const UsdPrim&        usdPrim,
    const TfToken&        name,
    MaxUsdReadJobContext& jobCtx)
{
    HelperObject* pointHelperObj = static_cast<HelperObject*>(
        GetCOREInterface()->CreateInstance(HELPER_CLASS_ID, Class_ID(POINTHELP_CLASS_ID, 0)));
    if (!pointHelperObj) {
        MaxUsd::Log::Warn(
            "Unable to create point helper object for '{0}'. Skipping node creation.",
            usdPrim.GetName().GetString());
        return false;
    }

    IParamBlock2* pb2 = pointHelperObj->GetParamBlockByID(0);
    if (!pb2) {
        MaxUsd::Log::Warn(
            "Malformed point helper object for '{0}'. Skipping node creation.",
            usdPrim.GetName().GetString());
        return false;
    }
    pb2->SetValue(pointobj_cross, 0, FALSE);

    INode* createdNode
        = MaxUsdTranslatorPrim::CreateAndRegisterNode(usdPrim, pointHelperObj, name, jobCtx);
    MaxUsdTranslatorXformable::Read(usdPrim, createdNode, jobCtx);

    return true;
}

// Get the usd attribute values for given time samples.
template <class T>
static bool GetValuesForTimeSamples(
    const UsdAttribute&        usdAttr,
    const std::vector<double>& timeSamples,
    std::vector<T>&            values)
{
    size_t numTimeSamples = timeSamples.size();
    values.resize(numTimeSamples);

    for (size_t i = 0; i < numTimeSamples; ++i) {
        const double timeSample = timeSamples[i];
        T            attrValue;
        if (!usdAttr.Get(&attrValue, timeSample)) {
            return false;
        }
        values[i] = attrValue;
    }
    return true;
}

bool ReadAnimatedUsdAttribute(
    const UsdAttribute&                                  usdAttr,
    const MaxUsdTranslatorUtil::AttributeSetterFunction& func,
    const MaxUsdReadJobContext&                          context)
{
    const auto       timeConfig = context.GetArgs().GetResolvedTimeConfig(context.GetStage());
    const GfInterval timeInterval(timeConfig.GetStartTimeCode(), timeConfig.GetEndTimeCode());

    // If this attribute isn't varying in the time interval,
    // we can early out and just let it be imported as a single value
    if (timeInterval.IsEmpty() || !usdAttr.IsValid() || !usdAttr.ValueMightBeTimeVarying()) {
        return false;
    }

    bool singleTimeCode = timeInterval.GetMin() == timeInterval.GetMax();

    // Get the list of time samples for the given time interval
    // get timeSamples ONLY if we deal with an import frame range
    std::vector<double> timeSamples;
    if (!singleTimeCode) {
        if (!usdAttr.GetTimeSamplesInInterval(timeInterval, &timeSamples)) {
            return false;
        }
    }

    // Edge cases are possible if the import is done inside or outside time samples
    bool   firstTimeSampleIsAlsoDefault { false };
    double defaultTimeSample {
        0.0
    }; // this value will be set only if firstTimeSampleIsAlsoDefault is true
    {
        double lower, upper;
        bool   hasTimeSamples;
        usdAttr.GetBracketingTimeSamples(timeInterval.GetMin(), &lower, &upper, &hasTimeSamples);

        // if 'lower < upper', start importing within a sample of the USD animated range
        // if 'lower == upper', importing outside the animated range or directly on a sample
        if (lower <= upper) {
            // the value at this time sample becomes the default value
            firstTimeSampleIsAlsoDefault = true;
            defaultTimeSample = timeInterval.GetMin();
        }
        if (timeSamples.empty() || timeSamples[0] != timeInterval.GetMin()) {
            if (lower < upper) {
                // if the import start time is between time samples
                // the import start time must be keyed
                timeSamples.insert(timeSamples.begin(), timeInterval.GetMin());
            }
        }
    }
    if (!timeSamples.empty() && timeSamples[timeSamples.size() - 1] != timeInterval.GetMax()) {
        double lower, upper;
        bool   hasTimeSamples;
        usdAttr.GetBracketingTimeSamples(timeInterval.GetMax(), &lower, &upper, &hasTimeSamples);

        if (lower < upper) {
            // if the import end time is between time samples
            // the import end time must be keyed
            timeSamples.push_back(timeInterval.GetMax());
        }
    }

    // properly set the default value on the attribute if needed
    if (firstTimeSampleIsAlsoDefault) {
        std::vector<VtValue> defaultValue;
        if (!GetValuesForTimeSamples(usdAttr, { defaultTimeSample }, defaultValue)) {
            return false;
        }

        if (!func(
                defaultValue[0],
                defaultTimeSample,
                MaxUsd::GetMaxTimeValueFromUsdTimeCode(
                    context.GetStage(), UsdTimeCode::Default()))) {
            return false;
        }
    }

    // if no keys need to be set, just exit successfully
    // the default value
    if (timeSamples.empty()) {
        return true;
    }

    // There's a bug in 3ds Max where the animation key is not created if the first time being
    // animated is 0. To go around that, have the time 0 being set later.
    if (timeSamples[0] == 0.0) {
        std::swap(*timeSamples.begin(), *(timeSamples.end() - 1));
    }

    // Retrieve values for all time samples.
    std::vector<VtValue> values;
    if (!GetValuesForTimeSamples(usdAttr, timeSamples, values)) {
        return false;
    }

    AnimateOn();
    size_t numTimeSamples = timeSamples.size();
    for (size_t i = 0; i < numTimeSamples; ++i) {
        if (!func(
                values[i],
                timeSamples[i],
                MaxUsd::GetMaxTimeValueFromUsdTimeCode(context.GetStage(), timeSamples[i]))) {
            AnimateOff();
            return false;
        }
    }
    AnimateOff();

    return true;
}

bool MaxUsdTranslatorUtil::ReadUsdAttribute(
    const UsdAttribute&            usdAttr,
    const AttributeSetterFunction& func,
    const MaxUsdReadJobContext&    context,
    bool                           onlyWhenAuthored)
{
    if (!usdAttr.IsValid() || (onlyWhenAuthored && !usdAttr.IsAuthored())) {
        return false;
    }

    if (ReadAnimatedUsdAttribute(usdAttr, func, context)) {
        return true;
    }

    VtValue value;
    if (!usdAttr.Get(&value, UsdTimeCode::EarliestTime())) {
        return false;
    }
    return func(
        value,
        UsdTimeCode::EarliestTime(),
        MaxUsd::GetMaxTimeValueFromUsdTimeCode(context.GetStage(), UsdTimeCode::EarliestTime()));
}

PXR_NAMESPACE_CLOSE_SCOPE
