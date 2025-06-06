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

#include "MaxUsd/MaxTokens.h"

#include <MaxUsdObjects/Objects/CreateCallbacks/CreateAtPosition.h>
#include <MaxUsdObjects/Objects/USDGeomObject.h>
#include <MaxUsdObjects/Objects/USDStageObject.h>
#include <MaxUsdObjects/Views/UsdGeomObjectIncludesRollup.h>
#include <MaxUsdObjects/Views/UsdGeomObjectParametersRollup.h>

#include <MaxUsd/Utilities/PluginUtils.h>

#include <maxscript/mxsplugin/mxsplugin.h>

#include <Geom/triangulate.h>
#include <IRefTargWrappingRefTarg.h>
#include <algorithm>
#include <iInstanceMgr.h>
#include <notify.h>

// clang-format off
ParamBlockDesc2 usdGeomParamblockDesc(PBLOCK_REF, 
	_M("USDGeomObject"),
	0,
	GetUSDGeomObjectDesc(), 
	P_AUTO_CONSTRUCT | P_AUTO_UI_QT | P_MULTIMAP,
	PBLOCK_REF,
	2,
	USDGeomObjectMapID_General,
        USDGeomObjectMapID_Includes,
	// Parameters
	USDGeomObjectParams_USDStage, _M("USDStage"), TYPE_INODE, 0, 0, p_end,
	USDGeomObjectParams_PrimPath,_M("PrimPath"), TYPE_STRING, 0, 0,p_default, L"", p_end,
        USDGeomObjectParams_ShowSource,_M("ShowSource"), TYPE_BOOL, 0, 0,p_default, FALSE, p_end,
        USDGeomObjectParams_LiveUpdates,_M("LiveUpdates"), TYPE_BOOL, 0, 0,p_default, FALSE, p_end,
        USDGeomObjectParams_IncludeInvisible,_M("IncludeInvisible"), TYPE_BOOL, 0, 0,p_default, FALSE, p_end,
        USDGeomObjectParams_ViewportDisplayPurposes,_M("ViewportDisplayPurposes"), TYPE_BOOL, 0, 0,p_default, TRUE, p_end,
        USDGeomObjectParams_IncludeProxy,_M("IncludeProxy"), TYPE_BOOL, 0, 0,p_default, TRUE, p_end,
        USDGeomObjectParams_IncludeRender,_M("IncludeRender"), TYPE_BOOL, 0, 0,p_default, FALSE, p_end,
        USDGeomObjectParams_IncludeGuide,_M("IncludeGuide"), TYPE_BOOL, 0, 0,p_default, FALSE, p_end,
	p_end);

#define IUSDGeomObject_ID Interface_ID(0x11ea24a7, 0x75830365)
static FPInterfaceDesc usdGeomObjectInterface(
	IUSDGeomObject_ID, _T("usdGeomObjectOps"), 0, GetUSDGeomObjectDesc(), FP_MIXIN,
	fnIdUSDGeomObjectRefresh, _T("Refresh"), "Refresh the USDGeomObject.", TYPE_VOID, 0, 0,
	p_end);
// clang-format on

USDGeomObject::USDGeomObject()
{
    GetUSDGeomObjectDesc()->MakeAutoParamBlocks(this);

    RegisterNotification(NotifyNodeAdded, this, NOTIFY_SCENE_ADDED_NODE);
    RegisterNotification(NotifyNodeDeleted, this, NOTIFY_SCENE_PRE_DELETED_NODE);
    // Register ourselves as a listener for USD stage change notifications. If source
    // prims change, we want to want to update the validity of the object.
    pxr::TfWeakPtr<USDGeomObject> me(this);
    onStageChangeNotice = pxr::TfNotice::Register(me, &USDGeomObject::OnStageChange);
    usdMesh = std::make_unique<HdMaxTriMesh>(&mesh);
    creating = true; // True until added to the scene / loaded from disk.
    guid = MaxUsd::GenerateGUID();
}

USDGeomObject::~USDGeomObject()
{
    UnRegisterNotification(NotifyNodeAdded, this, NOTIFY_SCENE_ADDED_NODE);
    UnRegisterNotification(NotifyNodeDeleted, this, NOTIFY_SCENE_PRE_DELETED_NODE);
    pxr::TfNotice::Revoke(onStageChangeNotice);
}

