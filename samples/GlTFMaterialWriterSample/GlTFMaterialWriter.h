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
#include <MaxUsd/Translators/ShaderWriter.h>

#include <pxr/pxr.h>

/// Shader writer for exporting 3ds Max's material shading nodes to USD.
class GlTFMaterialWriter : public pxr::MaxUsdShaderWriter
{
public:
    GlTFMaterialWriter(
        Mtl*                        material,
        const pxr::SdfPath&         usdPath,
        pxr::MaxUsdWriteJobContext& jobCtx);

    static ContextSupport CanExport(const MaxUsd::USDSceneBuilderOptions& exportArgs);
    void                  Write() override;
};