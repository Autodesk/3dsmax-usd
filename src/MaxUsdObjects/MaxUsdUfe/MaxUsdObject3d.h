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

#include <usdUfe/ufe/UsdObject3d.h>
#include <usdUfe/ufe/UsdSceneItem.h>

#include <ufe/object3d.h>

#include <MaxUsd.h>

namespace MAXUSD_NS_DEF {
namespace ufe {

/**
 * \brief USD runtime 3D object interface
/* This class implements the Object3d interface for USD prims.
*/

class MaxUsdObject3d : public UsdUfe::UsdObject3d
{
public:
    using Ptr = std::shared_ptr<MaxUsdObject3d>;

    using UsdObject3d::UsdObject3d;
    ~MaxUsdObject3d() override = default;

    // Delete the copy/move constructors assignment operators.
    MaxUsdObject3d(const UsdObject3d&) = delete;
    MaxUsdObject3d& operator=(const MaxUsdObject3d&) = delete;
    MaxUsdObject3d(MaxUsdObject3d&&) = delete;
    MaxUsdObject3d& operator=(MaxUsdObject3d&&) = delete;

    //! Create a MaxUsdObject3d.
    static MaxUsdObject3d::Ptr create(const UsdUfe::UsdSceneItem::Ptr& item);

    //! Base behavior is to use "make visible/make invisible", we instead
    // just want to set the attribute.
    void                      setVisibility(bool vis) override;
    Ufe::UndoableCommand::Ptr setVisibleCmd(bool vis) override;
    Ufe::UndoableCommand::Ptr makeVisibleCmd(bool vis);

}; // UsdObject3d

} // namespace ufe
} // namespace MAXUSD_NS_DEF