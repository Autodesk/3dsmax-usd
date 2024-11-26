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
#include "USDStageObjectclassDesc.h"

#include "USDStageObject.h"

#include <MaxUsdObjects/DLLEntry.h>
#include <MaxUsdObjects/USDExplorer.h>
#include <MaxUsdObjects/Views/UsdStageNodeAnimationRollup.h>
#include <MaxUsdObjects/Views/UsdStageNodeParametersRollup.h>
#include <MaxUsdObjects/Views/UsdStageNodePrimSelectionDialog.h>
#include <MaxUsdObjects/Views/UsdStageRenderSettingsRollup.h>
#include <MaxUsdObjects/Views/UsdStageViewportDisplayRollup.h>
#include <MaxUsdObjects/Views/UsdStageViewportPerformanceRollup.h>
#include <MaxUsdObjects/Views/UsdStageViewportSelectionRollup.h>
#include <MaxUsdObjects/resource.h>

#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/OptionUtils.h>

#include <Qt/QmaxMainWindow.h>
#include <maxscript/foundation/arrays.h>
#include <maxscript/foundation/strings.h>

#include <GetCoreInterface.h>
#include <QtWidgets/QFileDialog>
#include <ifnpub.h>

int USDStageObjectclassDesc::IsPublic() { return true; }

void* USDStageObjectclassDesc::Create(BOOL loading) { return new USDStageObject(); }

const MCHAR* USDStageObjectclassDesc::ClassName()
{
    return GetString(IDS_USDSTAGEOBJECT_CLASS_NAME);
}

Class_ID USDStageObjectclassDesc::ClassID() { return USDSTAGEOBJECT_CLASS_ID; }

const MCHAR* USDStageObjectclassDesc::InternalName() { return _M("USDStageObject"); }

const MCHAR* USDStageObjectclassDesc::NonLocalizedClassName() { return _T("USDStageObject"); }

SClass_ID USDStageObjectclassDesc::SuperClassID() { return GEOMOBJECT_CLASS_ID; }

const MCHAR* USDStageObjectclassDesc::Category() { return GetString(IDS_USD_CATEGORY); }

HINSTANCE USDStageObjectclassDesc::HInstance() { return hInstance; }

MaxSDK::QMaxParamBlockWidget* USDStageObjectclassDesc::CreateQtWidget(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock,
    const MapID     paramMapID,
    MSTR&           rollupTitle,
    int&            rollupFlags,
    int&            rollupCategory)
{
    switch (paramMapID) {
    case UsdStageGeneral: {
        const auto stageSetupUi = new UsdStageNodeParametersRollup(owner, paramBlock);
        rollupTitle = MaxSDK::GetResourceStringAsMSTR(IDS_USDSTAGEOBJECT_ROLL_OUT_PARAMETERS_TITLE);
        return stageSetupUi;
    }
    case UsdStageViewportDisplay: {
        const auto viewportDisplayUi = new UsdStageViewportDisplayRollup(owner, paramBlock);
        rollupTitle
            = MaxSDK::GetResourceStringAsMSTR(IDS_USDSTAGEOBJECT_ROLL_OUT_VIEWPORT_DISPLAY_TITLE);
        return viewportDisplayUi;
    }
    case UsdStageViewportPerformance: {
        const auto viewportDisplayUi = new UsdStageViewportPerformanceRollup(owner, paramBlock);
        rollupTitle = MaxSDK::GetResourceStringAsMSTR(
            IDS_USDSTAGEOBJECT_ROLL_OUT_VIEWPORT_PERFORMANCE_TITLE);
        return viewportDisplayUi;
    }
    case UsdStageAnimation: {
        const auto viewportDisplayUi = new UsdStageNodeAnimationRollup(owner, paramBlock);
        rollupTitle = MaxSDK::GetResourceStringAsMSTR(IDS_USDSTAGEOBJECT_ROLL_OUT_ANIMATION_TITLE);
        return viewportDisplayUi;
    }
    case UsdStageRenderSettings: {
        const auto renderSettingsUi = new UsdStageRenderSettingsRollup(owner, paramBlock);
        rollupTitle
            = MaxSDK::GetResourceStringAsMSTR(IDS_USDSTAGEOBJECT_ROLL_OUT_RENDER_SETUP_TITLE);
        return renderSettingsUi;
    }
    case UsdStageSelection: {
        // Only display the selection mode rollout in modify mode. Can't switch to sub-object
        // mode(s) before actually having an object created.
        if (GetCOREInterface()->GetCommandPanelTaskMode() != TASK_MODE_MODIFY) {
            return nullptr;
        }

        const auto viewportSelectionUI = new UsdStageViewportSelectionRollup(owner, paramBlock);
        rollupTitle = MaxSDK::GetResourceStringAsMSTR(
            IDS_USDSTAGEOBJECT_ROLL_OUT_VIEWPORT_SELECTION_SETUP_TITLE);
        return viewportSelectionUI;
    }
    default: return nullptr;
    }
}

