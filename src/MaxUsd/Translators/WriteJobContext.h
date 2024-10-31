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

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/MaxUSDAPI.h>

#include <pxr/usd/sdf/path.h>

PXR_NAMESPACE_OPEN_SCOPE

class MaxUsd_MaterialWriteJob;

class MaterialBinding
{
public:
    MaxUSDAPI MaterialBinding(Mtl* material, const std::list<SdfPath>& bindings)
        : material(material)
        , bindings(bindings)
    {
    }
    MaxUSDAPI MaterialBinding(const MaterialBinding& copy)
    {
        material = copy.material;
        bindings = copy.bindings;
    }
    MaxUSDAPI Mtl* GetMaterial() const { return material; }
    MaxUSDAPI std::list<SdfPath>& GetBindings() { return bindings; }

private:
    Mtl*               material;
    std::list<SdfPath> bindings;
};
using MaterialBindings = std::vector<MaterialBinding>;

/// Provides basic functionality and access to shared data for prim and shader writers.
class MaxUsdWriteJobContext
{
public:
    /**
     * \brief Constructor.
     */
    MaxUSDAPI MaxUsdWriteJobContext(
        UsdStageRefPtr                        stage,
        const std::string&                    filename,
        const MaxUsd::USDSceneBuilderOptions& args,
        bool                                  isUSDZ);

    /**
     * \brief Destructor.
     */
    MaxUSDAPI ~MaxUsdWriteJobContext() = default;

    /**
     * \brief Returns the export arguments.
     * \return Export arguments
     */
    MaxUSDAPI const MaxUsd::USDSceneBuilderOptions& GetArgs() const { return args; }

    /**
     * \brief The USD Stage we are inthe process of building.
     * \return The usd
     */
    MaxUSDAPI const UsdStageRefPtr& GetUsdStage() const { return stage; }

    /**
     * \brief Gets the file we are exporting to.
     * \return The file path.
     */
    MaxUSDAPI const std::string& GetFilename() const { return filename; }

    /**
     * \brief Sets the current nodes being exported and their respective paths
     * \param nodesToPrims map with the nodes being exported and their paths
     */
    MaxUSDAPI void SetNodeToPrimMap(const std::map<INode*, SdfPath>& nodesToPrims);

    /**
     * \brief Returns the nodes being exported and their respective prim paths.
     * \return Map with the nodes and their prim path.
     */
    MaxUSDAPI const std::map<INode*, SdfPath>& GetNodesToPrimsMap() const
    {
        return maxNodesToPrims;
    }

    /**
     * \brief Gets the map of exported materials.
     * \return The map of exported materials
     */
    MaxUSDAPI const std::map<Mtl*, SdfPath>& GetMaterialsToPrimsMap() const
    {
        return materialToPrims;
    }

    /**
     * \brief Add the exported path of a material to the map
     */
    MaxUSDAPI void AddExportedMaterial(Mtl* material, const SdfPath& path)
    {
        materialToPrims[material] = path;
    }

    /**
     * \brief Check if the file to be exported is a USDZ file.
     * \return "true" if the exported file should be a USDZ file.
     */
    MaxUSDAPI bool IsUSDZFile() const { return isUSDZ; }

    /**
     * \brief Returns the layers we've discovered while exporting.
     * \return The map matching the layer identifier and their pointer.
     */
    MaxUSDAPI const std::map<std::string, pxr::SdfLayerRefPtr>& GetLayerMap() const
    {
        return usdLayersMap;
    }

    /**
     * \brief Adds a layer to the map of layers we've discovered while exporting.
     * These layers are saved to disk at the end of the export.
     * The map is used to keep track of the layers.
     * \param layerIdentifier The identifier set by the user in the UI.
     * \param layer The layer to add to the map.
     */
    MaxUSDAPI void
    AddUsedLayerIdentifier(const std::string& layerIdentifier, const SdfLayerRefPtr& layer)
    {
        usdLayersMap[layerIdentifier] = layer;
    }

    /**
     * \brief Find and replace known tokens in the input string.
     * \param input String to resolve
     * \return The resolved string
     */
    MaxUSDAPI std::string ResolveString(const std::string& input) const;

    /**
     * \brief Get the materials and which prims they are bound to.
     * This information is available after the export of the geometry only.
     * \return The material bindings
     */
    MaxUSDAPI const MaterialBindings& GetMaterialBindings() const;

    /**
     * \brief Set the MaterialsBindings
     * \param materialBindings
     */
    MaxUSDAPI void SetMaterialBindings(const MaterialBindings& materialBindings);

protected:
    // Args for the export (any & all export options).
    const MaxUsd::USDSceneBuilderOptions& args;

    // Stage used to write out USD file
    UsdStageRefPtr stage;
    // Filename for the USD file
    std::string filename;
    // Max nodes being exported and their prims
    std::map<INode*, SdfPath> maxNodesToPrims;
    // Materials being exported and their exported paths
    std::map<Mtl*, SdfPath> materialToPrims;
    // Whether or not the exported file should be of type USDZ
    bool isUSDZ;
    // The layers discovered while exporting. The key is the identifier for the layer used in the
    // UI.
    std::map<std::string, SdfLayerRefPtr> usdLayersMap;
    // Tokens map, used to replace tokens received the UI.
    std::map<std::string, std::string> tokensMap;
    // The 3ds Max materials and which prims they are bound to.
    MaterialBindings materialBindings;
};

PXR_NAMESPACE_CLOSE_SCOPE
