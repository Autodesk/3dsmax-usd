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

#include <usdUfe/base/api.h>
#include <usdUfe/ufe/UsdSceneItem.h>
#include <usdUfe/ufe/UsdUIInfoHandler.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

//! \brief Overrides some UsdUiInfoHandler functions, for Max specific requirements.
class MaxUsdUIInfoHandler : public UsdUfe::UsdUIInfoHandler
{
public:
    typedef std::shared_ptr<MaxUsdUIInfoHandler> Ptr;

    MaxUsdUIInfoHandler();
    ~MaxUsdUIInfoHandler() override = default;

    // Delete the copy/move constructors assignment operators.
    MaxUsdUIInfoHandler(const MaxUsdUIInfoHandler&) = delete;
    MaxUsdUIInfoHandler& operator=(const MaxUsdUIInfoHandler&) = delete;
    MaxUsdUIInfoHandler(MaxUsdUIInfoHandler&&) = delete;
    MaxUsdUIInfoHandler& operator=(MaxUsdUIInfoHandler&&) = delete;

    //! Create a UsdUIInfoHandler.
    static MaxUsdUIInfoHandler::Ptr create();

    Ufe::UIInfoHandler::Icon treeViewIcon(const Ufe::SceneItem::Ptr& item) const override;
    std::string              treeViewTooltip(const Ufe::SceneItem::Ptr& item) const override;
};

} // namespace ufe
} // namespace MAXUSD_NS_DEF
