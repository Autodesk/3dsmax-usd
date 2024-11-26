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
#include "wrapReadJobContext.h"

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

MaxUsdReadJobContextWrapper::MaxUsdReadJobContextWrapper(MaxUsdReadJobContext& context)
    : readContext(context)
{
}

INT_PTR MaxUsdReadJobContextWrapper::GetNodeHandle(const SdfPath& path, bool findAncestors) const
{
    return GetAnimHandle(readContext.GetMaxRefTargetHandle(path, findAncestors));
}

void MaxUsdReadJobContextWrapper::RegisterNodeHandle(const SdfPath& path, INT_PTR maxNodeHandle)
{
    readContext.RegisterNewMaxRefTargetHandle(path, GetReferenceTarget(maxNodeHandle));
}

bool MaxUsdReadJobContextWrapper::GetPruneChildren() const
{
    return readContext.GetPruneChildren();
}

void MaxUsdReadJobContextWrapper::SetPruneChildren(bool prune)
{
    readContext.SetPruneChildren(prune);
}

const UsdStageRefPtr MaxUsdReadJobContextWrapper::GetStage() const
{
    return readContext.GetStage();
}

INT_PTR MaxUsdReadJobContextWrapper::GetAnimHandle(ReferenceTarget* ref)
{
    if (ref) {
        return Animatable::GetHandleByAnim(ref);
    }
    return 0;
}

ReferenceTarget* MaxUsdReadJobContextWrapper::GetReferenceTarget(INT_PTR handle)
{
    return dynamic_cast<ReferenceTarget*>(Animatable::GetAnimByHandle(handle));
}

void wrapReadJobContext()
{
    using namespace boost::python;

    class_<MaxUsdReadJobContextWrapper>("PrimReaderContext", no_init)
        .def(
            "GetNodeHandle",
            &MaxUsdReadJobContextWrapper::GetNodeHandle,
            (boost::python::args("context", "prim")),
            "Get the MAXScript AnimHandle on the node created for the given Prim.")
        .def(
            "RegisterCreatedNode",
            &MaxUsdReadJobContextWrapper::RegisterNodeHandle,
            (boost::python::args("self", "path", "anim_handle")),
            "Record 3ds Max node animHandle created for the prim path")
        .def("GetPruneChildren", &MaxUsdReadJobContext::GetPruneChildren)
        .def("SetPruneChildren", &MaxUsdReadJobContext::SetPruneChildren)
        .def("GetStage", &MaxUsdReadJobContextWrapper::GetStage);
}