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
#include "USDExportCustomChannelMappingsDialog.h"

#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <pxr/usd/usd/usdFileFormat.h>

#include <Qt/QmaxMainWindow.h>

#include <QtGui/QPaintEngine>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWhatsThis>
#include <helpsys.h>
#include <maxapi.h>

USDExportCustomChannelMappingsDialog::USDExportCustomChannelMappingsDialog(
    MaxUsd::USDSceneBuilderOptions& buildOptions)
    : buildOptions { buildOptions }
{
    setParent(GetCOREInterface()->GetQmaxMainWindow(), windowFlags());

    QDialog     mapDetailsDialog { this, Qt::WindowTitleHint | Qt::WindowCloseButtonHint };
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    mapDetailsDialog.setSizePolicy(sizePolicy);
    mapDetailsDialog.setWindowTitle(QObject::tr("Configure mapped channels."));

    QVBoxLayout* VLayout = new QVBoxLayout(&mapDetailsDialog);

    QLabel* tableNameLabel = new QLabel(QString("Channel Data:"));
    VLayout->addWidget(tableNameLabel);

    mapDetailsDialog.setLayout(VLayout);

    QTableWidget* table = new QTableWidget(MAX_MESHMAPS + NUM_HIDDENMAPS, 4);
    table->setSizePolicy(QSizePolicy { QSizePolicy::Expanding, QSizePolicy::Expanding });

    table->setHorizontalHeaderLabels({ QObject::tr("Export"),
                                       QObject::tr("Channel"),
                                       QObject::tr("Target Primvar"),
                                       QObject::tr("Primvar Type") });

    table->horizontalHeaderItem(0)->setToolTip(QObject::tr("Select to include in export."));
    table->horizontalHeaderItem(1)->setToolTip(QObject::tr("Specifies the map channel ID."));
    table->horizontalHeaderItem(2)->setToolTip(
        QObject::tr("Type in the specific primvar to target."));
    table->horizontalHeaderItem(3)->setToolTip(
        QObject::tr("Specifies the type of primvar for the channel."));

    QStringList headerNameList;
    for (int i = 0; i < table->model()->columnCount(); i++) {
        headerNameList.append(table->model()->headerData(i, Qt::Horizontal).toString());
    }

    QHeaderView* header = table->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeMode::ResizeToContents);

    /// Apply minimum width for columns with "Interactive" resize mode (e.g. all but the 'export'
    /// column)
    QObject::connect(
        header,
        &QHeaderView::sectionResized,
        [header, headerNameList](int logicalIndex, int /*oldSize*/, int newSize) {
            static bool recursGuard = false;
            if (recursGuard) {
                return;
            }

            const int          margins = MaxSDK::UIScaled(12);
            const QFontMetrics metrics(header->font());
            std::map<int, int> columnMinimumWidthMaps = {
                { 1, metrics.boundingRect(headerNameList[1]).width() + margins },
                { 2, metrics.boundingRect(headerNameList[2]).width() + margins },
                { 3, metrics.boundingRect(headerNameList[3]).width() + margins },
            };
            for (const auto& item : columnMinimumWidthMaps) {
                if (logicalIndex == item.first && newSize < item.second) {
                    recursGuard = true;
                    header->resizeSection(item.first, item.second);
                    recursGuard = false;
                }
            }
        });

    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);

    // Setup the table cells from a given config.
    auto setFromConfig = [this, table](const MaxUsd::MaxMeshConversionOptions conversionOptions) {
        const MaxUsd::MaxMeshConversionOptions defaultOptions;

        for (int channel = -NUM_HIDDENMAPS; channel < MAX_MESHMAPS; ++channel) {
            const auto& config = conversionOptions.GetChannelPrimvarConfig(channel);
            auto        row = channel + NUM_HIDDENMAPS;

            // Enable/disable channel export checkbox :

            QCheckBox* mapCheckbox = new QCheckBox();
            mapCheckbox->setMaximumSize(QSize { 25, 24 });
            QWidget*     checkBoxLayoutWidget = new QWidget();
            QHBoxLayout* checkBoxLayout = new QHBoxLayout();
            checkBoxLayout->setAlignment(Qt::AlignCenter);
            checkBoxLayout->addWidget(mapCheckbox);
            checkBoxLayoutWidget->setLayout(checkBoxLayout);

            // Currently a disabled channel is specified by an empty target primvar. This is not
            // very practical for the UI. Probably want to change this later, i.e. have a boolean
            // for enabled/disabled. This probably would impact the Maxscript API however.
            const bool channelEnabled = !config.GetPrimvarName().IsEmpty();
            mapCheckbox->setCheckState(
                channelEnabled ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
            QObject::connect(
                mapCheckbox, &QCheckBox::stateChanged, this, [this, row, table](bool checked) {
                    auto primvarName = table->cellWidget(row, 2);
                    primvarName->setDisabled(!checked);
                    auto primvarType = table->cellWidget(row, 3);
                    primvarType->setDisabled(!checked);
                });

            table->setCellWidget(row, 0, checkBoxLayoutWidget);

            QTableWidgetItem* item = new QTableWidgetItem();
            item->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 1, item);

            // Channel column (not editable) :

            item->setFlags(Qt::ItemIsEnabled);
            if (channel == MAP_SHADING) {
                item->setText(QObject::tr("Illumination"));
            } else if (channel == MAP_ALPHA) {
                item->setText(QObject::tr("Alpha"));
            } else if (channel == 0) {
                item->setText(QObject::tr("Color"));
            } else {
                item->setText(QString::fromWCharArray(std::to_wstring(channel).c_str()));
            }

            // Target primvar :

            QLineEdit* lineEdit = new QLineEdit();
            table->setCellWidget(row, 2, lineEdit);
            const pxr::TfToken& primvarName = channelEnabled
                ? config.GetPrimvarName()
                : defaultOptions.GetChannelPrimvarConfig(channel).GetPrimvarName();
            lineEdit->setText(primvarName.data());
            lineEdit->setEnabled(channelEnabled);

            lineEdit->setStyleSheet("");

            connect(lineEdit, &QLineEdit::editingFinished, this, [this, lineEdit]() {
                const QString lineEditText = lineEdit->text();
                this->okBtn->setEnabled(true);

                const std::string lineEditStdStringText = lineEditText.toStdString();
                const std::string validPrimvarName
                    = pxr::TfMakeValidIdentifier(lineEditStdStringText);

                const auto errorStyle = "QLineEdit{border: 2px solid red; padding-left: 5px; }";

                if (lineEditStdStringText.empty()) {
                    lineEdit->setToolTip(tr("Primvar name cannot be empty."));
                    lineEdit->setStyleSheet(errorStyle);
                } else if (validPrimvarName != lineEditStdStringText) {
                    lineEdit->setToolTip(tr(
                        "Primvar name can only contain alphanumeric characters and underscores."));
                    lineEdit->setStyleSheet(errorStyle);
                    this->okBtn->setEnabled(false);
                } else {
                    QToolTip::hideText();
                    lineEdit->setToolTip("");
                    lineEdit->setStyleSheet("");
                }
            });

            // Primvar type :

            QComboBox* primvarTypeCombo = new QComboBox();
            primvarTypeCombo->addItem(QString("TexCoord2fArray"));
            primvarTypeCombo->addItem(QString("TexCoord3fArray"));
            primvarTypeCombo->addItem(QString("FloatArray"));
            primvarTypeCombo->addItem(QString("Float2Array"));
            primvarTypeCombo->addItem(QString("Float3Array"));
            primvarTypeCombo->addItem(QString("Color3fArray"));

            const auto primvarType = channelEnabled
                ? config.GetPrimvarType()
                : defaultOptions.GetChannelPrimvarConfig(channel).GetPrimvarType();
            primvarTypeCombo->setCurrentIndex(static_cast<int>(primvarType));
            primvarTypeCombo->setEnabled(channelEnabled);
            table->setCellWidget(row, 3, primvarTypeCombo);
        }
    };

    const auto meshConversionOptions = this->buildOptions.GetMeshConversionOptions();
    setFromConfig(meshConversionOptions);

    // Resize the table from the content.
    table->resizeColumnsToContents();
    int size { 0 };
    for (int i = 0; i < table->columnCount(); ++i) {
        size += table->columnWidth(i);
    }
    table->setMinimumWidth(size + 10);
    VLayout->addWidget(table);

    QHBoxLayout* toggleButtonsLayout = new QHBoxLayout(&mapDetailsDialog);

    auto exportAllButton = new QPushButton(&mapDetailsDialog);
    exportAllButton->setText(QObject::tr("Select All"));
    QObject::connect(exportAllButton, &QPushButton::clicked, &mapDetailsDialog, [table]() {
        for (int i = 0; i < table->rowCount(); ++i) {
            const auto hLayout = static_cast<QHBoxLayout*>(table->cellWidget(i, 0)->layout());
            auto       checkBox = static_cast<QCheckBox*>(hLayout->itemAt(0)->widget());
            checkBox->setCheckState(Qt::Checked);
        }
    });

    auto exportNoneButton = new QPushButton(&mapDetailsDialog);
    exportNoneButton->setText(QObject::tr("Select None"));
    QObject::connect(exportNoneButton, &QPushButton::clicked, &mapDetailsDialog, [table]() {
        for (int i = 0; i < table->rowCount(); ++i) {
            const auto hLayout = static_cast<QHBoxLayout*>(table->cellWidget(i, 0)->layout());
            auto       checkBox = static_cast<QCheckBox*>(hLayout->itemAt(0)->widget());
            checkBox->setCheckState(Qt::Unchecked);
        }
    });

    QSpacerItem* horizontalSpacerItem
        = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto resetSettingsButton = new QPushButton(&mapDetailsDialog);
    resetSettingsButton->setText(QObject::tr("Reset Settings"));
    QObject::connect(
        resetSettingsButton, &QPushButton::clicked, &mapDetailsDialog, [table, &setFromConfig]() {
            setFromConfig(MaxUsd::MaxMeshConversionOptions {});
        });

    toggleButtonsLayout->addWidget(exportAllButton);
    toggleButtonsLayout->addWidget(exportNoneButton);
    toggleButtonsLayout->addSpacerItem(horizontalSpacerItem);
    toggleButtonsLayout->addWidget(resetSettingsButton);

    VLayout->addLayout(toggleButtonsLayout);

    QFrame* hFrame = new QFrame;
    hFrame->setFrameShape(QFrame::HLine);
    VLayout->addWidget(hFrame);

    QDialogButtonBox* okCancelButtons = new QDialogButtonBox(&mapDetailsDialog);
    okCancelButtons->setObjectName(QString::fromUtf8("Buttons"));
    okCancelButtons->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);

    this->okBtn = okCancelButtons->button(QDialogButtonBox::Ok);

    connect(
        okCancelButtons,
        &QDialogButtonBox::accepted,
        &mapDetailsDialog,
        [this, &mapDetailsDialog, table]() {
            const MaxUsd::MaxMeshConversionOptions defaultOptions;
            MaxUsd::MaxMeshConversionOptions       conversionOptions
                = this->buildOptions.GetMeshConversionOptions();
            for (int i = 0; i < table->rowCount(); ++i) {
                QHBoxLayout* hLayout = static_cast<QHBoxLayout*>(table->cellWidget(i, 0)->layout());
                QCheckBox*   checkBox = static_cast<QCheckBox*>(hLayout->itemAt(0)->widget());
                QLineEdit*   targetPrimvarWidget = static_cast<QLineEdit*>(table->cellWidget(i, 2));
                QComboBox*   primvarTypeComboBox = static_cast<QComboBox*>(table->cellWidget(i, 3));

                if (checkBox->isChecked()) {
                    MaxUsd::MappedAttributeBuilder::Type primvarType {
                        // default value required
                        // prevent C4701: potentially uninitialized local variable 'primvarType'
                        // used
                        MaxUsd::MappedAttributeBuilder::Type::TexCoord2fArray
                    };
                    QString selectedPrimvar = primvarTypeComboBox->currentText();
                    if (selectedPrimvar == "Color3fArray") {
                        primvarType = MaxUsd::MappedAttributeBuilder::Type::Color3fArray;
                    } else if (selectedPrimvar == "Float3Array") {
                        primvarType = MaxUsd::MappedAttributeBuilder::Type::Float3Array;
                    } else if (selectedPrimvar == "Float2Array") {
                        primvarType = MaxUsd::MappedAttributeBuilder::Type::Float2Array;
                    } else if (selectedPrimvar == "FloatArray") {
                        primvarType = MaxUsd::MappedAttributeBuilder::Type::FloatArray;
                    } else if (selectedPrimvar == "TexCoord3fArray") {
                        primvarType = MaxUsd::MappedAttributeBuilder::Type::TexCoord3fArray;
                    } else if (selectedPrimvar == "TexCoord2fArray") {
                        primvarType = MaxUsd::MappedAttributeBuilder::Type::TexCoord2fArray;
                    }

                    conversionOptions.SetChannelPrimvarConfig(
                        i - NUM_HIDDENMAPS,
                        { pxr::TfToken(targetPrimvarWidget->text().toStdString()), primvarType });
                } else {
                    const auto channel = i - NUM_HIDDENMAPS;
                    conversionOptions.SetChannelPrimvarConfig(
                        channel,
                        { pxr::TfToken(),
                          defaultOptions.GetChannelPrimvarConfig(channel).GetPrimvarType() });
                }
            }
            this->buildOptions.SetMeshConversionOptions(conversionOptions);
            mapDetailsDialog.accept();
        });

    connect(okCancelButtons, &QDialogButtonBox::rejected, &mapDetailsDialog, &QDialog::reject);

    VLayout->addWidget(okCancelButtons);

    mapDetailsDialog.adjustSize();
    mapDetailsDialog.exec();
}