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
#include "ItemSearch.h"

#include <ufe/hierarchy.h>

#include <QtCore/QRegularExpression>
#include <cctype>
#include <stack>

namespace UfeUi {

std::vector<Ufe::SceneItem::Ptr> ItemSearch::findMatchingPaths(
    const Ufe::SceneItem::Ptr&         sceneItem,
    const std::string&                 searchFilter,
    const TypeFilter&                  typeFilter,
    const Ufe::Hierarchy::ChildFilter& childFilter)
{
    // Using regular expressions when searching through the set of data can be expensive compared to
    // doing a plain text search. In addition, it may be possible for the User to want to search for
    // content containing the "*" character instead of using this token as wildcard, which is not
    // currently supported. In order to properly handle this, the UI could expose search options in
    // the future, where Users would be able to pick the type of search they wish to perform (likely
    // defaulting to a plain text search).
    const bool useWildCardSearch = searchFilter.find('*') != std::string::npos;
    std::vector<Ufe::SceneItem::Ptr> matchingUfeItems;

    const auto root = Ufe::Hierarchy::hierarchy(sceneItem);

    std::stack<Ufe::Hierarchy::Ptr> hierarchyStack;

    hierarchyStack.push(root);

    while (!hierarchyStack.empty()) {
        const auto hierarchy = hierarchyStack.top();
        hierarchyStack.pop();

        for (const auto& child : hierarchy->filteredChildren(childFilter)) {
            if (searchFilter.empty()
                || findString(child->nodeName(), searchFilter, useWildCardSearch)) {
                if (!typeFilter.names.empty()) {
                    if (typeFilter.mode == TypeFilter::Mode::Include
                        && std::find(
                               typeFilter.names.begin(), typeFilter.names.end(), child->nodeType())
                            == typeFilter.names.end()) {
                        continue;
                    }
                    if (typeFilter.mode == TypeFilter::Mode::Exclude
                        && std::find(
                               typeFilter.names.begin(), typeFilter.names.end(), child->nodeType())
                            != typeFilter.names.end()) {
                        continue;
                    }
                }
                matchingUfeItems.emplace_back(child);
            }
            hierarchyStack.push(Ufe::Hierarchy::hierarchy(child));
        }
    }
    return matchingUfeItems;
}

bool ItemSearch::findString(
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
        return regularExpression
            .match(QString::fromStdString(haystack), 0, QRegularExpression::MatchType::NormalMatch)
            .hasMatch();
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

} // namespace UfeUi
