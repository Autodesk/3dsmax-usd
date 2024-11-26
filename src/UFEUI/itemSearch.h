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
#ifndef UFEUI_ITEM_SEARCH_H
#define UFEUI_ITEM_SEARCH_H

#include <ufe/hierarchy.h>
#include <ufe/sceneItem.h>

#include <string>
#include <vector>

namespace UfeUi {

struct TypeFilter
{
    enum class Mode
    {
        NoFilter,
        Include,
        Exclude
    };
    Mode                     mode = Mode::NoFilter;
    std::vector<std::string> names = {};
};

class ItemSearch
{
public:
    /**
     * \brief Return the list of UFE items matching the given search filter, based on the name of the item.
     * \param sceneItem The UFE item to search from.
     * \param searchFilter The search filter against which to try and match UFE items in the given subtree.
     * \param typeFilter Type filtering config, to include or exclude items based on type name.
     * \param childFilter Ufe Hierarchy child filter, filters item when traversing the
     * hierarchy. Used by the runtime hierarchy implementation.
     * \return The list of UFE scene items matching the given search filter.
     */
    static std::vector<Ufe::SceneItem::Ptr> findMatchingPaths(
        const Ufe::SceneItem::Ptr&         sceneItem,
        const std::string&                 searchFilter,
        const TypeFilter&                  typeFilter,
        const Ufe::Hierarchy::ChildFilter& childFilter);

    /**
     * \brief Check if the given string needle is contained in the given string haystack, in a case-insensitive way.
     * \remarks This would benefit from being moved to another class in the future.
     * \param haystack The haystack in which to search for the given needle.
     * \param needle The needle to look for in the given haystack.
     * \param useWildCardSearch A flag indicating if the search should be performed in wildcard-type.
     * \return A flag indicating whether or not the given needle was found in the given haystack.
     */
    static bool findString(
        const std::string& haystack,
        const std::string& needle,
        bool               useWildCardSearch = false);
};

} // namespace UfeUi

#endif // UFEUI_ITEM_SEARCH_H
