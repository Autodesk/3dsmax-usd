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

#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/UsdToolsUtils.h>

#include <pxr/pxr.h>

#include <ufe/pathString.h>

#include <boost/python/def.hpp>

using namespace std;
using namespace boost::python;
using namespace boost;

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

void wrapUtilities()
{
    def("OpenInUsdView",
        _openInUsdView,
        boost::python::arg("usdFilePath"),
        "Opens the usd view program given a valid path to a usd file.");
    def("RunUsdChecker",
        _runUsdChecker,
        boost::python::args("usdFilePath", "outputPath"),
        "Runs the usdchecker tool whih will validate a usd file at usdFilePath and output all "
        "errorrs at outputPath");
    def("GetUsdPrimUfePath",
        _getUsdPrimUfePath,
        boost::python::args("stageObjectHandle", "primPath"),
        "Returns the UFE Path, associated with the given USD prim path, in the given stage.");
}
