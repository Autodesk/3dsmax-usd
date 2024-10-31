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
#include "PrimReader.h"
#include "ShaderReader.h"
#include "ShaderReaderRegistry.h"
#include "ShaderWriter.h"
#include "ShaderWriterRegistry.h"
#include "ShadingModeExporter.h"
#include "ShadingModeExporterContext.h"
#include "ShadingModeImporter.h"
#include "ShadingModeRegistry.h"
#include "ShadingUtils.h"

#include <MaxUsd/USDCore.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/tokens.h>

#include <ICustAttribContainer.h>
#include <custattrib.h>
#include <memory>
#include <stdmat.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    ((ArgName, "useRegistry"))
    ((NiceName, "Use Registry"))
    ((ExportDescription,
        "Use a registry based mechanism, complemented with material conversions,"
         " to export to a UsdShade network"))
    ((ImportDescription,
         "Use a registry based mechanism, complemented with material conversions,"
         " to import from a UsdShade network"))
);
// clang-format on

namespace UsdCustAttributes {
// Enum that match the custom attribute paramblock params
enum
{
    PathMethod = 0,
    PrimPath,
    SeparateLayer,
    FilePath,
};

// Enum that match the values of the pathMethod param
enum
{
    // Respect the export options
    RespectOptions = 1,
    // This Custom Attribute will overwrite some export options
    OverwriteOptions,
};
} // namespace UsdCustAttributes

class UseRegistryShadingModeExporter : public MaxUsdShadingModeExporter
{
public:
    UseRegistryShadingModeExporter() { }

private:
    /// Gets the exported ShadeNode associated with the \p depNode that was written under
    /// the path \p parentPath. If no such node exists, then one is created and written.
    ///
    /// If no shader writer can be found for the 3ds Max material or if the node
    /// otherwise should not be authored, an empty pointer is returned.
    MaxUsdShaderWriterSharedPtr
    _GetExportedShaderForNode(const SdfPath& parentPath, MaxUsdShadingModeExportContext& context)
    {
        Mtl* material = context.GetMaterial();

        const TfToken shaderUsdPrimName(
            TfMakeValidIdentifier(MaxUsd::MaxStringToUsdString(material->GetName())));

        const SdfPath shaderUsdPath = parentPath.AppendChild(shaderUsdPrimName);

        MaxUsdShaderWriterRegistry::WriterFactoryFn shaderWriterFactory
            = MaxUsdShaderWriterRegistry::Find(material->ClassID(), context.GetExportArgs());
        if (!shaderWriterFactory) {
            return nullptr;
        }

        MaxUsdShaderWriterSharedPtr shaderWriter
            = shaderWriterFactory(material, shaderUsdPath, context.GetWriteJobContext());
        if (!shaderWriter) {
            return nullptr;
        }

        shaderWriter->Write();

        // context add possible material dependencies
        if (shaderWriter->HasMaterialDependencies()) {
            std::vector<Mtl*> additionalMtls;
            shaderWriter->GetSubMtlDependencies(additionalMtls);
            context.AdditionalMaterials(additionalMtls);
        }

        return shaderWriter;
    }

