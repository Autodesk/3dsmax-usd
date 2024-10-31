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
#include "MaterialUtils.h"

#include <MaxUsd/MaxTokens.h>

#include <stdmat.h>

namespace MAXUSD_NS_DEF {
namespace MaterialUtils {

// clang-format off
const pxr::TfTokenVector USDPREVIEWSURFACE_MAPS = 
{
	pxr::MaxUsdUsdPreviewSurfaceTokens->diffuseColor,
	pxr::MaxUsdUsdPreviewSurfaceTokens->specularColor,
	pxr::MaxUsdUsdPreviewSurfaceTokens->metallic,
	pxr::MaxUsdUsdPreviewSurfaceTokens->normal,
	pxr::MaxUsdUsdPreviewSurfaceTokens->occlusion,
	pxr::MaxUsdUsdPreviewSurfaceTokens->emissiveColor,
	pxr::MaxUsdUsdPreviewSurfaceTokens->opacity,
	pxr::MaxUsdUsdPreviewSurfaceTokens->displacement,
	pxr::MaxUsdUsdPreviewSurfaceTokens->ior,
	pxr::MaxUsdUsdPreviewSurfaceTokens->clearcoat,
	pxr::MaxUsdUsdPreviewSurfaceTokens->clearcoatRoughness,
	pxr::MaxUsdUsdPreviewSurfaceTokens->roughness
};
// clang-format on

// In the standard viewport, only support the diffuse color.
const pxr::TfTokenVector USDPREVIEWSURFACE_STD_VP_MAPS
    = { pxr::MaxUsdUsdPreviewSurfaceTokens->diffuseColor };

std::string CreateSubsetName(Mtl* mtl, MtlID materialIndex)
{
    std::string name;

    // In the UI and maxscript, material indices start at 1. Use this for naming.
    const auto maxScriptId = materialIndex + 1;

    Mtl*      resolvedMtl = mtl ? mtl->ResolveWrapperMaterials(true) : nullptr;
    MultiMtl* multiMtl = dynamic_cast<MultiMtl*>(resolvedMtl);
    if (resolvedMtl == nullptr || multiMtl == nullptr) {
        // _{materialIndex}_ if the mtl is null or not a multiMtl
        name.append("_").append(std::to_string(maxScriptId)).append("_");
    } else {
        // For multi material try to use the slot name.
        MSTR slotName;
        multiMtl->GetSubMtlName(materialIndex, slotName);
        name = slotName.ToCStr().data();
        if (name.empty()) {
            // If the slot name is empty use _{maxScriptId}_{subMaterialName}
            name.append("_").append(std::to_string(maxScriptId)).append("_");
            Mtl* subMtl = resolvedMtl->GetSubMtl(materialIndex);
            if (subMtl) {
                name.append(subMtl->GetName().ToCStr().data());
            }
        }
    }

    return pxr::TfMakeValidIdentifier(name);
}

pxr::TfToken GetUsdUVTexturePrimvar(
    const pxr::HdMaterialNode&                                                   usdUvTextureNode,
    const pxr::HdMaterialNetwork&                                                materialNetwork,
    const pxr::TfHashMap<pxr::SdfPath, pxr::HdMaterialNode, pxr::SdfPath::Hash>& pathToNode)
{
    auto getInput = [&materialNetwork](
                        const pxr::HdMaterialNode& node, const pxr::TfToken& name) -> pxr::SdfPath {
        for (const auto& rel : materialNetwork.relationships) {
            if (rel.outputName != name) {
                continue;
            }
            if (node.path == rel.outputId) {
                return rel.inputId;
            }
        }
        return pxr::SdfPath {};
    };

    const pxr::SdfPath stInputPath = getInput(usdUvTextureNode, pxr::TfToken("st"));
    if (!stInputPath.IsEmpty()) {
        const auto itSt = pathToNode.find(stInputPath);
        if (itSt == pathToNode.end()) {
            return pxr::TfToken {};
        }

        const auto          stInputNode = itSt->second;
        pxr::HdMaterialNode primvarReaderNode;

        // Case where the primvar reader is connected to the UsdUVTexture directly.
        if (stInputNode.identifier == pxr::TfToken("UsdPrimvarReader_float2")) {
            primvarReaderNode = stInputNode;
        }
        // Case where the primvar reader is connected to a UsdTransform2d, which is then connected
        // to the UsdUVTexture.
        if (stInputNode.identifier == pxr::TfToken("UsdTransform2d")) {
            pxr::SdfPath inInputPath = getInput(stInputNode, pxr::TfToken("in"));

            const auto itIn = pathToNode.find(inInputPath);

            if (itIn == pathToNode.end()) {
                return pxr::TfToken {};
            }
            primvarReaderNode = itIn->second;
        }

        const auto it = primvarReaderNode.parameters.find(pxr::TfToken("varname"));
        if (it != primvarReaderNode.parameters.end()) {
            auto varName = it->second;
            if (varName.CanCast<pxr::TfToken>()) {
                return varName.Cast<pxr::TfToken>().Get<pxr::TfToken>();
            }
        }
    }
    return pxr::TfToken {};
}

std::string ToXML(const pxr::HdMaterialNetwork& materialNetwork, bool includeParams)
{
    PXR_NAMESPACE_USING_DIRECTIVE
    // Implementation below is taken from Maya-usd's vp2RenderDelegate/material.cpp

    std::string result;

    if (ARCH_LIKELY(!materialNetwork.nodes.empty())) {
        // Reserve enough memory to avoid memory reallocation.
        result.reserve(1024);

        result += "<nodes>\n";

        if (includeParams) {
            for (const pxr::HdMaterialNode& node : materialNetwork.nodes) {
                result += "  <node path=\"";
                result += node.path.GetString();
                result += "\" id=\"";
                result += node.identifier;
                result += "\">\n";

                result += "    <params>\n";

                for (auto const& parameter : node.parameters) {
                    result += "      <param name=\"";
                    result += parameter.first;
                    result += "\" value=\"";
                    result += TfStringify(parameter.second);
                    result += "\"/>\n";
                }

                result += "    </params>\n";

                result += "  </node>\n";
            }
        } else {
            for (const pxr::HdMaterialNode& node : materialNetwork.nodes) {
                result += "  <node path=\"";
                result += node.path.GetString();
                result += "\" id=\"";
                result += node.identifier;
                result += "\"/>\n";
            }
        }

        result += "</nodes>\n";

        if (!materialNetwork.relationships.empty()) {
            result += "<relationships>\n";

            for (const pxr::HdMaterialRelationship& rel : materialNetwork.relationships) {
                result += "  <rel from=\"";
                result += rel.inputId.GetString();
                result += ".";
                result += rel.inputName;
                result += "\" to=\"";
                result += rel.outputId.GetString();
                result += ".";
                result += rel.outputName;
                result += "\"/>\n";
            }

            result += "</relationships>\n";
        }

        if (!materialNetwork.primvars.empty()) {
            result += "<primvars>\n";

            for (pxr::TfToken const& primvar : materialNetwork.primvars) {
                result += "  <primvar name=\"";
                result += primvar;
                result += "\"/>\n";
            }

            result += "</primvars>\n";
        }
    }
    return result;
}

} // namespace MaterialUtils
} // namespace MAXUSD_NS_DEF
