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
#include "QDiagnosticMessagesModel.h"

namespace MAXUSD_NS_DEF {

QDiagnosticMessagesModel::QDiagnosticMessagesModel(QObject* parent) noexcept
    : QStandardItemModel { parent }
{
    // Nothing to do.
}

MaxUsd::Diagnostics::Message*
QDiagnosticMessagesModel::GetItemAtIndex(const QModelIndex& index) const
{
    if (index.isValid()) {
        return static_cast<MaxUsd::Diagnostics::Message*>(index.internalPointer());
    }
    return nullptr;
}

} // namespace MAXUSD_NS_DEF
