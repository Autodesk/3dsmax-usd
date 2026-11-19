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
#include <MaxUsd/Utilities/TranslationUtils.h>

// Modern menu system available in 2025+
#ifdef IS_MAX2025_OR_GREATER

#include "MaxUsd/Builders/USDSceneBuilderOptions.h"
#include "MaxUsd/ExportToStageCommand.h"
#include "MaxUsd/USDIOController.h"
#include "MaxUsd/Utilities/VtDictionaryUtils.h"
#include "MaxUsd/Views/USDExportDialog.h"
#include "Objects/USDStageObject.h"

#ifdef ADSK_COMPONENT_CREATOR_ENABLED
#include <MaxUsdObjects/ComponentCreator/Commands/CreateComponentFromNodesCmd.h>
#include <MaxUsdObjects/ComponentCreator/UtilComponentCreator.h>
#endif

#include <Qt/QmaxMainWindow.h>
#include <maxscript/maxscript.h>
#include <ufe/undoableCommandMgr.h>

#ifdef ADSK_COMPONENT_CREATOR_ENABLED
#include <MaxUsdObjects/ComponentCreator/ComponentCreateOptions.h>

#include <AdskUsdComponentCreator/Descriptions/ComponentDescription.h>
#include <AdskUsdComponentCreator/Options/Options.h>
#endif
#include <CUI/ICuiDynamicMenu.h>
#include <CUI/ICuiMenu.h>
#include <CUI/ICuiQuadMenu.h>
#include <CUI/ICuiQuadMenuManager.h>
#include <GetCOREInterface.h>
#include <QMessageBox>
#include <actiontable.h>
#include <maxapi.h>

inline constexpr ActionTableId usdActionTableId = 0xa726bacd;
inline constexpr int           usdActionItemId = 49237;
inline constexpr int           usdCreateComponentActionItemId = 49238;

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
// Item ids for the USD quad-menu dynamic submenus. All negative so they never collide with the
// positive stage-node handles the menus use as ids for scene stages; kept in one enum so the two
// submenus' ids stay distinct.
enum
{
    // "Create USD Component" submenu.
    CreateComponentId = -1,
    CreateWithVariantsId = -2,
    CreateComponentOptionsId = -3,
    // Stage-create submenu.
    OptionsId = -4,
    NewStageId = -5,
    NewStageInMemoryId = -6,
};
} // namespace

#ifdef ADSK_COMPONENT_CREATOR_ENABLED

