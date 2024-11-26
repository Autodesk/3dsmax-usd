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

#include "UsdStageViewportSelectionRollup.h"

#include "Ui_UsdStageViewportSelectionRollup.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <RenderDelegate/HdMaxDisplayPreferences.h>

#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/kind/registry.h>

#include <Qt/QMaxColorSwatch.h>

#include <GetCOREInterface.h>
#include <iparamb2.h>
#include <maxapi.h>
#include <notify.h>

using namespace MaxSDK;

static void NotifySubObjectLevelChanged(void* param, NotifyInfo* info)
{
    if (!info->callParam) {
        return;
    }
    const auto rollup = static_cast<UsdStageViewportSelectionRollup*>(param);
    rollup->UpdateSelectionMode();
}

UsdStageViewportSelectionRollup::UsdStageViewportSelectionRollup(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock)
    : ui(new Ui::UsdStageViewportSelectionRollup)
{
    SetParamBlock((ReferenceMaker*)&owner, (IParamBlock2*)&paramBlock);
    ui->setupUi(this);
    modelObj = static_cast<USDStageObject*>(&owner);

    connect(ui->stageRadioButton, &QRadioButton::toggled, [this](bool checked) {
        if (checked) {
            GetCOREInterface()->SetSubObjectLevel(static_cast<int>(SelectionMode::Stage));
        }
    });
    connect(ui->primRadioButton, &QRadioButton::toggled, [this](bool checked) {
        if (checked) {
            GetCOREInterface()->SetSubObjectLevel(static_cast<int>(SelectionMode::Prim));
        }
    });

    const auto selectionLevel = static_cast<SelectionMode>(modelObj->GetSubObjectLevel());
    selectionLevel == SelectionMode::Stage ? ui->stageRadioButton->setChecked(true)
                                           : ui->primRadioButton->setChecked(true);

    RegisterNotification(NotifySubObjectLevelChanged, this, NOTIFY_MODPANEL_SUBOBJECTLEVEL_CHANGED);

    // Kind selection UI setup.
    const static auto                      noneToken = pxr::TfToken("none");
    const static std::vector<pxr::TfToken> baseKindEntries = { noneToken,
                                                               pxr::KindTokens->model,
                                                               pxr::KindTokens->subcomponent,
                                                               pxr::KindTokens->component,
                                                               pxr::KindTokens->group,
                                                               pxr::KindTokens->assembly };

    // First add the basic kinds.
    int kindIdx = 0;
    for (const auto& baseKind : baseKindEntries) {
        ui->kindSelection->addItem(baseKind.GetString().c_str(), kindIdx);
        kindIdx++;
    }

    // Custom kinds.
    for (const auto& kind : pxr::KindRegistry::GetAllKinds()) {
        if (std::find(baseKindEntries.begin(), baseKindEntries.end(), kind)
            != baseKindEntries.end()) {
            continue;
        }
        ui->kindSelection->addItem(kind.GetString().c_str(), kindIdx);
        kindIdx++;
    }

    connect(
        ui->kindSelection,
        static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        [this](int index) {
            // Update the param block.
            const auto& kindStr = ui->kindSelection->itemText(index).toStdString();
            const WStr  kindParam = kindStr == noneToken.GetString()
                 ? WStr {}
                 : MaxUsd::UsdStringToMaxString(kindStr);

            const MCHAR* currentKindParam = nullptr;
            Interval     valid = FOREVER;
            this->paramBlock->GetValue(
                KindSelection, GetCOREInterface()->GetTime(), currentKindParam, valid);

            // If we changed the index programmatically in reaction to the parameter changing, we
            // dont need to do anything more.
            if (kindParam == currentKindParam) {
                return;
            }

            // Usability short hand, if the user selects a kind selection mode, auto-switch to prim
            // subobject level.
            GetCOREInterface()->SetSubObjectLevel(static_cast<int>(SelectionMode::Prim));
            // Update the PB with the new kind!
            if (!theHold.Holding()) {
                theHold.Begin();
                this->paramBlock->SetValueByName(L"KindSelection", kindParam, 0);
                theHold.Accept(_T("Kind Selection Parameter Change"));
            } else {
                this->paramBlock->SetValueByName(L"KindSelection", kindParam, 0);
            }
        });

    ui->selectionHighlightCheckbox->setChecked(
        HdMaxDisplayPreferences::GetInstance().GetSelectionHighlightEnabled());
    ui->selectionColorSwatch->setValue(HdMaxDisplayPreferences::GetInstance().GetSelectionColor());

    connect(ui->selectionHighlightCheckbox, &QCheckBox::toggled, [this](bool checked) {
        HdMaxDisplayPreferences::GetInstance().SetSelectionHighlightEnabled(checked);
        // Notify and complete redraw so that all usd stage objects get redrawn. We need to notify,
        // as internally we need to now use different render items / update selection buffers.
        BroadcastNotification(NOTIFY_SELECTION_HIGHLIGHT_ENABLED_CHANGED);
        GetCOREInterface()->ForceCompleteRedraw();
    });

    connect(
        ui->selectionColorSwatch, &QMaxColorSwatch::valueChanged, [this](const AColor& newColor) {
            HdMaxDisplayPreferences::GetInstance().SetSelectionColor(newColor);
            // If only the color changed, only need to redraw.
            GetCOREInterface()->ForceCompleteRedraw();
        });
}

