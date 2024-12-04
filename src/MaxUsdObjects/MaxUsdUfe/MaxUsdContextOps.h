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

#include <usdUfe/ufe/UsdContextOps.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

class MaxUsdContextOps : public UsdUfe::UsdContextOps
{
public:
    typedef std::shared_ptr<MaxUsdContextOps> Ptr;

    MaxUsdContextOps(const UsdUfe::UsdSceneItem::Ptr& item);
    ~MaxUsdContextOps() override;

    // Delete the copy/move constructors assignment operators.
    MaxUsdContextOps(const MaxUsdContextOps&) = delete;
    MaxUsdContextOps& operator=(const MaxUsdContextOps&) = delete;
    MaxUsdContextOps(MaxUsdContextOps&&) = delete;
    MaxUsdContextOps& operator=(MaxUsdContextOps&&) = delete;

    //! Create a MaxUsdContextOps.
    static MaxUsdContextOps::Ptr create(const UsdUfe::UsdSceneItem::Ptr& item);

    Items                     getItems(const ItemPath& itemPath) const override;
    Ufe::UndoableCommand::Ptr doOpCmd(const ItemPath& itemPath) override;

}; // MaxUsdContextOps

} // namespace ufe
} // namespace MAXUSD_NS_DEF
