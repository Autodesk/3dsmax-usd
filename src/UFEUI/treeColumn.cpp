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

#include "treeColumn.h"

#include "Views/explorer.h"

namespace UfeUi {

TreeColumn::TreeColumn(int visualIndex)
    : _visualIdx(visualIndex)
{
}

TreeColumn::~TreeColumn() = default;

void TreeColumn::addExplorer(Explorer* explorer)
{
    if (explorer) {
        auto it = std::find(_affectedExplorers.begin(), _affectedExplorers.end(), explorer);
        if (it == _affectedExplorers.end()) {
            _affectedExplorers.emplace_back(explorer);
        }
    }
}

void TreeColumn::removeExplorer(Explorer* explorer)
{
    if (explorer) {
        auto it = std::find(_affectedExplorers.begin(), _affectedExplorers.end(), explorer);
        if (it != _affectedExplorers.end()) {
            _affectedExplorers.erase(it);
        }
    }
}

} // namespace UfeUi