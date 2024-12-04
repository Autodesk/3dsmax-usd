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
#include "wrapUSDSceneBuilderOptions.h"

#include <MaxUsd/Translators/TranslatorUtils.h>

#include <pxr/base/tf/pyContainerConversions.h>

#include <maxscript/maxwrapper/mxsobjects.h>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>
#include <control.h> // 3ds Max Animating method

PXR_NAMESPACE_USING_DIRECTIVE

class TranslationUtilsWrapper
{
    TranslationUtilsWrapper() = delete;
    ~TranslationUtilsWrapper() = delete;

public:
    static boost::python::list
    GetKeyFramesFromValidityInterval(ULONG nodeHandle, const USDSceneBuilderOptionsWrapper& options)
    {
        const auto keyFrames = MaxUsd::GetFramesFromValidityInterval(
            GetCOREInterface()->GetINodeByHandle(nodeHandle), options.GetResolvedTimeConfig());

        boost::python::list frames;
        const auto&         maxTimeValues = keyFrames.first;
        const auto&         usdTimeCodes = keyFrames.second;
        for (int i = 0; i < maxTimeValues.size(); ++i) {
            frames.append(boost::python::make_tuple(
                static_cast<float>(maxTimeValues[i]) / static_cast<float>(GetTicksPerFrame()),
                usdTimeCodes[i].GetValue()));
        }
        return frames;
    }

    static void ReadUsdAttribute(
        const UsdAttribute&         usdAttr,
        PyObject*                   callable,
        MaxUsdReadJobContextWrapper jobCtx,
        bool                        onlyWhenAuthored)
    {
        MaxUsdTranslatorUtil::ReadUsdAttribute(
            usdAttr,
            [&](const VtValue& value, const UsdTimeCode& timeCode, const TimeValue& time) {
                bool res = boost::python::call<bool>(
                    callable,
                    value,
                    timeCode,
                    MaxUsd::GetMaxFrameFromUsdTimeCode(jobCtx.GetStage(), timeCode),
                    Animating() == 1);
                if (!res) {
                    // unable to properly read the attribute
                    MaxUsd::Log::Error(
                        "Unable to import the '{}' attribute on '{}' at time code {}.",
                        usdAttr.GetName().GetString(),
                        usdAttr.GetPrim().GetName().GetString(),
                        timeCode.GetValue());
                    return false;
                }
                return res;
            },
            jobCtx,
            onlyWhenAuthored);
    }
};

void wrapTranslationUtils()
{
    boost::python::class_<TranslationUtilsWrapper, boost::noncopyable> c(
        "TranslationUtils", boost::python::no_init);

    boost::python::scope s(c);

    c.def(
         "GetKeyFramesFromValidityInterval",
         &TranslationUtilsWrapper::GetKeyFramesFromValidityInterval,
         boost::python::args("node_handle", "options"))
        .staticmethod("GetKeyFramesFromValidityInterval")
        .def(
            "ReadUsdAttribute",
            &TranslationUtilsWrapper::ReadUsdAttribute,
            (boost::python::arg("job_context"),
             boost::python::arg("functor"),
             boost::python::arg("context"),
             boost::python::arg("only_when_authored") = true))
        .staticmethod("ReadUsdAttribute")
        .def(
            "GetMaxFrameFromUsdTimeCode",
            &MaxUsd::GetMaxFrameFromUsdTimeCode,
            boost::python::args("stage", "time_code"))
        .staticmethod("GetMaxFrameFromUsdTimeCode")
        .def(
            "GetMaxTimeValueFromUsdTimeCode",
            &MaxUsd::GetMaxTimeValueFromUsdTimeCode,
            boost::python::args("stage", "time_code"))
        .staticmethod("GetMaxTimeValueFromUsdTimeCode");
}
