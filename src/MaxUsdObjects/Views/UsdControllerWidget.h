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

#include <MaxUsdObjects/Objects/USDBaseController.h>

#include <Qt/QMaxParamBlockWidget.h>

#include <ui_UsdControllerWidget.h>

class IParamBlock2;

class UsdControllerWidget final : public MaxSDK::QMaxParamBlockWidget
{
    Q_OBJECT

public:
    explicit UsdControllerWidget(ReferenceMaker& owner, IParamBlock2& paramBlock);
    virtual ~UsdControllerWidget();

    // QMaxParamBlockWidget overrides.
    void SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock) override;
    void UpdateUI(const TimeValue t) override;
    void UpdateParameterUI(const TimeValue t, const ParamID paramId, const int tabIndex) override;

    /**
     * Sets the error message displayed in the path line edit tooltip if the path is wrong. The
     * erroneous path will be appended to the string provided here.
     * @param msg The error message.
     */
    void SetPathErrorMessage(const QString& msg);

    /**
     * Gets the error message displayed in the path line edit tooltip, if the path is wrong. That
     * will be appended to it.
     * @return The error message.
     */
    const QString& GetPathErrorMessage();

    /**
     * Sets the label text for the path line edit.
     * @param label The label text.
     */
    void SetLabel(const QString& label);

    /**
     * Gets the label text for the path line edit.
     * @return The label text.
     */
    QString GetLabel() const;

    /**
     * Sets the tooltip for the path label.
     * @param tooltip The tooltip to use.
     */
    void SetLabelTooltip(const QString& tooltip) const;

    /**
     * Gets the tooltip for the path label.
     * @return The tooltip in use.
     */
    QString GetLabelTooltip() const;

    /**
     * Sets the tooltip for the pick stage button.
     * @param tooltip The tooltip to use.
     */
    void SetPickButtonTooltip(const QString& tooltip) const;

    /**
     * Gets the tooltip for the pick stage button.
     * @return The tooltip in use.
     */
    QString GetPickButtonTooltip() const;

public Q_SLOTS:
    /// Qt callback functions based on named widgets in the associated .ui file
    void on_PickStageButton_clicked();
    void on_ClearButton_clicked() const;
    void on_ObjectPath_textChanged(const QString& text) const;

private:
    /// Model ParamBlock pointer
    IParamBlock2* paramBlock = nullptr;
    /// Reference to the Qt UI View of the rollup
    std::unique_ptr<Ui::UsdControllerWidget> ui { std::make_unique<Ui::UsdControllerWidget>() };
    /// The controller we are setting up from this UI.
    USDBaseController* controller;
    /// The default palette for the xformable path line edit.
    QPalette usdObjectPathBasePalette;
    /// The error message displayed in the path line edit tooltip, if the path is wrong
    QString pathErrorMessage;
};