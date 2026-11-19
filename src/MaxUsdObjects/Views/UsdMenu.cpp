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

#include "UsdMenu.h"
#include "UsdStageNodePrimSelectionDialog.h"

#include <MaxUsdObjects/AssetResolver/SettingsManagement.h>
#ifdef ADSK_COMPONENT_CREATOR_ENABLED
#include <MaxUsdObjects/ComponentCreator/Commands/CreateComponentFromNodesCmd.h>
#include <MaxUsdObjects/ComponentCreator/MaxVariantEditor.h>
#include <MaxUsdObjects/ComponentCreator/UtilComponentCreator.h>
#endif
#include <MaxUsdObjects/LayerEditor/MaxLayerEditor.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>
#include <MaxUsdObjects/RenderSetup/SettingsManagement.h>
#include <MaxUsdObjects/USDExplorer.h>

#include <MaxUsd/Utilities/OptionUtils.h>

#include <pxr/base/vt/dictionary.h>

#include <Qt/QmaxMainWindow.h>
#include <maxscript/maxscript.h>
#include <ufe/undoableCommandMgr.h>

#include <CUI/ICuiMenu.h>
#include <CUI/ICuiMenuManager.h>
#include <GetCOREInterface.h>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <notify.h>

PXR_NAMESPACE_USING_DIRECTIVE

BOOL USDMenuCreateStageFromFileActionItem::ExecuteAction()
{
    theHold.Begin();

    ExecuteMAXScriptScript(
        L"macros.run \"USD\" \"CreateUSDStage\"", MAXScript::ScriptSource::Embedded);
    if (GetCOREInterface()->GetSelNodeCount() != 1) {
        // Create stage operation was aborted.
        theHold.Cancel();
        return FALSE;
    }

    INode*     stageNode = GetCOREInterface()->GetSelNode(0);
    const auto usdStageObject = dynamic_cast<USDStageObject*>(stageNode->GetObjectRef());
    if (!usdStageObject) {
        DbgAssert(_T("Incorrect stage node object type."));
        theHold.Cancel();
        return FALSE;
    }

    theHold.Accept(L"Create Stage from File...");
    return TRUE;
}

BOOL USDMenuCreateStageWithNewLayerActionItem::ExecuteAction()
{
    theHold.Begin();

    const auto stageObject = static_cast<USDStageObject*>(
        GetCOREInterface()->CreateInstance(GEOMOBJECT_CLASS_ID, USDSTAGEOBJECT_CLASS_ID));
    GetCOREInterface()->CreateObjectNode(stageObject);
    stageObject->OpenInUsdExplorer();

    theHold.Accept(L"Stage with New Layer");
    return TRUE;
}

BOOL USDMenuToolsExplorerActionItem::ExecuteAction()
{
    USDExplorer::Instance()->Open();
    return TRUE;
}

BOOL USDMenuToolsLayerEditorActionItem::ExecuteAction()
{
    MaxLayerEditor::Instance()->Open();
    return TRUE;
}

#ifdef ADSK_COMPONENT_CREATOR_ENABLED
BOOL USDMenuToolsVariantEditorActionItem::ExecuteAction()
{
    MaxVariantEditor::Instance()->Open();
    return TRUE;
}

BOOL USDMenuCreateComponentFromSelectionActionItem::ExecuteAction()
{
    std::vector<INode*> nodes = MaxUsd::GetSelectedNodes();
    if (nodes.empty()) {
        return FALSE;
    }
    auto cmd = std::make_shared<CreateComponentFromNodesCmd>(
        std::move(nodes), AdskUsdComponentCreator::Options {});
    Ufe::UndoableCommandMgr::instance().executeCmd(cmd);
    return TRUE;
}
#endif

#ifdef ADSK_USD_ASSET_RESOLVER_ENABLED
BOOL USDMenuToolsPathEditorActionItem::ExecuteAction()
{
    AssetResolverSettingsManagement::ShowDialog();
    return TRUE;
}
#endif // ADSK_USD_ASSET_RESOLVER_ENABLED

#ifdef ADSK_RENDER_SETUP_ENABLED
BOOL USDMenuToolsRenderSetupActionItem::ExecuteAction()
{
    RenderSetupManagement::ShowDialog();
    return TRUE;
}
#endif // ADSK_RENDER_SETUP_ENABLED

