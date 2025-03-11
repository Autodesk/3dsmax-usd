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
#include "wrapMaxSceneBuilderOptions.h"
#include "wrapReadJobContext.h"

#include <MaxUsd/Translators/TranslatorMaterial.h>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

struct MaxUsdTranslatorMaterialWrapper
{
    static bool AssignMaterial(
        const MaxSceneBuilderOptionsWrapper& buildOptions,
        const UsdGeomGprim&                  prim,
        INT_PTR                              nodeHandle,
        pyboost::object&                     context)
    {
        INode* node = dynamic_cast<INode*>(Animatable::GetAnimByHandle(nodeHandle));
        if (node) {
            MaxUsdReadJobContextWrapper& contextRef
                = pyboost::extract<MaxUsdReadJobContextWrapper&>(context);
            return MaxUsdTranslatorMaterial::AssignMaterial(buildOptions, prim, node, contextRef);
        }
        return false;
    }
};

void wrapTranslatorMaterial()
{
    pyboost::class_<MaxUsdTranslatorMaterialWrapper, noncopyable>(
        "TranslatorMaterial", pyboost::no_init)
        .def(
            "AssignMaterial",
            &MaxUsdTranslatorMaterialWrapper::AssignMaterial,
            pyboost::args("options", "prim", "node_handle", "context"))
        .staticmethod("AssignMaterial");
}