// "Create USD Component" submenu: Create Component / Create Component with Variants / Options...
// The two create entries keep their existing enable logic (>=1 and >1 top-level nodes) and are
// listed only when applicable. Both create with the persisted options; Options... configures them.
class USDCreateComponentDynamicActionItem : public ActionItem
{
    int  GetId() override { return usdCreateComponentActionItemId; }
    BOOL ExecuteAction() override { return true; }
    void GetButtonText(MSTR& buttonText) override { buttonText = _T("Create USD Component"); }
    void GetMenuText(MSTR& menuText) override { GetButtonText(menuText); }
    void GetDescriptionText(MSTR& descText) override { GetButtonText(descText); }
    void GetCategoryText(MSTR& catText) override { catText = _T("USD"); }
    BOOL IsChecked() override { return false; }
    BOOL IsItemVisible() override { return true; }
    BOOL IsEnabled() override { return true; }
    void DeleteThis() override { delete this; }
    BOOL IsDynamicMenu() override { return true; }
    void PopulateDynamicMenu(MaxSDK::CUI::ICuiDynamicMenu* dynMenu, HWND, const IPoint2&) override
    {
        const size_t topLevel = MaxUsd::RemoveDescendantINodes(MaxUsd::GetSelectedNodes()).size();
        if (topLevel >= 1) {
            dynMenu->AddItem(CreateComponentId, QObject::tr("Create Component"));
        }
        if (topLevel > 1) {
            dynMenu->AddItem(CreateWithVariantsId, QObject::tr("Create Component with Variants"));
        }
        dynMenu->AddSeparator();
        dynMenu->AddItem(CreateComponentOptionsId, QObject::tr("Options..."));
    }
    void DynamicMenuItemSelected(int itemId) override
    {
        if (itemId == CreateComponentOptionsId) {
            MaxUsd::ComponentCreator::ConfigureCreateComponentOptions();
            return;
        }
        const auto persisted = MaxUsd::ComponentCreator::LoadPersistedCreateOptions();
        const auto options
            = MaxUsd::ComponentCreator::ResolveTemplateOptions(persisted.templateName);
        std::vector<INode*> nodes = MaxUsd::RemoveDescendantINodes(MaxUsd::GetSelectedNodes());
        if (itemId == CreateComponentId) {
            if (nodes.empty()) {
                return;
            }
            auto cmd = std::make_shared<CreateComponentFromNodesCmd>(
                std::move(nodes), options, persisted.exportOptions);
            Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
        } else if (itemId == CreateWithVariantsId) {
            if (nodes.size() < 2) {
                return;
            }
            MaxUsd::ComponentCreator::CreateMultiVariantsComponentFromNodes(
                nodes, options, persisted.exportOptions);
        }
    }
};
#endif

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
    dynMenu->AddItem(NewStageInMemoryId, QObject::tr("Stage with New Layer"));
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

    // Make the entire operation undoable in one go.
    theHold.Begin();

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
            theHold.Cancel();
            return;
        }
        stageNode = GetCOREInterface()->GetSelNode(0);

        // Restore the previous selection
        GetCOREInterface()->ClearNodeSelection();
        for (const auto& node : selectedNodes) {
            GetCOREInterface()->SelectNode(node, false);
        }
    } else if (itemId == NewStageInMemoryId) {

        const auto stageObject = static_cast<USDStageObject*>(
            GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, USDSTAGEOBJECT_CLASS_ID));
        stageNode = GetCOREInterface()->CreateObjectNode(stageObject);
        stageObject->OpenInUsdExplorer();
    } else {
        // A stage was picked from the menu...
        stageNode = GetCOREInterface()->GetINodeByHandle(itemId);
    }

    const auto usdStageObject = dynamic_cast<USDStageObject*>(stageNode->GetObjectRef());

    if (!usdStageObject) {
        DbgAssert("Incorrect stage node object type.");
        theHold.Cancel();
        return;
    }

    const auto io = MaxUsd::GetUSDIOController();

    MaxUsd::USDSceneBuilderOptions opts
        = io->GetExportUIOptions(MaxUsd::USDSceneBuilderOptions::Type::ToStage);

    bool    allowOverwrite = false;
    Matrix3 rootTransform;
    io->GetUIExportToStageExtraOptions(stageNode, allowOverwrite, rootTransform);

#ifdef ADSK_COMPONENT_CREATOR_ENABLED
    // When the chosen target is an Adsk USD Component, the generic export pipeline below
    // would dump geometry at the stage root and ignore the variant target. Instead delegate
    // to the component creator so the duplicated nodes land in the component's currently
    // targeted variant (/root/geo).
    if (MaxUsd::ComponentCreator::IsAdskUsdComponent(usdStageObject->GetUSDStage())) {
        const auto desc = AdskUsdComponentCreator::ComponentDescription::CreateFromStageMetadata(
            usdStageObject->GetUSDStage());
        const std::vector<INode*> nodes = MaxUsd::GetSelectedNodes();
        // allowOverwrite has no counterpart here: the duplicated nodes are added to the
        // targeted variant, where name conflicts are the creator's to resolve.
        if (MaxUsd::ComponentCreator::DuplicateINodesToComponent(
                desc, nodes, opts, rootTransform)) {
            theHold.Accept(L"Duplicate to Component");
        } else {
            // The most common reason for failure is that no variant is targeted for edition;
            // surface that rather than silently doing nothing.
            theHold.Cancel();
            QMessageBox::warning(
                GetCOREInterface()->GetQmaxMainWindow(),
                QObject::tr("Duplicate as USD Data"),
                QObject::tr("Could not duplicate to the component. Target a variant for editing "
                            "in the Variant Manager first."));
        }
        return;
    }