void USDMenuRegisterCallback(void* param, NotifyInfo* info)
{
    using namespace MaxSDK::CUI;
    ICuiMenuManager* menuMgr = GetNotifyParam<NOTIFY_CUI_REGISTER_MENUS>(info);
    if (!menuMgr) {
        return;
    }

    ICuiMenu* theMenu = menuMgr->GetMainMenuBar();
    ICuiMenu* usdMenu = theMenu->CreateSubMenu(kUsdMenuGUID, _T("USD"), kHelpMenuId);

    ICuiMenu* createSubMenu = usdMenu->CreateSubMenu(kUsdMenuCreateSubMenuGUID, _T("Create"));
    createSubMenu->CreateAction(
        kUsdMenuCreateStageFromFileGUID,
        usdMenuActionTableId,
        usdMenuCreateStageFromFileActionItemId,
        _T("Stage from File..."));
    createSubMenu->CreateAction(
        kUsdMenuCreateStageWithNewLayerGUID,
        usdMenuActionTableId,
        usdMenuCreateStageWithNewLayerActionItemId,
        _T("Stage with New Layer"));
#ifdef ADSK_COMPONENT_CREATOR_ENABLED
    createSubMenu->CreateAction(
        kUsdMenuCreateComponentFromSelectionGUID,
        usdMenuActionTableId,
        usdMenuCreateComponentFromSelectionActionItemId,
        _T("Component from Selection"));
#endif

    usdMenu->CreateSeparator(kUsdMenuSeparatorToolsGUID);

    ICuiMenu* toolsSubMenu = usdMenu->CreateSubMenu(kUsdMenuToolsSubMenuGUID, _T("Tools"));
    toolsSubMenu->CreateAction(
        kUsdMenuToolsExplorerGUID,
        usdMenuActionTableId,
        usdMenuToolsExplorerActionItemId,
        _T("USD Explorer"));
    toolsSubMenu->CreateAction(
        kUsdMenuToolsLayerEditorGUID,
        usdMenuActionTableId,
        usdMenuToolsLayerEditorActionItemId,
        _T("USD Layer Editor"));
#ifdef ADSK_COMPONENT_CREATOR_ENABLED
    toolsSubMenu->CreateAction(
        kUsdMenuToolsVariantEditorGUID,
        usdMenuActionTableId,
        usdMenuToolsVariantEditorActionItemId,
        _T("USD Variant Manager"));
#endif

#ifdef ADSK_USD_ASSET_RESOLVER_ENABLED
    toolsSubMenu->CreateAction(
        kUsdMenuToolsPathEditorGUID,
        usdMenuActionTableId,
        usdMenuToolsPathEditorActionItemId,
        _T("USD Path Editor"));
#endif // ADSK_USD_ASSET_RESOLVER_ENABLED

#ifdef ADSK_RENDER_SETUP_ENABLED
    toolsSubMenu->CreateAction(
        kUsdMenuToolsRenderSetupGUID,
        usdMenuActionTableId,
        usdMenuToolsRenderSetupActionItemId,
        _T("USD Render Setup"));
#endif // ADSK_RENDER_SETUP_ENABLED
}

void RegisterUSDMenuAction()
{
    auto* usdActionTable
        = new ActionTable(usdMenuActionTableId, kActionMainUIContext, _T("USD Menu Action Table"));

    usdActionTable->AppendOperation(new USDMenuCreateStageFromFileActionItem {});
    usdActionTable->AppendOperation(new USDMenuCreateStageWithNewLayerActionItem {});
#ifdef ADSK_COMPONENT_CREATOR_ENABLED
    usdActionTable->AppendOperation(new USDMenuCreateComponentFromSelectionActionItem {});
    usdActionTable->AppendOperation(new USDMenuToolsVariantEditorActionItem {});
#endif
    usdActionTable->AppendOperation(new USDMenuToolsExplorerActionItem {});
    usdActionTable->AppendOperation(new USDMenuToolsLayerEditorActionItem {});
#ifdef ADSK_USD_ASSET_RESOLVER_ENABLED
    usdActionTable->AppendOperation(new USDMenuToolsPathEditorActionItem {});
#endif // ADSK_USD_ASSET_RESOLVER_ENABLED
#ifdef ADSK_RENDER_SETUP_ENABLED
    usdActionTable->AppendOperation(new USDMenuToolsRenderSetupActionItem {});
#endif // ADSK_RENDER_SETUP_ENABLED
    IActionManager* actionManager = GetCOREInterface()->GetActionManager();
    if (actionManager) {
        actionManager->RegisterActionTable(usdActionTable);
        static ActionCallback usdActionCallback;
        actionManager->ActivateActionTable(&usdActionCallback, usdMenuActionTableId);
    }
}

#ifdef ADSK_USD_ASSET_RESOLVER_ENABLED
void InitializeAssetResolverSettings(void* param, NotifyInfo* info)
{
    AssetResolverSettingsManagement::InitializeSettings();
}
#endif // ADSK_USD_ASSET_RESOLVER_ENABLED
#endif // IS_MAX2025_OR_GREATER