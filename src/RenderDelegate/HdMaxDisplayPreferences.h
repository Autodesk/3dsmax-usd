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
#include "MaxUsd/Utilities/DictionaryOptionProvider.h"
#include "RenderDelegateAPI.h"

#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <pxr/base/vt/dictionary.h>

#pragma once
#ifdef IS_MAX2025_OR_GREATER
#include <Geom/acolor.h>
#else
#include <acolor.h>
#endif

PXR_NAMESPACE_OPEN_SCOPE
// clang-format off
#define PXR_MAXUSD_DISPLAY_PREFERENCES_TOKENS \
	/* Dictionary keys */ \
	(version) \
	(selectionColor) \
	(selectionHighlightEnabled)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(
    MaxUsdDisplayPreferencesTokens,
    RenderDelegateAPI,
    PXR_MAXUSD_DISPLAY_PREFERENCES_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

class RenderDelegateAPI HdMaxDisplayPreferences : MAXUSD_NS_DEF::DictionaryOptionProvider
{
public:
    /**
     * \Brief Get the instance of the class
     * @return The instance of the class
     */
    static HdMaxDisplayPreferences& GetInstance()
    {
        if (!instance) {
            instance = new HdMaxDisplayPreferences();
        }
        return *instance;
    }

    // Delete the copy constructor and assignment operator
    HdMaxDisplayPreferences(const HdMaxDisplayPreferences&) = delete;
    HdMaxDisplayPreferences& operator=(const HdMaxDisplayPreferences&) = delete;

    /**
     * \brief Enable selection highlighting, globally (all stage objects can share this setting).
     * \param enable True if selection highlighting should be enabled.
     */
    void SetSelectionHighlightEnabled(bool enable);

    /**
     * \brief Returns whether selection highlighting is enabled globally.
     * \return True if enabled.
     */
    bool GetSelectionHighlightEnabled();

    /**
     * \brief Sets the color used for selection highlighting.
     * \param color The color to use.
     */
    void SetSelectionColor(const AColor& color);

    /**
     * \brief Gets the color used for selection highlighting.
     * \return The color.
     */
    const AColor& GetSelectionColor();

    /**
     * \brief Saves the display preferences to the user saved preferences.
     */
    void Save();

private:
    // Private constructor/destructor
    HdMaxDisplayPreferences();
    ~HdMaxDisplayPreferences() = default;

    // The instance of the class
    static HdMaxDisplayPreferences* instance;

    /**
     * \brief Returns the default dictionary for the Display Preferences options.
     * \return The default dictionary for the Display Preferences options.
     */
    const pxr::VtDictionary& GetDefaultDictionary();

    /**
     * \brief Loads the display preferences from the user saved preferences.
     * If nothing was saved, the default values are used.
     */
    void Load();

    bool saveNeeded = false;
};
