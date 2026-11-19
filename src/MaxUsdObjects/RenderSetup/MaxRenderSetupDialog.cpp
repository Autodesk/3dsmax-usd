//**************************************************************************
// Copyright (c) 2026 Autodesk, Inc.
// All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
//**************************************************************************/

#include "MaxRenderSetupDialog.h"

#ifdef ADSK_RENDER_SETUP_ENABLED

#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>

#include <MaxUsd/Utilities/TranslationUtils.h>

#include <AdskUsdRenderSetup/RenderSetupWidget.h>
#include <ISceneEventManager.h>
#include <QAction>
#include <QMenuBar>
#include <QVBoxLayout>
#include <algorithm>
#include <inode.h>
#include <maxapi.h>
#include <notify.h>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

//! \p displayNode, when set, supplies the tree label; otherwise the first referencing node is used.
std::optional<AdskUsdRenderSetup::HostStage>
hostStageFrom(USDStageObject* stageObject, INode* displayNode = nullptr)
{
    const auto usdStage = stageObject->GetUSDStage();
    if (!usdStage) {
        return std::nullopt;
    }
    INode* node = displayNode ? displayNode : MaxUsd::GetFirstReferencingNode(stageObject);
    if (!node) {
        return std::nullopt;
    }
    AdskUsdRenderSetup::HostStage entry;
    entry.stage = usdStage;
    entry.displayName = MaxUsd::MaxStringToUsdString(node->GetName());
    entry.hostId = stageObject->GetGuid();
    return entry;
}

} // namespace

MaxRenderSetupDialog::MaxRenderSetupDialog(QWidget* parent)
    : QDialog(parent)
    , m_nodeEventCallback(this)
{
    setWindowTitle(tr("USD Render Setup"));
    resize(MaxSDK::UIScaled(720), MaxSDK::UIScaled(480));
    setAttribute(Qt::WA_DeleteOnClose, true);

    m_widget = new AdskUsdRenderSetup::RenderSetupWidget(this);

    auto* menuBar = new QMenuBar(this);
    auto* viewMenu = menuBar->addMenu(tr("View"));
    auto* hierarchyAction = viewMenu->addAction(tr("Display USD Hierarchy"));
    hierarchyAction->setCheckable(true);
    hierarchyAction->setChecked(false);
    connect(hierarchyAction, &QAction::toggled, [this](bool checked) {
        m_widget->setLayoutMode(
            checked ? AdskUsdRenderSetup::RenderTreeModel::LayoutMode::Hierarchy
                    : AdskUsdRenderSetup::RenderTreeModel::LayoutMode::Flat);
    });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(menuBar);
    layout->addWidget(m_widget);

    m_nodeEventCallbackKey = GetISceneEventManager()->RegisterCallback(&m_nodeEventCallback);

    RegisterNotification(
        &MaxRenderSetupDialog::s_OnNodesBatchPreDelete, this, NOTIFY_NODES_PRE_DELETE);
    RegisterNotification(&MaxRenderSetupDialog::s_OnNodeAdded, this, NOTIFY_SCENE_ADDED_NODE);
    RegisterNotification(
        &MaxRenderSetupDialog::s_OnStageLoadStateChanged, this, NOTIFY_STAGE_LOAD_STATE_CHANGED);
    RegisterNotification(&MaxRenderSetupDialog::s_OnSceneReset, this, NOTIFY_POST_SCENE_RESET);
}

MaxRenderSetupDialog::~MaxRenderSetupDialog()
{
    GetISceneEventManager()->UnRegisterCallback(m_nodeEventCallbackKey);
    UnRegisterNotification(
        &MaxRenderSetupDialog::s_OnNodesBatchPreDelete, this, NOTIFY_NODES_PRE_DELETE);
    UnRegisterNotification(&MaxRenderSetupDialog::s_OnNodeAdded, this, NOTIFY_SCENE_ADDED_NODE);
    UnRegisterNotification(
        &MaxRenderSetupDialog::s_OnStageLoadStateChanged, this, NOTIFY_STAGE_LOAD_STATE_CHANGED);
    UnRegisterNotification(&MaxRenderSetupDialog::s_OnSceneReset, this, NOTIFY_POST_SCENE_RESET);
    m_widget->setStages({});
}

void MaxRenderSetupDialog::refreshStages()
{
    m_hostStages.clear();

    std::unordered_set<std::string> seenHostIds;
    for (USDStageObject* stageObject : StageObjectMap::GetInstance()->GetAllStageObjects()) {
        if (!seenHostIds.insert(stageObject->GetGuid()).second) {
            continue;
        }
        if (auto entry = hostStageFrom(stageObject)) {
            m_hostStages.push_back(std::move(*entry));
        }
    }

    _applyStages();
}

