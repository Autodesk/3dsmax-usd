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
#include "USDSearchThread.h"

#include "QTreeModel.h"

namespace MAXUSD_NS_DEF {

USDSearchThread::USDSearchThread(
    const pxr::UsdStageRefPtr&          stage,
    const std::string&                  searchFilter,
    TreeModelFactory::TypeFilteringMode filterMode,
    const std::vector<std::string>&     filteredTypeNames,
    QObject*                            parent)
    : QThread { parent }
    , stage { stage }
    , searchFilter { searchFilter }
    , filterMode { filterMode }
    , filteredTypeNames { filteredTypeNames }
{
    // Nothing to do.
}

std::unique_ptr<QTreeModel> USDSearchThread::ConsumeResults() { return std::move(results); }

void USDSearchThread::run()
{
    results
        = TreeModelFactory::CreateFromSearch(stage, searchFilter, filterMode, filteredTypeNames);
}

} // namespace MAXUSD_NS_DEF
