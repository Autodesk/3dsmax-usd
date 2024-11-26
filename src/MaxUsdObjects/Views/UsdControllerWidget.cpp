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
#include "UsdControllerWidget.h"

#include "ui_UsdControllerWidget.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <Qt/QmaxToolClips.h>
#include <maxscript/maxscript.h>

#include <iparamb2.h>
#include <maxicon.h>

namespace {
/**
 * A 3dsMax Pick mode to pick USD Stage objects in the viewport and scene explorer
 * for a USD controller.
 */
class PickStageMode
    : public PickModeCallback
    , PickNodeCallback
{
public:
    static PickStageMode* GetInstance()
    {
        if (instance == nullptr) {
            instance = std::unique_ptr<PickStageMode>(new PickStageMode());
        }
        return instance.get();
    }

    void Setup(QPushButton* button, IParamBlock2* paramBlock)
    {
        this->button = button;
        this->paramBlock = paramBlock;
    }

    BOOL HitTest(IObjParam* ip, HWND hWnd, ViewExp* vpt, IPoint2 m, int flags) override
    {
        INode* node = ip->PickNode(hWnd, m);
        return Filter(node);
    }

    BOOL Pick(IObjParam* ip, ViewExp* vpt) override
    {
        if (!paramBlock) {
            return FALSE;
        }

        if (INode* node = vpt->GetClosestHit()) {
            if (dynamic_cast<USDStageObject*>(node->GetObjectRef()->FindBaseObject())) {
                paramBlock->SetValue(USDControllerParams_USDStage, 0, node);
                return TRUE;
            }
        }
        return FALSE;
    }
    void EnterMode(IObjParam* ip) override
    {
        if (button) {
            button->setCheckable(true);
            button->setChecked(true);
        }
    }
    void ExitMode(IObjParam* ip) override
    {
        if (button) {
            button->setChecked(false);
            button->setCheckable(false);
        }
        button = nullptr;
        paramBlock = nullptr;
    }
    BOOL RightClick(IObjParam* ip, ViewExp* vpt) override { return TRUE; }

    PickNodeCallback* GetFilter() override { return this; }

    BOOL Filter(INode* node) override
    {
        if (!node) {
            return FALSE;
        }
        return node->GetObjectRef()->FindBaseObject()->ClassID() == USDSTAGEOBJECT_CLASS_ID;
    }

    QPointer<QPushButton> GetButton() const { return button; }

private:
    PickStageMode() = default;

    /// The pick button.
    QPushButton* button = nullptr;
    /// The paramblock of the controller, where to set the stage node reference.
    IParamBlock2* paramBlock = nullptr;
    // Singleton instance.
    static std::unique_ptr<PickStageMode> instance;
};
std::unique_ptr<PickStageMode> PickStageMode::instance = nullptr;
} // namespace

UsdControllerWidget::UsdControllerWidget(ReferenceMaker& owner, IParamBlock2& paramBlock)
    : ui(new Ui::UsdControllerWidget)
{
    SetParamBlock(&owner, &paramBlock);
    controller = static_cast<USDBaseController*>(&owner);

    ui->setupUi(this);
    ui->ClearButton->setIcon(MaxSDK::LoadMaxMultiResIcon(
        "CommandPanel/Motion/BipedRollout/CopyAndPaste/DeleteSelectedPosture"));
    // Disable max styling for tooltips - long strings are not well supported.
    MaxSDK::QmaxToolClips::disableToolClip(ui->ObjectPath);
    // The line edit's border color is used to communicate errors - keep a copy of the original
    // palette.
    usdObjectPathBasePalette = ui->ObjectPath->palette();

    UpdateUI(GetCOREInterface()->GetTime());
}

UsdControllerWidget::~UsdControllerWidget()
{
    // Abort any ongoing pick if the widget is destroyed.
    const auto pickMode = PickStageMode::GetInstance();
    if (GetCOREInterface()->GetCurPickMode() == pickMode) {
        GetCOREInterface()->ClearPickMode();
    }
}

void UsdControllerWidget::SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    controller = static_cast<USDBaseController*>(owner);
}

void UsdControllerWidget::UpdateUI(const TimeValue t)
{
    UpdateParameterUI(t, USDControllerParams_USDStage, 0);
    UpdateParameterUI(t, USDControllerParams_Path, 0);
}

