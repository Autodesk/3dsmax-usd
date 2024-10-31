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

#include "MaxSessionState.h"

#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>
#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/ListenerUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/UiUtils.h>

#include <UsdLayerEditor/saveLayersDialog.h>
#include <UsdLayerEditor/stringResources.h>
#include <UsdLayerEditor/tokens.h>
#include <UsdLayerEditor/utilOptions.h>
#include <UsdLayerEditor/utilString.h>

#include <maxscript/maxscript.h>
#include <ufe/observableSelection.h>
#include <ufe/pathString.h>

#include <Max.h>
#include <QtCore/QTimer>
#include <QtWidgets/QMenu>
#include <notify.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace UsdLayerEditor;

MaxSessionState::MaxSessionState()
    : _ufeCommandHook(this)
{
    _commandObserver = std::make_shared<CommandObserver>();
    registerNotifications();
}

MaxSessionState::~MaxSessionState()
{
    try {
        unregisterNotifications();
    } catch (const std::exception&) {
        // Ignore errors in destructor.
    }
}

void MaxSessionState::setStageEntry(const StageEntry& inEntry)
{
    PARENT_CLASS::setStageEntry(inEntry);
    if (!inEntry._stage) {
        _currentStageEntry.clear();
    }
}

bool MaxSessionState::getStageEntry(StageEntry* entry, USDStageObject* object)
{
    if (!object) {
        return false;
    }
    const auto stage = object->GetUSDStage();
    if (!stage) {
        return false;
    }

    const auto        layerNameWithExt = stage->GetRootLayer()->GetDisplayName();
    const size_t      lastIndex = layerNameWithExt.find_last_of(".");
    const std::string layerName = layerNameWithExt.substr(0, lastIndex);

    entry->_stage = stage;
    entry->_displayName = layerName;
    entry->_id = object->GetGuid();
    const auto path = MaxUsd::ufe::getUsdStageObjectPath(object).string();
    entry->_dccObjectPath = path;

    return true;
}

std::vector<SessionState::StageEntry> MaxSessionState::allStages() const
{
    std::vector<StageEntry> stages;
    for (const auto& stageObject : StageObjectMap::GetInstance()->GetAllStageObjects()) {

        if (!stageObject->IsInCreateMode()) {
            const auto nodes = MaxUsd::GetReferencingNodes(stageObject);
            if (nodes.Count() == 0) {
                continue;
            }
        }

        StageEntry entry;
        if (!getStageEntry(&entry, stageObject)) {
            continue;
        }
        stages.push_back(entry);
    }

    return stages;
}

std::vector<SessionState::StageEntry> MaxSessionState::selectedStages() const
{
    std::vector<StageEntry> stages;
    for (const auto& stageObject : StageObjectMap::GetInstance()->GetAllStageObjects()) {

        const auto nodes = MaxUsd::GetReferencingNodes(stageObject);
        for (const auto& node : nodes) {

            StageEntry entry;
            if (node->Selected() && getStageEntry(&entry, stageObject)) {
                stages.push_back(entry);
                break;
            }
        }
    }
    return stages;
}

AbstractCommandHook* MaxSessionState::commandHook() { return &_ufeCommandHook; }

void MaxSessionState::registerNotifications()
{
    RegisterNotification(onSceneNodesChanged, this, NOTIFY_SCENE_ADDED_NODE);
    RegisterNotification(onSceneNodesChanged, this, NOTIFY_SCENE_PRE_DELETED_NODE);
    RegisterNotification(onStageLoadStateChanged, this, NOTIFY_STAGE_LOAD_STATE_CHANGED);
    RegisterNotification(onMaxSelectionChanged, this, NOTIFY_SELECTIONSET_CHANGED);
    _ufeCommandHook.addObserver(_commandObserver);
}

void MaxSessionState::unregisterNotifications()
{
    UnRegisterNotification(onSceneNodesChanged, this, NOTIFY_SCENE_ADDED_NODE);
    UnRegisterNotification(onSceneNodesChanged, this, NOTIFY_SCENE_PRE_DELETED_NODE);
    UnRegisterNotification(onStageLoadStateChanged, this, NOTIFY_STAGE_LOAD_STATE_CHANGED);
    UnRegisterNotification(onMaxSelectionChanged, this, NOTIFY_SELECTIONSET_CHANGED);
    _ufeCommandHook.removeObserver(_commandObserver);
}

void MaxSessionState::CommandObserver::operator()(const Ufe::Notification& notification)
{
    if (const auto ce = dynamic_cast<const UfeCommandHook::CommandExecuted*>(&notification)) {
        GetCOREInterface()->RedrawViews(GetCOREInterface()->GetTime());
    }
}

void MaxSessionState::refreshCurrentStageEntry()
{
    refreshStageEntry(_currentStageEntry._dccObjectPath);
}