UsdStageViewportSelectionRollup::~UsdStageViewportSelectionRollup()
{
    UnRegisterNotification(
        NotifySubObjectLevelChanged, this, NOTIFY_MODPANEL_SUBOBJECTLEVEL_CHANGED);
}

void UsdStageViewportSelectionRollup::SetParamBlock(
    ReferenceMaker*     owner,
    IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    modelObj = static_cast<USDStageObject*>(owner);
}

void UsdStageViewportSelectionRollup::UpdateUI(const TimeValue t)
{
    UpdateParameterUI(GetCOREInterface()->GetTime(), KindSelection, t);
}

void UsdStageViewportSelectionRollup::UpdateParameterUI(
    const TimeValue t,
    const ParamID   paramId,
    const int /*tabIndex*/)
{
    if (KindSelection == paramId) {
        // Get the new kind to select.
        const MCHAR* kindSelectionPb = nullptr;
        Interval     valid = FOREVER;
        paramBlock->GetValue(KindSelection, GetCOREInterface()->GetTime(), kindSelectionPb, valid);

        // Find its index in the combobox.
        const auto kindStr = MaxUsd::MaxStringToUsdString(kindSelectionPb);

        auto findNewIndex = [this, kindStr] {
            if (kindStr.empty()) {
                return 0;
            }

            for (int i = 0; i < ui->kindSelection->count(); ++i) {
                if (ui->kindSelection->itemText(i).toStdString() == kindStr) {
                    return i;
                }
            }
            DbgAssert(0 && "Invalid kind set for selection.");
            return 0;
        };

        const int newIdx = findNewIndex();

        // Only update the index if it actually changed, to avoid QT signal noise.
        if (newIdx != ui->kindSelection->currentIndex()) {
            ui->kindSelection->setCurrentIndex(newIdx);
        }
    }
}

void UsdStageViewportSelectionRollup::UpdateSelectionMode() const
{
    const auto level = GetCOREInterface()->GetSubObjectLevel();
    switch (level) {
    // Stage
    case 0:
        ui->stageRadioButton->setChecked(true);
        ui->primRadioButton->setChecked(false);
        break;
    case 1:
        // Prim
        ui->stageRadioButton->setChecked(false);
        ui->primRadioButton->setChecked(true);
        break;
    default: DbgAssert(0 && "Unsupported sub-object level");
    }
}
