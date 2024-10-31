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

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <ufe/path.h>

//! Global registry mapping USDStage objects to their path in UFE, their stage, and back.
class StageObjectMap
{
public:
    /**
     * \brief Sets a new object in the map.
     * \param object the USD Stage object to add.
     */
    void Set(USDStageObject* object);

    /**
     * \brief Remove a USD Stage object from the map.
     * \param object The USD Stage object to remove.
     */
    void Remove(USDStageObject* object);

    /**
     * \brief Returns the USD Stage object at the given UFE path.
     * \param path The UFE path of the object to get.
     * \return The USDStageObject.
     */
    USDStageObject* Get(const Ufe::Path& path);

    /**
     * \brief Returns a USD Stage object from the stage pointer.
     * \param stage The stage to get the object from.
     * \return The USDStageObject.
     */
    USDStageObject* Get(const pxr::UsdStageWeakPtr& stage);

    /**
     * Returns all USDStageObjects currently held.
     * @return All stage objects.
     */
    std::vector<USDStageObject*> GetAllStageObjects() const;

    /**
     * \brief Get the global instance of the map.
     * \return The instance.
     */
    static StageObjectMap* GetInstance() { return &instance; }

private:
    //! Global instance
    static StageObjectMap instance;

    std::unordered_map<Ufe::Path, USDStageObject*>                     pathToObject;
    pxr::TfHashMap<pxr::UsdStageWeakPtr, USDStageObject*, pxr::TfHash> stageToObject;
};
