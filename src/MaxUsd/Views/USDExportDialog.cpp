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
#include "USDExportDialog.h"

#include "USDExportCustomChannelMappingsDialog.h"
#include "UsdExportAdvancedRollup.h"
#include "UsdExportAnimationRollup.h"
#include "UsdExportFileRollup.h"
#include "UsdExportGeneralSettingsRollup.h"
#include "UsdExportIncludeRollup.h"
#include "UsdExportMaterialsRollup.h"
#include "ui_USDExportDialog.h"

#include <MaxUsd/Builders/JobContextRegistry.h>
#include <MaxUsd/MaxTokens.h>
#include <MaxUsd/Translators/ShadingModeRegistry.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/OptionUtils.h>
#include <MaxUsd/Utilities/PluginUtils.h>
#include <MaxUsd/Utilities/UiUtils.h>

#include <pxr/base/tf/token.h>

#include <Qt/QmaxMainWindow.h>
#include <Qt/QmaxRollup.h>
#include <Qt/QmaxRollupContainer.h>
#include <Qt/QmaxToolClips.h>

#include <QtGui/qevent.h>
#include <QtGui/qscreen.h>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/qabstractspinbox.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qwhatsthis.h>
#include <helpsys.h>
#include <maxapi.h>

#define idh_usd_export _T("idh_usd_export")

USDExportDialog::USDExportDialog(const MaxUsd::IUSDExportOptions& buildOptions)
    : ui { std::make_unique<Ui::ExportDialog>() }
    , buildOptions { buildOptions }
{
    setWindowFlags(windowFlags() | Qt::WindowContextHelpButtonHint);
    ui->setupUi(this);
    setParent(GetCOREInterface()->GetQmaxMainWindow(), windowFlags());

    const int smallPadding = MaxSDK::UIScaled(6);
    ui->footer->setContentsMargins(smallPadding, smallPadding, smallPadding, smallPadding);
    ui->footer->setSpacing(smallPadding);

    // The rollup container is a custom widget that does not play well with the
    // assumptions of the Qt Designer, so there is a dummy placeholder QWidget
    // in the .ui file that gets replaced here.
    auto rollupContainer = new MaxSDK::QmaxRollupContainer(this);
    layout()->replaceWidget(ui->rollupContainer, rollupContainer);
    ui->rollupContainer->deleteLater();
    ui->rollupContainer = rollupContainer;

    QPixmap headerIcon = style()->standardPixmap(QStyle::SP_MessageBoxInformation);
    ui->left_label->setPixmap(headerIcon);
    ui->version_label->setText(QString::fromStdString(MaxUsd::GetPluginDisplayVersion()));

    MaxUsd::Ui::IterateOverChildrenRecursively(this, [this](QObject* object) {
        // 3dsMax ToolClips do not behave so well (linger and do not disappear
        // or move with the dialog). Disable until these issues are fixed.
        MaxSDK::QmaxToolClips::disableToolClip(object);

        // The exporter dialog has a scroll area. As you are scrolling down, we
        // do not want spin and combo boxes to grab focus and scroll through
        // their values. Fix this with the "StrongFocus" policy and an event
        // filter filtering out unwanted wheel events on those widgets.
        if (QAbstractSpinBox::staticMetaObject.cast(object)
            || QComboBox::staticMetaObject.cast(object)) {
            object->installEventFilter(this);
            qobject_cast<QWidget*>(object)->setFocusPolicy(Qt::StrongFocus);
        }
    });

    transformFormat = buildOptions.GetTransformFormat();
}

void USDExportDialog::addRollup(QWidget* w, bool open)
{
    MaxSDK::QmaxRollup* rollup = new MaxSDK::QmaxRollup(w->windowTitle());
    rollup->setWidget(w);
    rollup->setOptions(MaxSDK::QmaxRollup::FixedCat);
    if (loadedRollupState.find(w->windowTitle()) != loadedRollupState.end()) {
        open = loadedRollupState[w->windowTitle()];
    }
    rollup->setOpen(open);

    auto rollupContainer = dynamic_cast<MaxSDK::QmaxRollupContainer*>(ui->rollupContainer);
    rollupContainer->addRollup(rollup);
}

