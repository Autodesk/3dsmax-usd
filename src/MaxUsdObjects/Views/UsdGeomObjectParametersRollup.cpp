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
#include "UsdGeomObjectParametersRollup.h"

#include "ui_UsdGeomObjectParametersRollup.h"

#include <MaxUsdObjects/FindUSDGeomObjectsProc.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <Qt/QmaxToolClips.h>

#include <maxicon.h>

UsdGeomObjectParametersRollup::UsdGeomObjectParametersRollup(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock)
    : ui(new Ui::UsdGeomObjectParametersRollup)
{
    SetParamBlock(&owner, &paramBlock);
    geomObject = static_cast<USDGeomObject*>(&owner);
    ui->setupUi(this);

    ui->PathLineEdit->setMinimumHeight(MaxSDK::UIScaled(20));
    ui->PathLineEdit->setAlignment({ Qt::AlignLeft | Qt::AlignVCenter });

    // 3dsMax tooltip styling doesn't handle long strings very well.
    MaxSDK::QmaxToolClips::disableToolClip(ui->PathLineEdit);

    ui->RefreshButton->setIcon(MaxSDK::LoadMaxMultiResIcon("StateSets\\Refresh.png"));
    const int iconSize = MaxSDK::UIScaled(16);
    ui->RefreshButton->setIconSize(QSize(iconSize, iconSize));
    UpdateRefreshButtonState();

    // Update icon warning of conflicts on the "show source" setting with
    // other USDGeomObjects sharing some source prims.
    UpdateShowSourceConflictWarning();
}

void UsdGeomObjectParametersRollup::SetParamBlock(
    ReferenceMaker*     owner,
    IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    geomObject = static_cast<USDGeomObject*>(owner);
}

void UsdGeomObjectParametersRollup::UpdateUI(const TimeValue t)
{
    const auto prim = geomObject->GetPrim();
    if (!prim.IsValid()) {
        ui->PathLineEdit->setText(QObject::tr("N/A"));
        return;
    }

    const auto pathStr = QString::fromStdString(prim.GetPath().GetString());
    ui->PathLineEdit->setText(pathStr); // Might be elided on long path, so also set as tooltip.

    QString tooltip = QString("Stage: ");
    tooltip.append(QString::fromWCharArray(geomObject->GetStageNode()->GetName()));
    tooltip.append("\nPrim Path: ");
    tooltip.append(pathStr);
    ui->PathLineEdit->setToolTip(tooltip);

    UpdateShowSourceConflictWarning();
    UpdateRefreshButtonState();
}

void UsdGeomObjectParametersRollup::UpdateShowSourceConflictWarning() const
{
    const auto             stageNode = geomObject->GetStageNode();
    FindUSDGeomObjectsProc proc(stageNode);
    stageNode->DoEnumDependents(&proc);

    const auto& geomNodes = proc.GetNodes();
    if (geomNodes.empty()) {
        return;
    }

    // Compare the "show source" setting with other USDGeomObject dependants
    // of the same USD stage object.

    const auto thisPrimPath = geomObject->GetPrimPath();
    const bool thisShowSource = MaxUsd::GetParamBlockValue<BOOL>(
        geomObject->GetParamBlock(0), USDGeomObjectParams_ShowSource);

    std::vector<std::wstring> conflictingObjects;

    for (const auto& entry : geomNodes) {

        const auto& geom = entry.second;

        if (geom == geomObject) {
            continue;
        }

        // Same config, no conflict.
        if (thisShowSource
            == MaxUsd::GetParamBlockValue<BOOL>(
                geom->GetParamBlock(0), USDGeomObjectParams_ShowSource)) {
            continue;
        }

        // Is there a ancestor/descendant relationship between the source prims? If not, no
        // problem.
        auto path = geom->GetPrimPath();
        if (!path.HasPrefix(thisPrimPath) && !thisPrimPath.HasPrefix(path)) {
            continue;
        }

        // We have a conflict.
        const auto& node = entry.first;
        conflictingObjects.push_back(node->GetName());
    }

    // Always reserve some space for the icon, had QT display refresh issues otherwise (icon
    // appearing cropped for half a second).
    const auto iconSize = QSize(MaxSDK::GetUIScaleFactor() * 16, MaxSDK::GetUIScaleFactor() * 16);
    ui->WarningLabel->setFixedSize(iconSize);

    if (!conflictingObjects.empty()) {
        ui->WarningLabel->setPixmap(
            style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(iconSize));
        QString conflictMsg = UsdGeomObjectParametersRollup::tr(
            "The following objects have conflicting Show Source configurations: ");
        for (int i = 0; i < conflictingObjects.size(); ++i) {
            conflictMsg.append(QString::fromStdWString(conflictingObjects[i]));
            if (i != conflictingObjects.size() - 1) {
                conflictMsg.append(", ");
            }
        }
        ui->WarningLabel->setToolTip(conflictMsg);
    } else {
        ui->WarningLabel->setPixmap({});
        ui->WarningLabel->setToolTip({});
    }
}

void UsdGeomObjectParametersRollup::on_RefreshButton_clicked() { geomObject->Refresh(); }

void UsdGeomObjectParametersRollup::UpdateRefreshButtonState()
{
    const auto liveUpdate
        = MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_LiveUpdates);
    ui->RefreshButton->setEnabled(!liveUpdate);
}
