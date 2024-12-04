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

#include <MaxUsd/Builders/MaxSceneBuilderOptions.h>

#include <maxscript/foundation/ValueHolderMember.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief USD Scene Build configuration options.
 */

#define IUSDImportOptions_INTERFACE_ID Interface_ID(0x2469489d, 0x73a855c8)

class MaxUSDAPI IUSDImportOptions
    : public FPMixinInterface
    , public MaxSceneBuilderOptions
{

public:
    typedef MaxSceneBuilderOptions BaseClass;

    // Inherited methods
    FPInterfaceDesc* GetDesc();
    const TCHAR*     GetName() { return _T("IUSDImportOptions"); }
    BaseInterface*   AcquireInterface() { return this; }
    void             ReleaseInterface() { delete this; }

    /**
     * \brief Constructor.
     */
    IUSDImportOptions();

    /**
     * \brief Copy constructor.
     * \param Options to copy from.
     */
    IUSDImportOptions(const IUSDImportOptions& options);

    /**
     * \brief Copy constructor from base class.
     * \param Options to copy from.
     */
    IUSDImportOptions(const MaxSceneBuilderOptions& options);

    /**
     * \brief Assignement operator
     * \param Options to assign
     * \return The assigned options object.
     */
    const IUSDImportOptions& operator=(const IUSDImportOptions& options);

    /**
     * \brief Gets the currently configured stage mask paths. Only USD prims at or below these paths
     * will be imported.
     * \return The mask paths.
     */
    Tab<const wchar_t*> GetStageMaskPaths() const;

    /**
     * \brief Sets the stage mask's paths. Only USD prims at or below these paths will be imported.
     * \param value The mask paths.
     */
    void SetStageMaskPaths(Tab<const wchar_t*> value);

    /**
     * \brief Gets the list of MaxUsd::MetaData::MetaDataType that will be included during import.
     * \return The list of MaxUsd::MetaData::MetaDataType that will be included during import.
     */
    Tab<int> GetMetaDataIncludes() const;

    /**
     * \brief Sets the list of MaxUsd::MetaData::MetaDataType that will be included during import.
     * \param value The list of MaxUsd::MetaData::MetaDataType that will be included during import.
     */
    void SetMetaDataIncludes(Tab<int> value);

    /**
     * \brief Sets the time mode that will be used by when importing USD.
     * \param value The new time mode value. ImportTimeMode::AllRange (0), ImportTimeMode::CustomRange (1),
     * ImportTimeMode::StartTime (2) or ImportTimeMode::EndTime (3)
     */
    void SetTimeMode(int value);

    /**
     * \brief Gets the time mode that will be used when importing USD.
     */
    int GetTimeMode() const;

    /**
     * \brief Sets the USD Stage initial load set to use for the import of content into 3ds Max.
     * \param The USD Stage initial load set to use for the import of content into 3ds Max.
     */
    void SetInitialLoadSet(int value);

    /**
     * \brief Gets the USD Stage initial load set to use for the import of content into 3ds Max.
     * \return The USD Stage initial load set to use for the import of content into 3ds Max.
     */
    int GetInitialLoadSet();

    /**
     * \brief Sets defaults primvar to channels mappings.
     */
    void SetPrimvarChannelMappingDefaults();

    /**
     * \brief Gets whether or not to import unmapped primvars to available max channels.
     * \return True if we should import unmapped primvars, false otherwise.
     */
    bool GetImportUnmappedPrimvars() const;

    /**
     * \brief Sets whether or not to import unmapped primvars to available max channels.
     * \param importUnmappedPrimvars True if we should import unmapped primvars, false otherwise.
     * \return
     */
    void SetImportUnmappedPrimvars(bool importUnmappedPrimvars);

    /**
     * \brief Sets a primvar to channel mapping.
     * \param primvarName The name of the primvar.
     * \param channel The channel this primvar should be imported to.
     */
    void SetPrimvarChannelMapping(const wchar_t* primvarName, Value* channel);

    /**
     * \brief Gets the channel to which a primvar maps to.
     * \param primvarName The name of the primvar for which to retrieve the target channel.
     * \return The target channel for this primvar.
     */
    Value* GetPrimvarChannel(const wchar_t* primvarName);

    /**
     * \brief Gets the list of all currently mapped primvars.
     * \return The vector of primvar names to be filled.
     */
    Tab<const wchar_t*> GetMappedPrimvars() const;

    /**
     * \brief Checks if a primvar is currently mapped to a channel.
     * \param primvarName The primvar to check.
     * \return True if the primvar is mapped, false otherwise.
     */
    bool IsMappedPrimvar(const wchar_t* primvarName);

    /**
     * \brief Clears all primvar mappings.
     */
    void ClearMappedPrimvars();

    /**
     * \brief Serialize the options to a json string.
     * \return A json formated string representing the options
     */
    const MCHAR* Serialize() const;

// clang-format off
    enum
    {
        fnIdGetStageMask, fnIdSetStageMask,
        fnIdGetStartTimeCode, fnIdSetStartTimeCode,
        fnIdGetEndTimeCode, fnIdSetEndTimeCode,
        fnIdSetTimeMode, fnIdGetTimeMode,
        fnIdGetInitialLoadSet, fnIdSetInitialLoadSet,
        fnIdReset,
        fnIdSetPrimvarChannelMappingDefaults,
        fnIdSetPrimvarChannelMapping,
        fnIdGetPrimvarChannel,
        fnIdGetMappedPrimvars,
        fnIdIsMappedPrimvar,
        fnIdClearMappedPrimvars,
        fnIdGetPreferredMaterial, fnIdSetPreferredMaterial,
        fnIdGetShadingModes, fnIdSetShadingModes,
        fnIdGetLogPath, fnIdSetLogPath,
        fnIdGetLogLevel, fnIdSetLogLevel,
        fnIdGetAvailableChasers,
        fnIdGetChaserNames, fnIdSetChaserNames,
        fnIdGetAllChaserArgs, fnIdSetAllChaserArgs,
        fnIdGetContextNames, fnIdSetContextNames,
        fnIdGetMetaDataIncludes, fnIdSetMetaDataIncludes,
        fnIdGetImportUnmappedPrimvars, fnIdSetImportUnmappedPrimvars,
        fnIdGetTranslateMaterials,
        fnIdGetUseProgressBar, fnIdSetUseProgressBar,
        fidSerialize
    };

    enum
    {
        eIdInitialLoadSet,
        eIdLogLevel,
        eIdMetaData,
        eIdTimeMode,
    };

    BEGIN_FUNCTION_MAP
        PROP_FNS(fnIdGetStageMask, GetStageMaskPaths, fnIdSetStageMask, SetStageMaskPaths, TYPE_STRING_TAB_BV);
        PROP_FNS(fnIdGetMetaDataIncludes, GetMetaDataIncludes, fnIdSetMetaDataIncludes, SetMetaDataIncludes, TYPE_ENUM_TAB_BV);
        PROP_FNS(fnIdGetStartTimeCode, GetStartTimeCode, fnIdSetStartTimeCode, SetStartTimeCode, TYPE_DOUBLE);
        PROP_FNS(fnIdGetEndTimeCode, GetEndTimeCode, fnIdSetEndTimeCode, SetEndTimeCode, TYPE_DOUBLE);
        PROP_FNS(fnIdGetTimeMode, GetTimeMode, fnIdSetTimeMode, SetTimeMode, TYPE_ENUM);
        PROP_FNS(fnIdGetInitialLoadSet, GetInitialLoadSet, fnIdSetInitialLoadSet, SetInitialLoadSet, TYPE_ENUM);
        PROP_FNS(fnIdGetLogPath, logInterface.GetLogPath, fnIdSetLogPath, logInterface.SetLogPath, TYPE_STRING);
        PROP_FNS(fnIdGetLogLevel, logInterface.GetLogLevel, fnIdSetLogLevel, logInterface.SetLogLevel, TYPE_ENUM);
        PROP_FNS(fnIdGetImportUnmappedPrimvars, GetImportUnmappedPrimvars, fnIdSetImportUnmappedPrimvars, SetImportUnmappedPrimvars, TYPE_BOOL);
        PROP_FNS(fnIdGetPreferredMaterial, GetPreferredMaterial, fnIdSetPreferredMaterial, SetPreferredMaterial, TYPE_STRING);
        PROP_FNS(fnIdGetShadingModes, GetShadingModes, fnIdSetShadingModes, SetShadingModes, TYPE_VALUE);
        PROP_FNS(fnIdGetChaserNames, GetChaserNamesMxs, fnIdSetChaserNames, SetChaserNamesMxs, TYPE_TSTR_TAB_BV);
        PROP_FNS(fnIdGetAllChaserArgs, GetAllChaserArgs, fnIdSetAllChaserArgs, SetAllChaserArgs, TYPE_VALUE);
        PROP_FNS(fnIdGetContextNames, GetContextNamesMxs, fnIdSetContextNames, SetContextNamesMxs, TYPE_TSTR_TAB_BV);
        PROP_FNS(fnIdGetUseProgressBar, GetUseProgressBar, fnIdSetUseProgressBar, SetUseProgressBar, TYPE_BOOL);
        VFN_0(fnIdReset, SetDefaults);
        FN_0(fnIdGetTranslateMaterials, TYPE_BOOL, GetTranslateMaterials)
        VFN_0(fnIdSetPrimvarChannelMappingDefaults, SetPrimvarChannelMappingDefaults);
        VFN_2(fnIdSetPrimvarChannelMapping, SetPrimvarChannelMapping, TYPE_STRING, TYPE_VALUE);
        FN_1(fnIdGetPrimvarChannel, TYPE_VALUE, GetPrimvarChannel, TYPE_STRING);
        FN_0(fnIdGetMappedPrimvars, TYPE_STRING_TAB_BV, GetMappedPrimvars);
        FN_1(fnIdIsMappedPrimvar, TYPE_BOOL, IsMappedPrimvar, TYPE_STRING);
        VFN_0(fnIdClearMappedPrimvars, ClearMappedPrimvars);
        FN_0(fnIdGetAvailableChasers, TYPE_STRING_TAB_BV, GetAvailableChasers);
        FN_0(fidSerialize, TYPE_STRING, Serialize);
    END_FUNCTION_MAP
// clang-format on
private:
    MaxUsd::IOLoggingMxsInterface logInterface { this };

    /**
     * Set the shading modes to use at import
     * This is the maxscript exposed method from the MaxSceneBuilderOptions base class
     * \param modes the Array of ShadingMode to use (#(#("useRegistry", "UsdPreviewSurface")))
     */
    void SetShadingModes(Value* shadingModesValue);
    /**
     * Get the shading modes to use at import
     * This is the maxscript exposed method from the MaxSceneBuilderOptions base class
     * \return the array of ShadingMode being use
     */
    Value* GetShadingModes();

    /**
     * \brief Sets the preferred conversion material to use for material import
     * This is the maxscript exposed method from the MaxSceneBuilderOptions base class
     * \param targetMaterial The preferred conversion material to use for material import
     */
    void SetPreferredMaterial(const wchar_t* targetMaterial);

    /**
     * \brief Gets the preferred conversion material set for material import
     * This is the maxscript exposed method from the MaxSceneBuilderOptions base class
     * \return The preferred conversion material set for material import
     */
    const wchar_t* GetPreferredMaterial() const;

    /**
     * \brief Gets the list of available import chaser from the ImportChaserRegistry
     * \return All registered import chaser (names)
     */
    Tab<TSTR*> GetAvailableChasers() const;

    /**
     * \brief Sets the chaser list to use at import
     * This is the maxscript exposed method from the MaxSceneBuilderOptions base class
     * (SetChaserNames) \param chaserArray The list of import chaser to call at import
     */
    void SetChaserNamesMxs(const Tab<TSTR*>& chaserArray);

    /**
     * \brief Gets the list of import chasers to be called at USD import
     * This is the maxscript exposed method from the MaxSceneBuilderOptions base class
     * (GetChaserNames) \return All import chasers to be called at USD import
     */
    Tab<TSTR*> GetChaserNamesMxs() const;

    /**
     * \brief Sets the import chasers' arguments map
     * This is the maxscript exposed method from the MaxSceneBuilderOptions base class
     * (SetAllChaserArgs) \param chaserArgs a map of arguments ([chaser:[[key:value][...]]]) or
     * using an array of arguments (#(chaser,key,value,chaser,key,value,...))
     */
    void SetAllChaserArgs(Value* chaserArgs);

    /**
     * \brief Gets the map of import chasers with their specified arguments
     * This is the maxscript exposed method from the MaxSceneBuilderOptions base class
     * (GetAllChaserArgs) \return The map of import chasers with their specified arguments
     */
    Value* GetAllChaserArgs();

    /**
     * \brief Sets the context list to use at import
     * This is the maxscript exposed method from the MaxSceneBuilderOptions base class
     * (SetContextNames) \param contextArray The list of context to apply at import
     */
    void SetContextNamesMxs(const Tab<TSTR*>& contextArray);

    /**
     * \brief Gets the list of contexts (plug-in configurations) to be applied on USD import
     * This is the maxscript exposed method from the MaxSceneBuilderOptions base class
     * (GetContextNames) \return All contexts names to be applied on USD import
     */
    Tab<TSTR*> GetContextNamesMxs() const;

    // maxscript value holder to prevent passed chaser arguments dictionary from being GC'ed
    ValueHolderMember allChaserArgsMxsHolder;

    // maxscript value holder to prevent passed shaderModes Array from being GC'ed
    ValueHolderMember shadingModesMxsHolder;
};

} // namespace MAXUSD_NS_DEF