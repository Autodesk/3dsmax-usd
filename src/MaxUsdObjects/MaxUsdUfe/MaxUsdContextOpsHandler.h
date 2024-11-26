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

#include <usdUfe/ufe/UsdContextOpsHandler.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

//! \brief Interface to create a MaxUsdContextOpsHandler interface object.
class MaxUsdContextOpsHandler : public UsdUfe::UsdContextOpsHandler
{
public:
    typedef std::shared_ptr<MaxUsdContextOpsHandler> Ptr;

    MaxUsdContextOpsHandler();
    ~MaxUsdContextOpsHandler() override;

    // Delete the copy/move constructors assignment operators.
    MaxUsdContextOpsHandler(const MaxUsdContextOpsHandler&) = delete;
    MaxUsdContextOpsHandler& operator=(const MaxUsdContextOpsHandler&) = delete;
    MaxUsdContextOpsHandler(MaxUsdContextOpsHandler&&) = delete;
    MaxUsdContextOpsHandler& operator=(MaxUsdContextOpsHandler&&) = delete;

    //! Create a MaxUsdContextOpsHandler.
    static MaxUsdContextOpsHandler::Ptr create();

    // UsdUfe::UsdContextOpsHandler overrides
    Ufe::ContextOps::Ptr contextOps(const Ufe::SceneItem::Ptr& item) const override;
}; // MaxUsdContextOpsHandler

} // namespace ufe
} // namespace MAXUSD_NS_DEF
