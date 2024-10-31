//
// Copyright 2024 Autodesk
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

#include "USDLinkUtils.h"

#include "TranslationUtils.h"

namespace MAXUSD_NS_DEF {

bool UpdateUsdSourceAttr(
    INode*&            stageNode,
    pxr::UsdAttribute& sourceAttr,
    IParamBlock2*      paramBlock,
    const ParamID&     stageParamId,
    const ParamID&     attrPathParamId)
{
    const auto prevNode = stageNode;
    const auto prevAttr = sourceAttr;
    auto       prevPath = prevAttr.IsValid() ? prevAttr.GetPath() : pxr::SdfPath::EmptyPath();

    auto hasChanged = [&stageNode, &sourceAttr, prevNode, prevPath]() {
        const auto currNode = stageNode;
        const auto currAttr = sourceAttr;
        auto       currPath = currAttr.IsValid() ? currAttr.GetPath() : pxr::SdfPath::EmptyPath();
        return currNode != prevNode || currPath != prevPath;
    };

    stageNode = nullptr;
    sourceAttr = pxr::UsdAttribute {};

    INode*   node = nullptr;
    Interval valid = FOREVER;
    paramBlock->GetValue(stageParamId, GetCOREInterface()->GetTime(), node, valid);

    if (!node) {
        return hasChanged();
    }

    const auto stageObject = dynamic_cast<USDStageObject*>(node->GetObjectRef());
    if (!stageObject) {
        return hasChanged();
    }

    stageNode = node;

    const auto stage = stageObject->GetUSDStage();
    if (!stage) {
        return hasChanged();
    }

    const MCHAR* pathStr = nullptr;
    valid = FOREVER;
    paramBlock->GetValue(attrPathParamId, GetCOREInterface()->GetTime(), pathStr, valid);

    const auto pathStdStr = MaxUsd::MaxStringToUsdString(pathStr);
    if (pathStdStr.empty()) {
        return hasChanged();
    }

    auto path = pxr::SdfPath { pathStdStr };

    const auto attr = stage->GetAttributeAtPath(path);
    if (!attr.IsValid()) {
        return hasChanged();
    }
    sourceAttr = attr;

    return hasChanged();
}

pxr::VtValue
GetAttrValue(INode* stageNode, const pxr::UsdAttribute& attribute, const TimeValue& time)
{
    if (!stageNode || !attribute.IsValid()) {
        return {};
    }
    const auto stageObject = dynamic_cast<USDStageObject*>(stageNode->GetObjectRef());
    if (!stageObject) {
        return {};
    }

    auto  pb = stageObject->GetParamBlock(0);
    float timeCodeParam;
    pb->GetValue(RenderUsdTimeCode, time, timeCodeParam);
    const auto   timeCode = timeCodeParam;
    pxr::VtValue value;
    attribute.Get(&value, timeCode);
    return value;
}

} // namespace MAXUSD_NS_DEF