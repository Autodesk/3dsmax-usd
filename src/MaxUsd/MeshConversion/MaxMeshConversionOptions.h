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
#include <MaxUsd/MappedAttributeBuilder.h>
#include <MaxUsd/Utilities/DictionaryOptionProvider.h>

PXR_NAMESPACE_OPEN_SCOPE
// clang-format off
#define MAXUSD_MAXMESH_CONVERSION_OPTIONS_TOKENS \
	(version) \
	(normalMode) \
	(meshFormat) \
	(bakeObjectOffsetTransform) \
	(preserveEdgeOrientation) \
	(channelToPrimvarConfig) \
	(primvarLayoutInference)
// clang-format on
TF_DECLARE_PUBLIC_TOKENS(
    MaxUsdMaxMeshConversionOptions,
    MaxUSDAPI,
    MAXUSD_MAXMESH_CONVERSION_OPTIONS_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

namespace MAXUSD_NS_DEF {

class MaxMeshConversionOptions : public DictionaryOptionProvider
{
public:
    /**
     * \brief Conversion mode for normals. Normals can either be exported as primvars, or as simple attributes.
     * \comment value of enum is important as it reflects the index in the ui of qcombobox
     */
    enum class MaxUSDAPI NormalsMode
    {
        AsPrimvar = 0,
        AsAttribute = 1,
        None = 2
    };

    /**
     * \brief Mesh format to be used when converting to USD. Meshes can either export as Polygonal meshes,
     * Triangular meshes, or use the same format as in the scene (EditMesh -> Triangles, EditPoly ->
     * Polys)
     */
    enum class MaxUSDAPI MeshFormat
    {
        FromScene = 0,
        PolyMesh = 1,
        TriMesh = 2
    };

    /**
     * \brief Defines if layout inference should be done when converting a Mesh to Usd. When set to IfStatic, layout
     * inference will be applied only if the mesh is not being animated over the selected export
     * period.
     */
    enum class PrimvarLayoutInference
    {
        Never = 0,
        IfStatic = 1
    };

    /**
     * \brief Constructor.
     */
    MaxUSDAPI MaxMeshConversionOptions();

    /**
     * \brief Constructor.
     * \param dict The dictionary to initialize the options from.
     */
    MaxUSDAPI MaxMeshConversionOptions(const pxr::VtDictionary& dict);

    /**
     * \brief Sets default mesh conversion options. This includes a call to SetDefaultChannelPrimvarMappings().
     */
    MaxUSDAPI void SetDefaults();

    /**
     * \brief Sets default channel to primvar mappings.
     *		alpha -> "displayOpacity"
     *		shading -> "mapShading"
     *		vertex color -> "displayColor"
     *		1-N -> mapN
     */
    MaxUSDAPI void SetDefaultChannelPrimvarMappings();

    /**
     * \brief Returns the default channel to primvar mappings.
     * @return The dictionary containing the default channel to primvar mappings.
     */
    static MaxUSDAPI const pxr::VtDictionary& GetDefaultChannelPrimvarMappings();

    /**
     * \brief Returns the channel to primvar map.
     * \return The mapping table.
     */
    MaxUSDAPI const pxr::VtDictionary& GetChannelMappings() const;

    /**
     * \brief Sets the channel to primvar map.
     * \param mappings The mapping table.
     */
    MaxUSDAPI void SetChannelMappings(const pxr::VtDictionary& mappings);

    /**
     * \brief Gets the normal conversion mode.
     * \return The normal conversion mode
     */
    MaxUSDAPI NormalsMode GetNormalMode() const;

    /**
     * \brief Sets the normal conversion mode.
     * \param normalMode The normal mode to be set.
     */
    MaxUSDAPI void SetNormalsMode(NormalsMode normalMode);

    /**
     * \brief Gets the MeshFormat to be used.
     * \return The mesh format.
     */
    MaxUSDAPI MeshFormat GetMeshFormat() const;

    /**
     * \brief Sets the mesh format to be used.
     * \param meshFormat The mesh format to be set.
     */
    MaxUSDAPI void SetMeshFormat(MeshFormat meshFormat);

    /**
     * \brief Gets how the layout inference should be applied when exporting a mesh.
     * \return The layout inference to be used when exporting a mesh
     */
    MaxUSDAPI PrimvarLayoutInference GetPrimvarLayoutInference() const;

    /**
     * \brief Sets the layout inference to be used when exporting a mesh.
     * \param layoutInference the layout inference that will be used when exporting a mesh.
     */
    MaxUSDAPI void SetPrimvarLayoutInference(PrimvarLayoutInference layoutInference);

    /**
     * \brief  Sets whether or not the Object-offset transform should be baked into the geometry.
     * Otherwise, an Xform will be used to represent the node transform's versus the offset.
     * The object-offset transform is the offset of the object from the node it is attached too.
     * \param bakeObjectOffset True if we should bake the transform, false otherwise.
     */
    MaxUSDAPI void SetBakeObjectOffsetTransform(bool bakeObjectOffset);

    /**
     * \brief Gets whether or not the Object-offset transform should be baked into the geometry.
     * \return True if object offset transform are baked, false otherwise.
     */
    MaxUSDAPI bool GetBakeObjectOffsetTransform() const;

    /**
     * \brief  Sets whether or not to preserve max edge orientation.
     * \param preserve True if we should preserve the edge orientation, false otherwise.
     */
    MaxUSDAPI void SetPreserveEdgeOrientation(bool preserve);

    /**
     * \brief Gets whether or not to preserve max edge orientation.
     * \return True if we should preserve the edge orientation, false otherwise.
     */
    MaxUSDAPI bool GetPreserveEdgeOrientation() const;

    /**
     * \brief Configures a channel to primvar mapping. The specified channel will be exported using the given
     * configuration - essentially a target primvar name and a type.
     * \param channel The channel to configure.
     * \param config The configuration to use when exporting the given channel to a primvar. It is possible to
     * specify we do not want this channel exported by specifying an empty target primvar name.
     */
    MaxUSDAPI void
    SetChannelPrimvarConfig(int channel, const MappedAttributeBuilder::Config& config);

    /**
     * \brief Returns the primvar configuration for the specified channel.
     * \param channelId The channel
     * \return The primvar configuration.
     */
    MaxUSDAPI MappedAttributeBuilder::Config GetChannelPrimvarConfig(int channelId) const;

protected:
    static const pxr::VtDictionary& GetDefaultDictionary();
};

} // namespace MAXUSD_NS_DEF