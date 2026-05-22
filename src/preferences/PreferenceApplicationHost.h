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

#include "PreferencesExport.h"

#include <MaxUsd/Utilities/MaxSupportUtils.h>

#ifdef IS_MAX2026_OR_GREATER

#include <AssetResolverPreferences/ApplicationHost.h>

class MaxUsdPreferencesAPI PreferenceApplicationHost : public Adsk::ApplicationHost
{
public:
    static void CreateInstance(QObject* parent = nullptr);

    float uiScale() const override;
    QIcon icon(const IconName& name) const override;
    int   pm(const PixelMetric& metric) const override;

protected:
    PreferenceApplicationHost(QObject* parent = nullptr);
    ~PreferenceApplicationHost() override = default;

    static PreferenceApplicationHost* s_instance;
};

#endif // IS_MAX2026_OR_GREATER