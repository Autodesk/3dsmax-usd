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

#include <MaxUsdObjects/LayerEditor/MaxLayerEditor.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>
#include <MaxUsdObjects/USDExplorer.h>

#include <MaxUsd/Utilities/OptionUtils.h>

#ifdef IS_MAX2026_OR_GREATER
#include <preferences/PreferencesDialog.h>
#include <preferences/PreferencesManagement.h>
#endif

#include <Qt/QmaxMainWindow.h>
#include <maxscript/maxscript.h>

#include <CUI/ICuiMenu.h>
#include <CUI/ICuiMenuManager.h>
#include <GetCOREInterface.h>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <notify.h>

#include <pxr/base/vt/dictionary.h>

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

#ifdef IS_MAX2026_OR_GREATER
BOOL USDMenuPreferencesActionItem::ExecuteAction()
{
    const UsdPreferenceOptions&           options = PreferencesManagement::GetUsdPreferences();
    std::unique_ptr<UsdPreferencesDialog> usdPreferencesDialog
        = std::make_unique<UsdPreferencesDialog>(options, GetCOREInterface()->GetQmaxMainWindow());

   QRect savedGeometry = QRect(-1, -1, -1, -1);
    {
        VtDictionary dict;
        MaxUsd::OptionUtils::LoadUiOptions("USD Preferences", dict);
        auto           it = dict.find("Dialog Geometry");
        VtArray<float> val = { -1.0, -1.0, -1.0, -1.0 };
        if (it != dict.end()) {
            if (it->second.IsHolding<VtArray<float>>()) {
                val = it->second.GetWithDefault<VtArray<float>>(val);
            } else if (it->second.CanCast<VtArray<float>>()) {
                val = it->second.Cast<VtArray<float>>().GetWithDefault<VtArray<float>>(val);
            }
        }
        if (val.size() == 4 && val[2] >= 0.0f) {
            savedGeometry = QRect(
                MaxSDK::UIScaled(val[0]),
                MaxSDK::UIScaled(val[1]),
                MaxSDK::UIScaled(val[2]),
                MaxSDK::UIScaled(val[3]));
            usdPreferencesDialog->setGeometry(savedGeometry);
        }
    }

    QRect dialogGeometry = savedGeometry;
    QObject::connect(
        usdPreferencesDialog.get(),
        &UsdPreferencesDialog::geometryChanged,
        [&dialogGeometry](const QRect& geometry) {
            dialogGeometry = geometry;
        });

    QObject::connect(
        usdPreferencesDialog.get(), &QDialog::finished, [&dialogGeometry, &savedGeometry]() {
        if (dialogGeometry != savedGeometry) {
            VtDictionary   dict;
            VtArray<float> val
                = { MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.left())),
                    MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.top())),
                    MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.width())),
                    MaxSDK::UIUnScaled(static_cast<float>(dialogGeometry.height())) };
            dict["Dialog Geometry"] = val;
            MaxUsd::OptionUtils::SaveUiOptions("USD Preferences", dict);
        }
    });

    if (usdPreferencesDialog->exec() == QDialog::Accepted) {
        auto newOptions = usdPreferencesDialog->getOptions();
        PreferencesManagement::ApplyUsdPreferences(options, newOptions);
        PreferencesManagement::SaveUsdPreferences(newOptions);
    }
    return TRUE;
}
#endif

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

#ifdef IS_MAX2026_OR_GREATER
    usdMenu->CreateSeparator(kUsdMenuSeparatorPreferencesGUID);
    usdMenu->CreateAction(
        kUsdMenuPreferencesGUID,
        usdMenuActionTableId,
        usdMenuPreferencesActionItemId,
        _T("USD Preferences"));
#endif
}

void RegisterUSDMenuAction()
{
    auto* usdActionTable
        = new ActionTable(usdMenuActionTableId, kActionMainUIContext, _T("USD Menu Action Table"));

    usdActionTable->AppendOperation(new USDMenuCreateStageFromFileActionItem {});
    usdActionTable->AppendOperation(new USDMenuCreateStageWithNewLayerActionItem {});
    usdActionTable->AppendOperation(new USDMenuToolsExplorerActionItem {});
    usdActionTable->AppendOperation(new USDMenuToolsLayerEditorActionItem {});
#ifdef IS_MAX2026_OR_GREATER
    usdActionTable->AppendOperation(new USDMenuPreferencesActionItem {});
#endif
    IActionManager* actionManager = GetCOREInterface()->GetActionManager();
    if (actionManager) {
        actionManager->RegisterActionTable(usdActionTable);
        static ActionCallback usdActionCallback;
        actionManager->ActivateActionTable(&usdActionCallback, usdMenuActionTableId);
    }
}


#ifdef IS_MAX2026_OR_GREATER
void InitializeUsdPreferences(void* param, NotifyInfo* info)
{
    PreferencesManagement::InitializeUsdPreferences();
}
#endif
#endif