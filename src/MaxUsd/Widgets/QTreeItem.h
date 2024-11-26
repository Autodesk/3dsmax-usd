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
#include <pxr/usd/usd/prim.h>

#include <MaxUsd.h>
#include <QtGui/QStandardItem>

namespace MAXUSD_NS_DEF {

/**
 * \brief Item representing a node used to build a Qt QTreeModel.
 * \remarks This item is intended to hold references to USD Prims in the future, so additional information can be
 * displayed to the User when interacting with Tree content.
 */
class QTreeItem : public QStandardItem
{
public:
    /**
     * \brief Constructor.
     * \param prim The USD Prim to represent with this item.
     * \param text Column text to display on the View of the the Qt QTreeModel.
     */
    explicit QTreeItem(const pxr::UsdPrim& prim, const QString& text) noexcept;

    /**
     * \brief Destructor.
     */
    virtual ~QTreeItem() = default;

    /**
     * \brief Return the USD Prim that is represented by the item.
     * \return The USD Prim that is represented by the item.
     */
    pxr::UsdPrim GetPrim() const;

    /**
     * \brief Return a flag indicating the type of the item.
     * \remarks This is used by Qt to distinguish custom items from the base class.
     * \return A flag indicating that the type is a custom iem, different from the base class.
     */
    int type() const override;

protected:
    /// The USD Prim that the item represents in the QTreeModel.
    pxr::UsdPrim prim;
};

} // namespace MAXUSD_NS_DEF
