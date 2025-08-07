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

#include <MaxUsd/Utilities/MaxSupportUtils.h>

// Modern menu system available in 2025+
#ifdef IS_MAX2025_OR_GREATER

#include "MaxUsd/Builders/USDSceneBuilderOptions.h"
#include "MaxUsd/ExportToStageCommand.h"
#include "MaxUsd/USDIOController.h"
#include "MaxUsd/Utilities/VtDictionaryUtils.h"
#include "MaxUsd/Views/USDExportDialog.h"
#include "Objects/USDStageObject.h"

#include <maxscript/maxscript.h>

#include <CUI/ICuiDynamicMenu.h>
#include <CUI/ICuiMenu.h>
#include <CUI/ICuiQuadMenu.h>
#include <CUI/ICuiQuadMenuManager.h>
#include <GetCOREInterface.h>
#include <actiontable.h>
#include <maxapi.h>

inline constexpr ActionTableId usdActionTableId = 0xa726bacd;
inline constexpr int           usdActionItemId = 49237;

class USDDynamicActionItem : public ActionItem
{
    // ActionItem overrides
    int  GetId() override { return usdActionItemId; }
    BOOL ExecuteAction() override { return true; }
    void GetButtonText(MSTR& buttonText) override { buttonText = _T("Duplicate as USD Data"); }
    void GetMenuText(MSTR& menuText) override { GetButtonText(menuText); }
    void GetDescriptionText(MSTR& descText) override { GetButtonText(descText); }
    void GetCategoryText(MSTR& catText) override { catText = _T("USD"); }
    BOOL IsChecked() override { return false; }
    BOOL IsItemVisible() override { return true; }
    BOOL IsEnabled() override { return true; }
    void DeleteThis() override { delete this; }
    BOOL IsDynamicMenu() override { return true; }
    void PopulateDynamicMenu(
        MaxSDK::CUI::ICuiDynamicMenu* dynMenu,
        HWND                          hWnd,
        const IPoint2&                cursorPos) override;
    void DynamicMenuItemSelected(int itemId) override;
};

namespace {

// Menu entry ids in the dynamic menu.
// The menu ids for the USD Stages we might export to, are their handles in the scene.
// To make sure we do not conflict with them, use negative values for other entries.
enum
{
    OptionsId = -1,
    NewStageId = -2
};

} // namespace

void USDDynamicActionItem::PopulateDynamicMenu(
    MaxSDK::CUI::ICuiDynamicMenu* dynMenu,
    HWND                          hWnd,
    const IPoint2&                cursorPos)
{
    if (GetCOREInterface()->GetSelNodeCount() < 1) {
        return;
    }

    // Find all USDStageObject nodes.
    std::vector<INode*>         allStages;
    std::function<void(INode*)> traverse = [&](INode* node) {
        if (!node) {
            return;
        }
        auto usdStage = dynamic_cast<USDStageObject*>(node->GetObjectRef());
        if (usdStage && usdStage->GetUSDStage()) {
            allStages.push_back(node);
        }
        for (int i = 0; i < node->NumberOfChildren(); ++i) {
            INode* childNode = node->GetChildNode(i);
            traverse(childNode); // Recursive call for the child node
        }
    };
    traverse(GetCOREInterface()->GetRootNode());

    for (const auto& stageNode : allStages) {
        dynMenu->AddItem(stageNode->GetHandle(), stageNode->GetName());
    }

    dynMenu->AddSeparator();
    dynMenu->AddItem(NewStageId, QObject::tr("Stage from File"));
    dynMenu->AddSeparator();
    dynMenu->AddItem(OptionsId, QObject::tr("Options..."));
}

