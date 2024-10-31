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
#include "wrapInterval.h"

#include "pythonObjectRegistry.h"

#include <boost/python/class.hpp>
#include <boost/python/make_constructor.hpp>
#include <boost/python/return_value_policy.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

void wrapInterval()
{
    using namespace boost::python;

    boost::python::class_<IntervalWrapper> c(
        "Interval", "A python wrapper for a 3dsMax Interval object.", init<double, double>());
    c.def(
         "Start",
         &IntervalWrapper::Start,
         return_value_policy<return_by_value>(),
         (boost::python::arg("self")),
         "The start frame of the interval.")
        .def(
            "End",
            &IntervalWrapper::End,
            return_value_policy<return_by_value>(),
            (boost::python::arg("self")),
            "The end frame of the interval.")
        .add_property("Forever", &IntervalWrapper::Forever)
        .add_property("Never", &IntervalWrapper::Never);
}