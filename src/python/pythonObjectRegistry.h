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

#include <BoostPythonWrapper.h>

#include <pxr/pxr.h>

#include <boost/python/class.hpp>
#include <map>
#include <string>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

//---------------------------------------------------------------------------------------------
/// \brief Keeps track of registered Python classes and allows updating and unregistering them
//---------------------------------------------------------------------------------------------
class MaxUsdPythonObjectRegistry
{
protected:
    static const size_t UPDATED = 0xFFFFFFFF;

    // Registers or updates a Python class for the provided key.
    static size_t RegisterPythonObject(pyboost::object cl, const std::string& key);

    // Unregister a Python class for a given key. This will cause the associated factory function to
    // stop producing this Python class.
    static void UnregisterPythonObject(pyboost::object cl, const std::string& key);

    static pyboost::object GetPythonObject(size_t index) { return _sClassVec[index]; }

    static bool IsPythonClass(pyboost::object cl);

    static std::string ClassName(pyboost::object cl);

private:
    // Static table of all registered Python classes, with associated index:
    typedef std::vector<pyboost::object>  TClassVec;
    static TClassVec                      _sClassVec;
    typedef std::map<std::string, size_t> TClassIndex;
    static TClassIndex                    _sIndex;

    static void HookInterpreterExit();

public:
    // To be called when the Python interpreter exits.
    static void OnInterpreterExit();
};

PXR_NAMESPACE_CLOSE_SCOPE
