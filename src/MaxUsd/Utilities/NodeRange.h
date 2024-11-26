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
#include <cassert>
#include <cstddef>
#include <inode.h>
#include <iterator>
#include <type_traits>

namespace MAXUSD_NS_DEF {

/**
 * \brief Builds a new NodeRange depth first iterating over a node hierarchy.
 */
class NodeRange
{
public:
    /**
     *
     */
    template <typename T> class iteratorbase
    {
        friend class NodeRange;

        /**
         * \brief Builds a end sentinel iterator.
         */
        iteratorbase() noexcept { }

        /**
         * \brief Builds an iterator visiting \a startNode.
         */
        iteratorbase(T startNode) noexcept
            : nodes({ startNode })
        {
        }

        /**
         * \brief Builds an iterator visiting \a nodes.
         */
        template <typename IT>
        iteratorbase(IT begin, IT end)
            : nodes(begin, end)
        {
        }

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        /**
         * \brief Returns the currently visited node.
         */
        T operator*() const noexcept { return nodes.front(); }

        /**
         * \brief Increment the iterator and return its new, post increment, value.
         */
        iteratorbase& operator++()
        {
            increment();
            return *this;
        }

        /**
         * \brief Increment the iterator and return its current, pre increment, value.
         */
        iteratorbase operator++(int)
        {
            auto other = *this;
            increment();
            return other;
        }

        /**
         * \brief Returns if \a this is equal to \a other.
         */
        bool operator==(const iteratorbase<T>& other) const noexcept
        {
            return nodes == other.nodes;
        }

        /**
         * \brief Returns if \a this is different from \a other.
         */
        bool operator!=(const iteratorbase<T>& other) const noexcept
        {
            return nodes != other.nodes;
        }

        /**
         * \brief Copy operation.
         */
        iteratorbase<T>& operator=(const iteratorbase<T>& other) = default;

        /**
         * \brief Conversion operator when underlying iterator type is convertible.
         */
        template <typename TT, typename enabler = std::enable_if_t<std::is_convertible_v<T, TT>>>
        operator iteratorbase<TT>&() const noexcept
        {
            static_assert(std::is_convertible_v<T, TT>, "");
            return iteratorbase<TT>(nodes.begin(), nodes.end());
        }

    private:
        /// Stores the set of nodes currently processed.
        std::deque<T> nodes;

        /**
         * \brief Moves to the next node.
         */
        void increment()
        {
            assert(!nodes.empty());
            auto node = nodes.front();
            assert(node);
            nodes.pop_front();
            for (auto i = 0; i < node->NumberOfChildren(); ++i) {
                nodes.push_front(node->GetChildNode(i));
            }
        }
    };

    /// Mutable iterator over this range.
    using iterator = iteratorbase<INode*>;

    /// Non mutable iterator over this range.
    using const_iterator = iteratorbase<const INode*>;

    /**
     * \brief Create a range traversing hierarchy rooted at \a root.
     */
    NodeRange(INode* root)
        : it(root)
    {
    }

    ~NodeRange() { }

    iterator begin() { return it; }

    const_iterator begin() const { return it; }

    const_iterator cbegin() const { return it; }

    iterator end() { return iterator(); }

    const_iterator end() const { return const_iterator(); }

    const_iterator cend() const { return const_iterator(); }

    void test() { const_iterator t = iterator(it); }

    void increment() { ++it; }

private:
    iterator it;
};

} // namespace MAXUSD_NS_DEF
