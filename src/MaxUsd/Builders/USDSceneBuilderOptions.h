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

#include "SceneBuilderOptions.h"

#include <MaxUsd/MaxUSDAPI.h>
#include <MaxUsd/MeshConversion/MaxMeshConversionOptions.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/usdFileFormat.h>
#include <pxr/usd/usd/usdcFileFormat.h>

PXR_NAMESPACE_OPEN_SCOPE
// clang-format off
#define PXR_MAXUSD_USD_SCENE_BUILDER_TOKENS \
	/* Dictionary keys */ \
	(contentSource) \
	(translateMeshes) \
	(translateShapes) \
	(translateLights) \
	(translateCameras) \
	(translateMaterials) \
	(translateHidden) \
	(translateSkin) \
	(translateMorpher) \
	(useUSDVisibility) \
	(allowNestedGprims) \
	(shadingMode) \
	(allMaterialConversions) \
	(usdStagesAsReferences) \
	(fileFormat) \
	(upAxis) \
	/* Mesh Conversion Options */ \
	(meshConversionOptions) \
	(timeMode) \
	/* Time Config (start)*/ \
	(startFrame) \
	(endFrame) \
	(samplesPerFrame) \
	/* Time Config (end)*/ \
	(rootPrimPath) \
	(openInUsdView) \
	(mtlSwitcherExportStyle) \
	(useProgressBar) \
	(separateMaterialLayer) \
	(materialLayerPath) \
	(materialPrimPath) \
	(useLastResortUSDPreviewSurfaceWriter) \
	(bonesPrimName) \
	(animationsPrimName) \
	(version)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(
    MaxUsdUsdSceneBuilderOptionsTokens,
    MaxUSDAPI,
    PXR_MAXUSD_USD_SCENE_BUILDER_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

namespace MAXUSD_NS_DEF {

/**
 * \brief USD Scene Build configuration options.
 */
class USDSceneBuilderOptions : public SceneBuilderOptions
{
public:
    /**
     * \brief Source of the	content to build.
     */
    enum class MaxUSDAPI ContentSource
    {
        RootNode,  ///< Build from the Root Node of the 3ds Max scene.
        Selection, ///< Build from the Nodes selected in the 3ds Max scene.
        NodeList   ///< Build from a nodes list.
    };

    /**
     * \brief Up axis to use upon export.
     * \comment value of enum is important as it reflects the index in the ui of qcombobox
     */
    enum class MaxUSDAPI UpAxis
    {
        Z = 0,
        Y = 1
    };

    /**
     * \brief The USD file type to be used on export.
     */
    enum class MaxUSDAPI FileFormat
    {
        ASCII,
        Binary
    };

    /**
     * \brief Time mode for export.
     */
    enum class MaxUSDAPI TimeMode
    {
        CurrentFrame,
        ExplicitFrame,
        AnimationRange,
        FrameRange
    };

    static MaxUSDAPI const double MIN_SAMPLES_PER_FRAME;
    static MaxUSDAPI const double MAX_SAMPLES_PER_FRAME;

#ifdef IS_MAX2024_OR_GREATER
    /**
     * \brief Material Switcher export style.
     */
    enum class MaxUSDAPI MtlSwitcherExportStyle
    {
        AsVariantSets,
        ActiveMaterialOnly
    };
#endif

public:
    /**
     * \brief Constructor.
     */
    MaxUSDAPI explicit USDSceneBuilderOptions() noexcept;

    /**
     * \brief This is used internally to initialize the options from a dictionary,
     * and it is a costly operation due to the validation of the dictionary.
     * \param dict The dictionary to initialize the options from.
     */
    MaxUSDAPI explicit USDSceneBuilderOptions(const pxr::VtDictionary& dict) noexcept;

    /**
     * \brief Copies the values from an existing options object.
     */
    MaxUSDAPI void SetOptions(const USDSceneBuilderOptions& options);

    /**
     * \brief Set the the default option values.
     */
    MaxUSDAPI void SetDefaults();

    /**
     * \brief Returns a copy of the current USDSceneBuilderOptions with
     * the JobContext option overrides applied on that copy
     */
    MaxUSDAPI USDSceneBuilderOptions OptionsWithAppliedContexts() const;

    /**
     * \brief Build USD content from the given 3ds Max content source.
     * \param contentSource Source of the 3ds Max content from which to build the USD scene.
     */
    MaxUSDAPI void SetContentSource(const ContentSource& contentSource);

    /**
     * \brief Return the 3ds Max content source from which to build the USD scene.
     * \return The 3ds Max content source from which to build the USD scene.
     */
    MaxUSDAPI ContentSource GetContentSource() const;

    /**
     * \brief Translate 3ds Max meshes into USD meshes.
     * \param translateMeshes "true" to translate 3ds Max meshes into USD meshes.
     */
    MaxUSDAPI void SetTranslateMeshes(bool translateMeshes);

    /**
     * \brief Check if 3ds Max meshes should be translated into	USD meshes.
     * \return "true" if the 3ds Max meshes should be translated into USD meshes.
     */
    MaxUSDAPI bool GetTranslateMeshes() const;

    /**
     * \brief Translate 3ds Max shapes into USD meshes.
     * \param translateMeshes "true" to translate 3ds Max meshes into USD meshes.
     */
    MaxUSDAPI void SetTranslateShapes(bool translateShapes);

    /**
     * \brief Check if 3ds Max shapes should be translated into USD meshes.
     * \return "true" if the 3ds Max shapes should be translated into USD meshes.
     */
    MaxUSDAPI bool GetTranslateShapes() const;

    /**
     * \brief Translate 3ds Max lights into USD lights.
     * \param translateLights "true" to translate 3ds Max lights into USD lights.
     */
    MaxUSDAPI void SetTranslateLights(bool translateLights);

    /**
     * \brief Check if 3ds Max lights should be translated into USD lights.
     * \return "true" if the 3ds Max lights should be translated into USD lights.
     */
    MaxUSDAPI bool GetTranslateLights() const;

    /**
     * \brief Translate 3ds Max cameras into USD cameras.
     * \param translateCameras "true" to translate 3ds Max cameras into USD cameras.
     */
    MaxUSDAPI void SetTranslateCameras(bool translateCameras);

    /**
     * \brief Check if 3ds Max cameras should be translated into USD cameras.
     * \return "true" if the 3ds Max cameras should be translated into USD cameras.
     */
    MaxUSDAPI bool GetTranslateCameras() const;

    /**
     * \brief Translate 3ds Max materials into USD
     * \param translateMaterials "true" to translate materials
     */
    MaxUSDAPI void SetTranslateMaterials(bool translateMaterials);

    /**
     * \brief Check if materials should be translated
     * \return "true" if materials should be translated
     */
    MaxUSDAPI bool GetTranslateMaterials() const;

    /**
     * \brief Translate 3ds Max skin and skeleton into USD
     * \param trasnlateSkin "true" to translate skin and skeleton
     */
    MaxUSDAPI void SetTranslateSkin(bool trasnlateSkin);

    /**
     * \brief Check if skin and skeleton should be translated
     * \return "true" if skin and skeleton should be translated
     */
    MaxUSDAPI bool GetTranslateSkin() const;

    /**
     * \brief Translate 3ds Max morphers modifiers into USD as Blendshapes
     * \param translateMorpher "true" to translate morphers modifiers
     */
    MaxUSDAPI void SetTranslateMorpher(bool translateMorpher);

    /**
     * \brief Check if morpher modifiers should be translated as USD Blendshapes
     * \return "true" if morphers modifiers should be translated as USD Blendshapes
     */
    MaxUSDAPI bool GetTranslateMorpher() const;

    /**
     * \brief Sets the shading schema (mode) to use for material export
     * \param shadingMode The shading schema (mode) to apply on material export
     */
    MaxUSDAPI void SetShadingMode(const pxr::TfToken& shadingMode);

    /**
     * \brief Gets the shading schema (mode) to use for material export
     * \return The shading schema (mode) to apply on materials
     */
    MaxUSDAPI pxr::TfToken GetShadingMode() const;

    /**
     * \brief Sets the set of targeted materials for material conversion
     * \param materialConversions USD Material target set to convert to
     */
    MaxUSDAPI void SetAllMaterialConversions(const std::set<pxr::TfToken>& materialConversions);

    /**
     * \brief Gets the set of targeted materials for material conversion
     * \return USD Material target set to convert to
     */
    MaxUSDAPI const std::set<pxr::TfToken>& GetAllMaterialConversions() const;

    /**
     * \brief Sets the USD material type targeted to convert the 3ds Max materials
     * \param shader The USD Material type to convert to
     */
    MaxUSDAPI void SetConvertMaterialsTo(pxr::TfToken shader) { convertMaterialsTo = shader; }

    /**
     * \brief Token identifier of the USD material type targeted to convert the 3ds Max materials
     * \return USD Material type to convert to
     */
    MaxUSDAPI pxr::TfToken GetConvertMaterialsTo() const { return convertMaterialsTo; }

    /**
     * \brief Sets whether USD Stage Objects should be exported as USD References.
     * \param usdStagesAsReferences "true" if USD Stages should be exported as references.
     */
    MaxUSDAPI void SetUsdStagesAsReferences(bool usdStagesAsReferences);

    /**
     * \brief Checks if USD Stage Objects should be exported as USD References.
     * \return "true" if USD Stages should be exported as references.
     */
    MaxUSDAPI bool GetUsdStagesAsReferences() const;

    /**
     * \brief Sets whether hidden objects should be translated.
     * \param translateHiddenObjects True to translate hidden objects, false otherwise.
     */
    MaxUSDAPI void SetTranslateHidden(bool translateHiddenObjects);

    /**
     * \brief Check if hidden objects should be translated.
     * \return True if hidden objects should be translated, false otherwise.
     */
    MaxUSDAPI bool GetTranslateHidden() const;

    /**
     * \brief Sets whether we should attempt to match the Hidden state in Max with the USD
     * visibility attribute.
     * \param useUSDVibility True if hidden objects should be flagged invisible using the USD
     * visibility attribute.
     */
    MaxUSDAPI void SetUseUSDVisibility(bool useUSDVibility);

    /**
     * \brief Check if we should attempt to match the Hidden state in Max with the USD visibility
     * attribute.
     * \return True if hidden objects should be flagged invisible using the USD visibility
     * attribute.
     */
    MaxUSDAPI bool GetUseUSDVisibility() const;

    /**
     * \brief Set the internal format of the USD file to export.
     * \param saveFormat Format of the file to export.
     */
    MaxUSDAPI void SetFileFormat(const FileFormat& format);

    /**
     * \brief Return the format of the file to export.
     * \return The format of the file to export.
     */
    MaxUSDAPI FileFormat GetFileFormat() const;

    /**
     * \brief Sets how normals should be exported.
     * \param normalsMode The normals export mode.
     */
    MaxUSDAPI void SetNormalsMode(const MaxMeshConversionOptions::NormalsMode& normalsMode);

    /**
     * \brief Return how normals should be exported.
     * \return The normals export mode.
     */
    MaxUSDAPI MaxMeshConversionOptions::NormalsMode GetNormalsMode() const;

    /**
     * \brief Sets how meshes should be exported.
     * \param meshFormat The mesh format to be used.
     */
    MaxUSDAPI void SetMeshFormat(const MaxMeshConversionOptions::MeshFormat& meshFormat);

    /**
     * \brief Return how meshes should be exported.
     * \return The mesh format in use.
     */
    MaxUSDAPI MaxMeshConversionOptions::MeshFormat GetMeshFormat() const;

    /**
     * \brief Set the "up axis" of the USD Stage produced from the translation of the 3ds Max
     * content.
     * \param upAxis Up axis of the USD Stage produced from the translation of the 3ds Max content.
     */
    MaxUSDAPI void SetUpAxis(const UpAxis& upAxis);

    /**
     * \brief Return the "up axis" of the USD Stage produced from the translation of the 3ds Max
     * content.
     * \return The "up axis" of the USD Stage produced from the translation of the 3ds Max content.
     */
    MaxUSDAPI UpAxis GetUpAxis() const;

    /**
     * \brief Gets the mesh conversion options
     * \return The mesh conversion options.
     */
    MaxUSDAPI const MaxMeshConversionOptions GetMeshConversionOptions() const;

    /**
     * \brief Sets the mesh conversion options
     * \param meshConversionOptions The mesh conversion options to set.
     */
    MaxUSDAPI void SetMeshConversionOptions(const MaxMeshConversionOptions& meshConversionOptions);

    /**
     * \brief Sets the nodes to convert to USD.
     * \param nodes The nodes to convert to USD.
     */
    MaxUSDAPI void SetNodesToExport(const Tab<INode*>& nodes);

    /**
     * \brief Gets the nodes to convert to USD.
     * \return The nodes to convert to USD.
     */
    MaxUSDAPI const Tab<INode*>& GetNodesToExport() const;

    /**
     * \brief Sets the time mode for export, either CURRENT or EXPLICIT. If explicit, export from
     * the time specified by the Time property.
     * \param timeMode The time mode to set.
     */
    MaxUSDAPI void SetTimeMode(const TimeMode& timeMode);

    /**
     * \brief Gets the time mode to be used for export.
     * \return The time mode.
     */
    MaxUSDAPI TimeMode GetTimeMode() const;

    /**
     * \brief Sets the first frame from which to export, only used if the time mode is EXPLICIT or
     * FRAME_RANGE.
     * \param time The first frame to use for export.
     */
    MaxUSDAPI void SetStartFrame(double startFrame);

    /**
     * \brief Gets the first frame from which to export, only used if the time mode is configured as
     * EXPLICIT or FRAME_RANGE.
     * \return The first frame to use for export.
     */
    MaxUSDAPI double GetStartFrame() const;

    /**
     * \brief Sets the last frame from which to export, only used if the time mode is FRAME_RANGE.
     * \param time The last frame to use for export.
     */
    MaxUSDAPI void SetEndFrame(double endFrame);

    /**
     * \brief Gets the last frame from which to export, only used if the time mode is configured as
     * FRAME_RANGE.
     * \return The last frame to use for export.
     */
    MaxUSDAPI double GetEndFrame() const;

    /**
     * \brief Sets the number of samples to be exported to USD, per frame.
     * \param samplesPerFrame The number of samples per frame.
     */
    MaxUSDAPI void SetSamplesPerFrame(double samplesPerFrame);

    /**
     * \brief Gets the number of samples to be exported to USD, per frame.
     * \return The number of samples per frame.
     */
    MaxUSDAPI double GetSamplesPerFrame() const;

    /**
     * \brief Resolves the actual time configuration from the selected TimeMode, the set values for
     * startFrame and endFrame, and the current max time slider configuration. For example, if the
     * time mode is ANIMATION_RANGE, the start and end frames in the returned time config will be
     * set from the current time slider configuration in the scene.
     * \return The time configuration, directly usable.
     */
    MaxUSDAPI MaxUsd::TimeConfig GetResolvedTimeConfig() const;

    /**
     * \brief Sets the root prim path to export to.
     * \param value The root prim path to configure.
     */
    MaxUSDAPI void SetRootPrimPath(const pxr::SdfPath& rootPrimPath);

    /**
     * \brief Gets the configured root prim path.
     * \return The configured root prim path.
     */
    MaxUSDAPI const pxr::SdfPath& GetRootPrimPath() const;

    /**
     * \brief Gets the configured bone prim name.
     * \return The configured bone prim name.
     */
    MaxUSDAPI const pxr::TfToken& GetBonesPrimName() const;

    /**
     * \brief Set the the name to be used for the bone prim
     * \param bonesPrimName New name to be used for the bone prim
     */
    MaxUSDAPI void SetBonesPrimName(const pxr::TfToken& bonesPrimName);

    /**
     * \brief Gets the configured animation prim name.
     * \return The configured animation prim name.
     */
    MaxUSDAPI const pxr::TfToken& GetAnimationsPrimName() const;

    /**
     * \brief Set the the name to be used for the Animation prim
     * \param animationsPrimName New name to be used for the Animation prim
     */
    MaxUSDAPI void SetAnimationsPrimName(const pxr::TfToken& animationsPrimName);

    /**
     * \brief Whether or not the produced USD file should be opened in Usdview at
     * the end of the export.
     * \param openInUsdview "true" to open the file in usdview when the export completes.
     */
    MaxUSDAPI void SetOpenInUsdview(bool openInUsdview);

    /**
     * \brief Check if the produced USD file should be opened in Usdview at
     * the end of the export.
     * \return "true" if the produced file should be open in usdview.
     */
    MaxUSDAPI bool GetOpenInUsdview() const;

    /**
     * \brief Sets whether or not the exporter should allow nested Gprims. Nested gprims
     * are technically illegal in USD, but will still work in many usage scenarios. Allowing
     * nested Gprims may significantly reduces the number of total primitives, potentially
     * improving performance. The output USD structure will also match closer to the source
     * 3dsMax scene structure.
     * \param allowNestedGprims True if nesting GPrims is allowed.
     */
    MaxUSDAPI void SetAllowNestedGprims(bool allowNestedGprims);

    /**
     * \brief Gets whether or not the exporter should allow nested Gprims. Nested gprims
     * are technically illegal in USD, but will still work in many usage scenarios. Allowing
     * nested Gprims may significantly reduces the number of total primitives, potentially
     * improving performance. The output USD structure will also match closer to the source
     * 3dsMax scene structure.
     * \return True if nesting GPrims is allowed. False otherwise.
     */
    MaxUSDAPI bool GetAllowNestedGprims() const;

#ifdef IS_MAX2024_OR_GREATER
    /**
     * \brief Sets the Material Switcher export style to use at export
     * \param exportStyle The Material Switcher export style to apply at export
     */
    MaxUSDAPI void SetMtlSwitcherExportStyle(const MtlSwitcherExportStyle& exportStyle);

    /**
     * \brief Gets the Material Switcher export style to use at export
     * \return Material Switcher export style used at export
     */
    MaxUSDAPI const MtlSwitcherExportStyle GetMtlSwitcherExportStyle() const;
#endif

    /**
     * \brief Gets whether to use the progress bar or not.
     * \return True if the progress bar should be used.
     */
    MaxUSDAPI bool GetUseProgressBar() const;

    /**
     * \brief Sets whether to use the progress bar.
     */
    MaxUSDAPI void SetUseProgressBar(bool useProgressBar);

    /**
     * \brief Sets the file path to export materials to.
     * \param matPath The file path.
     */
    MaxUSDAPI void SetMaterialLayerPath(const std::string& matPath);

    /**
     * \brief Gets the file path to where materials are exported to.
     * \return The file path.
     */
    MaxUSDAPI const std::string& GetMaterialLayerPath() const;

    /**
     * \brief Sets the prim path to export materials to.
     * \param matRootpath The prim path.
     */
    MaxUSDAPI void SetMaterialPrimPath(const pxr::SdfPath& matRootpath);

    /**
     * \brief Gets the prim path where materials are exported to.
     * \return The prim path.
     */
    MaxUSDAPI const pxr::SdfPath& GetMaterialPrimPath() const;

    /**
     * \brief Sets whether we should export to a separate material layer.
     * \param useMaterialLayer True if layer should be part of another file.
     */
    MaxUSDAPI void SetUseSeparateMaterialLayer(bool useMaterialLayer);

    /**
     * \brief Checks whether we should export to a separate material layer.
     * \return True if layer should be part of another file.
     */
    MaxUSDAPI bool GetUseSeparateMaterialLayer() const;

    /**
     * \brief Sets if the USD Preview Surface Material target should use the last resort shader
     * writer if no writer can handle the conversion from a material type to UsdPreviewSurface, the
     * last resort writer will just look at the Diffuse color of the material, which is part of the
     * base material interface, and setup a UsdPreviewSurface with that diffuse color.
     * \param useLastResortUSDPreviewSurfaceWriter True if the Last resort fallback should be used.
     */
    MaxUSDAPI void
    SetUseLastResortUSDPreviewSurfaceWriter(bool useLastResortUSDPreviewSurfaceWriter);

    /**
     * \brief Checks if the USD Preview Surface Material target should use the last resort shader
     * writer if no writer can handle the conversion from a material type to UsdPreviewSurface, the
     * last resort writer will just look at the Diffuse color of the material, which is part of the
     * base material interface, and setup a UsdPreviewSurface with that diffuse color.
     * \return True if if the Last resort fallback should be used.
     */
    MaxUSDAPI bool GetUseLastResortUSDPreviewSurfaceWriter() const;

    // export dialog specific state settings
    // they differ from the export settings retained by the user at export
    // some options need data to be displayed for the user to make a choice
    // and that data is not kept between exports otherwise
    struct MaxUSDAPI AnimationRollupData
    {
        bool   frameNumberDefault { true };
        double frameNumber;
        bool   frameRangeDefault { true };
        double frameRangeStart;
        double frameRangeEnd;
    };

    /**
     * \brief Retrieve the animation rollup data used to initialize the export dialog
     */
    MaxUSDAPI void FetchAnimationRollupData(AnimationRollupData& animationRollupData) const;
    /**
     * \brief Save the animation rollup data to be used for a subsequent export dialog
     * initialization Called when the user export to USD (dialog accept)
     */
    MaxUSDAPI void SaveAnimationRollupData(const AnimationRollupData& animationRollupData);

protected:
    /**
     * \brief The dictionary holding the default state of all the options.
     * \return The default options dictionary.
     */
    static const pxr::VtDictionary& GetDefaultDictionary();

    /// Specifies the current targeted material being treated by the material export process
    ///	this member is set by the process ONLY
    pxr::TfToken convertMaterialsTo;
    /// A List of nodes to export (if any).
    /// Used when exporting with option USDSceneBuilderOptions::ContentSource::NodeList
    Tab<INode*> nodesToExport;
    // export animation rollup data
    AnimationRollupData animationRollupData;
};

} // namespace MAXUSD_NS_DEF
