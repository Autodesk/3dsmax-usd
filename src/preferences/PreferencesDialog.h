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
#include <MaxUsd/Utilities/MaxSupportUtils.h> // needed before the 3ds Max version check macros

#ifdef IS_MAX2026_OR_GREATER
#include <AssetResolverPreferences/AssetResolverSettings.h>
#endif
#include <QDialog>

#ifdef IS_MAX2026_OR_GREATER
namespace Adsk{
    class USDAssetResolverSettingsWidget;
} // namespace Adsk
#endif
namespace Ui {
class PreferencesDialog;
} // namespace Ui


class MaxUsdPreferencesAPI UsdPreferencesDialog : public QDialog
{
	Q_OBJECT
public:
    UsdPreferencesDialog(QWidget* parent = nullptr);
    ~UsdPreferencesDialog() override;

#ifdef IS_MAX2026_OR_GREATER
    /// Get the options from the dialog UI
    const Adsk::AssetResolverSettings getOptions() const;
#endif

Q_SIGNALS:
    void geometryChanged(const QRect& geometry);

protected:
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    /// Reference to the Qt UI View of the dialog:
    std::unique_ptr<Ui::PreferencesDialog> ui { std::make_unique<Ui::PreferencesDialog>() };

#ifdef IS_MAX2026_OR_GREATER
    Adsk::USDAssetResolverSettingsWidget* assetResolverSettingsWidget { nullptr };
#endif
};
