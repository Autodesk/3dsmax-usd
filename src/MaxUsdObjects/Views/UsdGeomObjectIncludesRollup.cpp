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
#include "UsdGeomObjectIncludesRollup.h"

#include "ui_UsdGeomObjectIncludesRollup.h"

#include <MaxUsdObjects/FindUSDGeomObjectsProc.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <Qt/QmaxToolClips.h>
#include <maxscript/maxscript.h>

UsdGeomObjectIncludesRollup::UsdGeomObjectIncludesRollup(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock)
    : ui(new Ui::UsdGeomObjectIncludesRollup)
{
    SetParamBlock(&owner, &paramBlock);
    geomObject = static_cast<USDGeomObject*>(&owner);
    ui->setupUi(this);

    // No auto-binding for ViewportDisplayPurposes
    UpdatePurposesUI();

    connect(ui->ActiveRadioButton, &QRadioButton::toggled, [this](bool checked) {
        if (checked) {
            theHold.Begin();
            this->paramBlock->SetValue(USDGeomObjectParams_ViewportDisplayPurposes, 0, true);
            theHold.Accept(L"Parameter Change");
            ui->CustomPurposesWidget->setEnabled(false);
        }
    });
    connect(ui->CustomRadioButton, &QRadioButton::toggled, [this](bool checked) {
        if (checked) {
            theHold.Begin();
            this->paramBlock->SetValue(USDGeomObjectParams_ViewportDisplayPurposes, 0, false);
            theHold.Accept(L"Parameter Change");
            ui->CustomPurposesWidget->setEnabled(true);
        }
    });

    // Update icon showing warnings on conflicting purpose include settings between USD Geom
    // Objects.
    UpdatePurposeConfigConflictWarning();
}

void UsdGeomObjectIncludesRollup::SetParamBlock(
    ReferenceMaker*     owner,
    IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    geomObject = static_cast<USDGeomObject*>(owner);
}

void UsdGeomObjectIncludesRollup::UpdateUI(const TimeValue t)
{
    UpdateParameterUI(
        GetCOREInterface()->GetTime(), USDGeomObjectParams_ViewportDisplayPurposes, t);

    UpdatePurposeConfigConflictWarning();
}

void UsdGeomObjectIncludesRollup::UpdateParameterUI(
    const TimeValue time_value,
    const ParamID   paramId,
    const int       i)
{
    if (USDGeomObjectParams_ViewportDisplayPurposes == paramId) {
        UpdatePurposesUI();
    }
}

void UsdGeomObjectIncludesRollup::UpdatePurposesUI()
{
    const auto useActivePurposes
        = MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_ViewportDisplayPurposes);

    if (!ui->ActiveRadioButton->isChecked()) {
        ui->ActiveRadioButton->setChecked(useActivePurposes);
    }
    if (!ui->CustomRadioButton->isChecked()) {
        ui->CustomRadioButton->setChecked(!useActivePurposes);
    }
    if (ui->CustomPurposesWidget->isEnabled() != !useActivePurposes) {
        ui->CustomPurposesWidget->setEnabled(!useActivePurposes);
    }
}

