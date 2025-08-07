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
#include "MaxUsdContextOps.h"

#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>

#include <MaxUsd.h>
#include <maxusd/Builders/USDSceneBuilderOptions.h>

#include <usdUfe/ufe/Global.h>
#include <usdUfe/ufe/UsdSceneItem.h>

#include <ufe/globalSelection.h>
#include <ufe/observableSelection.h>
#include <ufe/undoableCommand.h>

#include <vector>

// Using QT to access the clipboard.
#include "MaxUsd/Builders/USDSceneBuilder.h"
#include "MaxUsd/ExportToStageCommand.h"
#include "MaxUsd/USDIOController.h"
#include "MaxUsdObject3d.h"
#include "StageObjectMap.h"

#include <maxscript/maxscript.h>
#include <maxscript/protocols/primitives.inl>

#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>

namespace MAXUSD_NS_DEF {
namespace ufe {

static constexpr char USDToggleVisibilityItem[] = "Toggle Visibility";
static constexpr char USDCopyPrimPathItem[] = "Copy Prim Path";
static constexpr char USDCopyPrimPathLabel[] = "Copy Prim Path";
static constexpr char USDSetAsDefaultPrim[] = "Set as Default Prim";
static constexpr char USDClearDefaultPrim[] = "Clear Default Prim";

static constexpr char AddNewPrimItem[] = "Add New Prim";
static constexpr char AddPrimFromSelectionItem[] = "From 3ds Max Selection";
static constexpr char AddPrimFromSelectionLabel[] = "From 3ds Max Selection";
static constexpr char AddPrimFromListItem[] = "From 3ds Max Nodes";
static constexpr char AddPrimFromListLabel[] = "From 3ds Max Nodes...";
static constexpr char AddPrimFrom3dsMaxOptionsItem[] = "Add Prim From Max Options";
static constexpr char AddPrimFrom3dsMaxOptionsLabel[] = "Options...";

static constexpr char PromoteTo3dsMaxObjectItem[] = "Promote to 3ds Max Object";
static constexpr char PromoteTo3dsMaxObjectLabel[] = "Promote to 3ds Max Object";

MaxUsdContextOps::MaxUsdContextOps(const UsdUfe::UsdSceneItem::Ptr& item)
    : UsdUfe::UsdContextOps(item)
{
    // Adjust bulk items for 3dsMax. Only support bulk editing on the same stage.
    for (const auto& bulkItem : _bulkItems) {
        if (bulkItem->path().popSegment() == item->path().popSegment()) {
            continue;
        }
        _bulkItems.remove(bulkItem);
    }
    // Clear bulk items if we under up with just one, not a bulk edit anymore.
    if (_bulkItems.size() == 1) {
        _bulkItems.clear();
        _bulkType.clear();
    }
}

MaxUsdContextOps::~MaxUsdContextOps() { }

/*static*/
MaxUsdContextOps::Ptr MaxUsdContextOps::create(const UsdUfe::UsdSceneItem::Ptr& item)
{
    return std::make_shared<MaxUsdContextOps>(item);
}

Ufe::ContextOps::Items MaxUsdContextOps::getItems(const ItemPath& itemPath) const
{
    if (isBulkEdit()) {
        return getBulkItems(itemPath);
    }

    auto items = UsdContextOps::getItems(itemPath);

    // Temporarily remove the context ops related to the default prim. Indeed, setting
    // and clearing the default prim can only happen on the root layer, but we currently
    // always target the session layer.
    auto removeOp = [&items](const std::string& name) {
        const auto it
            = std::find_if(items.begin(), items.end(), [&name](const Ufe::ContextItem& ci) {
                  return ci.item == name;
              });
        if (it != items.end()) {
            items.erase(it);
        }
    };
    removeOp(USDSetAsDefaultPrim);
    removeOp(USDClearDefaultPrim);

    // only add copy prim path to the root menu context option
    if (itemPath.empty()) {
        // 3dsMax specific context ops :
        items.insert(items.begin(), { USDCopyPrimPathItem, USDCopyPrimPathLabel });
        items.insert(items.begin(), Ufe::ContextItem::kSeparator);
        if (prim().IsA<pxr::UsdGeomImageable>()) {
            items.insert(items.begin(), { PromoteTo3dsMaxObjectItem, PromoteTo3dsMaxObjectLabel });
        }
    }
    // Submenus (depth = 1)
    else if (itemPath.size() == 1) {
        // Look if we are in the "add prim" submenu.
        const auto item = itemPath[0];
        if (item == AddNewPrimItem) {
            items.insert(items.begin(), Ufe::ContextItem::kSeparator);
            items.insert(
                items.begin(), { AddPrimFrom3dsMaxOptionsItem, AddPrimFrom3dsMaxOptionsLabel });
            items.insert(items.begin(), { AddPrimFromListItem, AddPrimFromListLabel });
            items.insert(items.begin(), { AddPrimFromSelectionItem, AddPrimFromSelectionLabel });
        }
    }

    return items;
}

Ufe::UndoableCommand::Ptr MaxUsdContextOps::doOpCmd(const ItemPath& itemPath)
{
    if (itemPath[0] == USDCopyPrimPathItem) {
        // Adding the prim path to the clipboard is not an undoable command, just do it right away.
        QApplication::clipboard()->setText(QString::fromStdString(prim().GetPath().GetString()));
        return nullptr;
    }

    if (itemPath[0] == PromoteTo3dsMaxObjectItem) {
        const auto objectPath = getUsdStageObjectPath(_item->path());
        const auto usdStageObject = StageObjectMap::GetInstance()->Get(objectPath);
        if (usdStageObject) {
            usdStageObject->PromoteTo3dsMaxObject(prim().GetPath(), true /*auto select*/);
        }
        return nullptr;
    }

    // Override the base behavior for toggling of visibility.
    // We reimplemented Object3d::setVisibility() to only author the prim's attribute
    // instead of using make visible/make invisible. But we also created a new command
    // to make actually make visible, which we trigger from the contextOps. We call
    // it here.
    if (itemPath[0] == USDToggleVisibilityItem) {
        const auto object3d = MaxUsdObject3d::create(_item);
        if (!object3d) {
            return nullptr;
        }
        // Don't use UsdObject3d::visibility() - it looks at the authored visibility
        // attribute. Instead, compute the effective visibility, which is what we want
        // to toggle.
        const auto imageable = pxr::UsdGeomImageable(prim());
        const auto current = imageable.ComputeVisibility() != pxr::UsdGeomTokens->invisible;
        return object3d->makeVisibleCmd(!current);
    }

    // Submenus
    if (itemPath.size() > 1) {

        const auto io = GetUSDIOController();

        // Check if we hit of the "add prim from 3dsmax" sub menu actions.
        const bool fromSelection = itemPath[1] == AddPrimFromSelectionItem;
        const bool fromList = itemPath[1] == AddPrimFromListItem;
        if (fromSelection || fromList) {

            const auto objectPath = getUsdStageObjectPath(_item->path());
            const auto usdStageObject = StageObjectMap::GetInstance()->Get(objectPath);

            auto nodes = GetReferencingNodes(usdStageObject);
            if (nodes.size() == 0) {
                return nullptr;
            }

            USDSceneBuilderOptions opts
                = io->GetExportUIOptions(USDSceneBuilderOptions::Type::ToStage);

            if (fromSelection) {
                opts.SetContentSource(USDSceneBuilderOptions::ContentSource::Selection);
            } else {
                opts.SetContentSource(USDSceneBuilderOptions::ContentSource::NodeList);
                const auto handle = nodes[0]->GetHandle();

                // Call maxscript to pick the nodes to export. It has a neat handler with the rubber
                // band. May revisit this if we want to customize this behavior further.
                std::wstring mxsCmd(L"_stageNode = maxOps.getNodeByHandle ");
                mxsCmd.append(std::to_wstring(handle));
                mxsCmd.append(L";");
                mxsCmd.append(L"pickObject count:#multiple select:true rubberBand:_stageNode.pos");
                FPValue result;
                if (!ExecuteMAXScriptScript(
                        mxsCmd.c_str(), MAXScript::ScriptSource::Dynamic, false, &result)) {
                    return nullptr;
                }
                if (result.type != TYPE_INODE_TAB) {
                    return nullptr;
                }
                opts.SetNodesToExport(*result.n_tab);
            }

            opts.SetRootPrimPath(prim().GetPath());

            // Export to stage specific options.
            bool    allowOverwrite = false;
            Matrix3 rootTransform;
            io->GetUIExportToStageExtraOptions(nodes[0], allowOverwrite, rootTransform);

            return ExportToStageCommand::create(
                opts, usdStageObject->GetUSDStage(), allowOverwrite, rootTransform);
        }

        if (itemPath[1] == AddPrimFrom3dsMaxOptionsItem) {
            // Show the UI to configure the export options.
            io->ConfigureExportToStageOptions();
            // No associated command - not undoable.
            return nullptr;
        }
    }

    // Call into base implementation.
    if (auto cmd = UsdUfe::UsdContextOps::doOpCmd(itemPath)) {
        return cmd;
    }
    return nullptr;
}

} // namespace ufe
} // namespace MAXUSD_NS_DEF