FPInterfaceDesc* USDGeomObject::GetDesc() { return &usdGeomObjectInterface; }

BaseInterface* USDGeomObject::GetInterface(Interface_ID id)
{
    if (id == IUSDGeomObject_ID) {
        return this;
    }
    return Object::GetInterface(id);
}

void USDGeomObject::OnStageChange(pxr::UsdNotice::ObjectsChanged const& notice)
{
    if (!MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_LiveUpdates)) {
        return;
    }

    const auto prim = GetPrim();
    if (!prim.IsValid()) {
        return;
    }

    // Any change happening at or below the prim, will flag the object invalid.
    auto changed = [&prim](const pxr::UsdNotice::ObjectsChanged::PathRange& paths) {
        for (const auto& path : paths) {
            if (path.HasPrefix(prim.GetPath())) {
                return true;
            }
        }
        return false;
    };

    if (notice.AffectedObject(prim) || changed(notice.GetChangedInfoOnlyPaths())
        || changed(notice.GetResyncedPaths())) {
        ivalid = NEVER;
    }
}

void USDGeomObject::SetReference(int i, RefTargetHandle rtarg)
{
    paramBlock = dynamic_cast<IParamBlock2*>(rtarg);
}

int USDGeomObject::NumRefs() { return 1; }

ReferenceTarget* USDGeomObject::GetReference(int i) { return (i == 0) ? paramBlock : nullptr; }

int USDGeomObject::NumParamBlocks() { return 1; }

IParamBlock2* USDGeomObject::GetParamBlock(int i) { return (i == 0) ? paramBlock : nullptr; }

IParamBlock2* USDGeomObject::GetParamBlockByID(BlockID id)
{
    if (paramBlock && paramBlock->ID() == id) {
        return paramBlock;
    }
    return nullptr;
}

int USDGeomObject::NumSubs() { return 1; }

Animatable* USDGeomObject::SubAnim(int i) { return paramBlock; }

TSTR USDGeomObject::SubAnimName(int i, bool localized) { return _T("Parameters"); }

int USDGeomObject::SubNumToRefNum(int subNum)
{
    if (subNum == 0) {
        return subNum;
    }
    return -1;
}

INode* USDGeomObject::GetStageNode()
{
    INode*     node = nullptr;
    Interval   valid = FOREVER;
    const auto paramBlock = this->GetParamBlock(0);
    if (!paramBlock) {
        return nullptr;
    }
    paramBlock->GetValue(USDGeomObjectParams_USDStage, GetCOREInterface()->GetTime(), node, valid);
    return node;
}

USDStageObject* USDGeomObject::GetStageObject()
{
    const auto node = GetStageNode();
    if (!node) {
        return nullptr;
    }

    return dynamic_cast<USDStageObject*>(node->GetObjectRef());
}

pxr::UsdStagePtr USDGeomObject::GetUSDStage()
{
    const auto stageObject = GetStageObject();
    if (!stageObject) {
        return nullptr;
    }
    return stageObject->GetUSDStage();
}

pxr::UsdPrim USDGeomObject::GetPrim()
{
    const auto stage = GetUSDStage();
    if (!stage) {
        return {};
    }
    return stage->GetPrimAtPath(GetPrimPath());
}

pxr::SdfPath USDGeomObject::GetPrimPath() const
{
    const MCHAR* pathStr = nullptr;
    Interval     valid = FOREVER;
    paramBlock->GetValue(
        USDGeomObjectParams_PrimPath, GetCOREInterface()->GetTime(), pathStr, valid);
    const auto pathStdStr = MaxUsd::MaxStringToUsdString(pathStr);
    if (pathStdStr.empty()) {
        return {};
    }
    return pxr::SdfPath { pathStdStr };
}

pxr::TfTokenVector USDGeomObject::GetIncludedPurposes() const
{
    pxr::TfTokenVector purposes;
    if (MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_IncludeProxy)) {
        purposes.push_back(pxr::HdRenderTagTokens->proxy);
    }
    if (MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_IncludeGuide)) {
        purposes.push_back(pxr::HdRenderTagTokens->guide);
    }
    if (MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_IncludeRender)) {
        purposes.push_back(pxr::HdRenderTagTokens->render);
    }

    return purposes;
}

