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

#pragma once

#include "MaxUsd/MeshConversion/MeshConverter.h"
#include "MaxUsdObjects/DLLEntry.h"
#include "MaxUsdObjects/Objects/USDStageObject.h"

#include <pxr/usd/usd/editContext.h>

#include <Qt/QMaxParamBlockWidget.h>

#include <QLabel.h>
#include <QPushButton>
#include <QVBoxLayout>
#include <simpobj.h>

enum USDGeomObjectParams
{
    USDGeomObjectParams_USDStage,
    USDGeomObjectParams_PrimPath,
    USDGeomObjectParams_ShowSource,
    USDGeomObjectParams_LiveUpdates,
    USDGeomObjectParams_ViewportDisplayPurposes,
    USDGeomObjectParams_IncludeRender,
    USDGeomObjectParams_IncludeProxy,
    USDGeomObjectParams_IncludeGuide,
    USDGeomObjectParams_IncludeInvisible
};

enum USDGeomObjectFunctions
{
    fnIdUSDGeomObjectRefresh
};

enum USDGeomObjectParamMapID
{
    USDGeomObjectMapID_General,
    USDGeomObjectMapID_Includes
};

#define USDGeomObject_CLASS_ID Class_ID(0x39ee4251, 0x1e83391e)

/**
 * Max Object holding geometry pushed from a USD Stage Object.
 */
class USDGeomObject
    : public SimpleObject2
    , public pxr::TfWeakBase // For USD notices.
    , public FPMixinInterface
{

public:
    // Constructor/Destructor
    USDGeomObject();
    virtual ~USDGeomObject();

    // From BaseObject
    CreateMouseCallBack* GetCreateMouseCallBack() override { return nullptr; };
    const MCHAR* GetObjectName(bool localized) const override { return _T("USD Geometry Object"); }
    void         SetReference(int i, RefTargetHandle rtarg) override;
    int          NumRefs() override;
    ReferenceTarget* GetReference(int i) override;
    int              NumParamBlocks() override;
    IParamBlock2*    GetParamBlock(int i) override;
    IParamBlock2*    GetParamBlockByID(BlockID id) override;
    int              NumSubs() override;
    Animatable*      SubAnim(int i) override;
    TSTR             SubAnimName(int i, bool localized) override;
    int              SubNumToRefNum(int subNum) override;

    // From Animatable
    void      BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev) override;
    void      EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override;
    void      UpdateGeomSourcePurpose();
    RefResult NotifyRefChanged(
        const Interval& changeInt,
        RefTargetHandle hTarget,
        PartID&         partID,
        RefMessage      message,
        BOOL            propagate) override;
    Class_ID  ClassID() override { return USDGeomObject_CLASS_ID; }
    SClass_ID SuperClassID() override { return GEOMOBJECT_CLASS_ID; }

    // From SimpleObject
    void BuildMesh(TimeValue t) override;

    // From reference maker
    IOResult Load(ILoad* iload) override;
    // From ReferenceTarget
    void NotifyTarget(int message, ReferenceMaker* hMaker) override;

    /**
     * Returns the 3dsMax node referencing the USD Stage Object holding the source prims..
     * @return The USD stage node.
     */
    INode* GetStageNode();

    /**
     * Returns the USDStageObject holding the source prims.
     * @return The USD stage object.
     */
    USDStageObject* GetStageObject();

    /**
     * Returns the USD Stage where the source prims are from.
     * @return The USD Stage.
     */
    pxr::UsdStagePtr GetUSDStage();

    /**
     * Returns The root prim of the USD subtree used as source.
     * @return The prim.
     */
    pxr::UsdPrim GetPrim();

    /**
     * Returns The root prim path of the USD subtree used as source.
     * @return The prim path.
     */
    pxr::SdfPath GetPrimPath() const;

    /**
     * Returns USD purposes configured to be included in the geometry.
     * These are the purposes included if the option to use the purposes
     * currently displayed by the USDStageObject in the viewport is disabled.
     * @return Included purposes.
     */
    pxr::TfTokenVector GetIncludedPurposes() const;

    /**
     * Returns true if currently invisible prims should be included.
     * @return True if invisible prims are included.
     */
    bool GetIncludeInvisible() const;

    /**
     * Forces a refresh of the mesh.
     */
    void Refresh();

    // MXS function publishing.
    BEGIN_FUNCTION_MAP
    VFN_0(fnIdUSDGeomObjectRefresh, Refresh);
    END_FUNCTION_MAP

    // From FPMixinInterface
    FPInterfaceDesc* GetDesc() override;
    BaseInterface*   GetInterface(Interface_ID iid) override;

    /**
     * Dirty the source prims (usually to force fresh conversion).
     */
    void DirtySourceRPrims();

    /**
     * Returns the unique identifier for the USDGeomObject
     */
    std::string GetGuid();

private:
    /**
     * React to changes in the USD Stage.
     * @param notice Usd object changed notice.
     */
    void OnStageChange(pxr::UsdNotice::ObjectsChanged const& notice);

    // Max scene notifs.
    static void NotifyNodeAdded(void* param, NotifyInfo* info);
    static void NotifyNodeDeleted(void* param, NotifyInfo* info);

    // Paramblock for the object
    IParamBlock2* paramBlock;
    // Triangular Mesh backing the object.
    std::unique_ptr<HdMaxTriMesh> usdMesh;
    // Notice to react to changes to the stage.
    pxr::TfNotice::Key onStageChangeNotice;

    class PostLoadCB : public PostLoadCallback
    {
    public:
        PostLoadCB(USDGeomObject* geom) { this->geom = geom; }
        void proc(ILoad* iload);

    private:
        USDGeomObject* geom;
    };

    // Flag indicating the object is still getting created, and was not
    // yet added to the scene. This is useful to avoid needless object evaluations
    // while we are still setting things up.
    bool creating = false;
    // A unique identifier for the USD Geom Object.
    std::string guid;
    // The stage Node. Just used to detect stage node changes. Usually
    // would get the stage node from the PB with GetStageNode() instead.
    INode* currStageNode = nullptr;
};

class UsdGeomObjectClassDesc : public ClassDesc2
{
public:
    virtual int          IsPublic() { return FALSE; }
    virtual void*        Create(BOOL /*loading = FALSE*/) { return new USDGeomObject(); }
    virtual const TCHAR* ClassName() { return _T("USDGeomObject"); }
    virtual const TCHAR* NonLocalizedClassName() { return _T("USDGeomObject"); }
    virtual SClass_ID    SuperClassID() { return GEOMOBJECT_CLASS_ID; }
    virtual Class_ID     ClassID() { return USDGeomObject_CLASS_ID; }
    virtual const TCHAR* Category() { return _T("USD"); }
    virtual const TCHAR* InternalName() { return _T("USDGeomObject"); }
    virtual HINSTANCE    HInstance() { return hInstance; }
    virtual bool         UseOnlyInternalNameForMAXScriptExposure() { return true; }

    MaxSDK::QMaxParamBlockWidget* CreateQtWidget(
        ReferenceMaker& owner,
        IParamBlock2&   paramBlock,
        const MapID     paramMapID,
        MSTR&           rollupTitle,
        int&            rollupFlags,
        int&            rollupCategory);
};

ClassDesc2* GetUSDGeomObjectDesc();
