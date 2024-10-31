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
#include "UsdStageNodeParametersRollup.h"

#include "QtWidgets/QDialog"
#include "UsdStageNodePrimSelectionDialog.h"
#include "ui_UsdStageNodeParametersRollup.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/MathUtils.h>
#include <MaxUsd/Utilities/OptionUtils.h>

// max sdk
#include <Qt/QmaxToolClips.h>

#include <GetCOREInterface.h>
#include <IPathConfigMgr.h>
#include <iparamb2.h>
#include <maxapi.h>
#include <maxicon.h>

using namespace MaxSDK;

UsdStageNodeParametersRollup::UsdStageNodeParametersRollup(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock)
    : ui(new Ui::UsdStageNodeParametersRollup)
{
    SetParamBlock((ReferenceMaker*)&owner, (IParamBlock2*)&paramBlock);

    ui->setupUi(this);

    QSizePolicy sp = this->ui->progressBar->sizePolicy();

    // 2023+ not using the embedded progress bar. Instead, uses the global progress bar, which is
    // more feature rich in those versions.
#ifdef IS_MAX2023_OR_GREATER
    sp.setRetainSizeWhenHidden(false);
#else
    sp.setRetainSizeWhenHidden(true);
#endif

    this->ui->progressBar->setSizePolicy(sp);
    this->ui->progressBar->setVisible(false);

    modelObj = static_cast<USDStageObject*>(&owner);
    RegisterProgressReporter();

    // Disable Max tooltips as they do not handle long strings well.
    MaxSDK::QmaxToolClips::disableToolClip(ui->FilePath);

    ui->ReloadLayersButton->setIcon(
        MaxSDK::LoadMaxMultiResIcon("CommandPanel/Motion/BipedRollout/MotionMixer/ReloadFiles"));
    ui->ClearSessionLayerButton->setIcon(
        MaxSDK::LoadMaxMultiResIcon("PolyTools/ViewportCanvas/DeleteLayer"));
    const int iconSize = MaxSDK::UIScaled(16);
    ui->ReloadLayersButton->setIconSize(QSize(iconSize, iconSize));
    ui->ClearSessionLayerButton->setIconSize(QSize(iconSize, iconSize));

    // Listen to stage events, some widgets should react to changes in the stage.
    pxr::TfWeakPtr<UsdStageNodeParametersRollup> me(this);
    onStageChangeNotice
        = pxr::TfNotice::Register(me, &UsdStageNodeParametersRollup::OnStageChanged);

    UpdateClearSessionLayerButtonState();
}

void UsdStageNodeParametersRollup::OnStageChanged(
    pxr::UsdNotice::StageContentsChanged const& notice)
{
    const auto stage = modelObj->GetUSDStage();
    if (stage == notice.GetStage()) {
        UpdateClearSessionLayerButtonState();
    }
}

UsdStageNodeParametersRollup::~UsdStageNodeParametersRollup()
{
    modelObj->UnregisterProgressReporter();
    pxr::TfNotice::Revoke(onStageChangeNotice);
}

void UsdStageNodeParametersRollup::RegisterProgressReporter()
{
    if (!modelObj) {
        return;
    }

    // In Max2023 and a later, the global progress bar can be configured to disable
    // cancellation, and avoid suspending object edition, this was not possible in <= 2022.
    // Therefor, in 2022, we use an embedded QProgressBar instead of the global one.
#ifdef IS_MAX2023_OR_GREATER
    auto start = [this](const std::wstring& title) {
        GetCOREInterface()->ProgressStart(title.c_str(), false);
        GetCOREInterface()->ProgressUpdate(0);
    };
    auto update = [this](int progress) {
        const auto value = std::min(100, std::max(0, progress));
        GetCOREInterface()->ProgressUpdate(value);
    };
    auto end = [this]() {
        GetCOREInterface()->ProgressUpdate(100);
        GetCOREInterface()->ProgressEnd();
    };
#else
    auto start = [this](const std::wstring& title) {
        QString format = QString::fromStdWString(title) + QString("%p%");
        this->ui->progressBar->setFormat(format);
        this->ui->progressBar->setVisible(true);
    };
    auto update = [this](int progress) {
        const auto value = std::min(100, std::max(0, progress));
        this->ui->progressBar->setValue(value);
    };
    auto end = [this]() {
        this->ui->progressBar->setVisible(false);
        this->ui->progressBar->setValue(0);
        this->ui->progressBar->setFormat(QString {});
    };
#endif

    const MaxUsd::ProgressReporter progressReporter { start, update, end };
    modelObj->RegisterProgressReporter(progressReporter);
}

