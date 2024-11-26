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
#include "ShadingModeExporter.h"

#include "ShadingModeExporterContext.h"
#include "ShadingUtils.h"
#include "WriteJobContext.h"

#include <MaxUsd/DLLEntry.h>
#include <MaxUsd/resource.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/collectionAPI.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdUtils/authoring.h>

#include <deque>
#include <stack>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

MaxUsdShadingModeExporter::MaxUsdShadingModeExporter() { }

/* virtual */
MaxUsdShadingModeExporter::~MaxUsdShadingModeExporter() { }

void MaxUsdShadingModeExporter::DoExport(
    MaxUsdWriteJobContext&                                  writeJobContext,
    const pxr::TfHashSet<pxr::SdfPath, pxr::SdfPath::Hash>& primsToMaterialBind,
    MaxUsd::MaxProgressBar&                                 progress)
{
    const MaxUsd::USDSceneBuilderOptions& exportArgs = writeJobContext.GetArgs();
    const UsdStageRefPtr&                 stage = writeJobContext.GetUsdStage();

    MaxUsdShadingModeExportContext context(writeJobContext);

    PreExport(&context);

    const auto materialBindings
        = MaxUsdShadingUtils::FetchMaterials(writeJobContext, primsToMaterialBind);
    writeJobContext.SetMaterialBindings(materialBindings);
    if (materialBindings.empty()) {
        return;
    }

    std::stack<MaterialBinding> materialToExportStack(
        std::deque<MaterialBinding>(materialBindings.rbegin(), materialBindings.rend()));
    auto materialExportedOrToExport = [&materialToExportStack, &writeJobContext](Mtl* mat) {
        // was it already exported
        if (writeJobContext.GetMaterialsToPrimsMap().find(mat)
            != writeJobContext.GetMaterialsToPrimsMap().end()) {
            return true;
        }
        // is it the stack to export
        auto stackcopy = materialToExportStack;
        while (!stackcopy.empty()) {
            if (stackcopy.top().GetMaterial() == mat) {
                return true;
            }
            stackcopy.pop();
        }
        return false;
    };

    auto nbMaterials = materialBindings.size();
    progress.SetTotal(materialBindings.size());
    progress.UpdateProgress(0, true, GetString(IDS_EXPORT_MATERIALS_PROGRESS_MESSAGE));
    size_t progressCount = 0;

    if (nbMaterials > 0) {
        pxr::SdfPath rootPath(exportArgs.GetRootPrimPath());
        pxr::UsdGeomScope::Define(stage, rootPath.AppendPath(exportArgs.GetMaterialPrimPath()));
    }

    // iterate over the exported 3ds Max nodes with materials
    while (!materialToExportStack.empty()) {
        auto matIter = materialToExportStack.top();
        context.SetMaterialAndBindings(matIter.GetMaterial(), &matIter.GetBindings());

        UsdShadeMaterial mat;
        SdfPathSet       boundPrimPaths;
        // TODO - revisit the use of 'boundPrimPath' for material bindings in relation to material
        // collections.
        Export(context, &mat, &boundPrimPaths);

        // unstack exported material
        materialToExportStack.pop();

        // push any supplemental material
        auto additionalMaterials = context.GetAdditionalMaterials();
        if (!additionalMaterials.empty()) {
            std::for_each(
                additionalMaterials.rbegin(),
                additionalMaterials.rend(),
                [&materialToExportStack, &materialExportedOrToExport](Mtl* mat) {
                    if (!materialExportedOrToExport(mat)) {
                        materialToExportStack.push({ mat, {} });
                    }
                });
            progress.SetTotal((nbMaterials += additionalMaterials.size()));
        }

        progress.UpdateProgress(
            ++progressCount, true, GetString(IDS_EXPORT_MATERIALS_PROGRESS_MESSAGE));
    }

    context.SetMaterialAndBindings(nullptr, nullptr);

    PostExport(context);
}

void MaxUsdShadingModeExporter::PostExport(const MaxUsdShadingModeExportContext& context)
{
    for (auto writer : context.GetShaderWriters()) {
        writer->PostWrite();
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
