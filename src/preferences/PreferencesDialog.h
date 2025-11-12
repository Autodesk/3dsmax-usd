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

#include "PreferencesExport.h"
#include "PreferencesOptions.h"
#include "PreferenceApplicationHost.h"

#include <QDialog>

namespace Adsk{
    class USDAssetResolverSettingsWidget;
} // namespace Adsk
namespace Ui {
class PreferencesDialog;
} // namespace Ui


class MaxUsdPreferencesAPI UsdPreferencesDialog : public QDialog
{
	Q_OBJECT
public:
    UsdPreferencesDialog(const UsdPreferenceOptions& options, QWidget* parent = nullptr);
    ~UsdPreferencesDialog() override;

    /// Get the options from the dialog UI
    const UsdPreferenceOptions getOptions() const;

protected:
    /// Load the options into the dialog UI
    void loadOptions(const UsdPreferenceOptions& options);

    /// Reference to the Qt UI View of the dialog:
    std::unique_ptr<Ui::PreferencesDialog> ui { std::make_unique<Ui::PreferencesDialog>() };

    Adsk::USDAssetResolverSettingsWidget* assetResolverSettingsWidget { nullptr };
};