void UsdStageNodeParametersRollup::SetParamBlock(
    ReferenceMaker*     owner,
    IParamBlock2* const paramBlock)
{
    this->paramBlock = paramBlock;
    modelObj = static_cast<USDStageObject*>(owner);
    RegisterProgressReporter();
}

void UsdStageNodeParametersRollup::UpdateUI(const TimeValue t)
{
    auto stage = modelObj->GetUSDStage();
    if (stage) {
        FLOAT    sourceMPU = 0;
        Interval valid = FOREVER;
        paramBlock->GetValue(
            PBParameterIds::SourceMetersPerUnit, GetCOREInterface()->GetTime(), sourceMPU, valid);

        const MCHAR* stageMaskValue = nullptr;
        paramBlock->GetValue(StageMask, GetCOREInterface()->GetTime(), stageMaskValue, valid);

        const MCHAR* rootLayerFilename = nullptr;
        paramBlock->GetValue(StageFile, GetCOREInterface()->GetTime(), rootLayerFilename, valid);

        double mpu = MaxUsd::MathUtils::RoundToSignificantDigit(sourceMPU, 5);
        ui->SourceMetersPerUnit->setText(QString::number(mpu));
        ui->StageMaskValue->setText(QString::fromWCharArray(stageMaskValue));

        ui->FilePath->setToolTip(QString::fromWCharArray(rootLayerFilename));

        ui->ExploreButton->setEnabled(true);
    } else {
        ui->SourceMetersPerUnit->setText("N/A");
        ui->StageMaskValue->setText("/");
        ui->FilePath->setToolTip("");

        ui->ExploreButton->setEnabled(false);
    }
}

void UsdStageNodeParametersRollup::UpdateParameterUI(
    const TimeValue t,
    const ParamID   paramId,
    const int /*tabIndex*/)
{
    if (paramId == SourceMetersPerUnit) {
        FLOAT    sourceMPU = 0;
        Interval valid = FOREVER;
        paramBlock->GetValue(
            PBParameterIds::SourceMetersPerUnit, GetCOREInterface()->GetTime(), sourceMPU, valid);

        double mpu = MaxUsd::MathUtils::RoundToSignificantDigit(sourceMPU, 5);

        ui->SourceMetersPerUnit->setText(QString::number(mpu));
    }
}

void UsdStageNodeParametersRollup::on_StageMaskButton_clicked() { SelectLayerAndPrim(false); }

void UsdStageNodeParametersRollup::on_RootLayerPathButton_clicked() { SelectLayerAndPrim(true); }

void UsdStageNodeParametersRollup::on_ReloadLayersButton_clicked() { modelObj->Reload(); }

void UsdStageNodeParametersRollup::on_ClearSessionLayerButton_clicked()
{
    modelObj->ClearSessionLayer();
}