bool USDGeomObject::GetIncludeInvisible() const
{
    return MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_IncludeInvisible);
}

void USDGeomObject::Refresh()
{
    // Invalidate the mesh and force a full translation in hydra.
    ivalid = NEVER;
    DirtySourceRPrims();
    Interval valid = FOREVER;
    this->ForceNotify(valid);
    GetCOREInterface()->RedrawViews(GetCOREInterface()->GetTime());
}

void USDGeomObject::DirtySourceRPrims()
{
    const auto stageObject = GetStageObject();
    if (!stageObject) {
        return;
    }
    const auto engine = stageObject->GetHydraEngine();
    if (!engine) {
        return;
    }
    const auto path = GetPrimPath();
    auto&      changeTracker = engine->GetChangeTracker();
    for (auto& rd : engine->GetRenderDelegate()->GetAllMeshRenderData()) {
        if (rd.rPrimPath.HasPrefix(path)) {
            // If the RPrim is not yet fully initialized in the render index, marking things
            // dirty can lead to crashes in USD 24.11. This seems to be a defect in
            // recent USD versions. We can sometimes avoid this situation by detecting all initial
            // dirty bits being set.
            const auto dirty = changeTracker.GetRprimDirtyBits(rd.rPrimPath);
            const auto initial = pxr::HdMaxMesh::GetInitialDirtyBits();
            if ((dirty & initial) == initial) {
                continue;
            }
            // Sometimes, we can't avoid it, catch the issue and move on...
            try {
                changeTracker.MarkRprimDirty(rd.rPrimPath);
            } catch (...) {
                // UsdExpiredPrimAccessError exception thrown (from pxr/usd/usd/errors.h)
            }
        }
    }
}

std::string USDGeomObject::GetGuid() { return guid; }

void USDGeomObject::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
    SimpleObject2::BeginEditParams(ip, flags, prev);
    GetUSDGeomObjectDesc()->BeginEditParams(ip, this, flags, prev);
}

void USDGeomObject::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
    SimpleObject2::EndEditParams(ip, flags, next);
    GetUSDGeomObjectDesc()->EndEditParams(ip, this, flags, next);
}

void USDGeomObject::UpdateGeomSourcePurpose()
{
    if (auto stageObject = GetStageObject()) {
        auto showSource
            = MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_ShowSource);
        if (showSource) {
            stageObject->UnRegisterGeomObjectSource(guid);
        } else {
            USDStageObject::GeomObjectSource source;
            source.path = GetPrimPath();
            const auto useVpDisplay = MaxUsd::GetParamBlockValue<BOOL>(
                paramBlock, USDGeomObjectParams_ViewportDisplayPurposes);
            source.useCustomRenderTags = !useVpDisplay;
            if (source.useCustomRenderTags) {
                source.customRenderTags = GetIncludedPurposes();
            }
            stageObject->RegisterGeomObjectSource(guid, source);
        }
    }
}