void USDExportDialog::addContextsRollup()
{
    // -------------------------------------------------------------------------
    // Plug-in Configurations
    // -------------------------------------------------------------------------
    const auto& currentContexts = buildOptions.GetContextNames();
    auto        contexts = pxr::MaxUsdJobContextRegistry::ListJobContexts();

    // filters out the import contexts
    contexts.erase(
        std::remove_if(
            contexts.begin(),
            contexts.end(),
            [](pxr::TfToken& c) {
                return !pxr::MaxUsdJobContextRegistry::GetJobContextInfo(c).exportEnablerCallback;
            }),
        contexts.end());

    if (!contexts.empty()) {
        // sort the registered export chasers alphabetically
        std::sort(
            contexts.begin(), contexts.end(), [](const pxr::TfToken& a, const pxr::TfToken& b) {
                return pxr::MaxUsdJobContextRegistry::GetJobContextInfo(a).niceName
                    < pxr::MaxUsdJobContextRegistry::GetJobContextInfo(b).niceName;
            });

        if (auto rollup = new MaxSDK::QmaxRollup(tr("Plug-in Configurations"))) {
            rollup->setOptions(MaxSDK::QmaxRollup::FixedCat);

            auto widget = new QWidget(rollup);
            auto layout = new QVBoxLayout(widget);

            int offset = MaxSDK::UIScaled(3);
            int largeOffset = MaxSDK::UIScaled(6);
            layout->setContentsMargins(largeOffset, offset, offset, MaxSDK::UIScaled(4));
            layout->setSpacing(MaxSDK::UIScaled(2));

            rollup->setWidget(widget);

            bool first_context = true;

            for (const auto& context : contexts) {
                const auto& contextInfo = pxr::MaxUsdJobContextRegistry::GetJobContextInfo(context);

                auto        jobContext = contextInfo.jobContext;
                std::string contextName = jobContext.GetString();
                std::string contextNiceName = contextInfo.niceName.GetString();

                auto contextLayout = new QHBoxLayout();
                contextLayout->setContentsMargins(0, 0, 0, 0);

                auto contextCheckBox
                    = new QCheckBox(QString::fromStdString(contextNiceName), widget);
                contextCheckBox->setObjectName(
                    QString("Enable_Context_%1").arg(QString::fromStdString(contextName)));
                contextCheckBox->setToolTip(contextInfo.exportDescription.GetString().c_str());
                contextCheckBox->setChecked(currentContexts.find(context) != currentContexts.end());
                contextLayout->addWidget(contextCheckBox, 1);

                if (contextInfo.exportOptionsCallback != nullptr) {
                    auto contextOptionsBtn = new QPushButton(tr("Options"), widget);
                    contextOptionsBtn->setObjectName(
                        QString("Options_Context_%1").arg(QString::fromStdString(contextName)));
                    contextLayout->addWidget(contextOptionsBtn, 0);

                    contextOptionsBtn->setEnabled(contextCheckBox->isChecked());
                    connect(
                        contextCheckBox,
                        &QAbstractButton::clicked,
                        contextOptionsBtn,
                        &QWidget::setEnabled);

                    auto optionsCallback = contextInfo.exportOptionsCallback;
                    connect(
                        contextOptionsBtn,
                        &QPushButton::clicked,
                        this,
                        [this, jobContext, contextName, optionsCallback]() {
                            pxr::VtDictionary contextOptions = optionsCallback(
                                contextName,
                                this,
                                this->buildOptions.GetJobContextOptions(jobContext));
                            this->buildOptions.SetJobContextOptions(jobContext, contextOptions);
                        });
                }

                connect(
                    contextCheckBox,
                    &QAbstractButton::clicked,
                    this,
                    [this, contextName](bool checked) {
                        std::set<std::string> contextNames = this->buildOptions.GetContextNames();
                        if (checked) {
                            auto result = contextNames.insert(contextName);
                            DbgAssert(result.second);
                        } else {
                            auto result = contextNames.erase(contextName);
                            DbgAssert(result == 1u);
                        }
                        this->buildOptions.SetContextNames(contextNames);
                    });

                if (first_context) {
                    first_context = false;
                } else {
                    auto separator = new QFrame();
                    separator->setFrameStyle(QFrame::HLine | QFrame::Sunken);
                    layout->addWidget(separator);
                }

                layout->addLayout(contextLayout);
            }
            if (loadedRollupState.find(rollup->title()) != loadedRollupState.end()) {
                rollup->setOpen(loadedRollupState[rollup->title()]);
            } else {
                rollup->setOpen(true);
            }
            const auto maxRollUpContainer
                = dynamic_cast<MaxSDK::QmaxRollupContainer*>(ui->rollupContainer);
            maxRollUpContainer->addRollup(rollup);
        }
    }
}

