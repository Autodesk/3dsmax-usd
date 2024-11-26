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

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/pyUtils.h>
#include <pxr/pxr.h>

#include <boost/python.hpp>

using namespace std;
using namespace boost::python;
using namespace boost;

PXR_NAMESPACE_USING_DIRECTIVE

// Registers or updates a Python class for the provided key.
size_t MaxUsdPythonObjectRegistry::RegisterPythonObject(object cl, const std::string& key)
{
    TClassIndex::const_iterator target = _sIndex.find(key);
    if (target == _sIndex.cend()) {
        HookInterpreterExit();
        // Create a new entry:
        size_t classIndex = _sClassVec.size();
        _sClassVec.emplace_back(cl);
        _sIndex.emplace(key, classIndex);
        // Return a new factory function:
        return classIndex;
    } else {
        _sClassVec[target->second] = cl;
        return UPDATED;
    }
}

void MaxUsdPythonObjectRegistry::UnregisterPythonObject(object cl, const std::string& key)
{
    TClassIndex::const_iterator target = _sIndex.find(key);
    if (target != _sIndex.cend()) {
        // Clear the Python class:
        _sClassVec[target->second] = object();
        _sIndex.erase(key);
    }
}

bool MaxUsdPythonObjectRegistry::IsPythonClass(object cl)
{
    auto classAttr = cl.attr("__class__");
    if (!classAttr) {
        return false;
    }
    auto nameAttr = classAttr.attr("__name__");
    if (!nameAttr) {
        return false;
    }
    std::string name = extract<std::string>(nameAttr);
    return name == "class";
}

std::string MaxUsdPythonObjectRegistry::ClassName(object cl)
{
    // Is it a Python class:
    if (!IsPythonClass(cl)) {
        // So far class is always first parameter, so we can have this check be here.
        TfPyThrowRuntimeError("First argument must be a Python class");
    }

    auto nameAttr = cl.attr("__name__");
    if (!nameAttr) {
        TfPyThrowRuntimeError("Unexpected Python error: No __name__ attribute");
    }

    return std::string(boost::python::extract<std::string>(nameAttr));
}

MaxUsdPythonObjectRegistry::TClassVec   MaxUsdPythonObjectRegistry::_sClassVec;
MaxUsdPythonObjectRegistry::TClassIndex MaxUsdPythonObjectRegistry::_sIndex;

void MaxUsdPythonObjectRegistry::HookInterpreterExit()
{
    if (_sClassVec.empty()) {
        if (import("atexit").attr("register")(&OnInterpreterExit).is_none()) {
            TF_CODING_ERROR("Couldn't register unloader to atexit");
        }
    }
}

void MaxUsdPythonObjectRegistry::OnInterpreterExit()
{
    // Release all Python classes:
    for (size_t i = 0; i < _sClassVec.size(); ++i) {
        _sClassVec[i] = object();
    }
}
