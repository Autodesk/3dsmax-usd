//
// Copyright 2024 Autodesk
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

#include <MaxUsdObjects/Objects/USDCameraObject.h>

#include <Qt/QMaxParamBlockWidget.h>

namespace Ui {
class UsdCameraObjectRollup;
}

class IParamBlock2;

class UsdCameraObjectRollup : public MaxSDK::QMaxParamBlockWidget
{
    Q_OBJECT

public:
    explicit UsdCameraObjectRollup(ReferenceMaker& owner, IParamBlock2& paramBlock);
    virtual ~UsdCameraObjectRollup() = default;

    void SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock) override;
    void UpdateUI(const TimeValue t) override;
    // Abstract in older max versions, but nothing for us to do in it.
    void UpdateParameterUI(const TimeValue, const ParamID, const int) override { };

private:
    /// Model ParamBlock pointer
    IParamBlock2* paramBlock = nullptr;
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdCameraObjectRollup> ui { std::make_unique<Ui::UsdCameraObjectRollup>() };
    // The camera we are setting up from this UI.
    USDCameraObject* cameraObject = nullptr;
};