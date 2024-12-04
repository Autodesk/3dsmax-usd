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
#pragma once

#include "ShadingModeExporter.h"
#include "ShadingModeImporter.h"

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>
#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/singleton.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/weakBase.h>
#include <pxr/pxr.h>

#include <string>

class Mtl;

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
// ShadingMode - shading schema to use
//         none - export/import no shading data to the USD
//  useRegistry - registry based to export/import the 3ds Max materials to/from an equivalent UsdShade network
// additional ShadingMode types can be added thru the use of the RegisterExporter/Importer method
#define PXR_MAXUSD_SHADINGMODE_TOKENS \
	(none) \
	(useRegistry)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(MaxUsdShadingModeTokens, MaxUSDAPI, PXR_MAXUSD_SHADINGMODE_TOKENS);

// clang-format off
// ShadingConversion - preferred 3ds Max material conversion type
//                  none - import to no specific 3ds Max material (default)
//  maxUsdPreviewSurface - import to MaxUsdPreviewSurface
//         pbrMetalRough - import to PBRMetalRough
//      physicalMaterial - import to PhysicalMaterial
#define PXR_MAXUSD_SHADINGCONVERSION_TOKENS \
	(none) \
	(maxUsdPreviewSurface) \
	(pbrMetalRough) \
	(physicalMaterial)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(
    MaxUsdPreferredMaterialTokens,
    MaxUSDAPI,
    PXR_MAXUSD_SHADINGCONVERSION_TOKENS);

TF_DECLARE_WEAK_PTRS(MaxUsdShadingModeRegistry);

/// This class provide macros that are entry points into the shading import/export
/// logic.
///
/// We understand that shading may want to be imported/exported in many ways
/// across studios.  Even within a studio, different workflows may call for
/// different shading modes.

/// The useRegistry exporters and importers can be specialized to support material conversions.
/// The most well known is the default conversion to UsdPreviewSurface shaders. This registry
/// allows introducing other material conversions as necessary to support other renderers. We
/// also allow specifying that an import path is available for these renderers if support has
/// been implemented.
///
/// To register a material conversion on export, you need to use the
/// REGISTER_SHADING_MODE_EXPORT_MATERIAL_CONVERSION macro for each material conversion
/// supported by the library. Multiple registration is supported, so each plugin should declare
/// once the material conversions it supports.
///
/// In order for the core system to discover the plugin, you need a \c plugInfo.json that
/// declares the plugin exposes shading modes:
/// \code
/// {
///   "Plugins": [
///     {
///       "Info": {
///         "MaxUsd": {
///          "ShadingModePlugin" : {}
///         }
///       },
///       "Name": "myUsdPlugin",
///       "LibraryPath": "../myUsdPlugin.dll",
///       "Type": "library"
///     }
///   ]
/// }
/// \endcode
///
/// The plugin at LibraryPath will be loaded via the regular USD plugin loading mechanism.
class MaxUsdShadingModeRegistry : public TfWeakBase
{
public:
    static MaxUsdShadingModeExporterCreator GetExporter(const TfToken& name)
    {
        return GetInstance()._GetExporter(name);
    }
    static MaxUsdShadingModeImporter GetImporter(const TfToken& name)
    {
        return GetInstance()._GetImporter(name);
    }
    static TfTokenVector ListExporters() { return GetInstance()._ListExporters(); }
    static TfTokenVector ListImporters() { return GetInstance()._ListImporters(); }

    /// Gets the nice name of an exporter.
    static const std::string& GetExporterNiceName(const TfToken& name)
    {
        return GetInstance()._GetExporterNiceName(name);
    }

    /// Gets the nice name of an importer.
    static const std::string& GetImporterNiceName(const TfToken& name)
    {
        return GetInstance()._GetImporterNiceName(name);
    }

    /// Gets the description of an exporter.
    static const std::string& GetExporterDescription(const TfToken& name)
    {
        return GetInstance()._GetExporterDescription(name);
    }
    /// Gets the description of an importer.
    static const std::string& GetImporterDescription(const TfToken& name)
    {
        return GetInstance()._GetImporterDescription(name);
    }

    MaxUSDAPI static MaxUsdShadingModeRegistry& GetInstance();

    MaxUSDAPI bool RegisterExporter(
        const std::string&               name,
        std::string                      niceName,
        std::string                      description,
        MaxUsdShadingModeExporterCreator fn);

    MaxUSDAPI bool RegisterImporter(
        const std::string&        name,
        std::string               niceName,
        std::string               description,
        MaxUsdShadingModeImporter fn);

    /// Get all registered export conversions:
    static TfTokenVector ListMaterialConversions()
    {
        return GetInstance()._ListMaterialConversions();
    }

    /// All the information registered for a specific material conversion.
    struct ConversionInfo
    {
        TfToken renderContext;
        TfToken niceName;
        TfToken exportDescription;
        TfToken importDescription;
        bool    hasExporter = false;
        bool    hasImporter = false;

        ConversionInfo() = default;
        ConversionInfo(TfToken rc, TfToken nn, TfToken ed, TfToken id, bool he, bool hi)
            : renderContext(rc)
            , niceName(nn)
            , exportDescription(ed)
            , importDescription(id)
            , hasExporter(he)
            , hasImporter(hi)
        {
        }
    };

    /// Gets the conversion information associated with \p materialConversion on export and import
    static const ConversionInfo& GetMaterialConversionInfo(const TfToken& materialConversion)
    {
        return GetInstance()._GetMaterialConversionInfo(materialConversion);
    }

