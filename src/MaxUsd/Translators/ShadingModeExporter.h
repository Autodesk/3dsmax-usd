//
// Copyright 2016 Pixar
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
#pragma once

#include "ShadingModeExporterContext.h"
#include "WriteJobContext.h"

#include <MaxUsd/MaxUSDAPI.h>
#include <MaxUsd/Utilities/MaxProgressBar.h>

#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usdShade/material.h>

#include <functional>
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class MaxUsdShadingModeExporter
{
public:
    MaxUSDAPI MaxUsdShadingModeExporter();
    MaxUSDAPI virtual ~MaxUsdShadingModeExporter();

    MaxUSDAPI void DoExport(
        MaxUsdWriteJobContext&                                  writeJobContexts,
        const pxr::TfHashSet<pxr::SdfPath, pxr::SdfPath::Hash>& primsToMaterialBind,
        MaxUsd::MaxProgressBar&                                 progress);

    /// Called once, before any exports are started.
    MaxUSDAPI virtual void PreExport(MaxUsdShadingModeExportContext* /* context */) { };

    /// Called inside of a loop, per-material
    MaxUSDAPI virtual void Export(
        MaxUsdShadingModeExportContext& context,
        UsdShadeMaterial* const         mat,
        SdfPathSet* const               boundPrimPaths,
        const pxr::SdfPath&             targetPath = {})
        = 0;

    /// Called once, after Export is called for all shading engines.
    MaxUSDAPI virtual void PostExport(const MaxUsdShadingModeExportContext& context);
};

using MaxUsdShadingModeExporterPtr = std::shared_ptr<MaxUsdShadingModeExporter>;
using MaxUsdShadingModeExporterCreator
    = std::function<std::shared_ptr<MaxUsdShadingModeExporter>()>;

PXR_NAMESPACE_CLOSE_SCOPE
