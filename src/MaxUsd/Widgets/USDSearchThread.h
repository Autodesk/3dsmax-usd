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
#include "TreeModelFactory.h"

#include <MaxUsd/MaxUSDAPI.h>

#include <MaxUsd.h>
#include <QtCore/QThread.h>

namespace MAXUSD_NS_DEF {

class QTreeModel;

/**
 * \brief Thread used to identify specific USD Prims within a provided USD Stage.
 */
class MaxUSDAPI USDSearchThread : public QThread
{
public:
    /**
     * \brief Constructor.
     * \param stage A reference to the USD Stage in which to perform the search.
     * \param searchFilter The search filter against which to try and match USD Prims in the given USD Stage.
     * \param filterMode The applied filter type. Include or exclude the Prim types contained in filteredTypeNames.
     * \param filteredTypeNames The Prim types used to filter the stage.
     * \param parent A reference to the parent of the thread.
     */
    explicit USDSearchThread(
        const pxr::UsdStageRefPtr&          stage,
        const std::string&                  searchFilter,
        TreeModelFactory::TypeFilteringMode filterMode
        = TreeModelFactory::TypeFilteringMode::NoFilter,
        const std::vector<std::string>& filteredTypeNames = {},
        QObject*                        parent = nullptr);

    /**
     * \brief Consume the QTreeModel built from the results of the search performed within the USD Stage.
     * \return The QTreeModel built from the results of the search performed within the USD Stage.
     */
    std::unique_ptr<QTreeModel> ConsumeResults();

protected:
    /**
     * \brief Perform the search in the Qt thread.
     */
    void run() override;

protected:
    /// Reference to the USD Stage within which to perform the search:
    pxr::UsdStageRefPtr stage;
    /// Search filter against which to try and match USD Prims in the USD Stage:
    std::string searchFilter;
    /// Reference to the QTreeModel built from the search performed within the USD Stage:
    std::unique_ptr<QTreeModel> results;

    TreeModelFactory::TypeFilteringMode filterMode;
    std::vector<std::string>            filteredTypeNames;
};

} // namespace MAXUSD_NS_DEF
