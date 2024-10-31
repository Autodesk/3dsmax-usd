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

#include <MaxUsd.h>
#include <functional>
#include <inode.h>
#include <vector>

namespace MAXUSD_NS_DEF {

class INodeRange
{
public:
    enum class TraversalType
    {
        PRE,
        POST,
        PRE_AND_POST,
    };

    class Iterator
    {
        friend class INodeRange;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = INode*;
        using reference = value_type&;
        using pointer = value_type*;
        using difference_type = std::ptrdiff_t;

        void PruneChildren();

        value_type operator*() { return dereference(); }

        const value_type operator*() const { return dereference(); }

        Iterator operator++() { return preincrement(); }

        Iterator operator++(int) { return postincrement(); }

        bool operator==(const Iterator& other) { return equal(other); }

        bool operator!=(const Iterator& other) { return !equal(other); }

    private:
        std::vector<int> childIndices;
        INode*           currentNode;
        TraversalType    traversalType;
        TraversalType    currentTraversalType;

        Iterator(INode* subtreeRoot, TraversalType traversalType)
            : currentNode(subtreeRoot)
            , traversalType(traversalType)
        {
            switch (traversalType) {
            case TraversalType::POST: {
                currentTraversalType = TraversalType::POST;
                if (subtreeRoot->NumberOfChildren()) {
                    currentNode = subtreeRoot->GetChildNode(0);
                    childIndices.push_back(0);
                }
                break;
            }
            }
        }

        Iterator(const Iterator&) = default;

        value_type dereference() { return currentNode; }

        const value_type dereference() const { return currentNode; }

        Iterator preincrement()
        {
            switch (traversalType) {
            case TraversalType::PRE: break;
            }
        }

        Iterator postincrement() { return Iterator(*this); }

        bool equal(const Iterator& other) const { return false; }
    };

    using iterator = Iterator;
    using const_iterator = Iterator;

    INodeRange(INode* subtreeRoot, TraversalType traversalType = TraversalType::PRE)
        : subtreeRoot(subtreeRoot)
        , traversalType(traversalType)
    {
    }

    iterator begin() { return iterator(subtreeRoot, traversalType); }

    const_iterator begin() const { return const_iterator(subtreeRoot, traversalType); }

    const_iterator cbegin() { return const_iterator(subtreeRoot, traversalType); }

    iterator end() { return iterator(nullptr, traversalType); }

    const_iterator end() const { return const_iterator(nullptr, traversalType); }

    const_iterator cend() { return const_iterator(nullptr, traversalType); }

private:
    INode*        subtreeRoot;
    TraversalType traversalType;
};

inline void DepthFirstTraverseGraph(
    INode*                                                                                 rootNode,
    const std::function<bool(INode* currentNode, const std::vector<INode*>& parentNodes)>& action)
{
    std::vector<INode*>                 parentNodes;
    std::vector<std::pair<int, INode*>> toProcess;
    toProcess.emplace_back(0, rootNode);

    while (!toProcess.empty()) {
        auto node = toProcess.back().second;
        auto depth = toProcess.back().first;
        toProcess.pop_back();

        parentNodes.resize(depth);

        if (action(node, parentNodes)) {
            for (auto i = 0; i < node->NumberOfChildren(); ++i) {
                toProcess.emplace_back(depth + 1, node->GetChildNode(i));
            }
        }
        parentNodes.push_back(node);
    }
}

} // namespace MAXUSD_NS_DEF