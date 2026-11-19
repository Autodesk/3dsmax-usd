//**************************************************************************
// Copyright (c) 2026 Autodesk, Inc.
// All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
//**************************************************************************/

#pragma once

#include <MaxUsd/Utilities/MaxSupportUtils.h>

#ifdef ADSK_RENDER_SETUP_ENABLED

#include <AdskUsdRenderSetup/HostStage.h>
#include <ISceneEventManager.h>
#include <QDialog>
#include <inode.h>
#include <vector>

struct NotifyInfo;

namespace AdskUsdRenderSetup {
class RenderSetupWidget;
}

/**
 * \class MaxRenderSetupDialog
 * \brief 3ds Max USD Render Setup dialog.
 *
 * Owns an Adsk::RenderSetupWidget directly and keeps its stage list in sync
 * with scene changes (node add/delete/rename). The dialog is destroyed when
 * closed so it does not retain stage references while hidden.
 */
class MaxRenderSetupDialog : public QDialog
{
public:
    explicit MaxRenderSetupDialog(QWidget* parent = nullptr);
    ~MaxRenderSetupDialog() override;

    void refreshStages();

private:
    void _applyStages();

    static bool _AllNodesInDeletedSet(const INodeTab& nodes, const INodeTab& deletedNodes);

    void _appendStage(INode* addedNode);
    void _removeStages(const INodeTab& deletedNodes);
    void _updateRenamedStages(INodeEventCallback::NodeKeyTab& nodes);

    class NodeRenameCallback : public INodeEventCallback
    {
    public:
        explicit NodeRenameCallback(MaxRenderSetupDialog* dlg)
            : m_dialog(dlg)
        {
        }
        void NameChanged(NodeKeyTab& nodes) override { m_dialog->_updateRenamedStages(nodes); }

    private:
        MaxRenderSetupDialog* m_dialog;
    };

    static void s_OnNodesBatchPreDelete(void* param, NotifyInfo* info);
    static void s_OnNodeAdded(void* param, NotifyInfo* info);
    static void s_OnStageLoadStateChanged(void* param, NotifyInfo* info);
    static void s_OnSceneReset(void* param, NotifyInfo* info);

    AdskUsdRenderSetup::RenderSetupWidget*     m_widget { nullptr };
    std::vector<AdskUsdRenderSetup::HostStage> m_hostStages;
    NodeRenameCallback                         m_nodeEventCallback;
    ISceneEventManager::CallbackKey            m_nodeEventCallbackKey { 0 };
};

#endif // ADSK_RENDER_SETUP_ENABLED
