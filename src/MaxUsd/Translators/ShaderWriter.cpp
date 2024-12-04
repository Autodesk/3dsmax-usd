//
// Copyright 2018 Pixar
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
// © 2022 Autodesk, Inc. All rights reserved.
//
#include "ShaderWriter.h"

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/usd/usdShade/material.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType) { TfType::Define<MaxUsdShaderWriter>(); }

MaxUsdShaderWriter::MaxUsdShaderWriter(
    Mtl*                   material,
    const SdfPath&         usdPath,
    MaxUsdWriteJobContext& jobCtx)
    : writeJobCtx(jobCtx)
    , material(material)
    , usdPath(usdPath)
{
}

void MaxUsdShaderWriter::GetSubMtlDependencies(std::vector<Mtl*>& subMtl) const
{
    for (int i = 0; i < material->NumSubMtls(); ++i) {
        if (auto mtl = material->GetSubMtl(i)) {
            subMtl.push_back(mtl);
        }
    }
}

const UsdStageRefPtr& MaxUsdShaderWriter::GetUsdStage() const { return writeJobCtx.GetUsdStage(); }

const SdfPath& MaxUsdShaderWriter::GetUsdPath() const { return usdPath; }

const UsdPrim& MaxUsdShaderWriter::GetUsdPrim() const { return usdPrim; }

void MaxUsdShaderWriter::SetUsdPrim(const UsdPrim& usdPrim) { this->usdPrim = usdPrim; }

Mtl* MaxUsdShaderWriter::GetMaterial() const { return material; }

const std::string& MaxUsdShaderWriter::GetFilename() const { return writeJobCtx.GetFilename(); }

const bool MaxUsdShaderWriter::IsUSDZFile() const { return writeJobCtx.IsUSDZFile(); }

const MaxUsd::USDSceneBuilderOptions& MaxUsdShaderWriter::GetExportArgs() const
{
    return writeJobCtx.GetArgs();
}

const std::map<Mtl*, SdfPath>& MaxUsdShaderWriter::GetMaterialsToPrimsMap() const
{
    return writeJobCtx.GetMaterialsToPrimsMap();
}
PXR_NAMESPACE_CLOSE_SCOPE