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
#ifndef UFEUI_UTILS_H
#define UFEUI_UTILS_H

#include "UFEUIAPI.h"

#include <ufe/hierarchy.h>
#include <ufe/path.h>
#include <ufe/selection.h>

#include <QtGui/QColor>
#include <functional>

class QTreeView;
class QSortFilterProxyModel;

namespace UfeUi {
class TreeItem;
class TreeModel;

namespace Utils {

/**
 * \brief Returns the DPI scaling. Should be relative to a default of 96 DPI.
 * \return The DPI scaling.
 */
UFEUIAPI double dpiScale();

/**
 * \brief Sets the DPI scaling. Should be relative to a default of 96 DPI.
 * \param dpiScale The DPI scaling to set.
 */
UFEUIAPI void setDpiScale(double dpiScale);

// hash combiner taken from:
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0814r0.pdf
// boost::hash implementation also relies on the same algorithm:
// https://www.boost.org/doc/libs/1_64_0/boost/functional/hash/hash.hpp
template <typename T> void hashCombine(std::size_t& seed, const T& value)
{
    ::std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

/**
 * \brief Finds the UFE paths of scene items currently expanded in the treeView.
 * \param model The model loaded in the tree view.
 * \param proxyModel The proxy model used by the tree view.
 * \param treeView The treeview itself.
 * \param subtreeRoot Root item used for the search.
 * \param expanded Vector of paths that the function will fill.
 */
void findExpandedPaths(
    TreeModel*              model,
    QSortFilterProxyModel*  proxyModel,
    QTreeView*              treeView,
    const TreeItem*         subtreeRoot,
    std::vector<Ufe::Path>& expanded);

/**
 * \brief Expands the tree items associated with the given ufe paths.
 * \param model The model loaded in the tree view.
 * \param proxyModel The proxy model used by the tree view.
 * \param treeView The treeview itself.
 * \param expandedPaths Vector of UFE paths of the items to be expanded in the tree.
 */
void expandPaths(
    QTreeView*              treeView,
    TreeModel*              model,
    QSortFilterProxyModel*  proxyModel,
    std::vector<Ufe::Path>& expandedPaths);

// Stores the tree expand state in the constructor, and restores it on destruction.
class ExpandStateGuard
{
public:
    ExpandStateGuard(
        QTreeView*             treeView,
        const TreeItem*        subtreeRoot,
        TreeModel*             model,
        QSortFilterProxyModel* proxyModel);
    ~ExpandStateGuard();

private:
    TreeModel*             _model = nullptr;
    QSortFilterProxyModel* _proxyModel = nullptr;
    QTreeView*             _treeView = nullptr;
    std::vector<Ufe::Path> _expandedPaths;
};

/**
 * \brief Checks if two hierarchy filters are equal.
 * \param filter1 The first filter.
 * \param filter2 The second filter.
 * \return True, if passed filters are equal, false otherwise.
 */
bool filtersAreEqual(
    const Ufe::Hierarchy::ChildFilter& filter1,
    const Ufe::Hierarchy::ChildFilter& filter2);

/**
 * \brief Mix two QColors.
 * \param color1 The first color.
 * \param color2 The second color.
 * \param color1Amount Amount for color1, a ratio over 255.
 * \return The mixed color.
 */
inline QColor mixColors(const QColor& color1, const QColor& color2, int color1Amount)
{
    color1Amount = qBound(0, color1Amount, 255);
    const int color2Amount = 255 - color1Amount;
    return { ((color1.red() * color1Amount) + (color2.red() * color2Amount)) / 255,
             ((color1.green() * color1Amount) + (color2.green() * color2Amount)) / 255,
             ((color1.blue() * color1Amount) + (color2.blue() * color2Amount)) / 255 };
}

/** Checks if two UFE selections are representing an equivalent set of
 * sceneItems (without any particular order).
 * \param a The first selection.
 * \param b The second selection.
 * \return True, if passed selections are equivalent, false otherwise. */
inline bool selectionsAreEquivalent(const Ufe::Selection& a, const Ufe::Selection& b)
{
    auto aSize = a.size();

    if (aSize != b.size()) // different number of items
    {
        return false;
    }

    if (aSize == 0u) // both are empty
    {
        return true;
    }

    for (const auto& item : a) {
        if (!b.contains(item->path())) {
            return false;
        }
    }
    return true;
}

} // namespace Utils
} // namespace UfeUi

#endif UFEUI_UTILS_H