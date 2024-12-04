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
#pragma once

#include "TranslationUtils.h"

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd.h>
#include <interval.h>

namespace MAXUSD_NS_DEF {

/**
 * \brief Updates a USD source from a param block expected to contain a reference to a USD stage node,
 * and a prim path. For example, a USDXformableController pulls its information from a USD
 * Xformable, or a USD Camera objects from a UsdGeomCamera prim.
 * \tparam T The schema over the prim at the given path.
 * \param stageNode The stageNode that will be updated.
 * \param source The source that will be updated (a USD schema over a USD prim)
 * \param paramBlock The param block holding the information.
 * \param stageParamId Parameter ID for the USD Stage node.
 * \param primPathParamId Parameter ID for the USD Prim's path.
 * \return True if something was updated, false otherwise.
 */
template <class T>
bool UpdateUsdSource(
    INode*&        stageNode,
    T&             source,
    IParamBlock2*  paramBlock,
    const ParamID& stageParamId,
    const ParamID& primPathParamId)
{
    const auto prevNode = stageNode;
    const auto prevPrim = source.GetPrim();
    auto       prevPath = prevPrim.IsValid() ? prevPrim.GetPath() : pxr::SdfPath::EmptyPath();

    auto hasChanged = [&stageNode, &source, prevNode, prevPath]() {
        const auto currNode = stageNode;
        const auto currPrim = source.GetPrim();
        auto       currPath = currPrim.IsValid() ? currPrim.GetPath() : pxr::SdfPath::EmptyPath();
        return currNode != prevNode || currPath != prevPath;
    };

    stageNode = nullptr;
    source = T {};

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

    const MCHAR* primPathStr = nullptr;
    valid = FOREVER;
    paramBlock->GetValue(primPathParamId, GetCOREInterface()->GetTime(), primPathStr, valid);

    const auto pathStdStr = MaxUsd::MaxStringToUsdString(primPathStr);
    if (pathStdStr.empty()) {
        return hasChanged();
    }

    auto       path = pxr::SdfPath { pathStdStr };
    const auto prim = stage->GetPrimAtPath(path);
    if (!prim.IsValid() || !prim.IsA<T>()) {
        return hasChanged();
    }
    source = T(prim);

    return hasChanged();
}

/**
 * \brief Updates a USD source attribute from a param block expected to contain a reference to a USD stage node,
 * and an attribute path.
 * \param stageNode The stageNode that will be updated.
 * \param source The source that will be updated (a USD attibute on a USD prim)
 * \param paramBlock The param block holding the information.
 * \param stageParamId Parameter ID for the USD Stage node.
 * \param attrPathParamId Parameter ID for the USD Prim's path.
 * \return True if something was updated, false otherwise.
 */
MaxUSDAPI bool UpdateUsdSourceAttr(
    INode*&            stageNode,
    pxr::UsdAttribute& sourceAttr,
    IParamBlock2*      paramBlock,
    const ParamID&     stageParamId,
    const ParamID&     attrPathParamId);

/**
 * Gets a USD attribute value from a USD Stage
 * @param stageNode The node holding the stage object.
 * @param attribute The USD attribute to get the value from.
 * @param time The time at which to get the value. The TimeValue is converted to a UsdTimeCode,
 * taking into account any animation parameters configured on the stage object.
 * @return The attribute's value.
 */
MaxUSDAPI pxr::VtValue
          GetAttrValue(INode* stageNode, const pxr::UsdAttribute& attribute, const TimeValue& time);

} // namespace MAXUSD_NS_DEF