USDExportDialog::~USDExportDialog() = default;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool USDExportDialog::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
#else
bool USDExportDialog::nativeEvent(const QByteArray& eventType, void* message, long* result)
#endif
{
    MSG* msg = static_cast<MSG*>(message); // safe to do because we are on windows only
    if (msg->message == WM_HELP) {
        MaxSDK::IHelpSystem::GetInstance()->ShowProductHelpForTopic(idh_usd_export);
        *result = TRUE;
        return true;
    }
    return false;
}

bool USDExportDialog::event(QEvent* ev)
{
    if (ev->type() == QEvent::EnterWhatsThisMode) {
        // We need to leave immediately the "What's this" mode, otherwise
        // system is waiting for a click on particular widget
        QWhatsThis::leaveWhatsThisMode();
        // Open a new web page containing help about usd component export
        MaxSDK::IHelpSystem::GetInstance()->ShowProductHelpForTopic(idh_usd_export);
        return true;
    }
    return __super::event(ev);
}

void USDExportDialog::showEvent(QShowEvent* ev)
{
    resize(MaxSDK::UIScaled(dialogWidth), MaxSDK::UIScaled(dialogHeight));

    loadedRollupState = MaxUsd::OptionUtils::LoadRollupStates(GetRollupCategory());
    setupRollups();

    constexpr float maxHeightPercent = 0.85f;
    QDialog::showEvent(ev);
    QRect screenDim = QGuiApplication::screenAt(this->pos())->availableGeometry();
    int   maxHeight = static_cast<int>(maxHeightPercent * screenDim.height());

    if (this->height() > maxHeight) {
        resize(this->width(), maxHeight);
    }

    // the export button is the one with focus on show dialog
    // if the focus has not changed when pressing 'enter' the export is launched
    ui->buttons->button(QDialogButtonBox::StandardButton::Ok)->setFocus(Qt::NoFocusReason);
}

void USDExportDialog::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter: {
        // ONLY if the export button is still the one with focus
        // when pressing 'enter' the export is launched
        QWidget* w = focusWidget();
        if (w != ui->buttons->button(QDialogButtonBox::StandardButton::Ok))
            return;
    } break;

    default: QDialog::keyPressEvent(e);
    }
}

bool USDExportDialog::eventFilter(QObject* object, QEvent* event)
{
    const QWidget* widget = dynamic_cast<QWidget*>(object);
    // Combo and Spin boxes need to be explicitly focused to accept wheel events to avoid
    // mistakenly scrolling through their values while scrolling in the dialog.
    if (event->type() == QEvent::Wheel && widget && !widget->hasFocus()
        && (qobject_cast<QAbstractSpinBox*>(object) || qobject_cast<QComboBox*>(object))) {
        event->ignore();
        return true;
    }
    return QWidget::eventFilter(object, event);
}

bool USDExportDialog::Execute() { return exec() == QDialog::Accepted; }

const MaxUsd::USDSceneBuilderOptions& USDExportDialog::GetBuildOptions() const
{
    return buildOptions;
}

std::map<QString, bool> USDExportDialog::GetRollupState() const
{
    std::map<QString, bool> rollupState;
    const auto maxRollUpContainer = dynamic_cast<MaxSDK::QmaxRollupContainer*>(ui->rollupContainer);
    for (int i = 0; i < maxRollUpContainer->numRollups(); ++i) {
        if (const auto rollup = maxRollUpContainer->rollup(i)) {
            rollupState[rollup->title()] = rollup->isOpen();
        }
    }
    return rollupState;
}

void USDExportDialog::accept()
{
    if (animationRollup) {
        animationRollup->SaveDialogState();
    }
    MaxUsd::OptionUtils::SaveRollupStates(GetRollupCategory(), GetRollupState());
    QDialog::accept();
}

USDExportToFileDialog::USDExportToFileDialog(
    const fs::path&                  filePath,
    const MaxUsd::IUSDExportOptions& buildOptions)
    : USDExportDialog(buildOptions)
    , exportPath { filePath }
{

    ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Export"));

    ui->openInUsdViewCheckbox->setChecked(buildOptions.GetOpenInUsdview());
    connect(ui->openInUsdViewCheckbox, &QAbstractButton::clicked, this, [this](bool checked) {
        this->buildOptions.SetOpenInUsdview(checked);
    });

    const auto pathQstr = QString::fromStdString(filePath.parent_path().u8string());
    ui->ExportPathLineEdit->setText(pathQstr);
    // Disable Max tooltips as they do not handle long strings well.
    ui->ExportPathLineEdit->setToolTip(pathQstr);

    dialogHeight = 850;
    dialogWidth = 430;
}

