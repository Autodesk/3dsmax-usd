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
#pragma once
#include <MaxUsd/Utilities/TimeUtils.h>

PXR_NAMESPACE_USING_DIRECTIVE

class IntervalWrapper : public Interval
{
public:
    IntervalWrapper()
        : Interval()
    {
    }

    IntervalWrapper(double start, double end)
        : Interval { MaxUsd::GetTimeValueFromFrame(start), MaxUsd::GetTimeValueFromFrame(end) }
    {
    }

    IntervalWrapper(const Interval& interval)
        : Interval { interval }
    {
    }

    double Start() { return MaxUsd::GetFrameFromTimeValue(Interval::Start()); }

    double End() { return MaxUsd::GetFrameFromTimeValue(Interval::End()); }

    static IntervalWrapper Forever()
    {
        static IntervalWrapper forever { FOREVER };
        return forever;
    }

    static IntervalWrapper Never()
    {
        static IntervalWrapper never { NEVER };
        return never;
    }
};
