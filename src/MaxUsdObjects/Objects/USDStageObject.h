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
#pragma once

#include <MaxUsdObjects/MaxUsdObjectsAPI.h>
#include <MaxUsdObjects/USDPickingRenderer.h>

#include <RenderDelegate/HdMaxEngine.h>

#include <MaxUsd/Interfaces/IUSDStageProvider.h>
#include <MaxUsd/Utilities/ProgressReporter.h>

#include <ISceneEventManager.h>
#include <future>
#include <qpointer.h>
#include <qwidget.h>
#include <splshape.h>

class SubObjectManip;

extern Class_ID USDSTAGEOBJECT_CLASS_ID;

// No way to ensure custom notification codes are unique...but with any luck, it will be!
#define NOTIFY_SELECTION_HIGHLIGHT_ENABLED_CHANGED REFMSG_USER + 0x29415134

enum
{
    PBLOCK_REF = 0
};

enum PBParameterIds
{
    StageFile,
    StageMask,
    CacheId,
    AxisAndUnitTransform,
    DisplayRender,
    DisplayProxy,
    DisplayGuide,
    DisplayMode,
    LoadPayloads, // obsolete
    SourceMetersPerUnit,
    SourceUpAxis,
    MeshMergeMode,
    MeshMergeDiagnosticView,
    MeshMergeMaxTriangles,
    MaxMergedMeshTriangles,
    MeshMergeMaxInstances,
    ShowIcon,
    IconScale,
    PointedPrim,
    CustomAnimationStartFrame,
    CustomAnimationSpeed,
    CustomAnimationEndFrame,
    CustomAnimationPlaybackTimecode,
    AnimationMode,
    SourceAnimationStartTimeCode,
    SourceAnimationEndTimeCode,
    SourceAnimationTPS,
    MaxAnimationStartFrame,
    MaxAnimationEndFrame,
    RenderUsdTimeCode,
    Guid,
    IsOpenInExplorer,
    KindSelection,
    GenerateCameras,
    GeneratePointInstancesDrawModes,
    PointInstancesDrawMode
};

// These correspond to the different rollouts.
enum ParamMapID
{
    UsdStageGeneral,
    UsdStageRenderSettings,
    UsdStageViewportDisplay,
    UsdStageViewportPerformance,
    UsdStageAnimation,
    UsdStageSelection
};

enum AnimationMode
{
    OriginalRange,
    CustomStartAndSpeed,
    CustomRange,
    CustomTimeCodePlayback
};

enum class DrawMode
{
    Default,
    BoxCards,
    CrossCards
};

enum class SelectionMode
{
    Stage,
    Prim
};

/**
 * \brief Derived 3dsMax hit data, which we use to log hits on USD prims, so that we can access
 * them back again when selecting sub-objects.
 */
class UsdHitData : public HitData
{
public:
    struct Hit
    {
        pxr::SdfPath primPath;
        int          instanceIdx = -1;
    };

    UsdHitData(const std::vector<Hit>& primPaths) { this->hits = primPaths; }

    const std::vector<Hit>& GetHits() { return this->hits; }

private:
    std::vector<Hit> hits;
};

static void NotifyNodeDeleted(void* param, NotifyInfo* info);
static void NotifyNodeCreated(void* param, NotifyInfo* info);
static void NotifyNodePreClone(void* param, NotifyInfo* info);
static void NotifyNodePostClone(void* param, NotifyInfo* info);