void UsdGeomObjectIncludesRollup::UpdatePurposeConfigConflictWarning() const
{
    const auto             stageNode = geomObject->GetStageNode();
    FindUSDGeomObjectsProc proc(stageNode);
    stageNode->DoEnumDependents(&proc);

    const auto& geomNodes = proc.GetNodes();
    if (geomNodes.empty()) {
        return;
    }

    const auto stageObject = geomObject->GetStageObject();
    if (!stageObject) {
        return;
    }

    // Compare included purposes with the included purposes of other USDGeomObject
    // dependants of the same stage object.

    const auto thisPrimPath = geomObject->GetPrimPath();
    const auto pb = geomObject->GetParamBlock(0);
    const bool useVpPurposes
        = MaxUsd::GetParamBlockValue<BOOL>(pb, USDGeomObjectParams_ViewportDisplayPurposes);

    std::vector<std::wstring> conflictingObjects;

    bool includeProxy, includeRender, includeGuide;
    if (useVpPurposes) {
        const auto pbStage = geomObject->GetStageObject()->GetParamBlock(0);
        includeProxy = MaxUsd::GetParamBlockValue<BOOL>(pbStage, DisplayProxy);
        includeRender = MaxUsd::GetParamBlockValue<BOOL>(pbStage, DisplayRender);
        includeGuide = MaxUsd::GetParamBlockValue<BOOL>(pbStage, DisplayGuide);
    } else {
        includeProxy = MaxUsd::GetParamBlockValue<BOOL>(pb, USDGeomObjectParams_IncludeProxy);
        includeRender = MaxUsd::GetParamBlockValue<BOOL>(pb, USDGeomObjectParams_IncludeRender);
        includeGuide = MaxUsd::GetParamBlockValue<BOOL>(pb, USDGeomObjectParams_IncludeGuide);
    }

    for (const auto& entry : geomNodes) {

        const auto& geom = entry.second;

        if (geom == geomObject) {
            continue;
        }

        // Is there a ancestor/descendant relationship between the source prims? If not, no
        // problem.
        auto path = geom->GetPrimPath();
        if (!path.HasPrefix(thisPrimPath) && !thisPrimPath.HasPrefix(path)) {
            continue;
        }

        const auto otherPb = geom->GetParamBlock(0);
        const bool otherGeomUseVp = MaxUsd::GetParamBlockValue<BOOL>(
            otherPb, USDGeomObjectParams_ViewportDisplayPurposes);

        // Both using viewport purposes, no conflict.
        if (useVpPurposes && otherGeomUseVp) {
            continue;
        }

        bool otherProxy, otherRender, otherGuide;
        if (otherGeomUseVp) {
            const auto pbStage = stageObject->GetParamBlock(0);
            otherProxy = MaxUsd::GetParamBlockValue<BOOL>(pbStage, DisplayProxy);
            otherRender = MaxUsd::GetParamBlockValue<BOOL>(pbStage, DisplayRender);
            otherGuide = MaxUsd::GetParamBlockValue<BOOL>(pbStage, DisplayGuide);
        } else {
            otherProxy
                = MaxUsd::GetParamBlockValue<BOOL>(otherPb, USDGeomObjectParams_IncludeProxy);
            otherRender
                = MaxUsd::GetParamBlockValue<BOOL>(otherPb, USDGeomObjectParams_IncludeRender);
            otherGuide
                = MaxUsd::GetParamBlockValue<BOOL>(otherPb, USDGeomObjectParams_IncludeGuide);
        }

        if (includeProxy == otherProxy && includeRender == otherRender
            && includeGuide == otherGuide) {
            continue;
        }

        // We have a conflict.
        const auto& node = entry.first;
        conflictingObjects.push_back(node->GetName());
    }

    // Always reserve some space for the icon, had QT display refresh issues otherwise (icon
    // appearing cropped for half a second).
    const auto iconSize = QSize(MaxSDK::GetUIScaleFactor() * 16, MaxSDK::GetUIScaleFactor() * 16);
    ui->ActiveWarning->setFixedSize(iconSize);
    ui->CustomWarning->setFixedSize(iconSize);

    if (!conflictingObjects.empty()) {

        QString conflictMsg
            = tr("The following objects have conflicting Display Purpose configurations: ");
        for (int i = 0; i < conflictingObjects.size(); ++i) {
            conflictMsg.append(QString::fromStdWString(conflictingObjects[i]));
            if (i != conflictingObjects.size() - 1) {
                conflictMsg.append(", ");
            }
        }
        if (useVpPurposes) {
            ui->ActiveWarning->setPixmap(
                style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(iconSize));
            ui->ActiveWarning->setToolTip(conflictMsg);
            ui->CustomWarning->setPixmap({});
            ui->CustomWarning->setToolTip({});
        } else {
            ui->CustomWarning->setPixmap(
                style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(iconSize));
            ui->CustomWarning->setToolTip(conflictMsg);
            ui->ActiveWarning->setPixmap({});
            ui->ActiveWarning->setToolTip({});
        }

    } else {
        ui->ActiveWarning->setPixmap({});
        ui->ActiveWarning->setToolTip({});
        ui->CustomWarning->setPixmap({});
        ui->CustomWarning->setToolTip({});
    }
}