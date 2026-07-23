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
#include "UsdStageToolsRollup.h"

#include "ui_UsdStageToolsRollup.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

// max sdk
#include <Qt/QmaxToolClips.h>

#include <iparamb2.h>

using namespace MaxSDK;

UsdStageToolsRollup::UsdStageToolsRollup(ReferenceMaker& owner, IParamBlock2& paramBlock)
    : ui(new Ui::UsdStageToolsRollup)
{
    SetParamBlock((ReferenceMaker*)&owner, (IParamBlock2*)&paramBlock);

    ui->setupUi(this);
#ifdef ADSK_ASSET_RESOLVER_ENABLED
    ui->PathEditorButton->show();
#else
    ui->PathEditorButton->hide();
#endif // ADSK_ASSET_RESOLVER_ENABLED
    modelObj = static_cast<USDStageObject*>(&owner);
    RegisterNotification(onStageLoadStateChanged, this, NOTIFY_STAGE_LOAD_STATE_CHANGED);
}

UsdStageToolsRollup::~UsdStageToolsRollup()
{
    UnRegisterNotification(onStageLoadStateChanged, this, NOTIFY_STAGE_LOAD_STATE_CHANGED);
}

void UsdStageToolsRollup::onStageLoadStateChanged(void* param, NotifyInfo* /*info*/)
{
    const auto rollup = static_cast<UsdStageToolsRollup*>(param);
    rollup->UpdateUI(GetCOREInterface()->GetTime());
}

void UsdStageToolsRollup::SetParamBlock(ReferenceMaker* owner, IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    modelObj = static_cast<USDStageObject*>(owner);
}

void UsdStageToolsRollup::UpdateUI(const TimeValue t)
{
    if (!modelObj) {
        return;
    }

    auto stage = modelObj->GetUSDStage();
    if (stage) {
        ui->ExploreButton->setEnabled(true);
        ui->LayerEditorButton->setEnabled(true);
#ifdef ADSK_ASSET_RESOLVER_ENABLED
        ui->PathEditorButton->setEnabled(true);
#endif // ADSK_ASSET_RESOLVER_ENABLED
    } else {
        ui->ExploreButton->setEnabled(false);
        ui->LayerEditorButton->setEnabled(false);
#ifdef ADSK_ASSET_RESOLVER_ENABLED
        ui->PathEditorButton->setEnabled(false);
#endif // ADSK_ASSET_RESOLVER_ENABLED
    }
}

void UsdStageToolsRollup::on_ExploreButton_clicked() { modelObj->OpenInUsdExplorer(); }

void UsdStageToolsRollup::on_LayerEditorButton_clicked() { modelObj->OpenInUsdLayerEditor(); }

void UsdStageToolsRollup::on_PathEditorButton_clicked()
{
#ifdef ADSK_ASSET_RESOLVER_ENABLED
    modelObj->OpenInUsdPathEditor();
#endif // ADSK_ASSET_RESOLVER_ENABLED
}