void MaxRenderSetupDialog::_applyStages() { m_widget->setStages(m_hostStages); }

bool MaxRenderSetupDialog::_AllNodesInDeletedSet(
    const INodeTab& nodes,
    const INodeTab& deletedNodes)
{
    if (nodes.Count() == 0) {
        return false;
    }
    for (int i = 0; i < nodes.Count(); ++i) {
        bool found = false;
        for (int j = 0; j < deletedNodes.Count(); ++j) {
            if (nodes[i] == deletedNodes[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

void MaxRenderSetupDialog::_appendStage(INode* addedNode)
{
    if (!addedNode) {
        return;
    }
    auto* stageObj = dynamic_cast<USDStageObject*>(addedNode->GetObjectRef());
    if (!stageObj) {
        return;
    }
    const auto usdStage = stageObj->GetUSDStage();
    if (!usdStage) {
        return;
    }
    for (const auto& existing : m_hostStages) {
        if (existing.stage == usdStage) {
            return;
        }
    }
    if (auto entry = hostStageFrom(stageObj, addedNode)) {
        m_hostStages.push_back(std::move(*entry));
        _applyStages();
    }
}

void MaxRenderSetupDialog::_removeStages(const INodeTab& deletedNodes)
{
    std::unordered_set<std::string> toRemoveHostIds;
    for (int i = 0; i < deletedNodes.Count(); ++i) {
        INode* node = deletedNodes[i];
        if (!node) {
            continue;
        }
        auto* stageObj = dynamic_cast<USDStageObject*>(node->GetObjectRef());
        if (!stageObj) {
            continue;
        }
        if (!_AllNodesInDeletedSet(MaxUsd::GetReferencingNodes(stageObj), deletedNodes)) {
            continue;
        }
        toRemoveHostIds.insert(stageObj->GetGuid());
    }

    if (toRemoveHostIds.empty()) {
        return;
    }

    m_hostStages.erase(
        std::remove_if(
            m_hostStages.begin(),
            m_hostStages.end(),
            [&](const AdskUsdRenderSetup::HostStage& entry) {
                return toRemoveHostIds.count(entry.hostId) != 0;
            }),
        m_hostStages.end());

    _applyStages();
}

void MaxRenderSetupDialog::_updateRenamedStages(INodeEventCallback::NodeKeyTab& nodes)
{
    bool changed = false;
    for (int i = 0; i < nodes.Count(); ++i) {
        INode* node = NodeEventNamespace::GetNodeByKey(nodes[i]);
        if (!node) {
            continue;
        }
        auto* stageObj = dynamic_cast<USDStageObject*>(node->GetObjectRef());
        if (!stageObj) {
            continue;
        }
        const auto usdStage = stageObj->GetUSDStage();
        if (!usdStage) {
            continue;
        }
        for (auto& entry : m_hostStages) {
            if (entry.stage == usdStage) {
                entry.displayName = MaxUsd::MaxStringToUsdString(node->GetName());
                changed = true;
                break;
            }
        }
    }
    if (changed) {
        _applyStages();
    }
}

void MaxRenderSetupDialog::s_OnNodesBatchPreDelete(void* param, NotifyInfo* info)
{
    if (!info->callParam) {
        return;
    }
    auto*           dialog = static_cast<MaxRenderSetupDialog*>(param);
    const INodeTab* deletedNodes = GetNotifyParam<NOTIFY_NODES_PRE_DELETE>(info);
    dialog->_removeStages(*deletedNodes);
}

void MaxRenderSetupDialog::s_OnNodeAdded(void* param, NotifyInfo* info)
{
    if (!info->callParam) {
        return;
    }
    auto*  dialog = static_cast<MaxRenderSetupDialog*>(param);
    INode* addedNode = GetNotifyParam<NOTIFY_SCENE_ADDED_NODE>(info);
    dialog->_appendStage(addedNode);
}

void MaxRenderSetupDialog::s_OnStageLoadStateChanged(void* param, NotifyInfo* /*info*/)
{
    static_cast<MaxRenderSetupDialog*>(param)->refreshStages();
}

void MaxRenderSetupDialog::s_OnSceneReset(void* param, NotifyInfo* /*info*/)
{
    static_cast<MaxRenderSetupDialog*>(param)->refreshStages();
}

#endif // ADSK_RENDER_SETUP_ENABLED
