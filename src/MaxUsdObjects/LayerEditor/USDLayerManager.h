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
#include <MaxUsdObjects/Views/SaveUSDOptionsDialog.h>

struct NotifyInfo;
class USDStageObject;

enum class SaveMode
{
    SaveAll,
    SaveAllEditsMax,
    Save3dsMaxOnly
};

/**
 * Singleton to manage and save USD Layers.
 */
class USDLayerManager
{
public:
    static USDLayerManager* Instance();

    ~USDLayerManager();

    /**
     * Returns a map of UsdStageObjects and associated dirty layers. If a UsdStageObject
     * has no dirty layers, or is not currently referenced by a node in the scene, it will not be
     * included in the map.
     * @return The map of stage objects to dirty layers.
     */
    std::unordered_map<USDStageObject*, std::vector<pxr::SdfLayerHandle>> GetDirtyLayersToSave();

    /**
     * Handles the 3dsMax scene saving. Typically prompting the user for input
     * on what do be done with any dirty USD layers. If called multiple times during
     * a 3dsMax scene save operation, subsequent calls will be no-ops.
     * @return True if successful. False if the save operation should be interrupted.
     */
    bool HandleMaxSceneSave();

    /**
     * Returns the Save Mode for the Layer Manager.
     * @return The save mode
     */
    SaveMode GetSaveMode();

    /**
     * Sets the Save Mode for the Layer Manager
     * @param saveMode The save mode to set
     */
    void SetSaveMode(SaveMode saveMode);

    // Delete the copy/move constructors assignment operators.
    USDLayerManager(const USDLayerManager&) = delete;
    USDLayerManager& operator=(const USDLayerManager&) = delete;
    USDLayerManager(USDLayerManager&&) = delete;
    USDLayerManager& operator=(USDLayerManager&&) = delete;

    /**
     * Get the map of identifiers -> SdfLayer of loaded layers
     * from the .max scene.
     * @return The map of loaded sdf layers
     */
    const std::unordered_map<std::string, pxr::SdfLayerRefPtr>& GetLoadedLayerMap();

    /**
     * Add a mapping into the loadedLayerMap map.
     * @param oldId the Id as it was stored into the .max scene on save
     * @param newLayer the newly created layer based on the .usda string.
     */
    void AddLoadedLayerMapping(std::string& oldId, pxr::SdfLayerRefPtr& newLayer);

private:
    USDLayerManager();

    static void NotifyFileSave(void* param, NotifyInfo* info);

    static std::unique_ptr<USDLayerManager> instance;

    // Flag keeping track of whether we need to do anything on
    // calls to HandleMaxSceneSave(). Initialized to true when a
    // a scene save operation begins.
    bool mustHandleSave = false;
    // Flag indicating whether the current save operation is an auto-save.
    // There is no direct way to know from the Max API - we always assume auto-save,
    // unless we received NOTIFY_FILE_CHECK_STATUS which only regular saves send out.
    bool isAutoSave = true;

    // The last used save mode
    SaveMode saveMode;

    /**
     * Clears the loadedLayerMap map. Note this is called after
     * NOTIFY_FILE_POST_OPEN notification is sent.
     */
    void ClearLoadedLayersMap();

    // Holds the mapping of layer identifiers loaded from the .max scene
    // onto the SdfLayer objects created as a result of ImportFromString calls.
    // This is needed (1) to recreate the mapping of layers because anon layer
    // identifiers cannot be set, so we recreate the same layer hierarchy based
    // on the data in this structure.
    // (2) Keep pointers to the layers of file-backed layers so that when
    // UsdStageObject that has a file-backed root layer loaded, it will
    // find the SdfLayer associated with the file's identifier and use it.
    std::unordered_map<std::string, pxr::SdfLayerRefPtr> loadedLayerMap;
};