    /// Registers an export material conversion with render context, nice name, and description.
    ///
    /// The materialConversion name gets used directly in the render option string as one of
    /// the "Materials export to" options of the USD export dialog.
    ///
    /// The renderContext gets used to specialize the binding point. See UsdShadeMaterial
    /// documentation for details. Use a value of "UsdShadeTokens->universalRenderContext" if the
    /// resulting UsdShade nodes are written using an API shared by multiple renderers, like
    /// UsdPreviewSurface. For UsdShade nodes targeting a specific rendering engine, please define a
    /// custom render context
    ///  understood by the renderer.
    ///
    /// The niceName is the name displayed in the "Materials export to" option of the USD export
    /// dialog
    ///
    /// The description gets displayed as a tooltip in the "Materials export to" option of the USD
    /// export dialog.
    MaxUSDAPI void RegisterExportConversion(
        const TfToken& materialConversion,
        const TfToken& renderContext,
        const TfToken& niceName,
        const TfToken& description);

    /// Registers an import material conversion, with render context, nice name and description.
    /// This is the import counterpart of the RegisterExportConversion to be used if importers are
    /// available for a specific materialConversion. This covers only the "where to look in USD"
    /// part of material import. Extra conversion to a specified 3ds Max material requires setting
    /// the optional preferredMaterial import option.
    ///
    /// The materialConversion name will be used directly in the import option string as one of
    /// the valid values of the second parameter to the shadingMode list to search on import.
    ///
    /// The renderContext will be used to locate the specialized binding point in the USD data.
    /// See UsdShadeMaterial documentation for details.
    ///
    /// The niceName is the name to be displayed in the import options dialog.
    ///
    /// The description is displayed as a tooltip in the import options dialog.
    MaxUSDAPI void RegisterImportConversion(
        const TfToken& materialConversion,
        const TfToken& renderContext,
        const TfToken& niceName,
        const TfToken& description);

private:
    MaxUSDAPI MaxUsdShadingModeExporterCreator _GetExporter(const TfToken& name);
    MaxUSDAPI const std::string& _GetExporterNiceName(const TfToken&);
    MaxUSDAPI const std::string& _GetExporterDescription(const TfToken&);

    MaxUSDAPI MaxUsdShadingModeImporter _GetImporter(const TfToken& name);
    MaxUSDAPI const std::string& _GetImporterNiceName(const TfToken&);
    MaxUSDAPI const std::string& _GetImporterDescription(const TfToken&);

    MaxUSDAPI TfTokenVector _ListExporters();
    MaxUSDAPI TfTokenVector _ListImporters();

    MaxUSDAPI TfTokenVector         _ListMaterialConversions();
    MaxUSDAPI const ConversionInfo& _GetMaterialConversionInfo(const TfToken&);

    MaxUsdShadingModeRegistry();
    ~MaxUsdShadingModeRegistry();
    friend class TfSingleton<MaxUsdShadingModeRegistry>;
};

#define DEFINE_SHADING_MODE_IMPORTER(name, niceName, description, contextName)   \
    static Mtl* _ShadingModeImporter_##name(                                     \
        MaxUsdShadingModeImportContext*, const MaxUsd::MaxSceneBuilderOptions&); \
    TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdShadingModeImportContext, name)          \
    {                                                                            \
        MaxUsdShadingModeRegistry::GetInstance().RegisterImporter(               \
            #name, niceName, description, &_ShadingModeImporter_##name);         \
    }                                                                            \
    Mtl* _ShadingModeImporter_##name(                                            \
        MaxUsdShadingModeImportContext* contextName, const MaxSceneBuilderOptions&)

#define DEFINE_SHADING_MODE_IMPORTER_WITH_JOB_ARGUMENTS(                         \
    name, niceName, description, contextName, jobArgumentsName)                  \
    static Mtl* _ShadingModeImporter_##name(                                     \
        MaxUsdShadingModeImportContext*, const MaxUsd::MaxSceneBuilderOptions&); \
    TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdShadingModeImportContext, name)          \
    {                                                                            \
        MaxUsdShadingModeRegistry::GetInstance().RegisterImporter(               \
            #name, niceName, description, &_ShadingModeImporter_##name);         \
    }                                                                            \
    Mtl* _ShadingModeImporter_##name(                                            \
        MaxUsdShadingModeImportContext*       contextName,                       \
        const MaxUsd::MaxSceneBuilderOptions& jobArgumentsName)

#define REGISTER_SHADING_MODE_EXPORT_MATERIAL_CONVERSION(                  \
    name, renderContext, niceName, description)                            \
    TF_REGISTRY_FUNCTION(MaxUsdShadingModeExportContext)                   \
    {                                                                      \
        MaxUsdShadingModeRegistry::GetInstance().RegisterExportConversion( \
            name, renderContext, niceName, description);                   \
    }

#define REGISTER_SHADING_MODE_IMPORT_MATERIAL_CONVERSION(                  \
    name, renderContext, niceName, description)                            \
    TF_REGISTRY_FUNCTION(MaxUsdShadingModeImportContext)                   \
    {                                                                      \
        MaxUsdShadingModeRegistry::GetInstance().RegisterImportConversion( \
            name, renderContext, niceName, description);                   \
    }

PXR_NAMESPACE_CLOSE_SCOPE
