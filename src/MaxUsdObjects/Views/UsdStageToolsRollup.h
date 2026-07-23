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

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <Qt/QMaxParamBlockWidget.h>

namespace Ui {
class UsdStageToolsRollup;
}

class IParamBlock2;

class UsdStageToolsRollup : public MaxSDK::QMaxParamBlockWidget,
	public pxr::TfWeakBase // to be able to listen to USD Stage events.
{
    Q_OBJECT

public:
    explicit UsdStageToolsRollup(ReferenceMaker& owner, IParamBlock2& paramBlock);
    ~UsdStageToolsRollup() override;

    void SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock) override;
    void UpdateUI(const TimeValue t) override;
    // Abstract in older max versions, but nothing for us to do in it.
    void UpdateParameterUI(const TimeValue, const ParamID, const int) override {};

public Q_SLOTS:
    /// Qt callback functions based on named widgets in the associated .ui file
    void on_ExploreButton_clicked();
    void on_LayerEditorButton_clicked();
    void on_PathEditorButton_clicked();

private:
    /// Model ParamBlock pointer
    IParamBlock2* paramBlock = nullptr;
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdStageToolsRollup> ui { std::make_unique<Ui::UsdStageToolsRollup>() };
    static void onStageLoadStateChanged(void* param, NotifyInfo* /*info*/);

    /// USDStageObject model pointer
    USDStageObject* modelObj;
};