    pxr::SdfPath materialTargetPath(
        const MaxUsdShadingModeExportContext& context,
        UsdShadeMaterial* const               mat,
        UsdEditTarget&                        editTarget)
    {
        // Build the material's path
        const MaxUsd::USDSceneBuilderOptions& buildOptions = context.GetExportArgs();
        SdfPath                               materialsRoot;
        Mtl*                                  maxMat = context.GetMaterial();
        std::string                           shaderName
            = TfMakeValidIdentifier(MaxUsd::MaxStringToUsdString(maxMat->GetName()));
        fs::path exportFolder(context.GetWriteJobContext().GetFilename());
        exportFolder = exportFolder.remove_filename();
        bool     foundCALayer = false;
        SdfPath  caPrimPath;
        Interval valid = FOREVER;
        if (auto custAttrContainer = maxMat->GetCustAttribContainer()) {
            bool        foundCA = false;
            CustAttrib* custAttr = nullptr;
            for (int i = 0; i < custAttrContainer->GetNumCustAttribs(); i++) {
                custAttr = custAttrContainer->GetCustAttrib(i);
                if (wcscmp(custAttr->GetName(false), L"UsdMaterialAttributeHolder") == 0) {
                    foundCA = true;
                    break;
                }
            }
            // Material has the USD custom attribute
            if (foundCA) {
                IParamBlock2* pb = custAttr->GetParamBlockByID(0);
                int           exportToVal = 0;
                pb->GetValue(UsdCustAttributes::PathMethod, 0, exportToVal, valid);
                // This material's CA overwrite some of the default export options.
                if (exportToVal == UsdCustAttributes::OverwriteOptions) {
                    const MCHAR* mxPrimPath = L"";
                    pb->GetValue(UsdCustAttributes::PrimPath, 0, mxPrimPath, valid);
                    auto primPathStr = MaxUsd::MaxStringToUsdString(mxPrimPath);
                    if (SdfPath::IsValidPathString(primPathStr)) {
                        caPrimPath = SdfPath(primPathStr);
                        // Not an absolute path, append it to the root prim path
                        if (!caPrimPath.IsAbsolutePath()) {
                            caPrimPath
                                = context.GetExportArgs().GetRootPrimPath().AppendPath(caPrimPath);
                        }
                    } else {
                        MaxUsd::Log::Warn(
                            "Invalid Scope path for : {}, will be set to '/mtl' for this export",
                            shaderName);
                        caPrimPath = SdfPath("/mtl");
                    }
                    materialsRoot = caPrimPath;

                    int separateLayer = 0;
                    pb->GetValue(UsdCustAttributes::SeparateLayer, 0, separateLayer, valid);
                    // This Material is targeting a specific layer.
                    if (separateLayer) {
                        foundCALayer = true;
                        const MCHAR* filePath = L"";
                        pb->GetValue(UsdCustAttributes::FilePath, 0, filePath, valid);
                        auto usdFilePath
                            = USDCore::sanitizedFilename(filePath, _T(".usda")).string();
                        auto ext = pxr::SdfFileFormat::FindByExtension(usdFilePath);

                        // Here we need to check if this layer is already used in the export so that
                        // multiple materials can write to it. We need to do this so that we don't
                        // overwrite the layer with each material that uses it.
                        auto layerMap = context.GetWriteJobContext().GetLayerMap();
                        bool layerAlreadyUsedInExport
                            = layerMap.find(usdFilePath) != layerMap.end();
                        SdfLayerRefPtr matLayer = nullptr;
                        // If this layer didn't exist create it.
                        if (!layerAlreadyUsedInExport) {
                            MaxUsd::Log::Info(
                                "Material Layer created in memory targeting path : {}",
                                usdFilePath);
                            matLayer = MaxUsd::CreateOrOverwriteLayer(ext, usdFilePath);
                            if (matLayer == nullptr) {
                                MaxUsd::Log::Error(
                                    "Material Layer for {} failed to be created", shaderName);
                                return SdfPath::EmptyPath();
                            }
                            editTarget = UsdEditTarget(matLayer);
                            context.GetWriteJobContext().AddUsedLayerIdentifier(
                                usdFilePath, matLayer);
                        } else {
                            matLayer = layerMap[usdFilePath];
                            editTarget = UsdEditTarget(matLayer);
                        }

                        // Sublayer it into the stage's root layer.
                        context.GetUsdStage()->GetRootLayer()->InsertSubLayerPath(
                            matLayer->GetIdentifier());
                    }
                }
            }
        }
        // No separate layer in the exporter options or CA
        if (!buildOptions.GetUseSeparateMaterialLayer() && !foundCALayer) {
            auto primPath = caPrimPath.IsEmpty() ? buildOptions.GetMaterialPrimPath() : caPrimPath;
            if (primPath.IsAbsolutePath()) {
                materialsRoot = primPath;
            } else {
                materialsRoot = buildOptions.GetRootPrimPath();
                materialsRoot = materialsRoot.AppendPath(primPath);
            }
        }
        // Material Layer was specified in the exporter options
        else if (!foundCALayer) {
            auto usdFilePath
                = USDCore::sanitizedFilename(buildOptions.GetMaterialLayerPath(), ".usda");
            auto usdFilePathStr = usdFilePath.string();
            auto layerMap = context.GetWriteJobContext().GetLayerMap();
            auto matLayer = layerMap[usdFilePathStr];

            if (matLayer == nullptr) {
                MaxUsd::Log::Error(
                    "Material Layer {0} for {1} failed to be found", usdFilePathStr, shaderName);
                return SdfPath::EmptyPath();
            }

            // Sublayer it into the stage's root layer.
            context.GetUsdStage()->GetRootLayer()->InsertSubLayerPath(matLayer->GetIdentifier());
            editTarget = UsdEditTarget(matLayer);
            if (!caPrimPath.IsEmpty()) {
                materialsRoot = caPrimPath;
            } else {
                if (!buildOptions.GetMaterialPrimPath().IsAbsolutePath()) {
                    materialsRoot = context.GetExportArgs().GetRootPrimPath().AppendPath(
                        buildOptions.GetMaterialPrimPath());
                } else {
                    materialsRoot = buildOptions.GetMaterialPrimPath();
                }
            }
        }
        return MaxUsd::MakeUniquePrimPath(
            context.GetUsdStage(), materialsRoot, TfToken(shaderName));
    }

