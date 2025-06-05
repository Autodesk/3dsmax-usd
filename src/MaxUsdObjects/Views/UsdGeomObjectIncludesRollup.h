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
class UsdGeomObjectIncludesRollup;
}

class IParamBlock2;

class UsdGeomObjectIncludesRollup : public MaxSDK::QMaxParamBlockWidget
{
    Q_OBJECT

public:
    explicit UsdGeomObjectIncludesRollup(ReferenceMaker& owner, IParamBlock2& paramBlock);
    ~UsdGeomObjectIncludesRollup() override = default;

    void SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock) override;
    void UpdateUI(const TimeValue t) override;
    void UpdateParameterUI(const TimeValue, const ParamID, const int) override;

private:
    void UpdatePurposesUI();

    /**
     * We do not support USDGeomObjects promoted from ancestor and descendant prims
     * having different configurations for what render purposes should be included.
     * When we find such conflicts, inform the user by displaying a warning icon in the
     * UI. This function refreshes the state of this icon, based on any current
     * conflict(s).
     * */
    void UpdatePurposeConfigConflictWarning() const;

    /// Model ParamBlock pointer
    IParamBlock2* paramBlock = nullptr;
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdGeomObjectIncludesRollup> ui {
        std::make_unique<Ui::UsdGeomObjectIncludesRollup>()
    };
    // The USDGeomObject object we are setting up from this UI.
    USDGeomObject* geomObject = nullptr;
};