RefResult USDGeomObject::NotifyRefChanged(
    const Interval& changeInt,
    RefTargetHandle hTarget,
    PartID&         partID,
    RefMessage      message,
    BOOL            propagate)
{
    const auto redraw = [this]() {
        Interval valid = FOREVER;
        this->ForceNotify(valid);
        GetCOREInterface()->RedrawViews(GetCOREInterface()->GetTime());
    };

    switch (message) {
    case REFMSG_CHANGE:
        if (hTarget == paramBlock) {
            ParamID changingParam = paramBlock->LastNotifyParamID();
            switch (changingParam) {
            case USDGeomObjectParams_USDStage: {
                const auto stageNode = GetStageNode();
                // Stage node deleted? Invalidate the mesh - Will now be empty.
                if (!stageNode) {
                    ivalid = NEVER;
                    currStageNode = nullptr;
                    redraw();
                    break;
                }
                // Stage node changed? Need to update everything.
                // If we are currently creating the object, no need to update,
                // it will be done part of initialization already.
                if (!creating && stageNode != currStageNode) {
                    currStageNode = stageNode;
                    ivalid = NEVER;
                    UpdateGeomSourcePurpose();
                    DirtySourceRPrims();
                    redraw();
                    break;
                }
                // First initialization of currStageNode.
                if (!currStageNode) {
                    currStageNode = stageNode;
                }
                break;
            }
            case USDGeomObjectParams_PrimPath: {
                ivalid = NEVER;
                UpdateGeomSourcePurpose();
                DirtySourceRPrims();
                redraw();
                break;
            }
            case USDGeomObjectParams_LiveUpdates: {
                const auto liveUpdates
                    = MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_LiveUpdates);

                // If not using live updates, object stays valid forever.
                ivalid = liveUpdates ? NEVER : FOREVER;

                // Force a full translation in hydra on toggling on.
                if (liveUpdates) {
                    DirtySourceRPrims();
                }

                // Tell the UI to refresh if it is active.
                if (const auto parametersMap = paramBlock->GetMap(USDGeomObjectMapID_General)) {
                    parametersMap->UpdateUI(GetCOREInterface()->GetTime());
                }

                redraw();
                break;
            }
            case USDGeomObjectParams_ViewportDisplayPurposes:
            case USDGeomObjectParams_IncludeProxy:
            case USDGeomObjectParams_IncludeGuide:
            case USDGeomObjectParams_IncludeRender: {
                ivalid = NEVER;
                if (const auto parametersMap = paramBlock->GetMap(USDGeomObjectMapID_Includes)) {
                    parametersMap->UpdateUI(GetCOREInterface()->GetTime());
                }
                UpdateGeomSourcePurpose();
                DirtySourceRPrims();
                redraw();
                break;
            }
            case USDGeomObjectParams_ShowSource: {
                if (const auto parametersMap = paramBlock->GetMap(USDGeomObjectMapID_General)) {
                    parametersMap->UpdateUI(GetCOREInterface()->GetTime());
                }
                UpdateGeomSourcePurpose();
                DirtySourceRPrims();
                redraw();
                break;
            }
            case USDGeomObjectParams_IncludeInvisible: {
                ivalid = NEVER;
                redraw();
                break;
            }
            }
        }
        break;
    }
    return REF_SUCCEED;
}

void USDGeomObject::NotifyNodeAdded(void* param, NotifyInfo* info)
{
    if (!info->callParam) {
        return;
    }
    const auto geomObject = static_cast<USDGeomObject*>(param);
    if (!geomObject) {
        return;
    }

    const auto addedNode = static_cast<INode*>(info->callParam);
    if (addedNode->GetObjectRef()->FindBaseObject() != geomObject) {
        return;
    }
    geomObject->creating = false;
    // Dirty the prim and its descendants - to make sure hd mesh update is hit when building
    // the promoted geometry.
    geomObject->DirtySourceRPrims();
    // Update USD render purposes to avoid doubling up the display of the source geometry.
    geomObject->UpdateGeomSourcePurpose();
}

void USDGeomObject::NotifyNodeDeleted(void* param, NotifyInfo* info)
{
    if (!info->callParam) {
        return;
    }
    const auto geomObject = static_cast<USDGeomObject*>(param);
    if (!geomObject) {
        return;
    }

    const auto deletedNode = static_cast<INode*>(info->callParam);
    if (deletedNode->GetObjectRef()->FindBaseObject() != geomObject) {
        return;
    }

    // If we are in the process of deleting the last node referencing this
    // geom object, unhide the source geometry, if necessary.
    INodeTab nodes;
    IInstanceMgr::GetInstanceMgr()->GetInstances(*deletedNode, nodes);
    if (nodes.Count() > 1) {
        return;
    }

    auto stageObject = geomObject->GetStageObject();
    if (!stageObject) {
        return;
    }

    // If source was hidden, show it.
    const auto paramBlock = geomObject->GetParamBlock(0);
    if (!MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_ShowSource)) {
        stageObject->UnRegisterGeomObjectSource(geomObject->guid);
    }
}

void USDGeomObject::BuildMesh(TimeValue t)
{
    if (creating) {
        return;
    }
    const auto liveUpdates
        = MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_LiveUpdates);

    if (ivalid != NEVER && !liveUpdates) {
        return;
    }

    auto newVal = Interval(t, t);
    if (ivalid == newVal) {
        return;
    }

    if (liveUpdates) {
        ivalid = newVal;
    } else {
        ivalid = FOREVER;
    }

    const auto stageObject = GetStageObject();
    if (!stageObject) {
        usdMesh->Reset();
        return;
    }

    const auto prim = GetPrim();
    if (!prim.IsValid()) {
        usdMesh->Reset();
        return;
    }

    const auto includeInvisible
        = MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_IncludeInvisible);

    stageObject->BuildPrimTriMesh(prim.GetPath(), t, *usdMesh, includeInvisible, true);
}

