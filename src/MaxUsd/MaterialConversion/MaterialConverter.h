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
#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/usd/usdShade/material.h>

#include <max.h>

namespace MAXUSD_NS_DEF {

/// Material Conversion utility Class
class MaxUSDAPI MaterialConverter
{
public:
    /**
     * \brief Converts a 3dsMax material to a UsdShade material. Note that MultiMtls are not supported at this time.
     * \param material The 3dsMax material to export.
     * \param stage The stage we are exporting this material to.
     * \param fileName The path of the layer we are exporting this material to, might be used for computing relative paths.
     * \param isUSDZ True if the target layer is intended to be packaged into a USDZ file.
     * \param targetPath The prim path to export the material to.
     * \param options The export options.
     * \param bindings Prim paths to bind the material to, optional.
     * \return The UsdShadeMaterial that was exporter, invalid if the export failed.
     */
    static pxr::UsdShadeMaterial ConvertToUSDMaterial(
        Mtl*                           material,
        const pxr::UsdStageRefPtr&     stage,
        const std::string&             fileName,
        bool                           isUSDZ,
        const pxr::SdfPath&            targetPath,
        const USDSceneBuilderOptions&  options,
        const std::list<pxr::SdfPath>& bindings);
};

} // namespace MAXUSD_NS_DEF
