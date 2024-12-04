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
#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/imaging/hd/material.h>

#include <MaxUsd.h>
#include <materials/Mtl.h>

namespace MAXUSD_NS_DEF {
namespace MaterialUtils {

/// The names of all map type for UsdPreviewSurface materials.
MaxUSDAPI extern const pxr::TfTokenVector USDPREVIEWSURFACE_MAPS;
/// Maps supported in the standard viewport, only the diffuse color.
MaxUSDAPI extern const pxr::TfTokenVector USDPREVIEWSURFACE_STD_VP_MAPS;

/**
 * \brief Create subsetName for a given mtl and material id
 * \param mtl Pointer to the material for which to generate a name
 * \param materialIndex id of the material on the mesh for multi material
 **/
MaxUSDAPI std::string CreateSubsetName(Mtl* mtl, MtlID materialIndex);

/**
 * \brief Returns the name of the primvar used by the given UsdUVTexture
 * \param usdUvTextureNode The UsdUVTexture for which to find the primvar.
 * \param materialNetwork The material network, which the node is part of.
 * \param pathToNode A map of paths to material nodes. All nodes in the material network are expected to be in this map.
 * \return The primvar that ends up being used by this UsdUVTexture, or empty if not found.
 */
MaxUSDAPI pxr::TfToken GetUsdUVTexturePrimvar(
    const pxr::HdMaterialNode&                                                   usdUvTextureNode,
    const pxr::HdMaterialNetwork&                                                materialNetwork,
    const pxr::TfHashMap<pxr::SdfPath, pxr::HdMaterialNode, pxr::SdfPath::Hash>& pathToNode);

/**
 * \brief Helper function to generate a XML string about nodes, relationships and primvars in the
 * specified material network.
 * \param materialNetwork The material network to convert to XML.
 * \param includeParams Whether or not to serialized the material's parameters.
 * \return And XML string representing the material network.
 */
MaxUSDAPI std::string
          ToXML(const pxr::HdMaterialNetwork& materialNetwork, bool includeParams = true);

} // namespace MaterialUtils
} // namespace MAXUSD_NS_DEF
