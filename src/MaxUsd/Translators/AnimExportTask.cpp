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
#include "AnimExportTask.h"

#include <MaxUsd/DLLEntry.h>
#include <MaxUsd/resource.h>

namespace MAXUSD_NS_DEF {

AnimExportTask::AnimExportTask(const TimeConfig& timeConfig)
    : timeConfig(timeConfig)
{
}

void AnimExportTask::AddObjectExportOp(
    std::function<Interval(TimeValue)>     intervalFunc,
    std::function<void(const ExportTime&)> writeAtTime,
    std::function<void()>                  postExport)
{
    objectExportOps.push_back({ intervalFunc, writeAtTime, postExport });
}

void AnimExportTask::AddTransformExportOp(
    std::function<void(const ExportTime&, pxr::UsdGeomXformOp&)> writeAtTime)
{
    transformExportOps.push_back({ writeAtTime });
}

void AnimExportTask::Execute(MaxProgressBar& progress)
{
    // Export the time samples for...
    // Object prims : Only export the required frames, which we infer from the export
    // data's validity interval (we get this from prim writers - typically the 3dsMax
    // object validity intervals).
    // Node transforms : Export the transforms at all time samples in the export range.

    // Simple struct defining what needs to be exported for a specific time.
    struct ExportReq
    {
        bool                      transform = false;
        std::vector<ObjectAnimOp> objects;
    };

    // Track all times that we need to export, and the object we need to export at each of them.
    std::map<TimeValue, ExportReq> exportTimes;

    // We do not currently use validity intervals for transforms, so at a minimum, we need to export
    // those at every time sample within the export range...
    const int       timeStep = timeConfig.GetTimeStep();
    const TimeValue startTime = timeConfig.GetStartTime();
    const TimeValue endTime = timeConfig.GetEndTime();
    for (TimeValue timeVal = startTime; timeVal <= endTime;) {
        exportTimes.insert({ timeVal, { true, {} } });
        if (timeVal == endTime) {
            break;
        }
        // Calculate the next time sample... Make sure the endTime is exported.
        timeVal = std::min(timeVal + timeStep, endTime);
    }

    // Next, cycle through all the objects we have queued for export, and figure out the first
    // time value at which we need to export them, from the validity interval at the anim start
    // time.
    for (auto& objectExpOp : objectExportOps) {
        const auto intervalAtStart = objectExpOp.getValidityInterval(startTime);

        TimeValue firstExportTime = 0;

        // Generally, we want to export the last time of the interval applicable at the start time.
        // [----00000------] (data change on frames marked with 0s, [...] indicates the export
        // range)
        //     ^--- this is the first meaningful frame, data doesn't change up until right after it.
        const auto lastTimeOfInterval = intervalAtStart.End();

        if (lastTimeOfInterval > startTime && lastTimeOfInterval < endTime) {
            firstExportTime = lastTimeOfInterval;
        } else {
            firstExportTime = startTime;
        }
        exportTimes[firstExportTime].objects.push_back(objectExpOp);
    }

    if (exportTimes.empty()) {
        return;
    }

    // We report progress differently depending on if we are exporting an animation or a single
    // frame. Animated : Report progress frame by frame. Non-Animated : Report progress per object
    // and then per transform (object are exported first, then their transforms).
    const bool animated = timeConfig.IsAnimated();

    // Get the resource strings only once per 3dsmax session.
    static const std::wstring framesProgMsg = GetString(IDS_EXPORT_FRAMES_PROGRESS_MESSAGE);
    static const std::wstring objectsProgMsg = GetString(IDS_EXPORT_OBJECTS_PROGRESS_MESSAGE);
    static const std::wstring transformProgMsg = GetString(IDS_EXPORT_TRANSFORMS_PROGRESS_MESSAGE);
    static const std::wstring objectPostExportProgMsg
        = GetString(IDS_EXPORT_POST_EXPORT_PROGRESS_MESSAGE);

    progress.SetTotal(animated ? exportTimes.size() : objectExportOps.size());
    progress.UpdateProgress(0, true, animated ? framesProgMsg.c_str() : objectsProgMsg.c_str());
    size_t frameProgress = 0;

    // Export at each time (exportTimes is ordered) - making sure that all objects & transforms
    // that need to be exported at each frames are exported in one go, and so we benefit from
    // the object state caching.
    for (auto& frame : exportTimes) {
        auto& req = frame.second;

        const auto maxTime = frame.first;
        const auto usdTime = animated ? pxr::UsdTimeCode(GetFrameFromTimeValue(maxTime))
                                      : pxr::UsdTimeCode::Default();
        // Write the object time samples we need at this frame...
        for (size_t i = 0; i < req.objects.size(); ++i) {
            auto& object = req.objects[i];

            const ExportTime expTime { maxTime, usdTime, object.firstFrame };
            object.write(expTime);

            // Non-animated case, report an object was exported.
            if (!animated) {
                progress.UpdateProgress(i + 1, true, objectsProgMsg.c_str());
            }

            // Figure out the next time sample we will need for this object, from the validity
            // interval of what we just exported.
            const auto nextTime = maxTime + timeStep;
            auto       interval = object.getValidityInterval(nextTime);
            const auto intervalEnd = interval.End();
            const auto intervalStart = interval.Start();

            TimeValue nextCandidateTime = 0;

            // Typically, we need to export at the start and end times of each object's interval,
            // unless the validity goes beyond the range of frames we are interested in.

            // If the object export interval goes up to, or beyond the range we are exporting,
            // consider the start time of the interval for export.
            if (intervalEnd >= endTime) {
                nextCandidateTime = intervalStart;
            }
            // Typical case, moving through the animation / object export intervals.
            else {
                // If we previously exported the start time of the interval, export the end time.
                if (maxTime == intervalStart) {
                    nextCandidateTime = intervalEnd;
                }
                // Otherwise, make sure we export the start time.
                else {
                    nextCandidateTime = intervalStart;
                }
            }

            // If for proper USD interpolation we should export a frame beyond the animation range,
            // make sure to at least export the object at the last time of the exported animation.
            if (nextCandidateTime > endTime) {
                nextCandidateTime = endTime;
            }

            // If we previously exported an equal or more advanced time, we are done.
            if (maxTime >= nextCandidateTime) {
                continue;
            }

            if (object.firstFrame) {
                object.firstFrame = false;
            }
            exportTimes[nextCandidateTime].objects.push_back(std::move(object));
        }
        req.objects.clear();

        // If we need to export transforms at this frame, do it!
        if (req.transform) {
            if (!animated) {
                progress.SetTotal(transformExportOps.size());
            }

            for (size_t i = 0; i < transformExportOps.size(); ++i) {
                auto& transformExpOp = transformExportOps[i];

                ExportTime expTime { maxTime, usdTime, false };
                transformExpOp.write(expTime, transformExpOp.usdGeomXFormOp);

                // Non-animated case - report that a transform was exported...
                if (!animated) {
                    progress.UpdateProgress(i + 1, true, transformProgMsg.c_str());
                }
            }

            // Animated case - report that a frame was exported...
            if (animated) {
                progress.UpdateProgress(frameProgress++, true, framesProgMsg.c_str());
            }
        }
    }

    progress.SetTotal(objectExportOps.size());

    int progressCounter = 0;
    for (const auto& objectExpOp : objectExportOps) {
        progress.UpdateProgress(progressCounter++, true, objectPostExportProgMsg.c_str());

        objectExpOp.postExport();
    }
}

} // namespace MAXUSD_NS_DEF