ClassDesc2* GetUSDStageObjectClassDesc()
{
    static USDStageObjectclassDesc classDesc;
    return &classDesc;
}

// Function Publishing
// This is a static Maxscript interface attached to the USDStageObject ClassDesc
// The goal is to expose static funtion callable from UsdStageObject itself.
class UsdStageObjectStaticInterface : public FPStaticInterface
{
protected:
    DECLARE_DESCRIPTOR(UsdStageObjectStaticInterface)

    /*
     * \brief Open a file explorer dialog at the specified path, allowing selection of an USD file,
     *  then open the prim selection dialog and allow selection of a specific prim.
     *  This method is exposed in maxscript as a static utility function.
     * \param path The path where the file picker dialog will open, if null will open the file picker dialog at the default location.
     * \param filterMode The applied filter type. Include or exclude the Prim types contained in filteredTypeNames.
     * \param filteredTypeNames The Prim types used to filter the stage.
     * \param showLoadPayloadsOption If true, the loadPayloads option is used / showed in the dialog.
     * \param showOpenInUsdExplorerOption If true, the showInExplorer option is used / showed in the dialog.
     * \param useUserSettings If true, will ignore the values of showLoadPayLoadsOption and showOpenInUsdExplorerOptions,
     * the UI element will be shown and set to the saved user preferences by default.
     * \return An array containing the file path to the selected USD file, the Prim path selected and whether payloads should be loaded.
     */
    Value* SelectRootLayerAndPrim(
        const wchar_t*     path,
        int                filterMode,
        Tab<const TCHAR*>* filteredTypeNames,
        bool               showLoadPayloadsOption,
        bool               showOpenInUsdExplorerOption,
        bool               useUserSettings)
    {
        QFileInfo fileInfo(QString::fromStdWString(path));

        // If the user path points to a valid file already, don't open the file picker dialog.
        // If the user specified a path to a file that don't exist, it'll open to the parent folder.
        // If the user used an empty string as argument, the file picker will open at the default
        // location ( Max versioned folder in the user documents )
        if (fileInfo.isDir() && fileInfo.exists() || !fileInfo.exists()) {
            fileInfo = QFileInfo { QFileDialog::getOpenFileName(
                GetCOREInterface()->GetQmaxMainWindow(),
                QCoreApplication::translate(
                    "MaxUsdObjects", "Select Universal Scene Description (USD) File"),
                fileInfo.absoluteFilePath(),
                QCoreApplication::translate("MaxUsdObjects", "USD (*.usd;*.usda;*.usdc)")) };
            if (!fileInfo.exists()) {
                return 0;
            }
        }

        std::vector<std::string> filters;
        if (filteredTypeNames != nullptr) {
            for (int i = 0; i < filteredTypeNames->Count(); i++) {
                filters.emplace_back(MaxUsd::MaxStringToUsdString((*filteredTypeNames)[i]));
            }
        }

        pxr::VtDictionary options;
        static const std::string optionsCategoryKey = "PrimSelectionDialogPreferences";
        if (useUserSettings) {
            MaxUsd::OptionUtils::LoadUiOptions(optionsCategoryKey, options);
            if (!options[pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads].IsHolding<bool>()) {
                options[pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads] = true;
            }
            if (!options[pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer].IsHolding<bool>()) {
                options[pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer] = true;
            }
        }
        else {
            if (showLoadPayloadsOption) {
                options[pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads] = true;
            }
            if (showOpenInUsdExplorerOption) {
                options[pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer] = true;
            }
        }

        std::unique_ptr<UsdStageNodePrimSelectionDialog> primSelectionDialog
            = std::make_unique<UsdStageNodePrimSelectionDialog>(
                fileInfo.absoluteFilePath(),
                nullptr,
                static_cast<MaxUsd::TreeModelFactory::TypeFilteringMode>(filterMode),
                filters,
                options,
                GetCOREInterface()->GetQmaxMainWindow());

        primSelectionDialog->setWindowTitle(
            QCoreApplication::translate("USDStageObject", "Select USD Prim from File"));

        if (primSelectionDialog->exec() == QDialog::Accepted) {
            // user hit OK
            QString rootLayerPath = primSelectionDialog->GetRootLayerPath();
            QString selectedPrim = primSelectionDialog->GetMaskPath();

            if (useUserSettings) {
                pxr::VtDictionary newOptions;
                newOptions[pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads]
                    = primSelectionDialog->GetPayloadsLoaded();
                newOptions[pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer]
                    = primSelectionDialog->GetOpenInUsdExplorer();
                if (newOptions != options) {
                    MaxUsd::OptionUtils::SaveUiOptions(optionsCategoryKey, newOptions);
                }
            }

            Array* results = new Array(0);
            results->append(new String(rootLayerPath.toStdWString().c_str()));
            results->append(new String(selectedPrim.toStdWString().c_str()));
            // Optionally displayed in the dialog - if not displayed, will be false here.
            results->append(primSelectionDialog->GetPayloadsLoaded() ? &true_value : &false_value);
            results->append(
                primSelectionDialog->GetOpenInUsdExplorer() ? &true_value : &false_value);
            return results;
        }
        return 0;
    }

