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

#include "HdMaxDisplayPreferences.h"

#include <MaxUsd/Utilities/OptionUtils.h>

PXR_NAMESPACE_OPEN_SCOPE
TF_DEFINE_PUBLIC_TOKENS(MaxUsdDisplayPreferencesTokens, PXR_MAXUSD_DISPLAY_PREFERENCES_TOKENS);
PXR_NAMESPACE_CLOSE_SCOPE

// Initialize the static instance variable to nullptr
HdMaxDisplayPreferences* HdMaxDisplayPreferences::instance = nullptr;

HdMaxDisplayPreferences::HdMaxDisplayPreferences() { Load(); }

void HdMaxDisplayPreferences::SetSelectionHighlightEnabled(bool enable)
{
    if (enable != GetSelectionHighlightEnabled()) {
        options[pxr::MaxUsdDisplayPreferencesTokens->selectionHighlightEnabled] = enable;
        saveNeeded = true;
    }
}

bool HdMaxDisplayPreferences::GetSelectionHighlightEnabled()
{
    return pxr::VtDictionaryGet<bool>(
        options, pxr::MaxUsdDisplayPreferencesTokens->selectionHighlightEnabled);
}

void HdMaxDisplayPreferences::SetSelectionColor(const AColor& color)
{
    if (color != GetSelectionColor()) {
        options[pxr::MaxUsdDisplayPreferencesTokens->selectionColor] = color;
        saveNeeded = true;
    }
}

const AColor& HdMaxDisplayPreferences::GetSelectionColor()
{
    return pxr::VtDictionaryGet<AColor>(
        options, pxr::MaxUsdDisplayPreferencesTokens->selectionColor);
}

const pxr::VtDictionary& HdMaxDisplayPreferences::GetDefaultDictionary()
{
    static pxr::VtDictionary d;
    static std::once_flag    once;
    std::call_once(once, []() {
        d[pxr::MaxUsdDisplayPreferencesTokens->version] = 1;
        // Base defaults.
        d[pxr::MaxUsdDisplayPreferencesTokens->selectionColor] = AColor { 1.0f, 0.0f, 0.0f, 1.0f };
        d[pxr::MaxUsdDisplayPreferencesTokens->selectionHighlightEnabled] = true;
    });
    return d;
}

namespace {
const std::string optionsCategoryKey = "DisplayPreferences";
}

void HdMaxDisplayPreferences::Load()
{
    MaxUsd::OptionUtils::LoadUiOptions(optionsCategoryKey, options, GetDefaultDictionary());
}

void HdMaxDisplayPreferences::Save()
{
    if (saveNeeded) {
        MaxUsd::OptionUtils::SaveUiOptions(optionsCategoryKey, options);
        saveNeeded = false;
    }
}
