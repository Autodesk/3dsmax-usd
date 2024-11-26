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

#include <MaxUsd.h>
#include <QtCore/QList>
#include <memory>
#include <string>
#include <vector>

class QObject;
class QStandardItem;

namespace MAXUSD_NS_DEF {

class QTreeModel;

/**
 * \brief Factory to create a tree-like structure of USD content suitable to be displayed in a TreeView.
 */
class MaxUSDAPI TreeModelFactory
{
private:
    /**
     * \brief Default Constructor (deleted).
     */
    TreeModelFactory() = delete;

public:
    enum class TypeFilteringMode
    {
        NoFilter,
        Include,
        Exclude
    };

    /**
     * \brief Create an empty QTreeModel.
     * \param parent A reference to the parent of the QTreeModel.
     * \return An empty QTreeModel.
     */
    static std::unique_ptr<QTreeModel> CreateEmptyTreeModel(QObject* parent = nullptr);

    /**
     * \brief Create a QTreeModel from the given USD Stage.
     * \param stage A reference to the USD Stage from which to create a QTreeModel.
     * \param parent A reference to the parent of the QTreeModel.
     * \return A QTreeModel created from the given USD Stage.
     */
    static std::unique_ptr<QTreeModel>
    CreateFromStage(const pxr::UsdStageRefPtr& stage, QObject* parent = nullptr);

    /**
     * \brief Create a QTreeModel from the given search filter applied to the given USD Stage.
     * \param stage A reference to the USD Stage from which to create a QTreeModel.
     * \param searchFilter A text filter to use as case-insensitive wildcard search pattern to find matching USD Prims
     * in the given USD Stage.
     * \param filterMode The applied filter type. Include or exclude the Prim types contained in filteredTypeNames.
     * \param filteredTypeNames The Prim types used to filter the stage.
     * \param parent A reference to the parent of the QTreeModel.
     * \return A QTreeModel created from the given search filter applied to the given USD Stage.
     */
    static std::unique_ptr<QTreeModel> CreateFromSearch(
        const pxr::UsdStageRefPtr&      stage,
        const std::string&              searchFilter,
        TypeFilteringMode               filterMode = TypeFilteringMode::NoFilter,
        const std::vector<std::string>& filteredTypeNames = {},
        QObject*                        parent = nullptr);

protected:
    /**
     * \brief Holder for the definition of the metadata information of SdfPath objects, so they can be used in
     * unordered/hashed STL containers.
     */
    struct SdfPathHash
    {
        /**
         * \brief Hashing function for SDF Path objects, leveraging the already-existing hashing function provided by
         * the USD library.
         * \param path The SDF Path to hash.
         * \return The hash value of the given SDF Path.
         */
        size_t operator()(const pxr::SdfPath& path) const { return path.GetHash(); }
    };

    /// Type definition for an STL unordered set of SDF Paths:
    using unordered_sdfpath_set = std::unordered_set<pxr::SdfPath, SdfPathHash>;

    /**
     * \brief Create the list of data cells used to represent the given USD Prim's data in the tree.
     * \param prim The USD Prim for which to create the list of data cells.
     * \return The List of data cells used to represent the given USD Prim's data in the tree.
     */
    static QList<QStandardItem*> CreatePrimRow(const pxr::UsdPrim& prim);

    /**
     * \brief Build the tree hierarchy starting at the given USD Prim.
     * \param prim The USD Prim from which to start building the tree hierarchy.
     * \param parentItem The parent into which to attach the tree hierarchy.
     */
    static void BuildTreeHierarchy(const pxr::UsdPrim& prim, QStandardItem* parentItem);
    /**
     * \brief Build the tree hierarchy starting at the given USD Prim.
     * \param prim The USD Prim from which to start building the tree hierarchy.
     * \param parentItem The parent into which to attach the tree hierarchy.
     * \param primsToIncludeInTree The list of USD Prim paths to include when building the tree.
     * \param insertionsRemaining The number of items yet to be inserted.
     */
    static void BuildTreeHierarchy(
        const pxr::UsdPrim&          prim,
        QStandardItem*               parentItem,
        const unordered_sdfpath_set& primsToIncludeInTree,
        size_t&                      insertionsRemaining);

    /**
     * \brief Return the list of SDF Paths of USD Prims matching the given search filter, based on the name of the Prim.
     * \remarks This would benefit from being moved to another class in the future, to better separate the logic of
     * instantiating Models from the logic of how to actually populate them.
     * \param stage A reference to the USD Stage in which to search for USD Prims matching the given search criteria.
     * \param searchFilter The search filter against which to try and match USD Prims in the given USD Stage.
     * \param filterMode The applied filter type. Include or exclude the Prim types contained in filteredTypeNames.
     * \param filteredTypeNames The Prim types used to filter the stage.
     * \return The list of SDF Paths of USD Prims matching the given search filter.
     */
    static std::vector<pxr::SdfPath> FindMatchingPrimPaths(
        const pxr::UsdStageRefPtr&      stage,
        const std::string&              searchFilter,
        TypeFilteringMode               filterMode,
        const std::vector<std::string>& filteredTypeNames);

    /**
     * \brief Check if the given string needle is contained in the given string haystack, in a case-insensitive way.
     * \remarks This would benefit from being moved to another class in the future.
     * \param haystack The haystack in which to search for the given needle.
     * \param needle The needle to look for in the given haystack.
     * \param useWildCardSearch A flag indicating if the search should be performed in wildcard-type.
     * \return A flag indicating whether or not the given needle was found in the given haystack.
     */
    static bool FindString(
        const std::string& haystack,
        const std::string& needle,
        bool               useWildCardSearch = false);
};

} // namespace MAXUSD_NS_DEF