    void OpenUsdExplorer() { USDExplorer::Instance()->Open(); }

    void CloseUsdExplorer() { USDExplorer::Instance()->Close(); }

    enum
    {
        fnIdSelectRootLayerAndPrim,
        fnIdOpenUsdExplorer,
        fnIdCloseUsdExplorer,
    };

    enum
    {
        eidFilteringType
    };

// clang-format off
    BEGIN_FUNCTION_MAP
        FN_6(fnIdSelectRootLayerAndPrim, TYPE_VALUE, SelectRootLayerAndPrim, TYPE_STRING, TYPE_ENUM, TYPE_STRING_TAB, TYPE_BOOL, TYPE_BOOL, TYPE_BOOL);
        VFN_0(fnIdOpenUsdExplorer, OpenUsdExplorer);
        VFN_0(fnIdCloseUsdExplorer, CloseUsdExplorer);
    END_FUNCTION_MAP
// clang-format on
};

#define USDSTAGEOBJECT_FP_INTERFACE Interface_ID(0x130335d6, 0xe7a7529)

// clang-format off
static UsdStageObjectStaticInterface usdStageObjectStaticInterface(
        USDSTAGEOBJECT_FP_INTERFACE, _T("UsdStageObjectInterface"), 0, GetUSDStageObjectClassDesc(), FP_STATIC_METHODS,
        // Functions
        UsdStageObjectStaticInterface::fnIdSelectRootLayerAndPrim, _T("SelectRootLayerAndPrim"), IDS_SELECTLAYERANDPRIM,
        TYPE_VALUE, FP_NO_REDRAW, 6,
        _T("rootFolderPath"), 0, TYPE_STRING, f_keyArgDefault, _T(""),
        _T("filterMode"), 0, TYPE_ENUM, UsdStageObjectStaticInterface::eidFilteringType, f_keyArgDefault, MaxUsd::TreeModelFactory::TypeFilteringMode::NoFilter,
        _T("filteredTypes"), 0, TYPE_STRING_TAB, f_keyArgDefault, NULL,
        _T("showLoadPayloadsOption"), 0, TYPE_BOOL, f_keyArgDefault, TRUE,
        _T("showOpenInExplorerOption"), 0, TYPE_BOOL, f_keyArgDefault, TRUE,
        _T("useUserSettings"), 0, TYPE_BOOL, f_keyArgDefault, FALSE,
        UsdStageObjectStaticInterface::fnIdOpenUsdExplorer, _T("OpenUsdExplorer"), IDS_OPENUSDEXPLORER, TYPE_VALUE, FP_NO_REDRAW, 0,
        UsdStageObjectStaticInterface::fnIdCloseUsdExplorer, _T("CloseUsdExplorer"), IDS_CLOSEUSDEXPLORER, TYPE_VALUE, FP_NO_REDRAW, 0,

        enums,
        UsdStageObjectStaticInterface::eidFilteringType, 3,
        _T("none"), MaxUsd::TreeModelFactory::TypeFilteringMode::NoFilter,
        _T("include"), MaxUsd::TreeModelFactory::TypeFilteringMode::Include,
        _T("exclude"), MaxUsd::TreeModelFactory::TypeFilteringMode::Exclude,

        p_end);
// clang-format on