class MaxUSDObjectsAPI USDStageObject
    : public GeomObject
    , MaxUsd::IUSDStageProvider
    , public pxr::TfWeakBase // Required for this to listen to stage events.
{

    friend class SelectionObserver;

public:
    friend void NotifyNodeDeleted(void* param, NotifyInfo* info);
    friend void NotifyNodeCreated(void* param, NotifyInfo* info);
    friend void NotifyNodePreClone(void* param, NotifyInfo* info);
    friend void NotifyNodePostClone(void* param, NotifyInfo* info);

    /**
     * \brief PBAccessor to evaluate some paramblock parameters at request time.
     */
    class USDPBAccessor : public PBAccessor
    {
        void
        Get(PB2Value&       v,
            ReferenceMaker* owner,
            ParamID         id,
            int             tabIndex,
            TimeValue       t,
            Interval&       valid) override;
        void
        PreSet(PB2Value& v, ReferenceMaker* owner, ParamID id, int tabIndex, TimeValue t) override;
    };

    /**
     * \brief The USDStageObject's constructor.
     */
    USDStageObject();

    /**
     * \brief The USDStageObject's destructor.
     */
    ~USDStageObject() override;

    /**
     * \brief Creates the mouse create callback. Telling Max what do when a USDStageObject is created via
     * a mouse click in the viewport.
     * \return
     */
    CreateMouseCallBack* GetCreateMouseCallBack() override;

    /**
     * \brief Evaluates the object's state.
     * \param t Time at which to evaluate the object.
     * \return The object's state.
     */
    ObjectState Eval(TimeValue t) override;

    /**
     * \brief Initializes a node name for a USDStageObject. This may not be the final name used,
     * as Max may append numbered suffixes.
     * \param name The base name.
     */
    void InitNodeName(MSTR& name) override;

    /**
     * \brief Returns the object name (used in the modifier stack).
     * \param localized Whether or not to get the localized name.
     * \return The object name.
     */
    const MCHAR* GetObjectName(bool localized) const override;

    /*
     * From Object. Handling of the object's paramblock.
     */
    void             SetReference(int i, RefTargetHandle rtarg) override;
    int              NumRefs() override;
    ReferenceTarget* GetReference(int i) override;
    int              NumParamBlocks() override;
    IParamBlock2*    GetParamBlock(int i) override;
    IParamBlock2*    GetParamBlockByID(BlockID id) override;
    int              NumSubs() override;
    Animatable*      SubAnim(int i) override;
    TSTR             SubAnimName(int i, bool localized) override;
    int              SubNumToRefNum(int subNum) override;
    void             BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev) override;
    void             EndEditParams(IObjParam* ip, ULONG flags, Animatable* next) override;
    RefResult        NotifyRefChanged(
               const Interval& changeInt,
               RefTargetHandle hTarget,
               PartID&         partID,
               RefMessage      message,
               BOOL            propagate) override;

    /*
     * From Object. Handling of the object's sub-object modes.
     */
    int          NumSubObjTypes() override;
    ISubObjType* GetSubObjType(int i) override;
    void         ActivateSubobjSel(int level, XFormModes& modes) override;
    void SelectSubComponent(HitRecord* hitRec, BOOL selected, BOOL all, BOOL invert) override;
    void ClearSelection(int level) override;
    void GetSubObjectCenters(SubObjAxisCallback* cb, TimeValue t, INode* node, ModContext* /*mc*/)
        override;
    void
    GetSubObjectTMs(SubObjAxisCallback* cb, TimeValue t, INode* node, ModContext* /*mc*/) override;
    void
    Move(TimeValue t, Matrix3& partm, Matrix3& tmAxis, Point3& val, BOOL /*localOrigin = FALSE*/)
        override;
    void
    Rotate(TimeValue t, Matrix3& partm, Matrix3& tmAxis, Quat& val, BOOL /*localOrigin = FALSE*/)
        override;
    void
    Scale(TimeValue t, Matrix3& partm, Matrix3& tmAxis, Point3& val, BOOL /*localOrigin = FALSE*/)
        override;
    void TransformStart(TimeValue t) override;
    void TransformFinish(TimeValue t) override;

    /**
     * \brief Returns the world bounding box of this USD stage.
     * \param t The time at which to get the bounding box.
     * \param inode The node carrying the USDStageObject.
     * \param vp The viewport properties.
     * \param box The bounding box to be filled with the world bounding box.
     */
    void GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;

    /**
     * \brief Returns the local bounding box of this USD stage. This corresponds to the stage's extents
     * transformed in max's space.
     * \param t The time at which to get the bounding box.
     * \param inode The node carrying the USDStageObject.
     * \param vp The viewport properties.
     * \param box The bounding box to be filled with the world bounding box.
     */
    void GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box) override;

    /**
     * \brief Returns the deform bounding box of this USD stage. This corresponds to the stage's extents
     * transformed in max's space.
     * \param t The time at which to get the bounding box.
     * \param box The box to be filled with the world deform bounding box.
     * \param tm The transform matrix.
     * \param useSel Whether or not to use the selection.
     */
    void GetDeformBBox(TimeValue t, Box3& box, Matrix3* tm, BOOL useSel) override;

    /**
     * \brief Returns the USDStageObject's FPInterfaceDesc.
     * \return The FPInterfaceDesc.
     */
    FPInterfaceDesc* GetDesc() override;

    /**
     * \brief Returns the USDStageObject's class ID.
     * \return The class id.
     */
    Class_ID ClassID() override;

    /**
     * \brief Retrieves the name of the plugin class
     */
    void GetClassName(MSTR& s, bool localized = true) const override;

    /**
     * \brief Whether or not the object is renderable.
     */
    int IsRenderable() override;

    /**
     * \brief Gets an interface over the object.
     * \param id The id of the requested interface.
     * \return The interface.
     */
    BaseInterface* GetInterface(Interface_ID id) override;

    /**
     * \brief Returns the USD stage held by this Max object.
     * \return The USD stage.
     */
    pxr::UsdStageWeakPtr GetUSDStage() const override;

    /**
     * \brief Clear the previous stage if needed, then load a new stage according to the layer path and mask values.
     * \param fromStage Optional. Load from an existing USD stage instead.
     * \param loadPayloads Whether to load payloads.
     * \return The USD stage.
     */
    pxr::UsdStageWeakPtr
    LoadUSDStage(const pxr::UsdStageRefPtr& fromStage = nullptr, bool loadPayloads = true);

    /**
     * \brief Creates and updates the render items necessary to display the USD stage's content in the Max
     * viewports. This will be called whenever the object is flagged as modified and a redraw is
     * performed. In some scenarios we trigger this ourselves, for example when the stage being
     * referenced has changed, in some other scenarios, max itself will trigger it as needed, for
     * example if the time frame changes, and the object is no longer valid.
     * \param updateDisplayContext The display context. Not used currently.
     * \param nodeContext The node context, giving access to the INode's RenderNode.
     * \param viewContext The view context, carries the view info for each viewport.
     * \param targetRenderItemContainer Render item array to be filled up.
     * \return True if render items were updated, false otherwise.
     */
    bool UpdatePerViewItems(
        const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
        MaxSDK::Graphics::UpdateNodeContext&          nodeContext,
        MaxSDK::Graphics::UpdateViewContext&          viewContext,
        MaxSDK::Graphics::IRenderItemContainer&       targetRenderItemContainer) override;

    /**
     * \brief Prepare display data for all nodes, thus avoid duplicate display data generation for each node or each view.
     * \param prepareDisplayContext The display context.
     * \return True if prepare successfully, false otherwise.
     */
    bool
    PrepareDisplay(const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext) override;

    /**
     * \brief Returns the stage's cache id.
     * \return The stage cache id, as an integer.
     */
    int GetStageCacheId();

    /**
     * \brief Returns the object display requirement. In our case, we specify that render
     * items must be updated "per-view". Needed so we can frustum cull items per viewport.
     * \return Object display requirements.
     */
    unsigned long GetObjectDisplayRequirement() const override;

    /**
     * \brief Queries the object's validity interval at a certain time.
     * \param time Time for which to fetch the object validity.
     * \return The object validity interval.
     */
    Interval ObjectValidity(TimeValue time) override;

    /**
     * \brief Performs hit testing against the held USD stage.
     * \param t Time at which to do the hit test.
     * \param iNode The node holding the Usd stage object
     * \param p Click position.
     * \param vpt The viewport.
     * \return True if a USD primitive was hit, false otherwise.
     */
    int
    HitTest(TimeValue t, INode* iNode, int type, int crossing, int flags, IPoint2* p, ViewExp* vpt)
        override;

    /**
     * \brief Performs hit testing against the held USD stage. This version is used in sub-object mode.
     * We simply forward the arguments to the method just above.
     * \param t Time at which to do the hit test.
     * \param iNode The node holding the Usd stage object
     * \param p Click position.
     * \param vpt The viewport.
     * \param mc Mode context, not used.
     * \return True if a USD primitive was hit, false otherwise.
     */
    int HitTest(
        TimeValue   t,
        INode*      iNode,
        int         type,
        int         crossing,
        int         flags,
        IPoint2*    p,
        ViewExp*    vpt,
        ModContext* mc) override;

    /**
     * @param viewport The viewport in which the picking happens.
     * @param node The node referencing the USD Stage object.
     * @param hitRegion Hit region, describe the area being picked.
     * @param drawMode Draw mode for picking (geometry, wireframe, points...)
     * @param pickTarget What is being picked (prims, edges, points, etc...)
     * @param time The time to pick the USD stage at.
     * @param excludedPaths Prims that should be ignored when picking.
     * @return Information on hit prims
     */
    std::vector<USDPickingRenderer::HitInfo> PickStage(
        ViewExp*                  viewport,
        INode*                    node,
        const HitRegion&          hitRegion,
        pxr::UsdImagingGLDrawMode drawMode,
        const pxr::TfToken&       pickTarget,
        TimeValue                 time,
        const pxr::SdfPathVector& excludedPaths);

    /**
     * \brief Implements the clone operation for a Usd Stage.
     * \param remap For remapping references.
     * \return The cloned Stage.
     */
    RefTargetHandle Clone(RemapDir& remap) override;

    /**
     * \brief Returns statistics on the geometry held by the USD Stage object.
     * \param t The time at which to get the polygon count.
     * \param numFaces The total number of faces in the USD Stage.
     * \param numVerts The total number of vertices in the USD Stage.
     * \return Whether PolyCount() is supported by the USD Stage object. Currently, we only
     * support it for the Current time value, as the stats are pulled from what is currently
     * being rendered.
     */
    BOOL PolygonCount(TimeValue t, int& numFaces, int& numVerts) override;

    /**
     * \brief Returns a mesh that can be used for rendering, containing the entire USD stage.
     * Ideally, GetMultipleRenderMesh() is used by renderers, but some do not support this API.
     * \param t The time at which to get the render mesh.
     * \param inode The 3dsMax node.
     * \param view View information, not used currently.
     * \param needDelete Whether or not to delete the mesh, always false in our case.
     * \return The render mesh.
     */
    Mesh* GetRenderMesh(TimeValue t, INode* inode, View& view, BOOL& needDelete) override;

    /**
     * \brief Returns the number of render meshes. Typically there is one render mesh per USD render prim.
     * \return The number of render meshes.
     */
    int NumberOfRenderMeshes() override;

    /**
     * \brief Returns the requested render mesh.
     * \param t The time at which to get the render mesh.
     * \param inode The 3dsMax Node
     * \param view View information, not used currently.
     * \param needDelete Whether or not the called should delete the mesh, always false in out case.
     * \param meshNumber The requested mesh's number.
     * \return The render mesh.
     */
    Mesh*
    GetMultipleRenderMesh(TimeValue t, INode* inode, View& view, BOOL& needDelete, int meshNumber)
        override;

    /**
     * \brief Returns the offset transform for the N'th render mesh.
     * \param t The time at which to get the meshe's render offset transform,
     * \param inode The 3dsMax node.
     * \param view View informatin, not used currently.
     * \param meshNumber The number of the mesh for which we are requesting the transform.
     * \param meshTM The mesh transform, returned by reference.
     * \param meshTMValid The validity interval for the transform.
     */
    void GetMultipleRenderMeshTM(
        TimeValue t,
        INode*    inode,
        View&     view,
        int       meshNumber,
        Matrix3&  meshTM,
        Interval& meshTMValid) override;

    /**
     * \brief Flag the object for redraw, and trigger redraw.
     * \param completeRedraw optionally, force a complete redraw (calls GetCOREInterface()->ForceCompleteRedraw())
     */
    void Redraw(bool completeRedraw = false);

    /**
     * \brief Invalidate paramblock parameters that have accessors and are computed, to signal
     * 3dsMax it should request the values again as they may have changed (likely to update UI)
     */
    void InvalidateParams();

    /**
     * \brief Gets the stage's bounding box at the given TimeValue. This can be called from
     * multiple threads, and should be kept thread safe.
     * \param rootTransform The stage's root transform (up axis and unit adjustments)
     * \param time The 3dsMax TimeValue at which to the the bounding box.
     * \param node Optional node parameter. If not passed, the root prim and the display settings are not updated
     * before performing the render that gets us the boundingbox
     * \param useSel Whether or not to use the selection.
     * \return The Stage's bounding box.
     */
    Box3 GetStageBoundingBox(
        pxr::GfMatrix4d rootTransform,
        TimeValue       time,
        INode*          node = nullptr,
        bool            useSel = false);

    /**
     * \brief Callback to be executed whenever the object's wire color changes.
     * \param newColor The new wire color.
     */
    void WireColorChanged(Color newColor);

    /**
     * \brief Force reloads all of the stage's layers.
     */
    void Reload() override;

    /**
     * \brief Clears the stage's session layer.
     */
    void ClearSessionLayer() override;

    /**
     * \brief Gets the transform required to align and scale the USD stage to Max's up axis and units.
     * \return The stage's root transform.
     */
    pxr::GfMatrix4d GetStageRootTransform() const;

    /**
     * \brief Used to register auxiliary files, this way, the referenced USD file is properly registered as
     * an asset, and will be considered when archiving the scene.
     * \param nameEnum The Callback object that gets called on all Auxiliary files.
     * \param flags Flags for the file enumeration.
     */
    void EnumAuxFiles(AssetEnumCallback& nameEnum, DWORD flags) override;

    /**
     * \brief Executed when the object is loaded from disk.
     * \param iload Load interface.
     * \return Load result.
     */
    IOResult Load(ILoad* iload) override;
    /**
     * \brief Executed when the object is saved.
     * \param isave Save interface.
     * \return Save Save result.
     */
    IOResult Save(ISave* isave) override;

    /**
     * \brief Does a full stage reset on a particular interval.
     */
    void FullStageReset();

    /**
     * \brief Used to set the USD Stage's root layer (file path) and mask. MXS version of the
     * function which includes validation and Runtime exceptions being thrown.
     * \param rootLayer The root layer (file path) of the USD stage.
     * \param stageMask The mask to be applied on the USD stage.
     * \param payloadsLoaded Sets if the payloads (if any) are loaded or not when opening the stage
     */
    void SetRootLayerMXS(const wchar_t* rootLayer, const wchar_t* stageMask, bool payloadsLoaded)
        override;

    /**
     * \brief Used to set the USD Stage's root layer (file path) and mask.
     * \param rootLayer The root layer (file path) of the USD stage.
     * \param stageMask The mask to be applied on the USD stage.
     * \param payloadsLoaded Sets if the payloads (if any) are loaded or not when opening the stage (true by default)
     */
    void SetRootLayer(
        const wchar_t* rootLayer,
        const wchar_t* stageMask,
        bool           payloadsLoaded = true) override;

    /**
     * \brief Returns the multimaterial representing the UsdPreviewSurface materials in the stage.
     * If the material doesn't yet exist, it will be built at this time - this can be a costly
     * operation.
     * \param sync If True, the multimaterial is synchronized with the usd source materials, (potentially
     * a destructive operation - for example if a user sloted in new materials to override usd
     * materials), otherwise it is returned as-is currently.
     * \return The multimaterial carrying UsdPreviewSurface materials.
     */
    Mtl* GetUsdPreviewSurfaceMaterials(bool sync) override;

    /**
     * \brief Set the Stage Icon to the current scale.
     */
    void UpdateViewportStageIcon();

    /**
     * \brief Registers a ProgressReporter with the UsdStageObject, lengthy operations
     * will then use this reporter to report progress, typically it is hooked up
     * to some UI.
     * \param reporter The reporter to register.
     */
    void RegisterProgressReporter(const MaxUsd::ProgressReporter& reporter);

    /**
     * \brief Unregisters the currently registered progress reporter, see RegisterProgressReporter().
     */
    void UnregisterProgressReporter();

    /**
     * \brief Clears the bounding box cache.
     */
    void ClearBoundingBoxCache();

    /**
     * \brief Reset primvar mappings to defaults.
     */
    void SetPrimvarChannelMappingDefaults() override;

    /**
     * \brief Sets a primvar to 3dsMax channel mapping.
     * \param primvarName The primvar to map.
     * \param channel The target 3dsmax map channel.
     */
    void SetPrimvarChannelMapping(const wchar_t* primvarName, Value* channel) override;

    /**
     * \brief Gets the target 3dsMax channel for a given primvar.
     * \param primvarName The primvar to get the channel for.
     * \return The channel.
     */
    Value* GetPrimvarChannel(const wchar_t* primvarName) override;

    /**
     * \brief Gets a tab of all currently mapped primvars.
     * \return All mapped primvars.
     */
    Tab<const wchar_t*> GetMappedPrimvars() const override;

    /**
     * \brief Checks if a primvar is currently mapped to a channel.
     * \param primvarName The name of the primvar to check.
     * \return True if the primvar is mapped, false otherwise.
     */
    bool IsMappedPrimvar(const wchar_t* primvarName) override;

    /**
     * \brief Clears all primvar mappings.
     */
    void ClearMappedPrimvars() override;

    /**
     * \brief Opens the stage in the USD explorer.
     */
    void OpenInUsdExplorer() override;

    /**
     * \brief Closes the stage in the USD explorer.
     */
    void CloseInUsdExplorer() override;

    /**
     * \brief Returns the GUID associated with the USD Stage object.
     * \return The GUID.
     */
    const std::string& GetGuid() const;

    /**
     * \brief Callback method to save the load rules so that switching the stage settings will
     * be able to preserve the load rules. Called when loading/unloading a payload.
     * (see UsdUfe::DCCFunctions::saveStageLoadRulesFn in UfeUtils)
     */
    void SaveStageLoadRules();

    /**
     * \brief Returns a pointer to the hydra engine used by the stage object.
     * \return The hydra engine.
     */
    HdMaxEngine* GetHydraEngine() const;

    /** Adds or remove the attribute rollups for the current prim selection of
     * the USD stage object.
     */
    void AdjustAttributeRollupsForSelection();

    /**
     * \brief Requests that the selection display in the viewport be updated on the next draw.
     * See UpdatePrimSelectionDisplay().
     */
    void DirtySelectionDisplay();

    /**
     * \brief Updates the prim selection display in the viewport if it is dirty. If we are at the
     * object level, prim selection is not displayed. The function will basically converts the
     * current global UFE selection to the hydra selection that the render delegate uses.
     */
    void UpdatePrimSelectionDisplay();

    /**
     * Simple struct describing USD objects that can be transformed in the USDStageObject's
     * prim subobject mode. Xformable prims and point instances can be transformed.
     */
    struct Transformable
    {
        // A Xformable prim
        pxr::UsdPrim prim;
        /// Point instance indices, empty is "prim" is not a UsdGeomPointInstancer
        std::vector<int> instanceIndices;
    };

    /**
     * \brief Returns Transformables from the UFE selection corresponding to the prims that need
     * to be transformed to affect the position of all prims / point instances in the selection.
     * If both a parent and child transformable are selected, we only need to move the parent,
     * as the transform will be inherited.
     * \return The root-most Transformables in the selection.
     */
    std::vector<Transformable> GetTransformablesFromSelection();

    /**
     * \brief Get the computed time code based on the Animation mode and related params of the param block.
     * \param time The TimeValue you want to resolve.
     */
    pxr::UsdTimeCode ResolveRenderTimeCode(TimeValue time) const;

    /**
     * \brief Builds 3dsMax cameras representing the USD Cameras contained in the stage, for all INodes
     * referencing the object.
     */
    void BuildCameraNodes();

    /**
     * \brief Builds 3dsMax cameras representing the USD Cameras contained in the stage.
     * \param stageNode The node referencing the USD Stage object. We need the node, to position
     * cameras correctly. Any camera that already exists on the 3dsMax side will be left untouched.
     */
    void BuildCameraNodes(INode* stageNode) const;

    /**
     * \brief Deletes all the 3dsMax cameras representing the USD Cameras contained in the stage.
     * \param stageNode The USD Stage object node owning the cameras objects.
     */
    void DeleteCameraNodes(INode* stageNode) const;

    /**
     * \brief Gets the full transform of a prim in the 3dsMax scene.
     * \param stageNode The node the stage object belongs to.
     * \param prim The prim we want the transform for.
     * \param time Time at which we want the transform.
     * \param includePivot Whether to include the translate pivot of the prim, if defined, in the final transform.
     * \return The world space position of the prim in the max scene.
     */
    static Matrix3 GetMaxScenePrimTransform(
        INode*              stageNode,
        const pxr::UsdPrim& prim,
        const TimeValue&    time,
        bool                includePivot);

    /**
     * Gets the full transforms of USD Point instances in the 3dsMax scene.
     * @param stageNode The node the stage object belongs to.
     * @param instancerPrim The UsdGeomPointInstancer prim the instances belong to.
     * @param instanceIndices The instances we want the transforms for.
     * @param time Time at which we want the transforms.
     * @return The world space position of the instances in the max scene.
     */
    static std::vector<Matrix3> GetMaxScenePointInstancesTransforms(
        INode*                  stageNode,
        const pxr::UsdPrim&     instancerPrim,
        const std::vector<int>& instanceIndices,
        const TimeValue&        time);

    /**
     * Sets up USD draw modes as configured for the USD Stage. Calling the function will
     * author missing draw modes / UsdGeomModelAPIs, and apply model kinds as necessary.
     */
    void GenerateDrawModes() override;

