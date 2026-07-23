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
#pragma once

#include <MaxUsd/Utilities/MaxSupportUtils.h> // needed before the 3ds Max version check macros

#ifdef ADSK_ASSET_RESOLVER_ENABLED
#include <AssetResolverExtensions/PathDialog/PathDialog.h>
#include <pxr/usd/usd/stage.h>
#endif // ADSK_ASSET_RESOLVER_ENABLED

namespace AssetResolverSettingsManagement {

#ifdef ADSK_ASSET_RESOLVER_ENABLED
// Initialize the USD Asset Resolver Settings, must be called once at startup
void InitializeSettings();

// Saves the given USD Asset Resolver Settings
void SaveSettings(const Adsk::AssetResolverSettings& options);

// Display the USD Preferences dialog
void ShowDialog(
    const Adsk::AssetResolverPathDialog::Tab& tab
    = Adsk::AssetResolverPathDialog::Tab::GlobalSettings,
    PXR_NS::UsdStageRefPtr stage = {});
#endif // ADSK_ASSET_RESOLVER_ENABLED

} // namespace AssetResolverSettingsManagement
