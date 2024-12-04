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

#include "IOLoggingMxsInterface.h"

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/DllEntry.h>
#include <MaxUsd/MaxUSDAPI.h>
#include <MaxUsd/MeshConversion/MaxMeshConversionOptions.h>
#include <MaxUsd/Utilities/Logging.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <maxscript/foundation/ValueHolderMember.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief USD Scene Build configuration options.
 */

#define IUSDExportOptions_INTERFACE_ID Interface_ID(0x21e7408, 0x14c075a2)

class MaxUSDAPI IUSDExportOptions
    : public FPMixinInterface
    , public USDSceneBuilderOptions
{

public:
    typedef USDSceneBuilderOptions BaseClass;

    // Inherited methods
    FPInterfaceDesc* GetDesc();
    const TCHAR*     GetName() { return _T("IUSDExportOptions"); }
    BaseInterface*   AcquireInterface() { return this; }
    void             ReleaseInterface() { delete this; }

    /**
     * \brief Constructor.
     */
    IUSDExportOptions();

    /**
     * \brief Copy constructor.
     * \param Options to copy from.
     */
    IUSDExportOptions(const IUSDExportOptions& options);

    /**
     * Copy constructor from base class.
     * \param options to copy from.
     */
    explicit IUSDExportOptions(const USDSceneBuilderOptions& options);

    /**
     * \brief Assignement operator
     * \param Options to assign
     * \return The assigned options object.
     */
    const IUSDExportOptions& operator=(const IUSDExportOptions& options);

    /**
     * \brief Set the internal format of the USD file to export.
     * \param saveFormat Format of the file to export.
     */
    void SetFileFormat(int format);

    /**
     * \brief Set the "up axis" of the USD Stage produced from the translation of the 3ds Max content.
     * \param upAxis Up axis of the USD Stage produced from the translation of the 3ds Max content.
     */
    void SetUpAxis(int upAxis);

    /**
     * \brief Set the normals mode, how to export normals.
     * \param normalsMode The normals mode.
     */
    void SetNormalsMode(int normalsMode);

    /**
     * \brief Set the mesh format, how to export meshes.
     * \param meshFormat The mesh format to be used.
     */
    void SetMeshFormat(int meshFormat);

    /**
     * \brief Sets the time mode, can be set to CURRENT or EXPLICIT.
     * \param timeMode The time mode to set.
     */
    void SetTimeMode(int timeMode);

    /**
     * \brief Sets how many samples per max frame are export to USD, when exporting animations.
     * \param samplesPerFrame The number of samples per frame.
     */
    void SetSamplesPerFrame(double samplesPerFrame);

    /**
     * \brief Gets whether or not the Object-offset transform should be baked into the geometry.
     * \return True if object offset transform are baked, false otherwise.
     */
    bool GetBakeObjectOffsetTransform() const;

    /**
     * \brief  Sets whether or not the Object-offset transform should be baked into the geometry.
     * Otherwise, an Xform will be used to represent the node transform's versus the offset.
     * The object-offset transform is the offset of the object from the node it is attached too.
     * \param bakeObjectOffset True if we should bake the transform, false otherwise.
     */
    void SetBakeObjectOffsetTransform(bool value);

    /**
     * \brief Gets whether or not to preserve edge orientation
     * \return True if preserving edge orientation, false otherwise.
     */
    bool GetPreserveEdgeOrientation() const;

    /**
     * \brief Sets whether or not to preserve edge orientation
     * \param True if preserving edge orientation, false otherwise.
     */
    void SetPreserveEdgeOrientation(bool preserve);

    /**
     * \brief Sets default channel to primvar mappings.
     */
    void SetChannelPrimvarMappingDefaults();

    /**
     * \brief Configures a channel to primvar mapping. The specified channel will be exported using the given
     * configuration - essentially a target primvar name and a type.
     * \param channel The channel to configure.
     * \param nameValue The primvar name.
     * \param type The primvar type.
     * \param autoExpandType The auto expand type.
     */
    void SetChannelPrimvarMapping(int channel, Value* nameValue, int type, bool autoExpandType);

    /**
     * \brief Gets the primvar name of a given channel.
     * \param channel The channel from which we want the primvar name.
     * \return The primvar name of the channel.
     */
    Value* GetChannelPrimvarName(int channel);

    /**
     * \brief Gets the primvar type of a given channel.
     * \param channel The channel from which we want the primvar type.
     * \return The primvar type of the channel.
     */
    int GetChannelPrimvarType(int channel);

    /**
     * \brief Gets the auto expand type of a given channel.
     * \param channel The channel from which we want the auto expand type.
     * \return The auto expand type of the channel.
     */
    int GetChannelPrimvarAutoExpandType(int channel);

    /**
     * \brief Gets the configured root prim path.
     * \return The configured root prim path.
     */
    const wchar_t* GetRootPrimPath() const;

    /**
     * \brief Gets the prim name that will be used to create the USD Skeleton prim.
     * \return Skeleton prim name
     */
    const wchar_t* GetBonesPrimName() const;

    /**
     * \brief Sets the prim name for where the Skeleton prim will be set to.
     * This will be the prim name for the Skeleton prim created when exporting
     * Max bones to USD.
     */
    void SetBonesPrimName(const wchar_t* bonePrimName);

    /**
     * \brief Gets the prim name that will be used to create the USD Skeleton Animations prim.
     * \return Skeleton Animations prim name
     */
    const wchar_t* GetAnimationsPrimName() const;

    /**
     * \brief Sets the prim name for where the Skeleton Animations prim will be set to.
     * This will be the prim name for the Skeleton Animations prim created when exporting
     * Max bones to USD.
     */
    void SetAnimationsPrimName(const wchar_t* animationsPrimName);

    /**
     * \brief Sets the root prim path to export to.
     * \param rootPath The root prim path to configure.
     */
    void SetRootPrimPath(const wchar_t* rootPath);

    /**
     * \brief Sets the file to which the Materials will be exported.
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * \param mtlPath The path and filename of the target material file
     */
    void SetMaterialLayerPath(const wchar_t* mtlPath);

    /**
     * \brief Gets the file to which the Materials will be exported.
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * \return The path and filename of the target material file
     */
    const wchar_t* GetMaterialLayerPath() const;

    /**
     * \brief Sets the prim path to which the Materials will be exported.
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * \param mtlPath The prim path of the material scope that will host the exported materials
     */
    void SetMaterialPrimPath(const wchar_t* mtlPath);

    /**
     * \brief Gets the scope path to which the Materials will be exported.
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * \return The prim path of the material scope that will host the exported materials
     */
    const wchar_t* GetMaterialPrimPath() const;

    /**
     * \brief Serialize the options to a json string.
     * \return A json formated string representing the options
     */
    const MCHAR* Serialize() const;

#ifdef IS_MAX2024_OR_GREATER
    /**
     * \brief Sets the Material Switcher export style.
     * \param exportStyle The Material Switcher export style to set.
     */
    void SetMtlSwitcherExportStyle(int exportStyle);
#endif
// clang-format off
    enum
    {
        fnIdGetTranslateMeshes, fnIdSetTranslateMeshes,
        fnIdGetTranslateShapes, fnIdSetTranslateShapes,
        fnIdGetTranslateLights, fnIdSetTranslateLights,
        fnIdGetTranslateCameras, fnIdSetTranslateCameras,
        fnIdGetTranslateSkin, fnIdSetTranslateSkin,
        fnIdGetTranslateMorpher, fnIdSetTranslateMorpher,
        fnIdGetTranslateHidden, fnIdSetTranslateHidden,
        fnIdGetUseUSDVisibility, fnIdSetUseUSDVisibility,
        fnIdGetAllowNestedGprims, fnIdSetAllowNestedGprims, 
        fnIdGetFileFormat, fnIdSetFileFormat,
        fnIdGetUpAxis, fnIdSetUpAxis,
        fnIdGetBakeObjectOffsetTransform, fnIdSetBakeObjectOffsetTransform,
        fnIdGetLogPath, fnIdSetLogPath,
        fnIdGetLogLevel, fnIdSetLogLevel,
        fnIdReset,
        fnIdSetChannelPrimvarMappingDefaults,
        fnIdSetChannelPrimvarMapping,
        fnIdGetChannelPrimvarType,
        fnIdGetChannelPrimvarName,
        fnIdGetChannelPrimvarAutoExpandType,
        fnIdGetPreserveEdgeOrientation, fnIdSetPreserveEdgeOrientation,
        fnIdGetNormals, fnIdSetNormals,
        fnIdGetMeshFormat, fnIdSetMeshFormat,
        fnIdGetTimeMode,fnIdSetTimeMode,
        fid_GetStartFrame, fid_SetStartFrame,
        fid_GetEndFrame, fid_SetEndFrame,
        fid_GetSamplesPerFrame, fid_SetSamplesPerFrame,
        fnIdGetTranslateMaterials, fnIdSetTranslateMaterials,
        fnIdGetShadingMode, fnIdSetShadingMode,
        fnIdGetAllMaterialConversions, fnIdSetAllMaterialConversions,
        fnIdGetAvailableMaterialConversions,
        fnIdGetUsdStagesAsReferences, fnIdSetUsdStagesAsReferences,
        fnIdGetRootPrimPath, fnIdSetRootPrimPath,
        fnIdGetBonesPrimName, fnIdSetBonesPrimName,
        fnIdGetAnimationsPrimName, fnIdSetAnimationsPrimName,
        fnIdGetOpenInUsdview, fnIdSetOpenInUsdview,
        fnIdGetAvailableChasers,
        fnIdGetChaserNames, fnIdSetChaserNames,
        fnIdGetAllChaserArgs, fnIdSetAllChaserArgs,
        fnIdGetContextNames, fnIdSetContextNames,
#ifdef IS_MAX2024_OR_GREATER
        fnIdGetMtlSwitcherExportStyle, fnIdSetMtlSwitcherExportStyle,
#endif
        fnIdGetMaterialLayerPath, fnIdSetMaterialLayerPath,
        fnIdGetMaterialPrimPath, fnIdSetMaterialPrimPath,
        fnIdGetUseSeparateMaterialLayer, fnIdSetUseSeparateMaterialLayer,
        fnIdGetUseProgressBar, fnIdSetUseProgressBar,
        fnIdGetUseLastResortUSDPreviewSurfaceWriter, fnIdSetUseLastResortUSDPreviewSurfaceWriter,
        fidSerialize,
    };

    enum
    {
        eIdFileFormat,
        eIdUpAxis,
        eIdPrimvarType,
        eIdLogLevel,
        eIdNormalsMode,
        eIdTimeMode,
        eIdMeshFormat,
#ifdef IS_MAX2024_OR_GREATER
        eIdMtlSwitcherExportStyle
#endif
    };

    BEGIN_FUNCTION_MAP
        PROP_FNS(fnIdGetTranslateMeshes, GetTranslateMeshes, fnIdSetTranslateMeshes, SetTranslateMeshes, TYPE_BOOL);
        PROP_FNS(fnIdGetTranslateShapes, GetTranslateShapes, fnIdSetTranslateShapes, SetTranslateShapes, TYPE_BOOL);
        PROP_FNS(fnIdGetTranslateLights, GetTranslateLights, fnIdSetTranslateLights, SetTranslateLights, TYPE_BOOL);
        PROP_FNS(fnIdGetTranslateCameras, GetTranslateCameras, fnIdSetTranslateCameras, SetTranslateCameras, TYPE_BOOL);
        PROP_FNS(fnIdGetTranslateMaterials, GetTranslateMaterials, fnIdSetTranslateMaterials, SetTranslateMaterials, TYPE_BOOL);
        PROP_FNS(fnIdGetTranslateSkin, GetTranslateSkin, fnIdSetTranslateSkin, SetTranslateSkin, TYPE_BOOL);
        PROP_FNS(fnIdGetTranslateMorpher, GetTranslateMorpher, fnIdSetTranslateMorpher, SetTranslateMorpher, TYPE_BOOL);
        PROP_FNS(fnIdGetShadingMode, GetShadingMode, fnIdSetShadingMode, SetShadingMode, TYPE_STRING);
        PROP_FNS(fnIdGetUsdStagesAsReferences, GetUsdStagesAsReferences, fnIdSetUsdStagesAsReferences, SetUsdStagesAsReferences, TYPE_BOOL);
        PROP_FNS(fnIdGetTranslateHidden, GetTranslateHidden, fnIdSetTranslateHidden, SetTranslateHidden, TYPE_BOOL);
        PROP_FNS(fnIdGetUseUSDVisibility, GetUseUSDVisibility, fnIdSetUseUSDVisibility, SetUseUSDVisibility, TYPE_BOOL);
        PROP_FNS(fnIdGetAllowNestedGprims, GetAllowNestedGprims, fnIdSetAllowNestedGprims, SetAllowNestedGprims, TYPE_BOOL);
        PROP_FNS(fnIdGetFileFormat, GetFileFormat, fnIdSetFileFormat, SetFileFormat, TYPE_ENUM);
        PROP_FNS(fnIdGetNormals, GetNormalsMode, fnIdSetNormals, SetNormalsMode, TYPE_ENUM);
        PROP_FNS(fnIdGetMeshFormat, GetMeshFormat, fnIdSetMeshFormat, SetMeshFormat, TYPE_ENUM);
        PROP_FNS(fnIdGetUpAxis, GetUpAxis, fnIdSetUpAxis, SetUpAxis, TYPE_ENUM);
        PROP_FNS(fnIdGetBakeObjectOffsetTransform, GetBakeObjectOffsetTransform, fnIdSetBakeObjectOffsetTransform, SetBakeObjectOffsetTransform, TYPE_BOOL);
        PROP_FNS(fnIdGetPreserveEdgeOrientation, GetPreserveEdgeOrientation, fnIdSetPreserveEdgeOrientation, SetPreserveEdgeOrientation, TYPE_BOOL);
        PROP_FNS(fnIdGetTimeMode, GetTimeMode, fnIdSetTimeMode, SetTimeMode, TYPE_ENUM);
        PROP_FNS(fid_GetStartFrame, GetStartFrame, fid_SetStartFrame, SetStartFrame, TYPE_DOUBLE);
        PROP_FNS(fid_GetEndFrame, GetEndFrame, fid_SetEndFrame, SetEndFrame, TYPE_DOUBLE);
        PROP_FNS(fid_GetSamplesPerFrame, GetSamplesPerFrame, fid_SetSamplesPerFrame, SetSamplesPerFrame, TYPE_DOUBLE);
        PROP_FNS(fnIdGetRootPrimPath, GetRootPrimPath, fnIdSetRootPrimPath, SetRootPrimPath, TYPE_STRING);
        PROP_FNS(fnIdGetBonesPrimName, GetBonesPrimName, fnIdSetBonesPrimName, SetBonesPrimName, TYPE_STRING);
        PROP_FNS(fnIdGetAnimationsPrimName, GetAnimationsPrimName, fnIdSetAnimationsPrimName, SetAnimationsPrimName, TYPE_STRING);
        PROP_FNS(fnIdGetLogPath, logInterface.GetLogPath, fnIdSetLogPath, logInterface.SetLogPath, TYPE_STRING);
        PROP_FNS(fnIdGetLogLevel, logInterface.GetLogLevel, fnIdSetLogLevel, logInterface.SetLogLevel, TYPE_ENUM);
        PROP_FNS(fnIdGetOpenInUsdview, GetOpenInUsdview, fnIdSetOpenInUsdview, SetOpenInUsdview, TYPE_BOOL);
        PROP_FNS(fnIdGetChaserNames, GetChaserNamesMxs, fnIdSetChaserNames, SetChaserNamesMxs, TYPE_TSTR_TAB_BV);
        PROP_FNS(fnIdGetAllChaserArgs, GetAllChaserArgs, fnIdSetAllChaserArgs, SetAllChaserArgs, TYPE_VALUE);
        PROP_FNS(fnIdGetContextNames, GetContextNamesMxs, fnIdSetContextNames, SetContextNamesMxs, TYPE_TSTR_TAB_BV);
#ifdef IS_MAX2024_OR_GREATER
        PROP_FNS(fnIdGetMtlSwitcherExportStyle, GetMtlSwitcherExportStyle, fnIdSetMtlSwitcherExportStyle, SetMtlSwitcherExportStyle, TYPE_ENUM);
#endif
        PROP_FNS(fnIdGetUseProgressBar, GetUseProgressBar, fnIdSetUseProgressBar, SetUseProgressBar, TYPE_BOOL);
        PROP_FNS(fnIdGetMaterialLayerPath, GetMaterialLayerPath, fnIdSetMaterialLayerPath, SetMaterialLayerPath, TYPE_STRING);
        PROP_FNS(fnIdGetMaterialPrimPath, GetMaterialPrimPath, fnIdSetMaterialPrimPath, SetMaterialPrimPath, TYPE_STRING);
        PROP_FNS(fnIdGetUseSeparateMaterialLayer, GetUseSeparateMaterialLayer, fnIdSetUseSeparateMaterialLayer, SetUseSeparateMaterialLayer, TYPE_BOOL);
        PROP_FNS(fnIdGetUseLastResortUSDPreviewSurfaceWriter, GetUseLastResortUSDPreviewSurfaceWriter, fnIdSetUseLastResortUSDPreviewSurfaceWriter, SetUseLastResortUSDPreviewSurfaceWriter, TYPE_BOOL);
        VFN_0(fnIdReset, SetDefaults);
        VFN_0(fnIdSetChannelPrimvarMappingDefaults, SetChannelPrimvarMappingDefaults);
        VFN_4(fnIdSetChannelPrimvarMapping, SetChannelPrimvarMapping, TYPE_INT, TYPE_VALUE, TYPE_ENUM, TYPE_BOOL);
        FN_1(fnIdGetChannelPrimvarName, TYPE_VALUE, GetChannelPrimvarName, TYPE_INT);
        FN_1(fnIdGetChannelPrimvarType, TYPE_ENUM, GetChannelPrimvarType, TYPE_INT);
        FN_1(fnIdGetChannelPrimvarAutoExpandType, TYPE_bool, GetChannelPrimvarAutoExpandType, TYPE_INT);
        FN_0(fnIdGetAllMaterialConversions, TYPE_TSTR_TAB_BV, GetAllMaterialTargets);
        VFN_1(fnIdSetAllMaterialConversions, SetAllMaterialTargets, TYPE_TSTR_TAB_BV);
        FN_0(fnIdGetAvailableMaterialConversions, TYPE_TSTR_TAB_BV, GetAvailableMaterialTargets);
        FN_0(fnIdGetAvailableChasers, TYPE_STRING_TAB_BV, GetAvailableChasers);
        FN_0(fidSerialize, TYPE_STRING, Serialize);
    END_FUNCTION_MAP
// clang-format on

private:
    IOLoggingMxsInterface logInterface { this };

    /**
     * \brief Returns the primvar config for a given channel. If an invalid channel is given, this function will
     * throw a Maxscript runtime exception.
     * \param channel The channel for which to get the primvar config.
     * \return The primvar config for this channel.
     */
    const MappedAttributeBuilder::Config GetValidPrimvarConfig(int channel);

    /**
     * \brief Sets the shading schema (mode) to use for material export
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * \param shadingMode The shading schema (mode) to apply on material export
     */
    void SetShadingMode(const wchar_t* shadingMode);

    /**
     * \brief Gets the shading schema (mode) to use for material export
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * \return The shading schema (mode) to apply on materials
     */
    const wchar_t* GetShadingMode() const;

    /**
     * \brief Gets the set of targeted materials for material conversion
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * (GetAllMaterialConversions) \return USD Material target set to convert to
     */
    Tab<TSTR*> GetAllMaterialTargets() const;

    /**
     * \brief Sets the set of targeted materials for material conversion
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * (SetAllMaterialConversions) \param materialArray USD Material target set to convert to
     */
    void SetAllMaterialTargets(const Tab<TSTR*>& materialArray);

    /**
     * \brief Gets the list of available material conversion types from the ShadingModeRegistry
     * \return All registered USD Material types to convert to
     */
    Tab<TSTR*> GetAvailableMaterialTargets() const;

    /**
     * \brief Gets the list of available export chaser from the ExportChaserRegistry
     * \return All registered export chaser (names)
     */
    Tab<TSTR*> GetAvailableChasers() const;

    /**
     * \brief Sets the chaser list to use at export
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * (SetChaserNames) \param chaserArray The list of export chaser to call at export
     */
    void SetChaserNamesMxs(const Tab<TSTR*>& chaserArray);

    /**
     * \brief Gets the list of export chasers to be called at USD export
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * (GetChaserNames) \return All export chasers to be called at USD export
     */
    Tab<TSTR*> GetChaserNamesMxs() const;

    /**
     * \brief Sets the export chasers' arguments map
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * (SetAllChaserArgs) \param chaserArgs a map of arguments ([chaser:[[key:value][...]]]) or
     * using an array of arguments (#(chaser,key,value,chaser,key,value,...))
     */
    void SetAllChaserArgs(Value* chaserArgs);

    /**
     * \brief Gets the map of export chasers with their specified arguments
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * (GetAllChaserArgs) \return The map of export chasers with their specified arguments
     */
    Value* GetAllChaserArgs();

    /**
     * \brief Sets the context list to use at export
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * (SetContextNames) \param contextArray The list of context to apply at export
     */
    void SetContextNamesMxs(const Tab<TSTR*>& contextArray);

    /**
     * \brief Gets the list of contexts (plug-in configurations) to be applied on USD export
     * This is the maxscript exposed method from the USDSceneBuilderOptions base class
     * (GetContextNames) \return All contexts names to be applied on USD export
     */
    Tab<TSTR*> GetContextNamesMxs() const;

    // maxscript value holder to prevent passed chaser arguments dictionary from being GC'ed
    ValueHolderMember allChaserArgsMxsHolder;
};

} // namespace MAXUSD_NS_DEF
