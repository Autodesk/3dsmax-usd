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
#include "pythonObjectRegistry.h"

#include <MaxUsd/Utilities/TimeUtils.h>

#include <boost/python/class.hpp>
#include <boost/python/make_constructor.hpp>
#include <boost/python/return_value_policy.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

void wrapExportTime()
{
    using namespace boost::python;

    boost::python::class_<MaxUsd::ExportTime> c(
        "ExportTime",
        "An export frame time config.",
        init<double, const pxr::UsdTimeCode&, bool>());
    c.def(
         "GetMaxTime",
         &MaxUsd::ExportTime::GetMaxFrame,
         return_value_policy<return_by_value>(),
         (boost::python::arg("self")),
         "The 3dsMax time for the frame being exported.")
        .def(
            "GetUsdTime",
            &MaxUsd::ExportTime::GetUsdTime,
            return_value_policy<return_by_value>(),
            (boost::python::arg("self")),
            "The target USD TimeCode for the frame being exported.")
        .def(
            "IsFirstFrame",
            &MaxUsd::ExportTime::IsFirstFrame,
            return_value_policy<return_by_value>(),
            (boost::python::arg("self")),
            "Whether this frame is the first one being exported for the object.");
}