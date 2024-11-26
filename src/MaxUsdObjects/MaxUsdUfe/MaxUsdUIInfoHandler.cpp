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
#include "MaxUsdUIInfoHandler.h"

#include <usdufe/ufe/UsdUIInfoHandler.h>
#include <usdufe/ufe/usdsceneitem.h>

#include <ufe/runtimemgr.h>
#include <ufe/sceneItem.h>

#include <MaxUsd.h>
#include <QtGui/qpalette.h>
#include <QtWidgets/QApplication>

namespace MAXUSD_NS_DEF {
namespace ufe {

MaxUsdUIInfoHandler::MaxUsdUIInfoHandler()
{
    const auto disabledColor
        = QApplication::palette().color(QPalette::Disabled, QPalette::WindowText);
    _invisibleColor = { disabledColor.redF(), disabledColor.greenF(), disabledColor.blueF() };
}

MaxUsdUIInfoHandler::Ptr MaxUsdUIInfoHandler::create()
{
    return std::make_shared<MaxUsdUIInfoHandler>();
}

Ufe::UIInfoHandler::Icon MaxUsdUIInfoHandler::treeViewIcon(const Ufe::SceneItem::Ptr& item) const
{
    auto icon = UsdUIInfoHandler::treeViewIcon(item);
    // The base implementation provides direct support many USD types.
    if (!icon.baseIcon.empty()) {
        return icon;
    }

    // If the base implementation couldn't figure out the icon, look at the ancestor types.
// clang-format off
    static std::set<std::string> supportedAncestorTypes{
        "BlendShape",
        "Camera",
        "Capsule",
        "Cone",
        "Cube",
        "Cylinder",
        "Def",
        "GeomSubset",
        "LightFilter",
        "LightPortal",
        "Material",
        "Mesh",
        "NurbsPatch",
        "PluginLight",
        "PointInstancer",
        "Points",
        "Scope",
        "Shader",
        "SkelAnimation",
        "Skeleton",
        "SkelRoot",
        "Sphere",
        "UsdGeomCurves",
        "UsdGeomXformable",
        "UsdLuxBoundableLightBase",
        "UsdLuxNonboundableLightBase",
        "UsdTyped",
        "Volume"
    };
// clang-format on

    const UsdUfe::UsdSceneItem::Ptr usdItem = std::dynamic_pointer_cast<UsdUfe::UsdSceneItem>(item);
    if (!usdItem) {
        return icon;
    }

    // Iterate through the ancestor node types (which according to ufe docs are
    // in order from closest ancestor to farthest, including this item as first
    // node type) looking for an icon.
    static const std::string baseIconName
        = "out_" + Ufe::RunTimeMgr::instance().getName(usdItem->runTimeId()) + "_";
    const auto ancestorNodeTypes = usdItem->ancestorNodeTypes();
    // At index 0 is the prim's own type.
    for (int i = 1; i < ancestorNodeTypes.size(); ++i) {
        const auto& ty = ancestorNodeTypes[i];
        if (supportedAncestorTypes.find(ty) == supportedAncestorTypes.end()) {
            continue;
        }
        icon.baseIcon = baseIconName + ty;
        return icon;
    }
    return icon;
}

std::string MaxUsdUIInfoHandler::treeViewTooltip(const Ufe::SceneItem::Ptr& item) const
{
    const UsdUfe::UsdSceneItem::Ptr usdItem = std::dynamic_pointer_cast<UsdUfe::UsdSceneItem>(item);
    if (!usdItem) {
        return {};
    }
    const auto& prim = usdItem->prim();
    if (!prim.IsValid()) {
        return {};
    }
    std::string tooltip = "<p><strong>Path: </strong>" + prim.GetPath().GetString() + "</p>";
    tooltip.append("<strong>Type: </strong> " + item->nodeType());
    auto baseToolTip = UsdUIInfoHandler::treeViewTooltip(item);
    if (!baseToolTip.empty()) {
        tooltip.append("<br>" + UsdUIInfoHandler::treeViewTooltip(item));
    }
    return tooltip;
}

} // namespace ufe
} // namespace MAXUSD_NS_DEF