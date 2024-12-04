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
#include "ClassDescs.h"

#include "Objects/USDAttrControllerClassDescs.h"
#include "Objects/USDSnaps.h"
#include "Objects/USDStageObjectclassDesc.h"
#include "Objects/USDTransformControllersClassDesc.h"
#include "Objects/UsdCameraObjectClassDesc.h"

#include <utility>

std::vector<ClassDesc2*> allClasses;

namespace {

std::once_flag f;
void           CreateClassList()
{
    std::vector<ClassDesc2*> classDescs;
    classDescs.push_back(GetUSDStageObjectClassDesc());
    classDescs.push_back(GetUSDCameraObjectClassDesc());
    classDescs.push_back(GetUSDXformableControllerClassDesc());
    classDescs.push_back(GetUSDPositionControllerClassDesc());
    classDescs.push_back(GetUSDScaleControllerClassDesc());
    classDescs.push_back(GetUSDRotationControllerClassDesc());
    classDescs.push_back(GetUSDFloatControllerClassDesc());
    classDescs.push_back(GetUSDPoint3ControllerClassDesc());
    classDescs.push_back(GetUSDPoint4ControllerClassDesc());
    classDescs.push_back(GetUSDSnapsClassDesc());
    allClasses = std::move(classDescs);
}

} // namespace

int GetNumClassDesc()
{
    std::call_once(f, CreateClassList);
    return int(allClasses.size());
}

ClassDesc2* GetClassDesc(int i)
{
    std::call_once(f, CreateClassList);
    return allClasses[i];
}
