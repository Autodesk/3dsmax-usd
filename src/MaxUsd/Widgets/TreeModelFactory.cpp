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
#include "TreeModelFactory.h"

#include "QTreeItem.h"
#include "QTreeModel.h"

#include <pxr/usd/usd/primrange.h>
#include <pxr/usd/usdGeom/subset.h>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QRegularExpression>
#include <cctype>
#include <type_traits>

namespace MAXUSD_NS_DEF {

// Ensure the TreeModelFactory is not constructible, as it is intended to be used only through
// static factory methods.
//
// While additional traits like std::is_copy_constructible or std::is_move_constructible could also
// be used, the fact that the Factory cannot be (traditionally) instantiated prevents other
// constructors and assignments operators from being useful.
static_assert(
    !std::is_constructible<TreeModelFactory>::value,
    "TreeModelFactory should not be constructible.");

std::unique_ptr<QTreeModel> TreeModelFactory::CreateEmptyTreeModel(QObject* parent)
{
    std::unique_ptr<QTreeModel> treeModel = std::make_unique<QTreeModel>(parent);
    treeModel->setHorizontalHeaderLabels(
        { QObject::tr("Prim Name"), QObject::tr("Path"), QObject::tr("Type") });
    return treeModel;
}

std::unique_ptr<QTreeModel>
TreeModelFactory::CreateFromStage(const pxr::UsdStageRefPtr& stage, QObject* parent)
{
    std::unique_ptr<QTreeModel> treeModel = CreateEmptyTreeModel(parent);
    BuildTreeHierarchy(stage->GetPseudoRoot(), treeModel->invisibleRootItem());
    return treeModel;
}

std::unique_ptr<QTreeModel> TreeModelFactory::CreateFromSearch(
    const pxr::UsdStageRefPtr&      stage,
    const std::string&              searchFilter,
    TypeFilteringMode               filterMode,
    const std::vector<std::string>& filteredTypeNames,
    QObject*                        parent)
{
    // Optimization: If the provided search filter is empty, fallback to directly importing the
    // content of the given USD Stage. This can can happen in cases where the User already typed
    // characters in the search box before pressing backspace up until all characters were removed.
    if (searchFilter.empty()
        && (filteredTypeNames.empty() || filterMode == TypeFilteringMode::NoFilter)) {
        return CreateFromStage(stage, parent);
    }

    std::unordered_set<pxr::SdfPath, SdfPathHash> primsToIncludeInTree;
    const auto                                    primPaths
        = FindMatchingPrimPaths(stage, searchFilter, filterMode, filteredTypeNames);
    for (const auto& matchingPath : primPaths) {
        pxr::UsdPrim prim = stage->GetPrimAtPath(matchingPath);

        // When walking up the ancestry chain, the Root Node will end up being considered once and
        // its parent (an invalid Prim) will be selected. Since there is no point iterating up the
        // hierarchy at this point, stop processing the current Prim an move on to the next one
        // matching the search filter.
        while (prim.IsValid()) {
            bool primAlreadyIncluded
                = primsToIncludeInTree.find(prim.GetPath()) != primsToIncludeInTree.end();
            if (primAlreadyIncluded) {
                // If the USD Prim is already part of the set of search results to be displayed, it
                // is unnecessary to walk up the ancestory chain in an attempt to process further
                // Prims, as it means they have already been added to the list up to the Root Node.
                break;
            }

            primsToIncludeInTree.insert(prim.GetPath());
            prim = prim.GetParent();
        }
    }

    // Optimization: Count the number of USD Prims expected to be inserted in the QTreeModel, so
    // that the search process can stop early if all USD Prims have already been found. While
    // additional "narrowing" techniques can be used in the future to further enhance the
    // performance, this may provide sufficent performance in most cases to remain as-is for early
    // User feedback.
    size_t                      insertionsRemaining = primsToIncludeInTree.size();
    std::unique_ptr<QTreeModel> treeModel = TreeModelFactory::CreateEmptyTreeModel(parent);
    BuildTreeHierarchy(
        stage->GetPseudoRoot(),
        treeModel->invisibleRootItem(),
        primsToIncludeInTree,
        insertionsRemaining);
    return treeModel;
}

std::vector<pxr::SdfPath> TreeModelFactory::FindMatchingPrimPaths(
    const pxr::UsdStageRefPtr&      stage,
    const std::string&              searchFilter,
    TypeFilteringMode               filterMode,
    const std::vector<std::string>& filteredTypeNames)
{
    // Using regular expressions when searching through the set of data can be expensive compared to
    // doing a plain text search. In addition, it may be possible for the User to want to search for
    // content containing the "*" character instead of using this token as wildcard, which is not
    // currently supported. In order to properly handle this, the UI could expose search options in
    // the future, where Users would be able to pick the type of search they wish to perform (likely
    // defaulting to a plain text search).
    bool                      useWildCardSearch = searchFilter.find('*') != std::string::npos;
    std::vector<pxr::SdfPath> matchingPrimPaths;

    for (const auto& prim : stage->TraverseAll()) {
        if (searchFilter.empty()
            || FindString(prim.GetName().GetString(), searchFilter, useWildCardSearch)) {
            if (!filteredTypeNames.empty()) {
                if (filterMode == TypeFilteringMode::Include
                    && std::find(
                           filteredTypeNames.begin(), filteredTypeNames.end(), prim.GetTypeName())
                        == filteredTypeNames.end()) {
                    continue;
                }
                if (filterMode == TypeFilteringMode::Exclude
                    && std::find(
                           filteredTypeNames.begin(), filteredTypeNames.end(), prim.GetTypeName())
                        != filteredTypeNames.end()) {
                    continue;
                }
            }
            matchingPrimPaths.emplace_back(prim.GetPath());
        }
    }
    return matchingPrimPaths;
}

QList<QStandardItem*> TreeModelFactory::CreatePrimRow(const pxr::UsdPrim& prim)
{
    // Cache the values to be displayed, in order to avoid querying the USD Prim too frequently
    // (despite it being cached and optimized for frequent access). Avoiding frequent conversions
    // from USD Strings to Qt Strings helps in keeping memory allocations low.
    return { new QTreeItem(
                 prim,
                 prim.IsPseudoRoot() ? QObject::tr("Root")
                                     : QString::fromStdString(prim.GetName().GetString())),
             new QTreeItem(prim, QString::fromStdString(prim.GetPath().GetString())),
             new QTreeItem(prim, QString::fromStdString(prim.GetTypeName().GetString())) };
}

void TreeModelFactory::BuildTreeHierarchy(const pxr::UsdPrim& prim, QStandardItem* parentItem)
{
    QList<QStandardItem*> primDataCells = CreatePrimRow(prim);
    parentItem->appendRow(primDataCells);

    for (const auto& childPrim : prim.GetAllChildren()) {
        if (!childPrim.IsA<pxr::UsdGeomSubset>()) {
            BuildTreeHierarchy(childPrim, primDataCells.front());
        }
    }
}

void TreeModelFactory::BuildTreeHierarchy(
    const pxr::UsdPrim&          prim,
    QStandardItem*               parentItem,
    const unordered_sdfpath_set& primsToIncludeInTree,
    size_t&                      insertionsRemaining)
{
    bool primShouldBeIncluded
        = primsToIncludeInTree.find(prim.GetPath()) != primsToIncludeInTree.end();
    if (primShouldBeIncluded) {
        QList<QStandardItem*> primDataCells = CreatePrimRow(prim);
        parentItem->appendRow(primDataCells);

        // Only continue processing additional USD Prims if all expected results have not already
        // been found:
        if (--insertionsRemaining > 0) {
            for (const auto& childPrim : prim.GetAllChildren()) {
                BuildTreeHierarchy(
                    childPrim, primDataCells.front(), primsToIncludeInTree, insertionsRemaining);
            }
        }
    }
}

bool TreeModelFactory::FindString(
    const std::string& haystack,
    const std::string& needle,
    bool               useWildCardSearch)
{
    // NOTE: Most of the time, the needle is unlikely to contain a wildcard search.
    if (useWildCardSearch) {
        // Needle contains at least one wildcard character, proceed with a regular expression
        // search.

        // NOTE: Both leading and trailing wildcards are added to the needle in order to make sure
        // search is made against Prims whose name contains the given search filter. Otherwise,
        // searching for "lorem*ipsum" would match "lorem_SOME-TEXT_ipsum" but not
        // "SOME-TEXT_lorem_ipsum", which is inconvenient as too restrictive for casual Users to
        // type. This ensure search results are handled in a similar way to Windows Explorer, for
        // example.
        const QRegularExpression regularExpression(
            QString::fromStdString("*" + needle + "*"), QRegularExpression::CaseInsensitiveOption);
        return regularExpression.match(QString::fromStdString(haystack)).hasMatch();
    } else {
        // Needle does not contain any wildcard characters, use a simple case-insensitive search:
        auto it = std::search(
            haystack.begin(),
            haystack.end(),
            needle.begin(),
            needle.end(),
            [](char charA, char charB) { return std::toupper(charA) == std::toupper(charB); });
        return it != haystack.end();
    }
}

} // namespace MAXUSD_NS_DEF