void USDExportToFileDialog::accept()
{
    if (buildOptions.GetUseSeparateMaterialLayer()) {
        auto fNameWithoutExt = exportPath.filename().replace_extension("").string();
        auto materialLayerPath = MaxUsd::ResolveToken(
            buildOptions.GetMaterialLayerPath(), "<filename>", fNameWithoutExt);

        auto toLowerCase = [](const std::string& str) {
            std::string res;
            res.reserve(str.size());
            std::transform(
                str.begin(), str.end(), std::back_inserter(res), [](const unsigned char c) {
                    return std::tolower(c);
                });
            return res;
        };

        if (toLowerCase(exportPath.filename().string()) == toLowerCase(materialLayerPath)
            || toLowerCase(exportPath.string()) == toLowerCase(materialLayerPath)) {
            MaxSDK::MaxMessageBox(
                GetCOREInterface()->GetMAXHWnd(),
                L"Export failed because the material layer file path matches the export's target "
                L"file path. Make the file paths unique to proceed.",
                _T("Export Error"),
                MB_ICONEXCLAMATION);
            return;
        }
    }
    if (MaxUsd::HasUnicodeCharacter(buildOptions.GetMaterialLayerPath())) {
        MaxSDK::MaxMessageBox(
            GetCOREInterface()->GetMAXHWnd(),
            L"Export failed. USD does not support unicode characters in its file paths. Please "
            L"remove these characters from the path given for materials.",
            _T("Unicode Error"),
            MB_ICONEXCLAMATION);
    } else {
        USDExportDialog::accept();
    }
}

void USDExportToFileDialog::setupRollups()
{
    addRollup(new UsdExportFileRollup(exportPath, this->buildOptions));

    // This needs to be done AFTER the file rollup is added - as the addRollup
    // call creates the internal widget.
    auto rollupContainer = dynamic_cast<MaxSDK::QmaxRollupContainer*>(ui->rollupContainer);
    if (auto widget = rollupContainer->widget()) {
        if (auto rollupContainterLayout = dynamic_cast<QVBoxLayout*>(widget->layout())) {
            rollupContainterLayout->insertWidget(0, ui->ExportPathGroupBox);
        }
        const int pixelPadding = MaxSDK::UIScaled(1);
        const int tinyPadding = MaxSDK::UIScaled(3);
        widget->setContentsMargins(tinyPadding, tinyPadding, pixelPadding, 0);
    }

    addContextsRollup();
    addRollup(new UsdExportIncludeRollup(this->buildOptions));
    addRollup(new UsdExportMaterialsRollup(this->buildOptions));
    animationRollup = new UsdExportAnimationRollup(this->buildOptions);
    addRollup(animationRollup, false);
    generalSettingsRollup = new UsdExportGeneralSettingsRollup(this->buildOptions);
    addRollup(generalSettingsRollup, false);
    addRollup(new UsdExportAdvancedRollup(this->buildOptions), false);

#ifdef USD_CURVES_SUPPORTED
    connect(
        animationRollup->GetAnimationTypeComboBox(),
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        [&](int index) {
            auto selectedAnimationType
                = static_cast<MaxUsd::USDSceneBuilderOptions::AnimationType>(index);
            auto transformFormatComboBox = generalSettingsRollup->GetTransformFormatComboBox();
            if (selectedAnimationType == MaxUsd::USDSceneBuilderOptions::AnimationType::Curves
                || selectedAnimationType == MaxUsd::USDSceneBuilderOptions::AnimationType::Both) {
                // Only cache the old transform format if we are switching away from time samples.
                // The user could be switching between "Curves" and "Both".
                if (buildOptions.GetAnimationType()
                    == MaxUsd::USDSceneBuilderOptions::AnimationType::TimeSamples) {

                    transformFormat = buildOptions.GetTransformFormat();
                }
                transformFormatComboBox->setCurrentIndex(
                    static_cast<int>(MaxUsd::TransformFormat::SplitComponents));
                transformFormatComboBox->setEnabled(false);
            } else {
                transformFormatComboBox->setCurrentIndex(static_cast<int>(transformFormat));
                transformFormatComboBox->setEnabled(true);
            }
            buildOptions.SetAnimationType(selectedAnimationType);
        });
#endif
}

const QString& USDExportToFileDialog::GetRollupCategory()
{
    static const QString rollupCategory = "ExportDialogRollups";
    return rollupCategory;
}

