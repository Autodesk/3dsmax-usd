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

#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/usd/usd/timeCode.h>

#include <MaxUsd.h>
#include <max.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief Returns the TimeValue (in ticks) associated with a given frame.
 * \param frame The frame.
 * \return The TimeValue
 */
inline TimeValue GetTimeValueFromFrame(double frame)
{
    return TimeValue(frame * GetTicksPerFrame());
}

/**
 * \brief Returns the frame associated with the given TimeValue.
 * \param time The time value
 * \return The associated frame.
 */
inline double GetFrameFromTimeValue(TimeValue time)
{
    return double(time) / double(GetTicksPerFrame());
}

/**
 * \brief Represents a time configuration for exporting a 3dsMax scene to USD, i.e. the frame
 * range to export, and the sampling rate.
 */
class TimeConfig
{
public:
    TimeConfig() {};

    TimeConfig(double startFrame, double endFrame, double samplesPerFrame)
    {
        SetStartFrame(startFrame);
        SetEndFrame(endFrame);
        SetSamplesPerFrame(samplesPerFrame);
    }

    TimeConfig(TimeValue startTime, TimeValue endTime, double samplesPerFrame)
    {
        SetStartTime(startTime);
        SetEndTime(endTime);
        SetSamplesPerFrame(samplesPerFrame);
    }

    void SetStartFrame(double startFrame)
    {
        this->startFrame = startFrame;
        this->startTime = GetTimeValueFromFrame(startFrame);
    }

    void SetEndFrame(double endFrame)
    {
        this->endFrame = endFrame;
        this->endTime = GetTimeValueFromFrame(endFrame);
    }

    void SetStartTime(TimeValue startTime)
    {
        this->startTime = startTime;
        this->startFrame = GetFrameFromTimeValue(startTime);
    }

    void SetEndTime(TimeValue endTime)
    {
        this->endTime = endTime;
        this->endFrame = double(endTime) / double(GetTicksPerFrame());
    }

    void SetSamplesPerFrame(double samplesPerFrame)
    {
        ValidateSamplePerFrame(samplesPerFrame);
        this->samplesPerFrame = samplesPerFrame;
    }

    static void ValidateSamplePerFrame(double& samplesPerFrame)
    {
        if (samplesPerFrame <= 0) {
            DbgAssert(0 && "Samples per frame should be greater than 0");
            samplesPerFrame = 0.01;
        }
    }

    double GetStartFrame() const { return startFrame; }

    double GetEndFrame() const { return endFrame; }

    TimeValue GetStartTime() const { return startTime; }

    TimeValue GetEndTime() const { return endTime; }

    double GetSamplesPerFrame() const { return samplesPerFrame; }

    bool IsAnimated() const { return startFrame != endFrame; }

    int GetTimeStep() const
    {
        return static_cast<int>(static_cast<double>(GetTicksPerFrame()) / samplesPerFrame);
    }

private:
    double    startFrame { 0.0 };
    double    endFrame { 0.0 };
    TimeValue startTime { 0 };
    TimeValue endTime { 0 };
    double    samplesPerFrame { 1.0 };
};

/**
 * \brief Represents the time configuration for importing USD stage into 3ds Max.
 */
class ImportTimeConfig
{
public:
    /**
     * \brief Default constructor.
     */
    MaxUSDAPI explicit ImportTimeConfig() = default;

    /**
     * \brief Constructor with non-default values for start and end time code.
     * \param startTimeCode value to be set as the start time code to be used when importing USD
     * \param endTimeCode value to be set as the end time code to be used when importing USD
     */
    MaxUSDAPI explicit ImportTimeConfig(double startTimeCode, double endTimeCode)
    {
        SetStartTimeCode(startTimeCode);
        SetEndTimeCode(endTimeCode);
    }

    /**
     * \brief Sets the start time code to be used when importing USD.
     * If the new startTimeCode value is greater than the end time code, this method will also
     * update the end time code to be equal to the start time code.
     * \param startTimeCode the value to be set as the start time code
     */
    MaxUSDAPI void SetStartTimeCode(double startTimeCode);

    /**
     * \brief Sets the end time code to be used when importing USD.
     * If the new endTimeCode is less than the start time code, it will be capped to the start time
     * code, as the end time code can't be smaller than the start time code.
     * \param endTimeCode the value to be set as the end time code
     */
    MaxUSDAPI void SetEndTimeCode(double endTimeCode);

    /**
     * \brief Gets the start time when importing USD.
     * \return Start time code to import USD
     */
    MaxUSDAPI double GetStartTimeCode() const { return startTimeCode; }

    /**
     * \brief Gets the end time when importing USD.
     * \return End time code to import USD.
     */
    MaxUSDAPI double GetEndTimeCode() const { return endTimeCode; }

    /**
     * \brief Checks if importing and animated range.
     * \return Is animated returns true if the start time code is different than the end time code.
     */
    MaxUSDAPI bool IsAnimated() const { return startTimeCode != endTimeCode; }

private:
    /// Represents the start time code to be imported
    double startTimeCode { 0 };
    /// Represents the end time code to be imported
    double endTimeCode { 0 };
};

/**
 * \brief Represents a single time sample export configuration. I.e. the 3dsMax time value, and what
 * USD timecode is corresponds to. Also allows to specify whether this time sample is the first
 * being exported for a object.
 */
class ExportTime
{
public:
    ExportTime(const TimeValue& maxTime, const pxr::UsdTimeCode& usdTime, bool isFirstFrame)
    {
        this->maxTime = maxTime;
        this->maxFrame = MaxUsd::GetFrameFromTimeValue(maxTime);
        this->usdTime = usdTime;
        this->isFirstFrame = isFirstFrame;
    }

    ExportTime(double maxFrame, const pxr::UsdTimeCode& usdTime, bool isFirstFrame)
    {
        this->maxFrame = maxFrame;
        this->maxTime = MaxUsd::GetTimeValueFromFrame(maxFrame);
        this->usdTime = usdTime;
        this->isFirstFrame = isFirstFrame;
    }

    TimeValue GetMaxTime() const { return maxTime; }

    // For convenience in python exposure.
    double GetMaxFrame() const { return maxFrame; }

    pxr::UsdTimeCode GetUsdTime() const { return usdTime; }

    bool IsFirstFrame() const { return isFirstFrame; }

private:
    TimeValue        maxTime;
    double           maxFrame;
    pxr::UsdTimeCode usdTime;
    bool             isFirstFrame = true;
};

} // namespace MAXUSD_NS_DEF
