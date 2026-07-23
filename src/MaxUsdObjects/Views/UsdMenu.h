//
// Copyright 2025 Autodesk
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

#include <MaxUsd/Utilities/MaxSupportUtils.h>
// Modern menu system available in 2025+
#ifdef IS_MAX2025_OR_GREATER

#include <actiontable.h>
#include <max.h>
#include <strclass.h>

static const ActionTableId usdMenuActionTableId = 0x29760f36; // Unique table ID

const MaxSDK::MaxGuid kUsdMenuGUID { "09503826-ad8c-4966-a3c4-0a8379ded6e2" };

const MaxSDK::MaxGuid kUsdMenuCreateSubMenuGUID { "0722206f-2296-4587-8f41-f5f4251be039" };
const MaxSDK::MaxGuid kUsdMenuCreateStageFromFileGUID { "52eba475-3281-4bd2-a27b-19fd6241186d" };
inline constexpr int  usdMenuCreateStageFromFileActionItemId = 42257;
const MaxSDK::MaxGuid kUsdMenuCreateStageWithNewLayerGUID {
    "01db5dc6-a8bd-47d0-9f99-7d0b6d358c5c"
};
inline constexpr int usdMenuCreateStageWithNewLayerActionItemId = 42258;

const MaxSDK::MaxGuid kUsdMenuSeparatorToolsGUID { "942a008c-d50a-4b08-9df3-64f2f83c4ce1" };

const MaxSDK::MaxGuid kUsdMenuToolsSubMenuGUID { "8b440778-6073-41f0-8fe3-fe0e37f7db51" };
const MaxSDK::MaxGuid kUsdMenuToolsExplorerGUID { "4b917b22-7bd4-4ef6-bedd-ac14beb2b711" };
inline constexpr int  usdMenuToolsExplorerActionItemId = 42259;
const MaxSDK::MaxGuid kUsdMenuToolsLayerEditorGUID { "0611d2c4-6ad5-4156-90e7-129c81da8527" };
inline constexpr int  usdMenuToolsLayerEditorActionItemId = 42260;
const MaxSDK::MaxGuid kUsdMenuToolsPathEditorGUID { "547baca6-3f1d-4add-a543-e5d08eb9c9e4" };
inline constexpr int  usdMenuToolsPathEditorActionItemId = 42261;

const MaxSDK::MaxGuid kHelpMenuId { "cee8f758-2199-411b-81e7-d3ff4a80d143" };

class USDMenuCreateStageFromFileActionItem : public ActionItem
{
    // ActionItem overrides
    int  GetId() override { return usdMenuCreateStageFromFileActionItemId; }
    BOOL ExecuteAction() override;
    void GetButtonText(MSTR& buttonText) override { buttonText = _T("Stage from File..."); }
    void GetMenuText(MSTR& menuText) override { GetButtonText(menuText); }
    void GetDescriptionText(MSTR& descText) override { GetButtonText(descText); }
    void GetCategoryText(MSTR& catText) override { catText = _T("USD"); }
    BOOL IsChecked() override { return false; }
    BOOL IsItemVisible() override { return true; }
    BOOL IsEnabled() override { return true; }
    void DeleteThis() override { delete this; }
    BOOL IsDynamicMenu() override { return false; }
};

class USDMenuCreateStageWithNewLayerActionItem : public ActionItem
{
    // ActionItem overrides
    int  GetId() override { return usdMenuCreateStageWithNewLayerActionItemId; }
    BOOL ExecuteAction() override;
    void GetButtonText(MSTR& buttonText) override { buttonText = _T("Stage with New Layer"); }
    void GetMenuText(MSTR& menuText) override { GetButtonText(menuText); }
    void GetDescriptionText(MSTR& descText) override { GetButtonText(descText); }
    void GetCategoryText(MSTR& catText) override { catText = _T("USD"); }
    BOOL IsChecked() override { return false; }
    BOOL IsItemVisible() override { return true; }
    BOOL IsEnabled() override { return true; }
    void DeleteThis() override { delete this; }
    BOOL IsDynamicMenu() override { return false; }
};

class USDMenuToolsExplorerActionItem : public ActionItem
{
    // ActionItem overrides
    int  GetId() override { return usdMenuToolsExplorerActionItemId; }
    BOOL ExecuteAction() override;
    void GetButtonText(MSTR& buttonText) override { buttonText = _T("USD Explorer"); }
    void GetMenuText(MSTR& menuText) override { GetButtonText(menuText); }
    void GetDescriptionText(MSTR& descText) override { GetButtonText(descText); }
    void GetCategoryText(MSTR& catText) override { catText = _T("USD"); }
    BOOL IsChecked() override { return false; }
    BOOL IsItemVisible() override { return true; }
    BOOL IsEnabled() override { return true; }
    void DeleteThis() override { delete this; }
    BOOL IsDynamicMenu() override { return false; }
};

class USDMenuToolsLayerEditorActionItem : public ActionItem
{
    // ActionItem overrides
    int  GetId() override { return usdMenuToolsLayerEditorActionItemId; }
    BOOL ExecuteAction() override;
    void GetButtonText(MSTR& buttonText) override { buttonText = _T("USD Layer Editor"); }
    void GetMenuText(MSTR& menuText) override { GetButtonText(menuText); }
    void GetDescriptionText(MSTR& descText) override { GetButtonText(descText); }
    void GetCategoryText(MSTR& catText) override { catText = _T("USD"); }
    BOOL IsChecked() override { return false; }
    BOOL IsItemVisible() override { return true; }
    BOOL IsEnabled() override { return true; }
    void DeleteThis() override { delete this; }
    BOOL IsDynamicMenu() override { return false; }
};

#ifdef ADSK_ASSET_RESOLVER_ENABLED
class USDMenuToolsPathEditorActionItem : public ActionItem
{
    // ActionItem overrides
    int  GetId() override { return usdMenuToolsPathEditorActionItemId; }
    BOOL ExecuteAction() override;
    void GetButtonText(MSTR& buttonText) override { buttonText = _T("USD Path Editor"); }
    void GetMenuText(MSTR& menuText) override { GetButtonText(menuText); }
    void GetDescriptionText(MSTR& descText) override { GetButtonText(descText); }
    void GetCategoryText(MSTR& catText) override { catText = _T("USD"); }
    BOOL IsChecked() override { return false; }
    BOOL IsItemVisible() override { return true; }
    BOOL IsEnabled() override { return true; }
    void DeleteThis() override { delete this; }
    BOOL IsDynamicMenu() override { return false; }
};
#endif // ADSK_ASSET_RESOLVER_ENABLED

void USDMenuRegisterCallback(void* param, NotifyInfo* info);
void RegisterUSDMenuAction();
#ifdef ADSK_ASSET_RESOLVER_ENABLED
void InitializeAssetResolverSettings(void* param, NotifyInfo* info);
#endif // ADSK_ASSET_RESOLVER_ENABLED
#endif // IS_MAX2025_OR_GREATER