private:
    class NodeEventCallback : public INodeEventCallback
    {
    public:
        NodeEventCallback(USDStageObject* stageObject);

    private:
        void            WireColorChanged(NodeKeyTab& nodes) override;
        USDStageObject* object;
    };

    /**
     * \brief OnStageChange event handler.
     * \param notice The stage objects changed notice - contains a reference to the changed stage.
     */
    void OnStageChange(pxr::UsdNotice::ObjectsChanged const& notice);

    /**
     * \brief Gets whether a not the given display purpose should be displayed.
     * \param purpose The purpose token.
     * \return True if the purpose should be displayed, false otherwise.
     */
    bool GetDisplayPurpose(pxr::TfToken purpose) const;

    /**
     * \brief Checks if the render cache is valid at the given time with the given
     * render tags. If not, the cache is cleared.
     * \param time The time to check.
     * \param renderTags The render tags.
     */
    void CheckRenderCache(TimeValue time, const pxr::TfTokenVector& renderTags);

    /**
     * \brief Clears all caches held by the UsdStageObject.
     */
    void ClearAllCaches();

    /**
     * \brief Clears the render data cache.
     */
    void ClearRenderCache();

    /**
     * \brief Returns the currently set render tags, as a token vector.
     * \return The render tags.
     */
    pxr::TfTokenVector GetRenderTags() const;

    /**
     * \brief Setup the hydra render delegate display settings, for the given node
     * and the PB param setup on the object.
     * \param node The node, to get the used wire color.
     */
    void SetupRenderDelegateDisplaySettings(INode* node) const;

    /**
     * \brief Executed whenever the primvar to channel configuration of the USD Stage changes.
     */
    void OnPrimvarMappingChanged();

    /**
     * \brief Transforms the prim currently being transformed by the prim subobject move, rotate or scale modes.
     * The most local transform op on the prim is what gets written to.
     * \param parentTm The parent transform, typically the node's transform.
     * \param axisTm The axis of the transform, will get different values depending on the current coord system
     * (world, local, etc.)
     * \param transform The transform to apply.
     */
    void TransformInteractive(
        const Matrix3& parentTm,
        const Matrix3& axisTm,
        const Matrix3& transform) const;

    /** Cleans up the prim attribute rollups for the current prim selection and
     * remembers their individual open/close states. */
    void CleanupPrimAttributeWidgets();

    /// Paramblock holding the Stage object's data.
    IParamBlock2* pb;
    /// The stage. Should not be used directly, instead use GetUSDStage(), which will load the stage
    /// as needed.
    pxr::UsdStageRefPtr stage;
    /// The hydra engine to render USD geometry in the viewport.
    std::unique_ptr<HdMaxEngine> hydraEngine;
    /// Offscreen renderer used for the picking of USD primitives.
    std::unique_ptr<USDPickingRenderer> pickingRenderer;
    /// Handle for the onStageChange notice so that we can revoke it upon destruction.
    pxr::TfNotice::Key onStageChangeNotice;
    /// Id of the stage in the stage cache.
    pxr::UsdStageCache::Id stageCacheId;
    /// A unique identifier for the USD Stage object. Used to map USD stages <-> 3dsMax objects.
    std::string guid;
    /// Flag to keep track of when the displayed purposes have changed. We
    /// need this to trigger a workaround with render purposes not getting flagged
    /// dirty from Hydra.
    bool displayPurposeUpdated = false;
    /// Multi-material used for offline rendering of the USD stage object. Holds the
    /// MaxUsdPreviewSurface materials converted from USD. Gets populated by HdmaxEngine::Render().
    MaxUsd::MaterialRef usdMaterials;
    bool                buildOfflineRenderMaterial = false;
    /// Cached offline render data.
    struct RenderCache
    {
        std::vector<std::shared_ptr<Mesh>> meshes;
        std::vector<Matrix3>               transforms;
        std::unique_ptr<Mesh>              fullMesh = nullptr;

        // Info on the cached data, only reuse if matching these..
        TimeValue          time = INT_MAX;
        pxr::TfTokenVector renderTags;
        Mtl*               material = nullptr;

        /**
         * \brief Check if the render cache is valid for the given input.
         * \param time Render time.
         * \param renderTags Active render tags.
         * \return True if the cache is valid, false otherwise.
         */
        bool IsValid(const TimeValue time, const pxr::TfTokenVector& renderTags) const
        {
            return this->time == time && this->renderTags == renderTags;
        }

        void SetValidity(TimeValue time, const pxr::TfTokenVector& renderTags, Mtl* material)
        {
            this->time = time;
            this->renderTags = renderTags;
            this->material = material;
        }
    };

    RenderCache renderCache;

    // Simple struct to hold cached hit testing information.
    struct HitTestCacheData
    {
        /// The cursor position on screen in pixels.
        IPoint2 cursorPos;
        // Hit info, what prim was hit.
        USDPickingRenderer::HitInfo hit;
    };

    // Hit testing cache. Maintain this per-node, multiple nodes can point to the same
    // USD Stage object.
    std::unordered_map<INode*, HitTestCacheData> hitTestingCache;

    /// Object statistics.
    size_t numFaces = 0;
    size_t numVerts = 0;

    // Viewport Stage icon
    SplineShape shapeIcon;

    // Bounding box cache. The boundingBox cache must be carefully maintained,
    // it should be cleared whenever anything might change the bounding box at a
    // specific time (icon toggle, usd stage changed notice, animation mode changes,
    // timeline changes, etc.)
    std::unordered_map<TimeValue, Box3> boundingBoxCache;

    /// Node even callback, we use it to react to wire color changes.
    ISceneEventManager::CallbackKey nodeEventCallbackKey;
    NodeEventCallback               nodeEventCallback { this };
    /// Progress reporter for length operations, typically hooked up to some UI.
    MaxUsd::ProgressReporter progressReporter;
    /// A reference to the session layer that was loaded from the max scene.
    pxr::SdfLayerRefPtr sessionLayerFromMaxScene;
    /// The payload rules applied by the USD Explorer
    std::string savedPayloadRules;

    /// True if a usd object is currently in create mode, in the command panel.
    bool isInCreateMode = false;
    /// The current sub object level for the object. We need to know if this specific object
    /// is in subobject mode, the global state does not tell us.
    int subObjectLevel = 0;
    /// True if currently in a node clone operation.
    bool inCloneOperation = false;

    /// The interface to interact with the command panel - when in edit mode...
    IObjParam* ip = nullptr;
    /// The rollups showing the attributes of the current selected prim(s).
    std::vector<QPointer<QWidget>> primAttributeWidgets;

    static bool                    primAttributeRollupOpenStatesLoaded;
    static std::map<QString, bool> primAttributeRollupStates;
    static std::map<QString, bool> loadedPrimAttributeRollupStates;
    static const QString           rollupCategory;

    /// Sub-object 3dsmax modes, used to select and transform prims.
    static SelectModBoxCMode*  selectMode;
    static MoveModBoxCMode*    moveMode;
    static RotateModBoxCMode*  rotateMode;
    static UScaleModBoxCMode*  uScaleMode;
    static NUScaleModBoxCMode* nuScaleMode;
    static SquashModBoxCMode*  squashMode;

    // When transforming in sub-object mode, we may need to affect multiple prims, in the
    // case of multiselection, there will be one entry for each USD transformable entity needing
    // edit in subObjectManips.
    std::vector<std::unique_ptr<SubObjectManip>> subObjectManips;

    /// UFE observer to react to changes in the selection.
    std::shared_ptr<SelectionObserver> selectionObserver;

    /// Flag to indicate that the selection display must be udpated upon the next draw.
    bool isSelectionDisplayDirty = false;
};
