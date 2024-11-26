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

#include <MaxUsdObjects/MaxUsdObjectsAPI.h>

#include <ufe/path.h>
#include <ufe/sceneItem.h>

#include <MaxUsd.h>

class USDStageObject;

namespace MAXUSD_NS_DEF {
namespace ufe {

//! Initializes the usdUfe library (setup some function pointers and global objects).
void initialize();

//! Finalize the usdUfe library (releases some resources).
void finalize();

//! Get USD stage corresponding to argument UFE path.
MaxUSDObjectsAPI pxr::UsdStageWeakPtr getStage(const Ufe::Path& path);

//! Get the full UFE path for a stage (with two segments, a 3dsMax segment a and USD segment).
MaxUSDObjectsAPI Ufe::Path getStagePath(pxr::UsdStageWeakPtr stage);

//! Return the USD prim corresponding to the argument UFE path.
MaxUSDObjectsAPI pxr::UsdPrim ufePathToPrim(const Ufe::Path& path);

//! Get the UFE path from a USD stage object.
MaxUSDObjectsAPI Ufe::Path getUsdStageObjectPath(const USDStageObject* object);

//! Get the UFE path associated with a USD prim or point instance in a given USDStageObject.
MaxUSDObjectsAPI Ufe::Path
getUsdPrimUfePath(USDStageObject* object, const pxr::SdfPath& primPath, int instanceIdx = -1);

//! Returns true if the path points to a point instance
MaxUSDObjectsAPI bool isPointInstance(const Ufe::SceneItem::Ptr& path);

//! Get the time for a UFE Path, this is a requirement from usdUfe. For now it will
//! just return the current time in UsdTime codes.
MaxUSDObjectsAPI pxr::UsdTimeCode getTime(const Ufe::Path&);

//! Save the load rules so that switching the stage settings will
//! be able to preserve the load rules. Called when loading/unloading a payload.
MaxUSDObjectsAPI void saveStageLoadRules(const PXR_NS::UsdStageRefPtr&);

//! Max specific implementation for UsdUfe::DCCFunctions::isRootChildFn. Returns true
//! if a single segment, which means we are at the stage object path / pseudo-root.
MaxUSDObjectsAPI bool isRootChild(const Ufe::Path&);

} // namespace ufe
} // namespace MAXUSD_NS_DEF
