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
#include "ExplorerSearchThread.h"

#include "Views/Explorer.h"

#include <ufe/hierarchy.h>

namespace UfeUi {

ExplorerSearchThread::ExplorerSearchThread(
    const Ufe::SceneItem::Ptr&         rootItem,
    const TreeColumns&                 columns,
    const std::string&                 searchFilter,
    const TypeFilter&                  typeFilter,
    const Ufe::Hierarchy::ChildFilter& childFilter,
    QObject*                           parent)
    : QThread { parent }
    , _rootItem { rootItem }
    , _columns { columns }
    , _searchFilter { searchFilter }
    , _typeFilter { typeFilter }
    , _childFilter { childFilter }
{
    // Nothing to do.
}

std::unique_ptr<TreeModel> ExplorerSearchThread::consumeResults() { return std::move(_results); }

void ExplorerSearchThread::run()
{
    _results = TreeModel::create(_columns, nullptr);
    _results->buildTreeFrom(
        _results->root(), _rootItem, _searchFilter, _typeFilter, _childFilter, true);
}

} // namespace UfeUi
