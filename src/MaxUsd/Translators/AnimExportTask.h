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

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/Utilities/MaxProgressBar.h>

namespace MAXUSD_NS_DEF {

// This class allows to queue up export operations, and later to execute them "batched" per 3dsMax
// time. During the USD->3dsMax export process, we need to evaluate 3dsMax objects. Internally,
// 3dsMax caches the object states for the last evaluated frame. It follows that, especially for
// scenes with alot of object interdependencies, we want to export everything that needs to be
// exported at a certain time, at the same time. This way, we can benefit from the caching. This is
// essentially what this class is for.
class AnimExportTask
{
public:
    /**
     * \brief Constructor
     * \param timeConfig The time configuration for the export.
     */
    AnimExportTask(const TimeConfig& timeConfig);

    /**
     * \brief Adds an object export operation.
     * \param intervalFunc A function returning the validity interval of the exported data at a certain time.
     * From this we can figure out at what frame(s) this object needs to be exported.
     * \param writeAtTime A function that actually writes to the prim at certain time.
     * \param postExport A function called after all prims and materials have been written.
     */
    void AddObjectExportOp(
        std::function<Interval(TimeValue)>     intervalFunc,
        std::function<void(const ExportTime&)> writeAtTime,
        std::function<void()>                  postExport);

    /**
     * \brief Adds a transform export operation.
     * \param writeAtTime A function that writes the transform for an object at a certain time.
     */
    void
    AddTransformExportOp(std::function<void(const ExportTime&, pxr::UsdGeomXformOp&)> writeAtTime);

    /**
     * \brief Executes all export operations, batching operations per 3dsMax times, so to limit object evaluations.
     * \param Reference to the progress bar in use, for progress reporting, as this can be a length operation.
     */
    void Execute(MaxProgressBar& progress);

private:
    /**
     * \brief Object export operation.
     */
    struct ObjectAnimOp
    {
        std::function<Interval(TimeValue)> getValidityInterval;
        std::function<void(ExportTime)>    write;
        std::function<void()>              postExport;
        bool                               firstFrame = true;
    };

    /**
     * \brief Transform export operation.
     */
    struct TransformAnimOp
    {
        std::function<void(const ExportTime&, pxr::UsdGeomXformOp&)> write;
        pxr::UsdGeomXformOp                                          usdGeomXFormOp;
    };

    TimeConfig                   timeConfig;
    std::vector<ObjectAnimOp>    objectExportOps;
    std::vector<TransformAnimOp> transformExportOps;
};
} // namespace MAXUSD_NS_DEF