void UsdControllerWidget::UpdateParameterUI(
    const TimeValue t,
    const ParamID   paramId,
    const int /*tabIndex*/)
{
    if (!paramBlock) {
        return;
    }

    if (USDControllerParams_Path == paramId) {
        auto         tooltip = QString();
        const MCHAR* primPathStr = nullptr;
        Interval     valid = FOREVER;
        paramBlock->GetValue(
            USDControllerParams_Path, GetCOREInterface()->GetTime(), primPathStr, valid);
        const auto pathQStr = QString::fromStdWString(primPathStr);

        if (pathQStr != ui->ObjectPath->text()) {
            ui->ObjectPath->setText(pathQStr);
        }

        if (!pathQStr.isEmpty()) {
            tooltip = pathQStr;
        }
        // Changes the border to red if the path is bad and adjust the tooltip!
        if (!controller->IsSourceObjectValid() && !pathQStr.isEmpty()) {
            QPalette              palette = ui->ObjectPath->palette();
            static constexpr auto borderColor = QColor(189, 59, 49);

            // Depending on where in 3dsMax the widget is placed, the max styling acts up
            // a bit differently. The border color is either AlternateBase, or Window, both
            // can be set without adverse effects.
            palette.setColor(QPalette::AlternateBase, borderColor);
            palette.setColor(QPalette::Window, borderColor);

            ui->ObjectPath->setPalette(palette);
            tooltip = GetPathErrorMessage() + tooltip;
        } else {
            ui->ObjectPath->setPalette(usdObjectPathBasePalette);
        }
        ui->ObjectPath->setToolTip(tooltip);
    }

    if (USDControllerParams_USDStage == paramId) {
        INode*   stageNode = nullptr;
        Interval valid = FOREVER;
        paramBlock->GetValue(
            USDControllerParams_USDStage, GetCOREInterface()->GetTime(), stageNode, valid, 0);
        if (!stageNode) {
            ui->PickStageButton->setText("None");
            return;
        }
        ui->PickStageButton->setText(QString::fromStdWString(stageNode->GetName()));
    }
}

void UsdControllerWidget::SetPathErrorMessage(const QString& msg) { pathErrorMessage = msg; }

const QString& UsdControllerWidget::GetPathErrorMessage() { return pathErrorMessage; }

void UsdControllerWidget::SetLabel(const QString& label) { ui->ObjectPathLabel->setText(label); }

QString UsdControllerWidget::GetLabel() const { return ui->ObjectPathLabel->text(); }

void UsdControllerWidget::SetLabelTooltip(const QString& tooltip) const
{
    ui->ObjectPathLabel->setToolTip(tooltip);
}

QString UsdControllerWidget::GetLabelTooltip() const { return ui->ObjectPathLabel->toolTip(); }

void UsdControllerWidget::SetPickButtonTooltip(const QString& tooltip) const
{
    ui->PickStageButton->setToolTip(tooltip);
}

QString UsdControllerWidget::GetPickButtonTooltip() const { return ui->PickStageButton->toolTip(); }

void UsdControllerWidget::on_PickStageButton_clicked()
{
    const auto pickMode = PickStageMode::GetInstance();
    const bool togglingOff = pickMode->GetButton() == ui->PickStageButton;
    // First exit any ongoing pick modes.
    GetCOREInterface()->ClearPickMode();
    // If we are toggling the same pick mode button, we are done.
    if (togglingOff) {
        return;
    }
    pickMode->Setup(ui->PickStageButton, paramBlock);
    GetCOREInterface()->SetPickMode(pickMode);
}

void UsdControllerWidget::on_ClearButton_clicked() const
{
    const auto pickMode = PickStageMode::GetInstance();
    if (GetCOREInterface()->GetCurPickMode() == pickMode) {
        GetCOREInterface()->ClearPickMode();
    }
    paramBlock->SetValue(USDControllerParams_USDStage, 0, static_cast<INode*>(nullptr));
}

void UsdControllerWidget::on_ObjectPath_textChanged(const QString& text) const
{
    paramBlock->SetValue(USDControllerParams_Path, 0, text.toStdWString().c_str());
}
