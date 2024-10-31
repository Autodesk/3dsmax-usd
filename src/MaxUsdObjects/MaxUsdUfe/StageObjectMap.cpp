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
#include "StageObjectMap.h"

#include "UfeUtils.h"

StageObjectMap StageObjectMap::instance;

void StageObjectMap::Set(USDStageObject* object)
{
    const auto path = MaxUsd::ufe::getUsdStageObjectPath(object);
    pathToObject[path] = object;
    stageToObject[object->GetUSDStage()] = object;
}

void StageObjectMap::Remove(USDStageObject* object)
{
    pathToObject.erase(MaxUsd::ufe::getUsdStageObjectPath(object));
    stageToObject.erase(object->GetUSDStage());
}

USDStageObject* StageObjectMap::Get(const Ufe::Path& path)
{
    const auto it = pathToObject.find(path);
    if (it == pathToObject.end()) {
        return nullptr;
    }
    return it->second;
}

USDStageObject* StageObjectMap::Get(const pxr::UsdStageWeakPtr& stage)
{
    const auto it = stageToObject.find(stage);
    if (it == stageToObject.end()) {
        return nullptr;
    }
    return it->second;
}

std::vector<USDStageObject*> StageObjectMap::GetAllStageObjects() const
{
    std::vector<USDStageObject*> stageObjects;
    for (auto& entry : stageToObject) {
        stageObjects.push_back(entry.second);
    }
    return stageObjects;
}