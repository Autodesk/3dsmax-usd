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

#include "ShaderWriter.h"
#include "WriteJobContext.h"

#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/types.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>

#include <vector>

class Mtl;

PXR_NAMESPACE_OPEN_SCOPE

#define MAT_ROOT_TOKENS ((materials, "mtl"))
TF_DECLARE_PUBLIC_TOKENS(MaterialsRootMaxTokens, MaxUSDAPI, MAT_ROOT_TOKENS);

class MaxUsdShadingModeExportContext
{
public:
    MaxUSDAPI MaxUsdShadingModeExportContext(MaxUsdWriteJobContext& writeJobContext);

    void SetMaterialAndBindings(Mtl* material, const std::list<SdfPath>* bindings)
    {
        this->material = material;
        this->bindings = bindings;
        this->additionalMtls.clear();
    }
    Mtl*                      GetMaterial() const { return material; }
    const std::list<SdfPath>& GetBindings() const { return *bindings; }

    const UsdStageRefPtr& GetUsdStage() const { return stage; }

    MaxUsdWriteJobContext& GetWriteJobContext() const { return writeJobContext; }

    const MaxUsd::USDSceneBuilderOptions& GetExportArgs() const
    {
        return writeJobContext.GetArgs();
    }

    /// Use this function to create a UsdShadeMaterial at the given path. If the path is left empty,
    /// the prim is created at a path determined from the export options and material name.
    MaxUSDAPI UsdPrim MakeStandardMaterialPrim(const pxr::SdfPath& path = {}) const;

    /// Use this function to bind a UsdShadeMaterial prim to known bindings.
    MaxUSDAPI void BindStandardMaterialPrim(const UsdPrim& materialPrim) const;

    MaxUSDAPI void  AdditionalMaterials(const std::vector<Mtl*>& additionalMtls);
    MaxUSDAPI const std::vector<Mtl*>& GetAdditionalMaterials() const;

    MaxUSDAPI void  AddShaderWriter(MaxUsdShaderWriterSharedPtr& writer);
    MaxUSDAPI const std::list<MaxUsdShaderWriterSharedPtr>& GetShaderWriters() const;

private:
    /// The 3ds Max material element to process only in the Export calls (not set in Pre/Post
    /// Export)
    Mtl* material;
    /// The USD prim bindings for the export material element in the Export calls (not set in
    /// Pre/Post Export)
    const std::list<SdfPath>* bindings;
    /// Additional materials to export that originates from the exported material
    std::vector<Mtl*> additionalMtls;

    /// The stage to which everything is being exported to
    const UsdStageRefPtr& stage;
    /// The job context for shared data required by the MaxUsdShaderWriters
    MaxUsdWriteJobContext& writeJobContext;
    /// List of ShaderWriter that got used in the export process
    std::list<MaxUsdShaderWriterSharedPtr> writers;
};

PXR_NAMESPACE_CLOSE_SCOPE