void UsdStageNodeParametersRollup::on_StageMaskValue_editingFinished()
{
    Interval     valid = FOREVER;
    const MCHAR* stageMaskValue = nullptr;
    paramBlock->GetValue(StageMask, GetCOREInterface()->GetTime(), stageMaskValue, valid);

    if (ui->StageMaskValue->text().isEmpty()) {
        ui->StageMaskValue->setText("/");
    }
    const auto newStageMask = ui->StageMaskValue->text();
    const int  changed = wcscmp(stageMaskValue, newStageMask.toStdWString().c_str());
    if (changed != 0) {
        Interval     valid = FOREVER;
        const MCHAR* rootLayer = nullptr;
        paramBlock->GetValue(
            PBParameterIds::StageFile, GetCOREInterface()->GetTime(), rootLayer, valid);
        BOOL payloadsLoaded = false;
        paramBlock->GetValue(
            PBParameterIds::LoadPayloads, GetCOREInterface()->GetTime(), payloadsLoaded, valid);

        // Use SetRootLayer() as it takes care of everything VS undo redo etc.
        modelObj->SetRootLayer(rootLayer, newStageMask.toStdWString().c_str(), payloadsLoaded);
    }
}

void UsdStageNodeParametersRollup::on_ExploreButton_clicked() { modelObj->OpenInUsdExplorer(); }

namespace {
pxr::VtDictionary options;
const std::string optionsCategoryKey = "PrimSelectionDialogPreferences";
} // namespace

void UsdStageNodeParametersRollup::SelectLayerAndPrim(bool forceFileSelection)
{
    QString rootLayerPath = ui->FilePath->text();
    QString stageMask = ui->StageMaskValue->text();

    // If the root layer path is empty - the first thing we want to do is pop-up the file selection
    // dialog.
    if (rootLayerPath.isEmpty() || forceFileSelection) {
        QFileInfo file = UsdStageNodePrimSelectionDialog::SelectFile(rootLayerPath);
        rootLayerPath = file.absoluteFilePath();
    }

    // If still empty (user did not select a file / cancelled), exit.
    if (rootLayerPath.isEmpty()) {
        return;
    }

    // Load the options from disk only once per session.
    static auto loadOptions = []() {
        MaxUsd::OptionUtils::LoadUiOptions(optionsCategoryKey, options);
        if (!options[pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads].IsHolding<bool>()) {
            options[pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads] = true;
        }
        if (!options[pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer].IsHolding<bool>()) {
            options[pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer] = true;
        }
        return true;
    };
    static bool optionLoaded = loadOptions();

    std::unique_ptr<UsdStageNodePrimSelectionDialog> primSelectionDialog
        = std::make_unique<UsdStageNodePrimSelectionDialog>(
            rootLayerPath,
            stageMask,
            MaxUsd::TreeModelFactory::TypeFilteringMode::Exclude,
            std::vector<std::string> { "Material", "Shader", "GeomSubset" },
            options);

    // Finally, open the dialog.
    if (primSelectionDialog->exec() == QDialog::Accepted) {
        // user hit OK
        QString rootLayerPath = primSelectionDialog->GetRootLayerPath();
        QString selectedPrim = primSelectionDialog->GetMaskPath();
        options[pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads]
            = primSelectionDialog->GetPayloadsLoaded();

        // Start by closing the current stage in the explorer. Depending on the option selected from
        // the UI, we may or may not want to reopen the new stage in the explorer.
        modelObj->CloseInUsdExplorer();

        modelObj->SetRootLayer(
            TSTR(rootLayerPath.toStdString().c_str()).data(),
            TSTR(selectedPrim.toStdString().c_str()).data(),
            options[pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads].UncheckedGet<bool>());

        // Trigger a UI refresh.
        UpdateUI(0);

        // Remember user choice, per session.
        options[pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer]
            = primSelectionDialog->GetOpenInUsdExplorer();
        if (options[pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer].UncheckedGet<bool>()) {
            modelObj->OpenInUsdExplorer();
        }
        MaxUsd::OptionUtils::SaveUiOptions(optionsCategoryKey, options);
    }
}

void UsdStageNodeParametersRollup::UpdateClearSessionLayerButtonState()
{
    bool enable = false;
    if (const auto stage = modelObj->GetUSDStage()) {
        if (const auto sessionLayer = stage->GetSessionLayer()) {
            enable = !sessionLayer->IsEmpty();
        }
    }
    if (ui->ClearSessionLayerButton->isEnabled() != enable) {
        ui->ClearSessionLayerButton->setEnabled(enable);
    }
}