class ObjectDetachRestore
    : public RestoreObj
    , SingleRefMaker
{
public:
    ObjectDetachRestore(USDGeomObject* geom)
        : geomObject(geom)
    {
        SetRef(geom);
        this->SetAutoDropRefOnShutdown(AutoDropRefOnShutdown::PrePluginShutdown);
    }

    void Restore(int isUndo) override
    {
        // Dirty the prim and its descendants - to make sure hd mesh update is hit when building
        // the promoted geometry.
        geomObject->DirtySourceRPrims();
        // Update USD render purposes to avoid doubling up the display of the source geometry.
        geomObject->UpdateGeomSourcePurpose();
    }
    void Redo() override
    {
        geomObject->GetStageObject()->UnRegisterGeomObjectSource(geomObject->GetGuid());
    }

    int  Size() override { return sizeof(geomObject); }
    TSTR Description() override { return TSTR(_T("USDGeomObject detatch.")); }

private:
    USDGeomObject* geomObject = nullptr;
};

void USDGeomObject::NotifyTarget(int message, ReferenceMaker* hMaker)
{
    INode* node = dynamic_cast<INode*>(hMaker);
    auto   geomObject = this;

    switch (message) {
    // When a USDGeomObject is fully converted, for example to a poly or mesh, we should
    // no longer hide the geometry in the source Stage Object. We can detect this via
    // the "detach" message, that is, the object is detatched from the node, as it is
    // being replaced by a TriObject, for example.
    case TARGETMSG_DETACHING_NODE: {
        // If there is more than one instance, no need to react, only when detaching from the
        // last instance do we need to restart display in the USD Stage.
        INodeTab nodes;
        IInstanceMgr::GetInstanceMgr()->GetInstances(*node, nodes);
        if (nodes.Count() > 1) {
            return;
        }
        auto stageObject = geomObject->GetStageObject();
        if (!stageObject) {
            return;
        }

        // If source was hidden, show it.
        const auto paramBlock = geomObject->GetParamBlock(0);
        if (!MaxUsd::GetParamBlockValue<BOOL>(paramBlock, USDGeomObjectParams_ShowSource)) {
            stageObject->UnRegisterGeomObjectSource(geomObject->guid);

            // We need to manually register a restore object, the redo would not
            // fire TARGETMSG_DETACHING_NODE again.
            theHold.Begin();
            theHold.Put(new ObjectDetachRestore(geomObject));
            theHold.Accept(L"Detach USDGeomObject");
        }
        break;
    }
    }
}

void USDGeomObject::PostLoadCB::proc(ILoad* iload)
{

    if (nullptr == geom) {
        return;
    }
    // On load, build the purposes layer.
    geom->creating = false;
    geom->UpdateGeomSourcePurpose();
    delete this;
}

IOResult USDGeomObject::Load(ILoad* iload)
{
    iload->RegisterPostLoadCallback(new PostLoadCB(this));
    return IO_OK;
}

MaxSDK::QMaxParamBlockWidget* UsdGeomObjectClassDesc::CreateQtWidget(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock,
    const MapID     paramMapID,
    MSTR&           rollupTitle,
    int&            rollupFlags,
    int&            rollupCategory)
{
    switch (paramMapID) {
    case USDGeomObjectMapID_General: {
        const auto geomObjectUI = new UsdGeomObjectParametersRollup(owner, paramBlock);
        rollupTitle = UsdGeomObjectParametersRollup::tr("Parameters");
        return geomObjectUI;
    }
    case USDGeomObjectMapID_Includes: {
        const auto includesUI = new UsdGeomObjectIncludesRollup(owner, paramBlock);
        rollupTitle = UsdGeomObjectIncludesRollup::tr("Includes");
        return includesUI;
    }
    }
    return nullptr;
}

ClassDesc2* GetUSDGeomObjectDesc()
{
    static UsdGeomObjectClassDesc maxUsdObjectDesc;
    return &maxUsdObjectDesc;
}