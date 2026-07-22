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

#include "UsdMenu.h"
#include "UsdStageNodePrimSelectionDialog.h"

#include <MaxUsdObjects/AssetResolver/SettingsManagement.h>
#include <MaxUsdObjects/LayerEditor/MaxLayerEditor.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>
#include <MaxUsdObjects/USDExplorer.h>

#include <MaxUsd/Utilities/OptionUtils.h>

#include <pxr/base/vt/dictionary.h>

#include <Qt/QmaxMainWindow.h>
#include <maxscript/maxscript.h>

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

#ifdef ADSK_ASSET_RESOLVER_ENABLED
BOOL USDMenuToolsPathEditorActionItem::ExecuteAction()
{
    AssetResolverSettingsManagement::ShowDialog();
    return TRUE;
}
#endif // ADSK_ASSET_RESOLVER_ENABLED

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

#ifdef ADSK_ASSET_RESOLVER_ENABLED
    toolsSubMenu->CreateAction(
        kUsdMenuToolsPathEditorGUID,
        usdMenuActionTableId,
        usdMenuToolsPathEditorActionItemId,
        _T("USD Path Editor"));
#endif // ADSK_ASSET_RESOLVER_ENABLED
}

void RegisterUSDMenuAction()
{
    auto* usdActionTable
        = new ActionTable(usdMenuActionTableId, kActionMainUIContext, _T("USD Menu Action Table"));

    usdActionTable->AppendOperation(new USDMenuCreateStageFromFileActionItem {});
    usdActionTable->AppendOperation(new USDMenuCreateStageWithNewLayerActionItem {});
    usdActionTable->AppendOperation(new USDMenuToolsExplorerActionItem {});
    usdActionTable->AppendOperation(new USDMenuToolsLayerEditorActionItem {});
#ifdef ADSK_ASSET_RESOLVER_ENABLED
    usdActionTable->AppendOperation(new USDMenuToolsPathEditorActionItem {});
#endif // ADSK_ASSET_RESOLVER_ENABLED
    IActionManager* actionManager = GetCOREInterface()->GetActionManager();
    if (actionManager) {
        actionManager->RegisterActionTable(usdActionTable);
        static ActionCallback usdActionCallback;
        actionManager->ActivateActionTable(&usdActionCallback, usdMenuActionTableId);
    }
}

#ifdef ADSK_ASSET_RESOLVER_ENABLED
void InitializeAssetResolverSettings(void* param, NotifyInfo* info)
{
    AssetResolverSettingsManagement::InitializeSettings();
}
#endif // ADSK_ASSET_RESOLVER_ENABLED
#endif // IS_MAX2025_OR_GREATER