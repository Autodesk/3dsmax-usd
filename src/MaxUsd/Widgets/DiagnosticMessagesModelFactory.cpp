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
#include "DiagnosticMessagesModelFactory.h"

#include "QDiagnosticMessagesModel.h"

namespace MAXUSD_NS_DEF {

// Ensure the DiagnosticMessagesModelFactory is not constructible, as it is intended to be used only
// through static factory methods.
//
// While additional traits like std::is_copy_constructible or std::is_move_constructible could also
// be used, the fact that the Factory cannot be (traditionally) instantiated prevents other
// constructors and assignments operators from being useful.
static_assert(
    !std::is_constructible<DiagnosticMessagesModelFactory>::value,
    "DiagnosticMessagesModelFactory should not be constructible.");

std::unique_ptr<QDiagnosticMessagesModel>
DiagnosticMessagesModelFactory::CreateEmptyTableModel(QObject* parent)
{
    std::unique_ptr<QDiagnosticMessagesModel> tableModel
        = std::make_unique<QDiagnosticMessagesModel>(parent);
    tableModel->setHorizontalHeaderLabels(
        { QObject::tr(""),
          QObject::tr("USD raised the following diagnostic information about the scene. You may "
                      "want to review it:") });
    return tableModel;
}

std::unique_ptr<QDiagnosticMessagesModel> DiagnosticMessagesModelFactory::CreateFromMessageList(
    const std::vector<MaxUsd::Diagnostics::Message>& messages,
    QObject*                                         parent)
{
    std::unique_ptr<QDiagnosticMessagesModel> tableModel = CreateEmptyTableModel(parent);
    for (const auto& message : messages) {
        tableModel->appendRow(CreateMessageRow(message));
    }
    return tableModel;
}

QList<QStandardItem*>
DiagnosticMessagesModelFactory::CreateMessageRow(const MaxUsd::Diagnostics::Message& message)
{
    QStandardItem* typeColumn = new QStandardItem();

    QString      typeText = QObject::tr("Other");
    QBrush       brush = typeColumn->foreground();
    const QColor errorColor(237, 28, 36);
    const QColor statusColor(195, 195, 195);
    const QColor warningColor(255, 201, 14);

    switch (message.type) {
    case MaxUsd::Diagnostics::Message::MessageType::Error:
        typeText = QObject::tr("Error");
        brush.setColor(errorColor);
        break;
    case MaxUsd::Diagnostics::Message::MessageType::Status:
        typeText = QObject::tr("Info");
        brush.setColor(statusColor);
        break;
    case MaxUsd::Diagnostics::Message::MessageType::Warning:
        typeText = QObject::tr("Warning");
        brush.setColor(warningColor);
        break;
    default:
        // Nothing to do.
        break;
    }

    // Format columns of the row:
    QFont font = typeColumn->font();
    font.setBold(true);
    typeColumn->setFont(font);
    typeColumn->setForeground(brush);
    typeColumn->setText(typeText);
    typeColumn->setTextAlignment(Qt::AlignmentFlag::AlignCenter);

    QString formattedTextContent
        = QString::fromStdString(message.message.GetCommentary().c_str()).trimmed();
    QStandardItem* messageColumn = new QStandardItem(formattedTextContent);

    return { typeColumn, messageColumn };
}

} // namespace MAXUSD_NS_DEF
