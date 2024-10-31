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

#include <usdUfe/ufe/UsdHierarchyHandler.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

/**
 * \brief 3dsMax USD runtime hierarchy handler.
 */
class MaxUsdHierarchyHandler : public UsdUfe::UsdHierarchyHandler
{
public:
    typedef std::shared_ptr<MaxUsdHierarchyHandler> Ptr;

    MaxUsdHierarchyHandler();
    ~MaxUsdHierarchyHandler() override;

    // Delete the copy/move constructors assignment operators.
    MaxUsdHierarchyHandler(const MaxUsdHierarchyHandler&) = delete;
    MaxUsdHierarchyHandler& operator=(const MaxUsdHierarchyHandler&) = delete;
    MaxUsdHierarchyHandler(MaxUsdHierarchyHandler&&) = delete;
    MaxUsdHierarchyHandler& operator=(MaxUsdHierarchyHandler&&) = delete;

    //! Create a MaxUsdHierarchyHandler.
    static MaxUsdHierarchyHandler::Ptr create();

    // UsdHierarchyHandler overrides
    Ufe::Hierarchy::Ptr hierarchy(const Ufe::SceneItem::Ptr& item) const override;
};

} // namespace ufe
} // namespace MAXUSD_NS_DEF