#endif

    opts.SetContentSource(MaxUsd::USDSceneBuilderOptions::ContentSource::Selection);

    auto cmd = MaxUsd::ExportToStageCommand::create(
        opts, usdStageObject->GetUSDStage(), allowOverwrite, rootTransform);
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);

    theHold.Accept(L"Duplicate to Stage");
}

void RegisterUSDDynamicActionItem()
{
    auto* usdActionTable
        = new ActionTable(usdActionTableId, kActionMainUIContext, _T("USD Action Table"));
    usdActionTable->AppendOperation(new USDDynamicActionItem {});
#ifdef ADSK_COMPONENT_CREATOR_ENABLED
    usdActionTable->AppendOperation(new USDCreateComponentDynamicActionItem {});
#endif
    // Register and activate new action table
    IActionManager* actionMgr = GetCOREInterface()->GetActionManager();
    actionMgr->RegisterActionTable(usdActionTable);
    static ActionCallback usdActionCallback;
    actionMgr->ActivateActionTable(&usdActionCallback, usdActionTableId);
}

namespace {

// Append the USD "Duplicate as USD Data" and "Create USD Component" actions (preceded by a
// separator) to the given quad-menu sub-menu. Each call site supplies its own unique GUIDs.
void AddUsdQuadMenuActions(
    MaxSDK::CUI::ICuiMenu* menu,
    const MaxSDK::MaxGuid& separatorGuid,
    const MaxSDK::MaxGuid& duplicateActionGuid,
    const MaxSDK::MaxGuid& createComponentGuid)
{
    menu->CreateSeparator(separatorGuid);
    menu->CreateAction(duplicateActionGuid, usdActionTableId, usdActionItemId);
#ifdef ADSK_COMPONENT_CREATOR_ENABLED
    // A single dynamic-submenu action ("Create USD Component") holding the create entries +
    // Options.
    menu->CreateAction(createComponentGuid, usdActionTableId, usdCreateComponentActionItemId);
#endif
}

} // namespace

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
    AddUsdQuadMenuActions(
        vpQuad->GetMenus()[2],
        MaxSDK::MaxGuid(L"87390108-800a-491d-b654-b0e94ac275f4"),
        MaxSDK::MaxGuid(L"2ef90997-e430-4267-bae1-26651e2def6b"),
        MaxSDK::MaxGuid(L"c3d8a12e-7f41-4b95-a8e0-1d3f62b74c89"));

    // Explorer hierarchy view
    auto explorerQuad
        = menuMgr->GetQuadMenuById(MaxSDK::MaxGuid(L"75091e5c-cf71-4fba-9472-b26ad686a050"));
    AddUsdQuadMenuActions(
        explorerQuad->GetMenus()[2],
        MaxSDK::MaxGuid(L"feca2c9b-64d4-4288-b55f-f405e55ba57b"),
        MaxSDK::MaxGuid(L"03aa3fbc-cb27-4999-b19e-4f3540b9aa76"),
        MaxSDK::MaxGuid(L"a5e1b24d-9c63-4f87-b3d0-2e7a85c91f34"));

    // Explorer layer view
    auto layerQuad
        = menuMgr->GetQuadMenuById(MaxSDK::MaxGuid(L"2097f00d-4042-482d-8ff6-9df377c26591"));
    AddUsdQuadMenuActions(
        layerQuad->GetMenus()[2],
        MaxSDK::MaxGuid(L"c8af2b77-eabf-4ca4-86cb-897c1c75b1d5"),
        MaxSDK::MaxGuid(L"82b45f54-2ca9-4837-9e12-779ef5dad621"),
        MaxSDK::MaxGuid(L"f8b3c95a-2d74-4e61-9a7b-4c5e10d83f21"));
}

#endif