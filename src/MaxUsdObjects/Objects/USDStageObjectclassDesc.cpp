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
#include <MaxUsdObjects/LayerEditor/MaxLayerEditor.h>
#include <MaxUsdObjects/LayerEditor/USDLayerManager.h>
#include <MaxUsdObjects/USDExplorer.h>
#include <MaxUsdObjects/Views/UsdStageMetadataRollup.h>
#include <MaxUsdObjects/Views/UsdStageNodeAnimationRollup.h>
#include <MaxUsdObjects/Views/UsdStageNodePrimSelectionDialog.h>
#include <MaxUsdObjects/Views/UsdStageNodeStageRollup.h>
#include <MaxUsdObjects/Views/UsdStageRenderSettingsRollup.h>
#include <MaxUsdObjects/Views/UsdStageToolsRollup.h>
#include <MaxUsdObjects/Views/UsdStageViewportDisplayRollup.h>
#include <MaxUsdObjects/Views/UsdStageViewportPerformanceRollup.h>
#include <MaxUsdObjects/Views/UsdStageViewportSelectionRollup.h>
#include <MaxUsdObjects/resource.h>

#include <MaxUsd/Utilities/OptionUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/sdf/layerStateDelegate.h>

#include <Qt/QmaxMainWindow.h>
#include <maxscript/foundation/arrays.h>
#include <maxscript/foundation/strings.h>

#include <GetCoreInterface.h>
#include <QtWidgets/QFileDialog>
#include <ifnpub.h>

// Bump this version number when saved data changes.
static int       USD_OBJECT_CLASS_DATA_SAVE_VERSION = 1;
constexpr USHORT SAVE_VERSION_CHUNK_ID = 100;
constexpr USHORT LAYER_EDITS_CHUNK_ID = 200;
constexpr USHORT LAYER_EDITS_LAYER_ID_SIZE_CHUNK_ID = 300;
constexpr USHORT LAYER_EDITS_LAYER_ID_CHUNK_ID = 400;
constexpr USHORT LAYER_EDITS_LAYER_DATA_SIZE_CHUNK_ID = 500;
constexpr USHORT LAYER_EDITS_LAYER_DATA_CHUNK_ID = 600;

int USDStageObjectclassDesc::IsPublic() { return true; }

void* USDStageObjectclassDesc::Create(BOOL loading) { return new USDStageObject(); }

const MCHAR* USDStageObjectclassDesc::ClassName()
{
    return GetString(IDS_USDSTAGEOBJECT_CLASS_NAME);
}

Class_ID USDStageObjectclassDesc::ClassID() { return USDSTAGEOBJECT_CLASS_ID; }

const MCHAR* USDStageObjectclassDesc::InternalName() { return _M("USDStageObject"); }

const MCHAR* USDStageObjectclassDesc::NonLocalizedClassName() { return _T("USD Stage"); }

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
    rollupFlags = 0;
    rollupCategory = ROLLUP_CAT_STANDARD;

    switch (paramMapID) {
    case UsdStageGeneral: {
        const auto stageSetupUi = new UsdStageNodeStageRollup(owner, paramBlock);
        rollupTitle = MaxSDK::GetResourceStringAsMSTR(IDS_USDSTAGEOBJECT_ROLLUP_STAGE_TITLE);
        return stageSetupUi;
    }
    case UsdStageViewportDisplay: {
        const auto viewportDisplayUi = new UsdStageViewportDisplayRollup(owner, paramBlock);
        rollupTitle
            = MaxSDK::GetResourceStringAsMSTR(IDS_USDSTAGEOBJECT_ROLLUP_VIEWPORT_DISPLAY_TITLE);
        return viewportDisplayUi;
    }
    case UsdStageViewportPerformance: {
        const auto viewportDisplayUi = new UsdStageViewportPerformanceRollup(owner, paramBlock);
        rollupTitle
            = MaxSDK::GetResourceStringAsMSTR(IDS_USDSTAGEOBJECT_ROLLUP_VIEWPORT_PERFORMANCE_TITLE);
        return viewportDisplayUi;
    }
    case UsdStageAnimation: {
        const auto viewportDisplayUi = new UsdStageNodeAnimationRollup(owner, paramBlock);
        rollupTitle = MaxSDK::GetResourceStringAsMSTR(IDS_USDSTAGEOBJECT_ROLLUP_ANIMATION_TITLE);
        return viewportDisplayUi;
    }
    case UsdStageRenderSettings: {
        const auto renderSettingsUi = new UsdStageRenderSettingsRollup(owner, paramBlock);
        rollupTitle = MaxSDK::GetResourceStringAsMSTR(IDS_USDSTAGEOBJECT_ROLLUP_RENDER_SETUP_TITLE);
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
            IDS_USDSTAGEOBJECT_ROLLUP_VIEWPORT_SELECTION_SETUP_TITLE);
        return viewportSelectionUI;
    }
    case UsdStageTools: {
        const auto toolsDisplayUi = new UsdStageToolsRollup(owner, paramBlock);
        rollupTitle = MaxSDK::GetResourceStringAsMSTR(IDS_USDSTAGEOBJECT_ROLLUP_TOOLS_TITLE);
        return toolsDisplayUi;
    }
    case UsdStageMetadata: {
        const auto metadataDisplayUi = new UsdStageMetadataRollup(owner, paramBlock);
        rollupTitle = MaxSDK::GetResourceStringAsMSTR(IDS_USDSTAGEOBJECT_ROLLUP_METADATA_TITLE);
        return metadataDisplayUi;
    }
    default: return nullptr;
    }
}

