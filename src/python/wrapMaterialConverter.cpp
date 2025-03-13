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
#include "wrapUSDSceneBuilderOptions.h"

#include <BoostPythonWrapper.h>
#include <MaxUsd/MaterialConversion/MaterialConverter.h>

#include <pxr/base/tf/pyContainerConversions.h>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

class MaterialConverterWrapper
{
    MaterialConverterWrapper() = delete;
    ~MaterialConverterWrapper() = delete;

public:
    static pxr::UsdShadeMaterial ConvertToUSDMaterial(
        ULONG                          mtlHandle,
        UsdStagePtr                    stage,
        const std::string&             filename,
        bool                           isUSDZ,
        SdfPath                        path,
        USDSceneBuilderOptionsWrapper& options,
        const pyboost::list            bindings)
    {
        std::list<pxr::SdfPath> mtlBindings;
        try {
            for (int i = 0; i < pyboost::len(bindings); ++i) {
                const std::string pathStr = pyboost::extract<std::string>(bindings[i]);
                if (!SdfPath::IsValidPathString(pathStr)) {
                    auto msg = pathStr + std::string(" is not a valid prim path.");
                    throw std::invalid_argument { msg };
                }
                mtlBindings.emplace_back(pathStr);
            }
        } catch (...) {
            MaxUsd::Log::Error(
                "ConvertToUSDMaterial() failed. Invalid material/prim binding list.");
            return pxr::UsdShadeMaterial {};
        }

        Mtl* material = dynamic_cast<Mtl*>(Animatable::GetAnimByHandle(mtlHandle));
        if (!material) {
            MaxUsd::Log::Error("ConvertToUSDMaterial() failed. Invalid material handle.");
            return UsdShadeMaterial {};
        }
        return MaxUsd::MaterialConverter::ConvertToUSDMaterial(
            material, stage, filename, isUSDZ, path, options, mtlBindings);
    }
};

void wrapMaterialConverter()
{
    pyboost::class_<MaterialConverterWrapper, noncopyable> c("MaterialConverter", pyboost::no_init);
    pyboost::scope                                         s(c);
    c.def(
         "ConvertToUSDMaterial",
         &MaterialConverterWrapper::ConvertToUSDMaterial,
         (pyboost::arg("anim_mtl_handle"),
          pyboost::arg("stage"),
          pyboost::arg("filename"),
          pyboost::arg("isUSDZ"),
          pyboost::arg("primPath"),
          pyboost::arg("options"),
          pyboost::arg("bindings") = pyboost::list {}),
         "Converts a 3dsMax material to a UsdShadeMaterial prim (note that MultiMtls are not "
         "currently supported).")
        .staticmethod("ConvertToUSDMaterial");
}