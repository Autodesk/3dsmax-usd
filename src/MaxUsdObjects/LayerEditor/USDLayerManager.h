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
     * Adds a dirty layer to dirtyLayersFromMaxScene vector. Used to simply
     * keep references to SdfLayers created in memory.
     * @param dirtyLayer loaded from .max scene file
     */
    void AddDirtyLayerFromMaxScene(const pxr::SdfLayerRefPtr dirtyLayer);

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
     * Clears the dirtyLayersFromMaxScene vector. Note this is called after
     * NOTIFY_FILE_POST_OPEN notification is sent.
     */
    void ClearMaxSceneDirtyLayers();

    /// Storage of dirty layers found in the Max Scene file on load.
    /**
     * NOTE: This vector is used to drive the mechanism of loading layers from the .max scene on
     * disk; first the layers are read from the .max scene file, then they are created as anonymous
     * layers with the same identifiers as they had when they were saved, and finally,
     * when the USD stages associated with the .max scene file are created, upon their
     * creation, they will find SdfLayers that exist in memory with the same identifier
     * and use them instead of the ones associated with the root .usd layer of the stage.
     */
    std::vector<pxr::SdfLayerRefPtr> dirtyLayersFromMaxScene;
};
