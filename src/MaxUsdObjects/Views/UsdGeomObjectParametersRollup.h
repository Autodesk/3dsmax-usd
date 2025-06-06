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

#include <MaxUsdObjects/Objects/USDGeomObject.h>

#include <Qt/QMaxParamBlockWidget.h>

namespace Ui {
class UsdGeomObjectParametersRollup;
}

class IParamBlock2;

class UsdGeomObjectParametersRollup : public MaxSDK::QMaxParamBlockWidget
{
    Q_OBJECT

public:
    explicit UsdGeomObjectParametersRollup(ReferenceMaker& owner, IParamBlock2& paramBlock);
    virtual ~UsdGeomObjectParametersRollup() = default;

    void SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock) override;
    void UpdateUI(const TimeValue t) override;
    // Abstract in older max versions, but nothing for us to do in it.
    void UpdateParameterUI(const TimeValue, const ParamID, const int) override {};

    /**
     * We do not support USDGeomObjects promoted from ancestor and descendant prims
     * having different configurations for the "Show Source" option.
     * When we find such a conflict, we inform the user by displaying a warning icon next
     * to the checkbox. This function refreshes the state of this icon, based on any current
     * conflict(s).
     * */
    void UpdateShowSourceConflictWarning() const;

    /**
     * Updates the enabled state of the refresh button, the button is disabled
     * when "live geometry updates" is turned on.
     * */
    void UpdateRefreshButtonState();

public Q_SLOTS:
    /// Qt callback functions based on named widgets in the associated .ui file
    void on_RefreshButton_clicked();

private:
    /// Model ParamBlock pointer
    IParamBlock2* paramBlock = nullptr;
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdGeomObjectParametersRollup> ui {
        std::make_unique<Ui::UsdGeomObjectParametersRollup>()
    };
    // The geom object we are setting up from this UI.
    USDGeomObject* geomObject = nullptr;
};