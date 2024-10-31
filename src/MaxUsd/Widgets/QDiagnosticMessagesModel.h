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
#include <MaxUsd/Utilities/DiagnosticDelegate.h>

#include <MaxUsd.h>
#include <QtGui/QStandardItemModel.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief Qt Model to list of Diagnostic Messages emitted by USD.
 * \remarks Populating the Model with Diagnostic Messages coming from USD is done through the APIs exposted by the
 * DiagnosticMessagesModelFactory.
 */
class QDiagnosticMessagesModel : public QStandardItemModel
{
public:
    /**
     * \brief Constructor.
     * \param parent A reference to the parent of the Model.
     */
    explicit QDiagnosticMessagesModel(QObject* parent = nullptr) noexcept;

    /**
     * \brief Return the item at the given index, or "nullptr" if the given index is invalid.
     * \param index The index for which to return the item.
     * \return The item at the given index, or "nullptr" if it is invalid.
     */
    MaxUsd::Diagnostics::Message* GetItemAtIndex(const QModelIndex& index) const;

    /**
     * \brief Order of the columns as they appear in the Table.
     * \remarks The order of the enumeration is important.
     */
    enum TABLE_COLUMNS
    {
        TYPE,    /// Type of the Diagnostic Message.
        CONTENT, /// Human-readable text content of the Message.
        LAST     /// Last element of the enum.
    };
};

} // namespace MAXUSD_NS_DEF
