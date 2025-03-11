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
#include <VariableGuard.h>
#include <iparamb2.h>
#include <maxapi.h>
#include <notify.h>

using namespace MaxSDK;

namespace {

void setSubObjectLevelLater(QPointer<QObject> sentinel, USDStageObject* object, SelectionMode mode)
{
    // Switching to sub-object level leads to the deletion and recreation of
    // rollups, what can interfere with the deferred update calls from the
    // automatic 3dsMax param mapping, so we have to defer the switching till we
    // can be sure the deferred update has been finished.

    qApp->processEvents();

    // As the signal is delivered via the Qt event-system asynchronously due
    // to the connection type of Qt::QueuedConnection, we need to pass in a
    // QPointer to the sentinel (as a copy) to have a way to verify, that at
    // the point in time where the lambda is executed, we do still exist.
    QTimer::singleShot(0, [sentinel, object, mode] {
        if (sentinel && object && object->IsInEditParams()) {
            GetCOREInterface()->SetSubObjectLevel(static_cast<int>(mode));
        }
    });
}

} // namespace

UsdStageViewportSelectionRollup::UsdStageViewportSelectionRollup(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock)
    : ui(new Ui::UsdStageViewportSelectionRollup)
    , sentinel(new QObject())
{
    SetParamBlock((ReferenceMaker*)&owner, (IParamBlock2*)&paramBlock);
    ui->setupUi(this);
    modelObj = static_cast<USDStageObject*>(&owner);

    QPointer<QObject> qpSentinel(sentinel);
    USDStageObject*   object = modelObj;

    UpdateSelectionMode();
    connect(
        ui->stageRadioButton,
        &QRadioButton::clicked,
        sentinel,
        // As the signal is delivered via the Qt event-system asynchronously due
        // to the connection type of Qt::QueuedConnection, we need to pass in a
        // QPointer to the sentinel (as a copy) to have a way to verify, that at
        // the point in time where the lambda is executed, we do still exist.
        [qpSentinel, object](bool checked) {
            if (checked && qpSentinel && object && object->IsInEditParams()
                && GetCOREInterface()->GetSubObjectLevel()
                    != static_cast<int>(SelectionMode::Stage)) {
                setSubObjectLevelLater(qpSentinel, object, SelectionMode::Stage);
            }
        },
        Qt::QueuedConnection);
    connect(
        ui->primRadioButton,
        &QRadioButton::clicked,
        sentinel,
        // As the signal is delivered via the Qt event-system asynchronously due
        // to the connection type of Qt::QueuedConnection, we need to pass in a
        // QPointer to the sentinel (as a copy) to have a way to verify, that at
        // the point in time where the lambda is executed, we do still exist.
        [qpSentinel, object](bool checked) {
            if (checked && qpSentinel && object && object->IsInEditParams()
                && GetCOREInterface()->GetSubObjectLevel()
                    != static_cast<int>(SelectionMode::Prim)) {
                setSubObjectLevelLater(qpSentinel, object, SelectionMode::Prim);
            }
        },
        Qt::QueuedConnection);

    // Kind selection UI setup.
    const static std::vector<pxr::TfToken> baseKindEntries
        = { pxr::TfToken("none"),       pxr::KindTokens->model, pxr::KindTokens->subcomponent,
            pxr::KindTokens->component, pxr::KindTokens->group, pxr::KindTokens->assembly };

    // First add the basic kinds.
    for (const auto& baseKind : baseKindEntries) {
        ui->KindSelection->addItem(baseKind.GetString().c_str());
    }

    // Custom kinds.
    for (const auto& kind : pxr::KindRegistry::GetAllKinds()) {
        if (std::find(baseKindEntries.begin(), baseKindEntries.end(), kind)
            == baseKindEntries.end()) {
            ui->KindSelection->addItem(kind.GetString().c_str());
        }
    }

#if MAX_VERSION_MAJOR >= 26
    // Usability short hand, when the user selects a kind selection mode,
    // auto-switch to prim sub-object level.
    // For technical reasons, this is only available in 3ds Max 2024 and later.
    connect(
        ui->KindSelection,
        qOverload<int>(&QComboBox::currentIndexChanged),
        sentinel,
        // As the signal is delivered via the Qt event-system asynchronously due
        // to the connection type of Qt::QueuedConnection, we need to pass in a
        // QPointer to the sentinel (as a copy) to have a way to verify, that at
        // the point in time where the lambda is executed, we do still exist.
        [qpSentinel, object, this](int index) {
            if (qpSentinel && !isUpdatingUI && index != -1 && object && object->IsInEditParams()) {
                setSubObjectLevelLater(qpSentinel, object, SelectionMode::Prim);
            }
        },
        Qt::QueuedConnection);
#endif // MAX_VERSION_MAJOR >= 26

    ui->selectionHighlightCheckbox->setChecked(
        HdMaxDisplayPreferences::GetInstance().GetSelectionHighlightEnabled());
    ui->selectionColorSwatch->setValue(HdMaxDisplayPreferences::GetInstance().GetSelectionColor());

    connect(ui->selectionHighlightCheckbox, &QCheckBox::toggled, [this](bool checked) {
        HdMaxDisplayPreferences::GetInstance().SetSelectionHighlightEnabled(checked);
        // Notify and complete redraw so that all usd stage objects get redrawn.
        // We need to notify, as internally we need to now use different render
        // items / update selection buffers.
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
    // This will also disconnect all signal-slot-connections, but as the signals
    // is delivered by an event via the Qt event system due to the queued
    // connection, we still need to pass in a copy of a QPointer to the sentinel
    // each time, to ensure that at the point in time where the lambda is
    // actually executed, we do still exist.
    //
    // The reason why a simple QPointer(this) won't work is the fact that the
    // "destroyed" signal (what nulls out the QPointer) gets emitted from the
    // destructor of the base QWidget (not from the destructor of our actual
    // derived class) what happens AFTER right after this de-constructor, but in
    // between those points in time, we are not allowed to access any members of
    // this class anymore (as we technically are just a plain QWidget).
    delete sentinel;
    sentinel = nullptr;
}

void UsdStageViewportSelectionRollup::SetParamBlock(
    ReferenceMaker*     owner,
    IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    modelObj = static_cast<USDStageObject*>(owner);
}

// PreConnectUI and PostConnectUI are only available in 3ds Max 2024 and later.
#if MAX_VERSION_MAJOR >= 26

void UsdStageViewportSelectionRollup::PreConnectUI(const MapID /*paramMapID*/)
{
    isUpdatingUI = true;
}

void UsdStageViewportSelectionRollup::PostConnectUI(const MapID /*paramMapID*/)
{
    QPointer<QObject> qpSentinel(sentinel);
    QTimer::singleShot(0, [qpSentinel, this] {
        if (qpSentinel) {
            qApp->processEvents();
            isUpdatingUI = false;
        }
    });
}

#endif // MAX_VERSION_MAJOR >= 26

void UsdStageViewportSelectionRollup::UpdateSelectionMode()
{
    MaxSDK::VariableGuard<bool> guard(isUpdatingUI, true);
    const auto                  level = GetCOREInterface()->GetSubObjectLevel();
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
    default: DbgAssert(false && "Unsupported sub-object level");
    }
}