    void Export(
        MaxUsdShadingModeExportContext& context,
        UsdShadeMaterial* const         mat,
        SdfPathSet* const               boundPrimPaths,
        const pxr::SdfPath&             targetPath) override
    {
        UsdPrim materialPrim;
        auto    materialPath = SdfPath::EmptyPath();
        auto    editTarget = UsdEditTarget(context.GetUsdStage()->GetRootLayer());

        if (targetPath.IsEmpty()) {
            materialPath = materialTargetPath(context, mat, editTarget);
        }

        pxr::UsdEditContext editContext(context.GetUsdStage(), editTarget);
        materialPrim
            = context.MakeStandardMaterialPrim(targetPath.IsEmpty() ? materialPath : targetPath);

        UsdShadeMaterial material(materialPrim);
        if (!material) {
            return;
        }

        auto cleanUpNodeGraph
            = [](const MaxUsdShadingModeExportContext& context, const SdfPath& materialExportPath) {
                  UsdPrim nodeGraphPrim(context.GetUsdStage()->GetPrimAtPath(materialExportPath));

                  if (nodeGraphPrim.GetAllChildren().empty()) {
                      context.GetUsdStage()->RemovePrim(materialExportPath);
                  }
              };

        if (mat != nullptr) {
            *mat = material;
        }

        auto       materialTargets = context.GetExportArgs().GetAllMaterialConversions();
        const auto agnosticMaterials = MaxUsdShaderWriterRegistry::GetAllTargetAgnosticMaterials();
        const bool targetIndifferent = std::find(
                                           agnosticMaterials.begin(),
                                           agnosticMaterials.end(),
                                           context.GetMaterial()->ClassID())
            != agnosticMaterials.end();

        // If the material is target agnostic, we only want to export it once, overwrite the
        // materialTargets.
        if (targetIndifferent && !materialTargets.empty()) {
            materialTargets = { TfToken("Agnostic") };
        }

        for (const TfToken& currentMaterialConversion : materialTargets) {

            MaxUsd::USDSceneBuilderOptions& currentIteration
                = const_cast<MaxUsd::USDSceneBuilderOptions&>(context.GetExportArgs());
            currentIteration.SetConvertMaterialsTo(currentMaterialConversion);

            const TfToken& renderContext
                = MaxUsdShadingModeRegistry::GetMaterialConversionInfo(currentMaterialConversion)
                      .renderContext;
            SdfPath materialExportPath = materialPrim.GetPath();

            if (materialTargets.size() > 1) {
                // Write each material in its own scope
                materialExportPath = materialExportPath.AppendChild(currentMaterialConversion);

                // This path needs to be a NodeGraph:
                UsdShadeNodeGraph::Define(context.GetUsdStage(), materialExportPath);
            }

            auto shaderInfo = _GetExportedShaderForNode(materialExportPath, context);
            if (!shaderInfo) {
                // Clean-up nodegraph if nothing was exported:
                if (materialTargets.size() > 1) {
                    cleanUpNodeGraph(context, materialExportPath);
                }
                continue;
            }
            context.AddShaderWriter(shaderInfo);

            UsdPrim        shaderPrim = shaderInfo->GetUsdPrim();
            UsdShadeShader shadeShader = UsdShadeShader(shaderPrim);

            MaxUsdShadingUtils::CreateShaderOutputAndConnectMaterial(
                shadeShader, material, UsdShadeTokens->surface, renderContext);

            // Clean-up nodegraph if nothing was exported:
            if (materialTargets.size() > 1) {
                cleanUpNodeGraph(context, materialExportPath);
            }
        }

        // If we did not actually export any shaders, cleanup after ourselves.
        if (materialPrim.GetAllChildren().empty() && !materialPrim.HasAuthoredReferences()
            && !materialPrim.HasVariantSets()) {
            context.GetUsdStage()->RemovePrim(materialPrim.GetPath());
        }
        // Otherwise, bind the material to the prims.
        else {
            // We want to have the material binding in the layer where the geometry is exported.
            pxr::UsdEditContext materialBindingEditContext(
                context.GetUsdStage(), context.GetUsdStage()->GetRootLayer());
            context.GetWriteJobContext().AddExportedMaterial(
                context.GetMaterial(), materialPrim.GetPath());
            context.BindStandardMaterialPrim(materialPrim);
        }
    }
};

} // anonymous namespace

TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdShadingModeExportContext, useRegistry)
{
    MaxUsdShadingModeRegistry::GetInstance().RegisterExporter(
        _tokens->ArgName.GetString(),
        _tokens->NiceName.GetString(),
        _tokens->ExportDescription.GetString(),
        []() -> MaxUsdShadingModeExporterPtr {
            return MaxUsdShadingModeExporterPtr(
                static_cast<MaxUsdShadingModeExporter*>(new UseRegistryShadingModeExporter()));
        });
}

namespace {

/// This class implements a shading mode importer which uses a registry keyed by the info:id USD
/// attribute to provide an importer class for each UsdShade node processed while traversing the
/// main connections of a UsdMaterial node.
class UseRegistryShadingModeImporter
{
public:
    UseRegistryShadingModeImporter(
        MaxUsdShadingModeImportContext*       context,
        const MaxUsd::MaxSceneBuilderOptions& jobArguments)
        : context(context)
        , jobArguments(jobArguments)
    {
    }

    /// Main entry point of the import process. On input we get a UsdMaterial which gets traversed
    /// in order to build a 3ds Max material that reproduces the information found in the USD
    /// shading network.
    Mtl* Read()
    {
        if (jobArguments.GetShadingModes().size() != 1) {
            // The material translator will make sure we only get a single shading mode
            // at a time.
            TF_CODING_ERROR("useRegistry importer can only handle a single shadingMode");
            return nullptr;
        }
        const TfToken& materialConversion = jobArguments.GetMaterialConversion();
        TfToken        renderContext
            = MaxUsdShadingModeRegistry::GetMaterialConversionInfo(materialConversion)
                  .renderContext;

        const UsdShadeMaterial& shadeMaterial = context->GetShadeMaterial();
        if (!shadeMaterial) {
            return nullptr;
        }

        Mtl* material { nullptr };

        // ComputeSurfaceSource will default to the universal render context if
        // renderContext is not found. Therefore we need to test first that the
        // render context output we are looking for really exists:
        if (shadeMaterial.GetSurfaceOutput(renderContext)) {
            UsdShadeShader surfaceShader = shadeMaterial.ComputeSurfaceSource({ renderContext });
            if (surfaceShader) {
                auto surfaceShaderPrimToRead = surfaceShader.GetPrim();
                if (surfaceShader.GetPrim().IsInstanceProxy()) {
                    surfaceShaderPrimToRead = surfaceShader.GetPrim().GetPrimInPrototype();
                }

                Mtl* mat;
                // was the material already imported?
                if (context->GetCreatedMaterial(surfaceShaderPrimToRead, &mat)) {
                    return mat;
                }

                TfToken shaderId;
                surfaceShader.GetIdAttr().Get(&shaderId);
                if (MaxUsdShaderReaderRegistry::ReaderFactoryFn factoryFn
                    = MaxUsdShaderReaderRegistry::Find(shaderId, jobArguments)) {
                    UsdPrim shaderPrim = surfaceShader.GetPrim();
                    auto    shaderReader = std::dynamic_pointer_cast<MaxUsdShaderReader>(
                        factoryFn(shaderPrim, context->GetReadJobContext()));
                    if (shaderReader->Read()) {
                        material
                            = shaderReader->GetCreatedMaterial(*context, surfaceShader.GetPrim());
                    }
                }
            }
        }

        return material;
    }

private:
    MaxUsdShadingModeImportContext*       context = nullptr;
    const MaxUsd::MaxSceneBuilderOptions& jobArguments;
};

}; // anonymous namespace

DEFINE_SHADING_MODE_IMPORTER_WITH_JOB_ARGUMENTS(
    useRegistry,
    _tokens->NiceName.GetString(),
    _tokens->ImportDescription.GetString(),
    context,
    jobArguments)
{
    UseRegistryShadingModeImporter importer(context, jobArguments);
    return importer.Read();
}

PXR_NAMESPACE_CLOSE_SCOPE
