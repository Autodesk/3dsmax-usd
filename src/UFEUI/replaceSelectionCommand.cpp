//
// Copyright 2024 Autodesk
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

#include "ReplaceSelectionCommand.h"

#include "utils.h"

#include <ufe/globalSelection.h>
#include <ufe/hierarchy.h>
#include <ufe/observableSelection.h>

// useful for debugging, so I keep it here
// #define DBG_REPLACE_SELECTION_COMMAND
#ifdef DBG_REPLACE_SELECTION_COMMAND

#include <qdebug.h>
namespace {

QString selectionToString(const Ufe::Selection& selection)
{
    QStringList result;
    for (const auto& item : selection) {
        result << QString::fromStdString(item->nodeName());
    }
    return QString("[%1]").arg(result.join(", "));
}

}; // namespace

#endif

namespace UfeUi {

ReplaceSelectionCommand::ReplaceSelectionCommand(const Ufe::Selection& selection)
    : _previousSelection { *Ufe::GlobalSelection::get() }
    , _selection { selection }
{
#ifdef DBG_REPLACE_SELECTION_COMMAND
    qDebug() << "ReplaceSelectionCommand::CREATE -> " << selectionToString(_previousSelection)
             << " to " << selectionToString(_selection);

    if (Utils::selectionsAreEquivalent(_selection, _previousSelection)) {
        qDebug() << "ReplaceSelectionCommand::CREATE with equivalent selections!";
    }
#endif
}

void ReplaceSelectionCommand::undo()
{
#ifdef DBG_REPLACE_SELECTION_COMMAND
    qDebug() << "ReplaceSelectionCommand::UNDO -> selecting "
             << selectionToString(_previousSelection);
#endif

    Ufe::GlobalSelection::get()->replaceWith(_previousSelection);
}

void UfeUi::ReplaceSelectionCommand::redo()
{
#ifdef DBG_REPLACE_SELECTION_COMMAND
    qDebug() << "ReplaceSelectionCommand::REDO -> selecting " << selectionToString(_selection);
#endif

    Ufe::GlobalSelection::get()->replaceWith(_selection);
}

}; // namespace UfeUi
