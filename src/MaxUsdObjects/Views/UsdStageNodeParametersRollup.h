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
class UsdStageNodeParametersRollup;
}

class IParamBlock2;

class UsdStageNodeParametersRollup : public MaxSDK::QMaxParamBlockWidget,
	public pxr::TfWeakBase // to be able to listen to USD Stage events.
{
    Q_OBJECT

public:
    explicit UsdStageNodeParametersRollup(ReferenceMaker& owner, IParamBlock2& paramBlock);
    virtual ~UsdStageNodeParametersRollup();
    void RegisterProgressReporter();

    virtual void SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock) override;
    virtual void UpdateUI(const TimeValue t) override;
    virtual void
    UpdateParameterUI(const TimeValue t, const ParamID paramId, const int tabIndex) override;

public Q_SLOTS:
    /// Qt callback functions based on named widgets in the associated .ui file
    void on_RootLayerPathButton_clicked();
    void on_StageMaskButton_clicked();
    void on_ReloadLayersButton_clicked();
    void on_ClearSessionLayerButton_clicked();
    void on_StageMaskValue_editingFinished();
    void on_ExploreButton_clicked();

private:
    void SelectLayerAndPrim(bool forceFileSelection);

    void UpdateClearSessionLayerButtonState();

    void OnStageChanged(pxr::UsdNotice::StageContentsChanged const& notice);

    /// Model ParamBlock pointer
    IParamBlock2* paramBlock = nullptr;
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdStageNodeParametersRollup> ui {
        std::make_unique<Ui::UsdStageNodeParametersRollup>()
    };
    /// USDStageObject model pointer
    USDStageObject* modelObj;
    /// USD stage notification listener key
    pxr::TfNotice::Key onStageChangeNotice;
};