USDExportToStageDialog::USDExportToStageDialog(
    const MaxUsd::IUSDExportOptions& buildOptions,
    const pxr::VtDictionary&         extraOptions)
    : USDExportDialog(buildOptions)
    , extraOptions(extraOptions)
{
    setWindowTitle(tr("Duplicate to USD"));
    ui->ExportPathGroupBox->setHidden(true);
    ui->openInUsdViewCheckbox->setHidden(true);
    ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Apply"));

    dialogHeight = 585;
    dialogWidth = 430;
}

const pxr::VtDictionary& USDExportToStageDialog::GetExtraOptions() const { return extraOptions; }

void USDExportToStageDialog::setupRollups()
{
    // Add a rollup specific to exporting to stages.

    auto rollup = new MaxSDK::QmaxRollup(tr("General Settings"));

    rollup->setOptions(MaxSDK::QmaxRollup::FixedCat);

    auto widget = new QWidget(rollup);
    auto layout = new QVBoxLayout(widget);

    int offset = MaxSDK::UIScaled(3);
    int largeOffset = MaxSDK::UIScaled(6);
    layout->setContentsMargins(largeOffset, offset, offset, MaxSDK::UIScaled(4));
    layout->setSpacing(MaxSDK::UIScaled(2));

    rollup->setWidget(widget);

    auto inheritTransformCheckbox = new QCheckBox(tr("Inherit Stage Object Transform"), widget);
    layout->addWidget(inheritTransformCheckbox);
    bool inheritTransform = false;
    if (auto inheritTransformVal
        = extraOptions.GetValueAtPath(pxr::MaxUsdExportTokens->inheritStageObjectTransform)) {
        inheritTransform = inheritTransformVal->Get<bool>();
    }
    inheritTransformCheckbox->setChecked(inheritTransform);
    connect(inheritTransformCheckbox, &QCheckBox::clicked, this, [this](bool checked) {
        extraOptions.SetValueAtPath(
            pxr::MaxUsdExportTokens->inheritStageObjectTransform, pxr::VtValue { checked });
    });

    bool overwrite = false;
    if (auto overwriteVal
        = extraOptions.GetValueAtPath(pxr::MaxUsdExportTokens->allowPrimOverwrite)) {
        overwrite = overwriteVal->Get<bool>();
    }
    auto overwritePrimCheckbox = new QCheckBox(QString { "Overwrite Existing Prims" }, widget);
    layout->addWidget(overwritePrimCheckbox);
    overwritePrimCheckbox->setChecked(overwrite);
    connect(overwritePrimCheckbox, &QCheckBox::clicked, this, [this](bool checked) {
        extraOptions.SetValueAtPath(
            pxr::MaxUsdExportTokens->allowPrimOverwrite, pxr::VtValue { checked });
    });

    if (loadedRollupState.find(rollup->title()) != loadedRollupState.end()) {
        rollup->setOpen(loadedRollupState[rollup->title()]);
    } else {
        rollup->setOpen(true);
    }
    const auto maxRollUpContainer = dynamic_cast<MaxSDK::QmaxRollupContainer*>(ui->rollupContainer);
    maxRollUpContainer->addRollup(rollup);

    addContextsRollup();
    addRollup(new UsdExportIncludeRollup(this->buildOptions));

    const auto materialRollup = new UsdExportMaterialsRollup(this->buildOptions);

    // Hide a few widgets in the material rollup - when exporting to a live stage,
    // we do not support using a seperate layer for materials - hide widgets related
    // to that option.
    materialRollup->findChild<QWidget*>("layerPathLabel")->setHidden(true);
    materialRollup->findChild<QWidget*>("materialLayerPath")->setHidden(true);
    materialRollup->findChild<QWidget*>("materialLayerPicker")->setHidden(true);
    materialRollup->findChild<QWidget*>("separateMaterialLayer")->setHidden(true);

    addRollup(materialRollup, false);
    animationRollup = new UsdExportAnimationRollup(this->buildOptions);
    addRollup(animationRollup, false);
    addRollup(new UsdExportAdvancedRollup(this->buildOptions), false);
}

const QString& USDExportToStageDialog::GetRollupCategory()
{
    static const QString rollupCategory = "ExportToStageDialogRollups";
    return rollupCategory;
}

void USDExportToStageDialog::accept()
{
    MaxUsd::OptionUtils::SaveExportOptions(
        buildOptions, MaxUsd::USDSceneBuilderOptions::Type::ToStage);
    MaxUsd::OptionUtils::SaveExportToStageExtraOptions(extraOptions);

    USDExportDialog::accept();
}