ClassDesc2* GetUSDStageObjectClassDesc()
{
    static USDStageObjectclassDesc classDesc;
    return &classDesc;
}

bool USDStageObjectclassDesc::RemoveParamMap(IParamMap2* pParamMap)
{
    auto& maps = GetParamMaps();
    int   mapCount = maps.Count();
    for (int i = 0; i < mapCount; ++i) {
        if (maps[i] == pParamMap) {
            maps.Delete(i, 1);
            return true;
        }
    }
    return false;
}

void USDStageObjectclassDesc::AddParamMap(IParamMap2* pParamMap)
{
    auto& maps = GetParamMaps();
    maps.Append(1, &pParamMap);
}

BOOL USDStageObjectclassDesc::NeedsToSave() { return TRUE; };

IOResult USDStageObjectclassDesc::Save(ISave* iSave)
{
    ULONG nb = 0;

    // Save the version first - if the saved format changes, we need to know what we are reading..
    iSave->BeginChunk(SAVE_VERSION_CHUNK_ID);
    iSave->Write(
        &USD_OBJECT_CLASS_DATA_SAVE_VERSION, sizeof(USD_OBJECT_CLASS_DATA_SAVE_VERSION), &nb);
    iSave->EndChunk();

    if (USDLayerManager::Instance()->GetSaveMode() == SaveMode::SaveAllEditsMax) {

        std::unordered_map<std::string, bool> writtenLayerNames;
        const auto dirtyLayersMap = USDLayerManager::Instance()->GetDirtyLayersToSave();
        for (auto dirtyLayerTup : dirtyLayersMap) {
            auto dirtyLayers = dirtyLayerTup.second;

            if (dirtyLayers.size() > 0) {
                for (auto& dirtylayer : dirtyLayers) {
                    std::string dirtyLayersStr;
                    std::string layerIdentifierStr = dirtylayer->GetIdentifier();

                    // Check if we already wrote this layer to storage
                    if (writtenLayerNames.find(layerIdentifierStr) != writtenLayerNames.end()) {
                        continue;
                    }
                    writtenLayerNames[layerIdentifierStr] = true;

                    const bool sessionExpResult = dirtylayer->ExportToString(&dirtyLayersStr);
                    // If there is an error, log it, but do not fail the entire max scene save.
                    if (!sessionExpResult) {
                        const auto msg = _T("USDStageObjectclassDesc save error. Unable to ")
                                         _T("serialize the dirty ")
                                         _T("layer to a string.");
                        DbgAssert(0 && msg);
                        GetCOREInterface()->Log()->LogEntry(SYSLOG_ERROR, NO_DIALOG, nullptr, msg);
                        dirtyLayersStr.clear();
                    }

                    int maxLayerSize = std::numeric_limits<int>().max(); // 2147483647
                    // HACK: unfortunately, the USD "ExportToString()" function seems to limit the
                    // amount of data exported to string to 2GB. As such, in order to avoid writing
                    // corrupt/incomplete data to disk, we only save layers that are under the 2GB
                    // size. The current condition checks if the file is exactly 2GB, so it is
                    // technically possible that a USD layer is exactly the below size, being a
                    // valid file for saving, but we do not save it.
                    if (dirtyLayersStr.size() >= maxLayerSize) {
                        const auto msg = _T("USDStageObjectclassDesc save error. Unable to ")
                                         _T("serialize the dirty ")
                                         _T("layer to a string. Size is over the 2GB limit.");
                        DbgAssert(0 && msg);
                        GetCOREInterface()->Log()->LogEntry(SYSLOG_ERROR, NO_DIALOG, nullptr, msg);
                        dirtyLayersStr.clear();
                    } else {
                        iSave->BeginChunk(LAYER_EDITS_CHUNK_ID);

                        iSave->BeginChunk(LAYER_EDITS_LAYER_ID_SIZE_CHUNK_ID);
                        auto layerIdentifierStrSize = static_cast<ULONG>(layerIdentifierStr.size());
                        iSave->Write(&layerIdentifierStrSize, sizeof(ULONG), &nb);
                        iSave->EndChunk();

                        iSave->BeginChunk(LAYER_EDITS_LAYER_ID_CHUNK_ID);
                        iSave->Write(layerIdentifierStr.c_str(), layerIdentifierStrSize, &nb);
                        iSave->EndChunk();

                        iSave->BeginChunk(LAYER_EDITS_LAYER_DATA_SIZE_CHUNK_ID);
                        auto dirtyLayersStrSize = static_cast<ULONG>(dirtyLayersStr.size());
                        iSave->Write(&dirtyLayersStrSize, sizeof(ULONG), &nb);
                        iSave->EndChunk();

                        iSave->BeginChunk(LAYER_EDITS_LAYER_DATA_CHUNK_ID);
                        iSave->Write(dirtyLayersStr.c_str(), dirtyLayersStrSize, &nb);
                        iSave->EndChunk();

                        iSave->EndChunk();
                    }
                }
            }
        }
    }
    return IO_OK;
}

