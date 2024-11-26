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

#include <MaxUsd/MeshConversion/MeshConverter.h>

#include <pxr/base/tf/pyContainerConversions.h>

#include <maxscript/maxwrapper/mxsobjects.h>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

class MeshConverterWrapper
{
    MeshConverterWrapper() = delete;
    ~MeshConverterWrapper() = delete;

public:
    static pxr::UsdGeomMesh ConvertToUSDMesh(
        ULONG                          nodeHandle,
        pxr::UsdStagePtr               stage,
        pxr::SdfPath                   path,
        USDSceneBuilderOptionsWrapper& options,
        bool                           applyOffsetTransform,
        const MaxUsd::ExportTime&      time)
    {
        INode*                node = GetCOREInterface()->GetINodeByHandle(nodeHandle);
        MaxUsd::MeshConverter converter;
        return converter.ConvertToUSDMesh(
            node,
            stage,
            path,
            options.GetMeshConversionOptions(),
            applyOffsetTransform,
            options.GetResolvedTimeConfig().IsAnimated(),
            time);
    }
};

void wrapMeshConverter()
{
    boost::python::class_<MeshConverterWrapper, boost::noncopyable> c(
        "MeshConverter", boost::python::no_init);
    boost::python::scope s(c);
    c.def(
         "ConvertToUSDMesh",
         &MeshConverterWrapper::ConvertToUSDMesh,
         boost::python::args(
             "node_handle", "stage", "path", "options", "applyOffsetTransform", "time"))
        .staticmethod("ConvertToUSDMesh");
}