void USDDynamicActionItem::DynamicMenuItemSelected(int itemId)
{
    if (itemId == OptionsId) {
        MaxUsd::GetUSDIOController()->ConfigureExportToStageOptions();
        return;
    }

    INode* stageNode = nullptr;

    if (itemId == NewStageId) {
        // Store the currently selected nodes.
        std::vector<INode*> selectedNodes;
        int                 selCount = GetCOREInterface()->GetSelNodeCount();
        for (int i = 0; i < selCount; ++i) {
            selectedNodes.push_back(GetCOREInterface()->GetSelNode(i));
        }

        GetCOREInterface()->ClearNodeSelection();
        ExecuteMAXScriptScript(
            L"macros.run \"USD\" \"CreateUSDStage\"", MAXScript::ScriptSource::Embedded);
        if (GetCOREInterface()->GetSelNodeCount() != 1) {
            // Create stage operation was aborted.
            return;
        }
        stageNode = GetCOREInterface()->GetSelNode(0);

        // Restore the previous selection
        GetCOREInterface()->ClearNodeSelection();
        for (const auto& node : selectedNodes) {
            GetCOREInterface()->SelectNode(node, false);
        }
    } else {
        // A stage was picked from the menu...
        stageNode = GetCOREInterface()->GetINodeByHandle(itemId);
    }

    const auto usdStageObject = dynamic_cast<USDStageObject*>(stageNode->GetObjectRef());

    if (!usdStageObject) {
        DbgAssert("Incorrect stage node object type.");
        return;
    }

    const auto io = MaxUsd::GetUSDIOController();

    MaxUsd::USDSceneBuilderOptions opts
        = io->GetExportUIOptions(MaxUsd::USDSceneBuilderOptions::Type::ToStage);

    opts.SetContentSource(MaxUsd::USDSceneBuilderOptions::ContentSource::Selection);

    bool    allowOverwrite = false;
    Matrix3 rootTransform;
    io->GetUIExportToStageExtraOptions(stageNode, allowOverwrite, rootTransform);

    auto cmd = MaxUsd::ExportToStageCommand::create(
        opts, usdStageObject->GetUSDStage(), allowOverwrite, rootTransform);
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
}

void RegisterUSDDynamicActionItem()
{
    auto* usdActionTable
        = new ActionTable(usdActionTableId, kActionMainUIContext, _T("USD Action Table"));
    usdActionTable->AppendOperation(new USDDynamicActionItem {});
    // Register and activate new action table
    IActionManager* actionMgr = GetCOREInterface()->GetActionManager();
    actionMgr->RegisterActionTable(usdActionTable);
    static ActionCallback usdActionCallback;
    actionMgr->ActivateActionTable(&usdActionCallback, usdActionTableId);
}

void InsertUsdMenuItems(MaxSDK::CUI::ICuiMenu* menu)
{
    menu->CreateSeparator(MaxSDK::MaxGuid::CreateMaxGuid());
    menu->CreateAction(MaxSDK::MaxGuid::CreateMaxGuid(), usdActionTableId, usdActionItemId);
}

void USDQuadMenuRegisterCallback(void* param, NotifyInfo* info)
{
    using namespace MaxSDK::CUI;
    ICuiQuadMenuManager* menuMgr = GetNotifyParam<NOTIFY_CUI_REGISTER_QUAD_MENUS>(info);
    if (!menuMgr) {
        return;
    }

    // Viewport
    ICuiQuadMenuContext* viewportContext = menuMgr->GetContextById(kViewportQuadContextId);
    auto                 vpQuad = viewportContext->GetRightClickMenuByModifiers(kNoModifier);
    auto                 vpQuadMenus = vpQuad->GetMenus();
    InsertUsdMenuItems(vpQuadMenus[2]);

    // Explorer hierarchy view
    auto explorerQuad
        = menuMgr->GetQuadMenuById(MaxSDK::MaxGuid(L"75091e5c-cf71-4fba-9472-b26ad686a050"));
    auto explorerQuadMenus = explorerQuad->GetMenus();
    InsertUsdMenuItems(explorerQuadMenus[2]);

    // Explorer layer view
    auto layerQuad
        = menuMgr->GetQuadMenuById(MaxSDK::MaxGuid(L"2097f00d-4042-482d-8ff6-9df377c26591"));
    auto layerQuadMenus = layerQuad->GetMenus();
    InsertUsdMenuItems(layerQuadMenus[2]);
}

#endif