IOResult USDStageObjectclassDesc::Load(ILoad* iLoad)
{
    IOResult res = IO_OK;
    ULONG    nb = 0;

    res = iLoad->OpenChunk();

    // Nothing to load. Could be a USDStageObjectclassDesc in an earlier version of the plugin.
    if (res == IO_END) {
        return IO_OK;
    }

    if (res != IO_OK) {
        DbgAssert(0 && _T("Problem in loading saved data USDStageObjectclassDesc."));
        return res;
    }

    if (iLoad->CurChunkID() != SAVE_VERSION_CHUNK_ID) {
        DbgAssert(iLoad->CurChunkID() == SAVE_VERSION_CHUNK_ID); // Should always be first
        return IO_ERROR;
    }

    // Read save model version
    int loadedVersion = -1;
    res = iLoad->Read(&loadedVersion, sizeof(loadedVersion), &nb);
    iLoad->CloseChunk();
    if (IO_OK != res) {
        DbgAssert(0 && _T("Problem in loading version of the USDStageObjectclassDesc"));
        return res;
    }

    // For now don't do anything. In the future there are actually multiple versions, we will
    // need to deal with them individually...
    if (loadedVersion != USD_OBJECT_CLASS_DATA_SAVE_VERSION) {
        return IO_OK;
    }

    int numDirtyLayer = 0;
    while (IO_OK == (res = iLoad->OpenChunk())) {
        switch (iLoad->CurChunkID()) {
        case LAYER_EDITS_CHUNK_ID: {
            std::string layerStr;
            std::string layerIdStr;
            ULONG       layerStrSize = 0;
            ULONG       layerIdStrSize = 0;
            char*       buffer = NULL;
            while (IO_OK == (res = iLoad->OpenChunk())) {
                switch (iLoad->CurChunkID()) {
                case LAYER_EDITS_LAYER_ID_SIZE_CHUNK_ID: {
                    const auto layerIdStarSizeRes
                        = iLoad->Read(&layerIdStrSize, sizeof(layerIdStrSize), &nb);
                    if (layerIdStarSizeRes != IO_OK) {
                        DbgAssert(
                            0
                            && _T("Error reading saved dirty layer id size in ")
                               _T("USDStageObjectclassDesc."));
                        return layerIdStarSizeRes;
                    }
                    break;
                }
                case LAYER_EDITS_LAYER_DATA_SIZE_CHUNK_ID: {
                    const auto layerStrSizeRes
                        = iLoad->Read(&layerStrSize, sizeof(layerStrSize), &nb);
                    if (layerStrSizeRes != IO_OK) {
                        DbgAssert(
                            0
                            && _T("Error reading saved dirty layer data size in ")
                               _T("USDStageObjectclassDesc."));
                        return layerStrSizeRes;
                    }
                    break;
                }
                case LAYER_EDITS_LAYER_ID_CHUNK_ID: {
                    buffer = new char[layerIdStrSize + 1];
                    const auto layerIdStrRes = iLoad->Read(buffer, layerIdStrSize, &nb);
                    if (layerIdStrRes != IO_OK) {
                        delete buffer;
                        DbgAssert(
                            0
                            && _T("Error reading saved dirty layer id in ")
                               _T("USDStageObjectclassDesc."));
                        return layerIdStrRes;
                    }
                    buffer[layerIdStrSize] = '\0';
                    layerIdStr = std::string(buffer);
                    delete buffer;
                    break;
                }
                case LAYER_EDITS_LAYER_DATA_CHUNK_ID: {
                    buffer = new char[layerStrSize + 1];
                    const auto layerStrRes = iLoad->Read(buffer, layerStrSize, &nb);
                    if (layerStrRes != IO_OK) {
                        delete buffer;
                        DbgAssert(
                            0
                            && _T("Error reading saved dirty layer data in ")
                               _T("USDStageObjectclassDesc."));
                        return layerStrRes;
                    }
                    buffer[layerStrSize] = '\0';
                    layerStr = std::string(buffer);
                    delete buffer;
                    break;
                }
                default: break;
                }
                iLoad->CloseChunk();
            }

            // Layer name used for anonymous layer created from disk
            const auto& layerName
                = "3dsmax_usd_dirty_layer_" + std::to_string(numDirtyLayer) + ".usda";
            // Create an anonymous layer to hold the data loaded from disk
            pxr::SdfLayerRefPtr dirtyLayerFromMaxScene = pxr::SdfLayer::CreateAnonymous(layerName);

            // Import the actual layer data loaded from the max file
            const bool layerImportRes = dirtyLayerFromMaxScene->ImportFromString(layerStr);

            // First check if the layer already exists in memory for whatever reason
            // (e.g. created via scripting)
            // Note that if it already exists with the same identifier, the one we load from the
            // max file takes priority always.
            pxr::SdfLayerRefPtr layerPtr = pxr::SdfLayer::Find(layerIdStr);
            if (layerPtr) {
                // Note: we do a content transfer here instead of just doing a
                // "ImportFromString()" call on the layer that we found from memory that has the
                // name id, because depending on if it was created with a ".usda" tag or not,
                // internally, "ImportFromString()" will behave differently. Doing a
                // "TransferContent" call will ensure that the data loaded from the max scene is
                // applied to the existing layer in memory that has the same identifier
                layerPtr->TransferContent(dirtyLayerFromMaxScene);
                dirtyLayerFromMaxScene = layerPtr;
                break;
            }

            // Set the identifier of the layer to the same identifier that we just loaded from disk
            dirtyLayerFromMaxScene->SetIdentifier(layerIdStr);

            // If there is an error, log it, but do not fail the entire max scene load.
            if (!layerImportRes) {
                const auto msg = _T("UsdStageObject load error. Unable to load the session layer ")
                                 _T("from the max file.");
                DbgAssert(0 && msg);
                GetCOREInterface()->Log()->LogEntry(SYSLOG_ERROR, NO_DIALOG, nullptr, msg);
            }

            // HACK: there are no APIs for setting a stage to the dirty state
            // so we abuse the "DeleteSpec()" function with dummy arguments
            // as it sets the layer to dirty in the first line of the function.
            // NOTE: ultimately, we do not need to explicitly load the layer
            // to the stage. The dirty state layer was already created the layer
            // in memory in the "Load()" function: when the stage is loaded
            // and composed, that layer will be loaded and associated to the stage
            // object via the identifier (i.e. filename) of the layer. We simply
            // need to mark it as dirty to return to the state of unsaved layer edits.
            dirtyLayerFromMaxScene->GetStateDelegate()->DeleteSpec(pxr::SdfPath(), false);

            USDLayerManager::Instance()->AddDirtyLayerFromMaxScene(dirtyLayerFromMaxScene);

            numDirtyLayer++;
        }
        default: break;
        }
        iLoad->CloseChunk();
    }
    return IO_OK;
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

        pxr::VtDictionary        options;
        static const std::string optionsCategoryKey = "PrimSelectionDialogPreferences";
        if (useUserSettings) {
            MaxUsd::OptionUtils::LoadUiOptions(optionsCategoryKey, options);
            if (!options[pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads].IsHolding<bool>()) {
                options[pxr::MaxUsdPrimSelectionDialogTokens->loadPayloads] = true;
            }
            if (!options[pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer].IsHolding<bool>()) {
                options[pxr::MaxUsdPrimSelectionDialogTokens->openInExplorer] = true;
            }
        } else {
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

    void OpenUsdLayerEditor() { MaxLayerEditor::Instance()->Open(); }

    void CloseUsdLayerEditor() { MaxLayerEditor::Instance()->Close(); }

    int GetDefaultSaveMode()
    {
        return static_cast<int>(USDLayerManager::Instance()->GetSaveMode());
    }

    void SetDefaultSaveMode(int saveMode)
    {
        USDLayerManager::Instance()->SetSaveMode(static_cast<SaveMode>(saveMode));
    }

    enum
    {
        fnIdSelectRootLayerAndPrim,
        fnIdOpenUsdExplorer,
        fnIdCloseUsdExplorer,
        fnIdOpenUsdLayerEditor,
        fnIdCloseUsdLayerEditor,
        fnIdGetDefaultSaveMode,
        fnIdSetDefaultSaveMode,
    };

    enum
    {
        eidFilteringType,
        eidSaveMode
    };

    // clang-format off
    BEGIN_FUNCTION_MAP
        FN_6(fnIdSelectRootLayerAndPrim, TYPE_VALUE, SelectRootLayerAndPrim, TYPE_STRING, TYPE_ENUM, TYPE_STRING_TAB, TYPE_BOOL, TYPE_BOOL, TYPE_BOOL);
        VFN_0(fnIdOpenUsdExplorer, OpenUsdExplorer);
        VFN_0(fnIdCloseUsdExplorer, CloseUsdExplorer);
        VFN_0(fnIdOpenUsdLayerEditor, OpenUsdLayerEditor);
        VFN_0(fnIdCloseUsdLayerEditor, CloseUsdLayerEditor);
        FN_0(fnIdGetDefaultSaveMode, TYPE_ENUM, GetDefaultSaveMode);
        VFN_1(fnIdSetDefaultSaveMode, SetDefaultSaveMode, TYPE_ENUM);
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
        UsdStageObjectStaticInterface::fnIdOpenUsdLayerEditor, _T("OpenUsdLayerEditor"), IDS_OPENUSDLAYEREDITOR, TYPE_VALUE, FP_NO_REDRAW, 0,
        UsdStageObjectStaticInterface::fnIdCloseUsdLayerEditor, _T("CloseUsdLayerEditor"), IDS_CLOSEUSDLAYEREDITOR, TYPE_VALUE, FP_NO_REDRAW, 0,
        UsdStageObjectStaticInterface::fnIdGetDefaultSaveMode, _T("GetDefaultSaveMode"), "Get the default save mode.", TYPE_ENUM, UsdStageObjectStaticInterface::eidSaveMode, FP_NO_REDRAW, 0,
        UsdStageObjectStaticInterface::fnIdSetDefaultSaveMode, _T("SetDefaultSaveMode"), "Set the default save mode.", TYPE_VOID, FP_NO_REDRAW, 1,
        _T("saveMode"), 0, TYPE_ENUM, UsdStageObjectStaticInterface::eidSaveMode,
        enums,
        UsdStageObjectStaticInterface::eidFilteringType, 3,
        _T("none"), MaxUsd::TreeModelFactory::TypeFilteringMode::NoFilter,
        _T("include"), MaxUsd::TreeModelFactory::TypeFilteringMode::Include,
        _T("exclude"), MaxUsd::TreeModelFactory::TypeFilteringMode::Exclude,
    	UsdStageObjectStaticInterface::eidSaveMode, 3,
	_T("saveAll"), SaveMode::SaveAll,
	_T("saveAllEditsMax"), SaveMode::SaveAllEditsMax,
	_T("save3dsMaxOnly"), SaveMode::Save3dsMaxOnly,
        p_end);
// clang-format on