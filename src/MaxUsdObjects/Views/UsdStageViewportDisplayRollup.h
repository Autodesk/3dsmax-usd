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
#pragma once

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <Qt/QMaxParamBlockWidget.h>

namespace Ui {
class UsdStageViewportDisplayRollup;
}

class IParamBlock2;

class UsdStageViewportDisplayRollup : public MaxSDK::QMaxParamBlockWidget
{
    Q_OBJECT

public:
    explicit UsdStageViewportDisplayRollup(ReferenceMaker& owner, IParamBlock2& paramBlock);
    virtual ~UsdStageViewportDisplayRollup();

    void SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock) override;
    void UpdateUI(const TimeValue t) override;
    void UpdateParameterUI(const TimeValue t, const ParamID paramId, const int tabIndex) override;

public Q_SLOTS:
    /// Qt callback functions based on named widgets in the associated .ui file
    void on_InvertProxyRenderButton_clicked();
    void on_PointInstancesDrawModesRebuildButton_clicked() const;

private:
    /// Model ParamBlock pointer
    IParamBlock2* paramBlock = nullptr;
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdStageViewportDisplayRollup> ui {
        std::make_unique<Ui::UsdStageViewportDisplayRollup>()
    };
    // USDStageObject model pointer
    USDStageObject* modelObj;
};