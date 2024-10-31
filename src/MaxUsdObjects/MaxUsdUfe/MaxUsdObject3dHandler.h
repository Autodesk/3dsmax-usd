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
#include <usdUfe/ufe/UsdObject3dHandler.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

//! \brief Overrides some UsdObject3dHandler functions, for Max specific requirements.
class MaxUsdObject3dHandler : public UsdUfe::UsdObject3dHandler
{
public:
    typedef std::shared_ptr<MaxUsdObject3dHandler> Ptr;

    MaxUsdObject3dHandler() = default;
    ~MaxUsdObject3dHandler() override = default;

    // Delete the copy/move constructors assignment operators.
    MaxUsdObject3dHandler(const MaxUsdObject3dHandler&) = delete;
    MaxUsdObject3dHandler& operator=(const MaxUsdObject3dHandler&) = delete;
    MaxUsdObject3dHandler(MaxUsdObject3dHandler&&) = delete;
    MaxUsdObject3dHandler& operator=(MaxUsdObject3dHandler&&) = delete;

    //! Create a UsdUIInfoHandler.
    static MaxUsdObject3dHandler::Ptr create();

    // UsdObject3dHandler overrides
    Ufe::Object3d::Ptr object3d(const Ufe::SceneItem::Ptr& item) const override;
};

} // namespace ufe
} // namespace MAXUSD_NS_DEF
