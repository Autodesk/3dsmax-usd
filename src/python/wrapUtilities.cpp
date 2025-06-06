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
#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <BoostPythonWrapper.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/UsdToolsUtils.h>

#include <pxr/pxr.h>

#include <ufe/pathString.h>

#include <boost/python/def.hpp>
#include <pybind11/pybind11.h>

PXR_NAMESPACE_USING_DIRECTIVE

bool _openInUsdView(const std::string& usdFilePath)
{
    return MaxUsd::UsdToolsUtils::OpenInUsdView(MaxUsd::UsdStringToMaxString(usdFilePath).data());
}

bool _runUsdChecker(const std::string& usdFilePath, const std::string& outputPath)
{
    return MaxUsd::UsdToolsUtils::RunUsdChecker(
        MaxUsd::UsdStringToMaxString(usdFilePath).data(),
        MaxUsd::UsdStringToMaxString(outputPath).data());
}

std::string _getUsdPrimUfePath(UINT stageHandle, const std::string& primPath)
{
    const auto node = GetCOREInterface()->GetINodeByHandle(stageHandle);
    if (!node) {
        return {};
    }
    const auto stageObject = dynamic_cast<USDStageObject*>(node->GetObjectRef());
    if (!stageObject) {
        return {};
    }
    return Ufe::PathString::string(
        MaxUsd::ufe::getUsdPrimUfePath(stageObject, pxr::SdfPath { primPath }));
}

UsdPrim _getUsdPrim(PyObject* ufePath)
{
    const Ufe::Path* cUfePath
        = pybind11::cast<Ufe::Path*>(pybind11::reinterpret_borrow<pybind11::object>(ufePath));
    if (!cUfePath) {
        return {};
    }
    return MaxUsd::ufe::ufePathToPrim(*cUfePath);
}

std::string _getUsdStageUfePath(UsdStageWeakPtr stage)
{
    return Ufe::PathString::string(MaxUsd::ufe::getStagePath(stage));
}

void wrapUtilities()
{
    pyboost::def(
        "OpenInUsdView",
        _openInUsdView,
        pyboost::arg("usdFilePath"),
        "Opens the usd view program given a valid path to a usd file.");
    pyboost::def(
        "RunUsdChecker",
        _runUsdChecker,
        pyboost::args("usdFilePath", "outputPath"),
        "Runs the usdchecker tool which will validate a usd file at usdFilePath and output all "
        "errors at outputPath");
    pyboost::def(
        "GetUsdStageUfePath",
        _getUsdStageUfePath,
        pyboost::args("usdStage"),
        "Returns the UFE Path, associated with the given USD Stage.");
    pyboost::def(
        "GetUsdPrimUfePath",
        _getUsdPrimUfePath,
        pyboost::args("stageObjectHandle", "primPath"),
        "Returns the UFE Path, associated with the given USD prim path, in the given stage.");
    pyboost::def(
        "GetUsdPrim",
        _getUsdPrim,
        pyboost::args("ufePath"),
        "Returns the USD Prim, associated with the given UFE path.");
}
