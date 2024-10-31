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

#include <ifnpub.h>
#include <ref.h>

enum
{
    fnIdReload,
    fnIdSetRootLayer,
    fnIdGetUsdPreviewSurfaceMaterials,
    fnIdSetPrimvarChannelMappingDefaults,
    fnIdSetPrimvarChannelMapping,
    fnIdGetPrimvarChannel,
    fnIdIsMappedPrimvar,
    fnIdGetMappedPrimvars,
    fnIdClearMappedPrimvars,
    fnIdClearSessionLayer,
    fnIdOpenInUsdExplorer,
    fnIdCloseInUsdExplorer,
    fnIdOpenInUsdLayerEditor,
    fnIdGenerateDrawModes
};

namespace MAXUSD_NS_DEF {

/// ID if the USD I/O operation interface.
#define IUSDStageProvider_ID Interface_ID(0x6be93509, 0x325e2773)

// No way to assign an obviously unique value. Let's hope this one is.
constexpr const auto REFMSG__IUSDSTAGEPROVIDER_MSGBASE = REFMSG_USER + 9560;
constexpr const auto REFMSG_IUSDSTAGEPROVIDER_STAGE_CHANGED = REFMSG__IUSDSTAGEPROVIDER_MSGBASE + 1;

/**
 * \brief Exposes an interface for proxies able to provide a USDStage
 */
class IUSDStageProvider : public FPMixinInterface
{
public:
    // Function Publishing System
    // Function Map For Mixin Interface
    //*************************************************
    BEGIN_FUNCTION_MAP
    VFN_0(fnIdReload, Reload);
    VFN_0(fnIdClearSessionLayer, ClearSessionLayer);
    VFN_0(fnIdOpenInUsdExplorer, OpenInUsdExplorer);
    VFN_0(fnIdCloseInUsdExplorer, CloseInUsdExplorer);
    VFN_0(fnIdOpenInUsdLayerEditor, OpenInUsdLayerEditor);
    VFN_3(fnIdSetRootLayer, SetRootLayerMXS, TYPE_STRING, TYPE_STRING, TYPE_BOOL);
    FN_1(fnIdGetUsdPreviewSurfaceMaterials, TYPE_MTL, GetUsdPreviewSurfaceMaterials, TYPE_BOOL);
    VFN_0(fnIdSetPrimvarChannelMappingDefaults, SetPrimvarChannelMappingDefaults);
    VFN_2(fnIdSetPrimvarChannelMapping, SetPrimvarChannelMapping, TYPE_STRING, TYPE_VALUE);
    FN_1(fnIdGetPrimvarChannel, TYPE_VALUE, GetPrimvarChannel, TYPE_STRING);
    FN_0(fnIdGetMappedPrimvars, TYPE_STRING_TAB_BV, GetMappedPrimvars);
    FN_1(fnIdIsMappedPrimvar, TYPE_BOOL, IsMappedPrimvar, TYPE_STRING);
    VFN_0(fnIdClearMappedPrimvars, ClearMappedPrimvars);
    VFN_0(fnIdGenerateDrawModes, GenerateDrawModes);
    END_FUNCTION_MAP

    /// Return a weak pointer to the stage held by this provider.
    virtual pxr::UsdStageWeakPtr GetUSDStage() const = 0;

    /// Reload all layers of the stage held by this provider.
    virtual void Reload() = 0;

    /// Clear the session layer of the stage held by this provider.
    virtual void ClearSessionLayer() = 0;

    /// Open the stage in the USD Explorer.
    virtual void OpenInUsdExplorer() = 0;

    /// Close the stage in the USD Explorer.
    virtual void CloseInUsdExplorer() = 0;

    /// Open the stage in the USD Layer Editor.
    virtual void OpenInUsdLayerEditor() = 0;

    /// Set the root layer and mask of the stage held by this provider.
    virtual void
    SetRootLayer(const wchar_t* rootLayer, const wchar_t* stageMask, bool payloadsLoaded = true)
        = 0;

    /// Set the root layer and mask of the stage held by this provider. MXS function.
    virtual void
    SetRootLayerMXS(const wchar_t* rootLayer, const wchar_t* stageMask, bool payloadsLoaded)
        = 0;

    // Returns the multi-materials representing the UsdPreviewSurface materials in the stage.
    virtual Mtl* GetUsdPreviewSurfaceMaterials(bool update) = 0;

    // Sets defaults primvar to channels mappings.
    virtual void SetPrimvarChannelMappingDefaults() = 0;

    // Sets a primvar to channel mapping.
    virtual void SetPrimvarChannelMapping(const wchar_t* primvarName, Value* channel) = 0;

    // Gets the channel to which a primvar maps to.
    virtual Value* GetPrimvarChannel(const wchar_t* primvarName) = 0;

    // Gets the list of all currently mapped primvars.
    virtual Tab<const wchar_t*> GetMappedPrimvars() const = 0;

    // Checks if a primvar is currently mapped to a channel.
    virtual bool IsMappedPrimvar(const wchar_t* primvarName) = 0;

    // Clears all primvar mappings.
    virtual void ClearMappedPrimvars() = 0;

    // Generate USD Draw modes as configured.
    virtual void GenerateDrawModes() = 0;
};

} // namespace MAXUSD_NS_DEF