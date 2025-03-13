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

#include <QMetaObject>

namespace Ui {
class UsdStageViewportSelectionRollup;
}

class IParamBlock2;

class UsdStageViewportSelectionRollup : public MaxSDK::QMaxParamBlockWidget
{
    Q_OBJECT

public:
    explicit UsdStageViewportSelectionRollup(ReferenceMaker& owner, IParamBlock2& paramBlock);
    ~UsdStageViewportSelectionRollup() override;

    void SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock) override;

#if MAX_VERSION_MAJOR >= 26
    // PreConnectUI and PostConnectUI are only available in 3ds Max 2024 and
    // later.
    void PreConnectUI(const MapID paramMapID) override;
    void PostConnectUI(const MapID paramMapID) override;
#else
    // UpdateUI and UpdateParameterUI provide default implementations in 3ds Max
    // 2024 and later only.
    void UpdateUI(const TimeValue) override {};
    void UpdateParameterUI(const TimeValue, const ParamID, const int) override {};
#endif // MAX_VERSION_MAJOR >= 26

    void UpdateSelectionMode();

private:
    /// Model ParamBlock pointer
    IParamBlock2* paramBlock = nullptr;
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdStageViewportSelectionRollup> ui {
        std::make_unique<Ui::UsdStageViewportSelectionRollup>()
    };
    // USDStageObject model pointer
    USDStageObject* modelObj = nullptr;
    bool            isUpdatingUI = false;
    QObject*        sentinel = nullptr; // this is used to manage the lifetime of signals
};