void MaxSessionState::refreshStageEntry(const std::string& dccObjectPath)
{
    // TODO LE-EXTRACT : Not used for now - will be important when we save anonymous layers
    // to disk. In that case the layer model is updated.
    StageEntry entry;
    const auto ufePath = Ufe::PathString::path(dccObjectPath);
    const auto object = StageObjectMap::GetInstance()->Get(ufePath);

    if (getStageEntry(&entry, object)) {
        if (entry._dccObjectPath == _currentStageEntry._dccObjectPath) {
            QTimer::singleShot(0, this, [this, entry]() { setStageEntry(entry); });
        }
    }
}

void MaxSessionState::onStageLoadStateChanged(void* param, NotifyInfo* /*info*/)
{
    const auto session = static_cast<MaxSessionState*>(param);
    QTimer::singleShot(0, [session]() { session->stageListChangedSignal(); });
}

void MaxSessionState::onMaxSelectionChanged(void* param, NotifyInfo* /*info*/)
{
    const auto session = static_cast<MaxSessionState*>(param);
    QTimer::singleShot(0, [session]() { session->dccSelectionChangedSignal(); });
}

void MaxSessionState::onSceneNodesChanged(void* param, NotifyInfo* info)
{
    if (!info->callParam) {
        return;
    }

    const auto session = static_cast<MaxSessionState*>(param);
    const auto addedNode = static_cast<INode*>(info->callParam);

    if (addedNode->GetObjectRef()->ClassID() == USDSTAGEOBJECT_CLASS_ID) {
        QTimer::singleShot(0, [session]() { session->stageListChangedSignal(); });
    }
}

bool MaxSessionState::saveLayerUI(
    QWidget*                      in_parent,
    std::string*                  out_filePath,
    const PXR_NS::SdfLayerRefPtr& parentLayer) const
{
    // TODO LE-EXTRACT Save Layers UI.
    return false;
}

std::vector<std::string>
MaxSessionState::loadLayersUI(const QString& in_title, const std::string& in_default_path) const
{
    // TODO LE-EXTRACT Setup load layers UI.
    return {};
}

void MaxSessionState::setupCreateMenu(QMenu* in_menu)
{
    auto action = in_menu->addAction(QObject::tr("USD Stage..."));
    QObject::connect(action, &QAction::triggered, [this]() {
        ExecuteMAXScriptScript(
            L"macros.run \"USD\" \"CreateUSDStage\"", MAXScript::ScriptSource::Embedded);
    });
}

// path to default load layer dialogs to
std::string MaxSessionState::defaultLoadPath() const
{
    // TODO LE-EXTRACT Load layer default path.
    return {};
}

// called when an anonymous root layer has been saved to a file
// in this case, the stage needs to be re-created on the new file
void MaxSessionState::rootLayerPathChanged(std::string const& in_path)
{
    // TODO LE-EXTRACT Save anonymous layers.
}

void MaxSessionState::setAutoHideSessionLayer(bool hideIt)
{
    int value = hideIt ? 1 : 0;
    UsdLayerEditor::Options::setOptionVarValue(
        UsdLayerEditorOptionVars->AutoHideSessionLayer, value);
    PARENT_CLASS::setAutoHideSessionLayer(hideIt);
}

void MaxSessionState::printLayer(const PXR_NS::SdfLayerRefPtr& layer) const
{
    std::string result = UsdLayerEditor::String::format(
        StringResources::kUsdLayerIdentifier.value, layer->GetIdentifier().c_str());

    result += "\n";
    if (layer->GetRealPath() != layer->GetIdentifier()) {

        result += UsdLayerEditor::String::format(
            StringResources::kRealPath.value, layer->GetRealPath().c_str());
        result += "\n";
    }
    std::string text;
    layer->ExportToString(&text);
    result += text;

    const int maxLineCount = 400;
    const int maxCharCount = 50000;

    auto lineCount = std::count(result.begin(), result.end(), '\n');
    lineCount += !result.empty() && result.back() != '\n';

    // Give users an opportunity to bail on large files.
    if (lineCount > maxLineCount || result.size() > maxCharCount) {
        std::wstring displayName
            = MaxUsd::UsdStringToMaxString(layer->GetDisplayName().c_str()).data();
        const auto caption = std::wstring(L"Print Processing Time Warning");
        const auto text = displayName + L" contains " + std::to_wstring(lineCount) + L" lines and "
            + std::to_wstring(result.size())
            + L" characters. Printing a large layer may take a considerable amount of time. Do you "
              L"want to proceed?";
        if (!MaxUsd::Ui::AskYesNoQuestion(text, caption)) {
            return;
        }
    }

    const auto maxStr = MaxUsd::UsdStringToMaxString(result);
    MaxUsd::Listener::Write(maxStr);
}