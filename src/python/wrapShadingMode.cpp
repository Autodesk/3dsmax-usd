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
#include <MaxUsd/Translators/ShadingModeRegistry.h>

#include <boost/python/class.hpp>
#include <boost/python/def.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

class ShadingModeRegistry
{
public:
    static void RegisterImportConversion(
        const TfToken& materialConversion,
        const TfToken& renderContext,
        const TfToken& niceName,
        const TfToken& description)
    {
        MaxUsdShadingModeRegistry::GetInstance().RegisterImportConversion(
            materialConversion, renderContext, niceName, description);
    }
    static void RegisterExportConversion(
        const TfToken& materialConversion,
        const TfToken& renderContext,
        const TfToken& niceName,
        const TfToken& description)
    {
        MaxUsdShadingModeRegistry::GetInstance().RegisterExportConversion(
            materialConversion, renderContext, niceName, description);
    }
};

//----------------------------------------------------------------------------------------------------------------------
void wrapShadingMode()
{
    boost::python::class_<ShadingModeRegistry>(
        "ShadingModeRegistry",
        "The only element exposed to Python from the shading mode registry is the ability"
        "to register conversion type (or material target).",
        boost::python::no_init)
        .def(
            "RegisterImportConversion",
            &ShadingModeRegistry::RegisterImportConversion,
            (boost::python::args(
                "material_conversion", "render_context", "niceName", "description")),
            "Registers an import material conversion with render context, nice name, and "
            "description. \n"
            "The materialConversion name gets used directly in the render option string as one of\n"
            "the \"Materials import to\" options of the USD export dialog.\n\n"
            "The renderContext gets used to specialize the binding point. See UsdShadeMaterial\n"
            "documentation for details. Use a value of \"UsdShadeTokens->universalRenderContext\" "
            "if\n"
            "the resulting UsdShade nodes are written using an API shared by multiple renderers,\n"
            "like UsdPreviewSurface. For UsdShade nodes targeting a specific rendering engine, "
            "please\n"
            "define a custom render context understood by the renderer.\n\n"
            "The niceName is the name displayed in the \"Materials import to\" option of the USD "
            "import dialog.\n"
            "The description gets displayed as a tooltip in the \"Materials import to\" option of "
            "the USD import dialog.")
        .staticmethod("RegisterImportConversion")

        .def(
            "RegisterExportConversion",
            &ShadingModeRegistry::RegisterExportConversion,
            (boost::python::args(
                "material_conversion", "render_context", "niceName", "description")),
            "Registers an export material conversion with render context, nice name, and "
            "description. \n"
            "The materialConversion name gets used directly in the render option string as one of\n"
            "the \"Materials export to\" options of the USD export dialog.\n\n"
            "The renderContext gets used to specialize the binding point. See UsdShadeMaterial\n"
            "documentation for details. Use a value of \"UsdShadeTokens->universalRenderContext\" "
            "if\n"
            "the resulting UsdShade nodes are written using an API shared by multiple renderers,\n"
            "like UsdPreviewSurface. For UsdShade nodes targeting a specific rendering engine, "
            "please\n"
            "define a custom render context understood by the renderer.\n\n"
            "The niceName is the name displayed in the \"Materials export to\" option of the USD "
            "export dialog.\n"
            "The description gets displayed as a tooltip in the \"Materials export to\" option of "
            "the USD export dialog.")
        .staticmethod("RegisterExportConversion");
}
