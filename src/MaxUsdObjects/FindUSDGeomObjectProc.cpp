//
// Copyright 2025 Autodesk
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

#include "FindUSDGeomObjectsProc.h"

FindUSDGeomObjectsProc::FindUSDGeomObjectsProc(INode* stageNode) { this->stageNode = stageNode; }

int FindUSDGeomObjectsProc::proc(ReferenceMaker* rmaker)
{
    if (rmaker == stageNode) {
        return DEP_ENUM_CONTINUE;
    }

    const auto depNode = dynamic_cast<INode*>(rmaker);
    if (!depNode) {
        return DEP_ENUM_CONTINUE;
    }

    if (const auto geomObject = dynamic_cast<USDGeomObject*>(depNode->GetObjectRef())) {
        usdGeomObjectNodes.push_back({ depNode, geomObject });
    }

    return DEP_ENUM_CONTINUE;
}