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
#pragma once
#include <MaxUsd/MaxUSDAPI.h>
#include <MaxUsd/Utilities/DiagnosticDelegate.h>

#include <MaxUsd.h>
#include <QtCore/QList>
#include <memory>
#include <vector>

class QObject;
class QStandardItem;

namespace MAXUSD_NS_DEF {

class QDiagnosticMessagesModel;

/**
 * \brief Factory to create a list of USD Diagnostic Messages suitable to be displayed in a Qt Table.
 */
class MaxUSDAPI DiagnosticMessagesModelFactory
{
private:
    /**
     * \brief Default Constructor (deleted).
     */
    DiagnosticMessagesModelFactory() = delete;

public:
    /**
     * \brief Create an empty QDiagnosticMessagesModel.
     * \param parent A reference to the parent of the QDiagnosticMessagesModel.
     * \return An empty QDiagnosticMessagesModel.
     */
    static std::unique_ptr<QDiagnosticMessagesModel>
    CreateEmptyTableModel(QObject* parent = nullptr);

    /**
     * \brief Create a QDiagnosticMessagesModel from the given list of USD Diagnostic Messages.
     * \param messages A reference to the list of USD Diagnostic Messages from which to create a QDiagnosticMessagesModel.
     * \param parent A reference to the parent of the QDiagnosticMessagesModel.
     * \return A QDiagnosticMessagesModel created from the given list of USD Diagnostic Messages.
     */
    static std::unique_ptr<QDiagnosticMessagesModel> CreateFromMessageList(
        const std::vector<MaxUsd::Diagnostics::Message>& messages,
        QObject*                                         parent = nullptr);

protected:
    /**
     * \brief Create the list of data cells used to represent the given Diagnostic Message's data in the table.
     * \param message The DiagnosticMessage for which to create the list of data cells.
     * \return The list of data cells used to represent the given USD DiagnosticMessage's data in the table.
     */
    static QList<QStandardItem*> CreateMessageRow(const MaxUsd::Diagnostics::Message& message);
};

} // namespace MAXUSD_NS_DEF
