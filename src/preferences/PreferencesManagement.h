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

#include "PreferencesExport.h"
#include <MaxUsd/Utilities/MaxSupportUtils.h> // needed before the 3ds Max version check macros

namespace PreferencesManagement {

#ifdef IS_MAX2026_OR_GREATER
// Initialize the USD Preferences system, must be called once at startup
MaxUsdPreferencesAPI void InitializeUsdPreferences();
#endif

// Display the USD Preferences dialog
MaxUsdPreferencesAPI void ShowPreferencesDialog();

} // namespace PreferencesManagement
