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

#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <UsdLayerEditor/UfeCommandHook.h>
#include <UsdLayerEditor/sessionState.h>

#include <vector>

struct NotifyInfo;

PXR_NAMESPACE_USING_DIRECTIVE

/**
 * @brief Implements the SessionState virtual class for 3dsMax USD.
 * Allowing the editor to find USD stages, and to respond to 3dsMax scene events.
 */
class MaxSessionState : public UsdLayerEditor::SessionState

{
    Q_OBJECT
public:
    typedef SessionState PARENT_CLASS;

    MaxSessionState();
    ~MaxSessionState() override;

    // SessionState overrides.
    void                                 setStageEntry(StageEntry const& in_entry) override;
    void                                 setAutoHideSessionLayer(bool hide) override;
    UsdLayerEditor::AbstractCommandHook* commandHook() override;
    std::vector<StageEntry>              allStages() const override;
    std::vector<StageEntry>              selectedStages() const override;
    std::string                          defaultLoadPath() const override;
    std::vector<std::string>
         loadLayersUI(const QString& title, const std::string& default_path) const override;
    bool saveLayerUI(
        QWidget*                      in_parent,
        std::string*                  out_filePath,
        const PXR_NS::SdfLayerRefPtr& parentLayer) const override;
    void printLayer(const PXR_NS::SdfLayerRefPtr& layer) const override;
    void setupCreateMenu(QMenu* in_menu) override;
    void rootLayerPathChanged(const std::string& in_path) override;
    void refreshCurrentStageEntry() override;
    void refreshStageEntry(const std::string& dccObjectPath) override;

    /**
     * Respond to changes to the nodes present in the 3dsMax scene.
     * @param param Pointer to the session state.
     * @param info Scene node change info.
     */
    static void onSceneNodesChanged(void* param, NotifyInfo* info);

    /**
     * Respond to changes to the load state of a USD Stage object (for example :
     * a changed root layer, an empty stage being assigned a layer, etc.)
     * @param param Pointer to the session state.
     * @param info Unused
     */
    static void onStageLoadStateChanged(void* param, NotifyInfo* info);

    /**
     * Respond to changes to the 3dsMax scene selection.
     * @param param Pointer to the session state.
     * @param info Unused
     */
    static void onMaxSelectionChanged(void* param, NotifyInfo* info);

    /**
     * Creates a USD Layer stage entry for a given USDStageObject.
     * @param outStageEntry The output stage entry that was created.
     * @param object The stage object.
     * @return True if successful, false otherwise.
     */
    static bool getStageEntry(StageEntry* outStageEntry, USDStageObject* object);

protected:
    /**
     * Register notifications to 3dsMax scene events and executed layer editor commands.
     */
    void registerNotifications();

    /**
     * Unregister notifications to 3dsMax scene events and executed layer editor commands.
     */
    void unregisterNotifications();

    /**
     * A simple observer of executed Layer Editor UFE commands, when layer editor commands
     * are executed, we want to refresh the 3dsMax viewport.
     */
    class CommandObserver : public Ufe::Observer
    {
        void operator()(const Ufe::Notification& notification) override;
    };
    std::shared_ptr<CommandObserver> _commandObserver;

    // UFE Command hook - using UFE undoable commands to wrap the layer editor commands.
    UsdLayerEditor::UfeCommandHook _ufeCommandHook;
};