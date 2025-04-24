//
// Copyright 2024 Autodesk
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

#include <BoostPythonWrapper.h>
#include <MaxUsd/Utilities/OptionUtils.h>

#include <pxr/pxr.h>

#include <boost/python/def.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

bool _setToPersistentStorage(const std::string& group, const std::string& key, PyObject* value)
{
    if (value == nullptr) {
        return false;
    }

    VtDictionary dict;
    MaxUsd::OptionUtils::LoadUiOptions(group, dict);

    if (PyFloat_Check(value)) {
        dict[key] = PyFloat_AsDouble(value);
    } else if (PyLong_Check(value)) {
        dict[key] = PyLong_AsLong(value);
    } else {
        return false;
    }

    MaxUsd::OptionUtils::SaveUiOptions(group, dict);
    return false;
}

PyObject*
_getFromPersistentStorage(const std::string& group, const std::string& key, PyObject* defaultValue)
{
    if (defaultValue == nullptr) {
        return nullptr;
    }

    VtDictionary dict;
    MaxUsd::OptionUtils::LoadUiOptions(group, dict);
    auto it = dict.find(key);

    if (PyFloat_Check(defaultValue)) {
        double val = PyFloat_AsDouble(defaultValue);
        if (it != dict.end()) {
            try {
                if (it->second.IsHolding<double>()) {
                    val = it->second.GetWithDefault<double>(val);
                } else if (it->second.CanCast<double>()) {
                    val = it->second.Cast<double>().GetWithDefault<double>(val);
                }
            } catch (...) {
            }
        }
        return PyFloat_FromDouble(val);
    }

    if (PyLong_Check(defaultValue)) {
        long val = PyLong_AsLong(defaultValue);
        if (it != dict.end()) {
            try {
                if (it->second.IsHolding<long>()) {
                    val = it->second.GetWithDefault<long>(val);
                } else if (it->second.CanCast<long>()) {
                    val = it->second.Cast<long>().GetWithDefault<long>(val);
                }
            } catch (...) {
            }
        }
        return PyLong_FromLong(val);
    }

    return nullptr;
}

void wrapPersistentStorage()
{
    pyboost::def(
        "SetToPersistentStorage",
        _setToPersistentStorage,
        pyboost::args("group", "key", "value"),
        "Saves the given value in to the usdUiSettings.json with the given key and group");
    pyboost::def(
        "GetFromPersistentStorage",
        _getFromPersistentStorage,
        pyboost::args("group", "key", "defaultValue"),
        "Reads the value with the given key and group from to the usdUiSettings.json or returns "
        "the defaultValue if not present.");
}
