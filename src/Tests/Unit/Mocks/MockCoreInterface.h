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

#include "MockViewExp.h"
#include "MockRenderer.h"
#include "MockDllDir.h"
#include "MockLog.h"
#include <Tests/Unit/TestUtils.h>

#include <bitmap.h>
#include <plugin.h>
#include <GetCOREInterface.h>
#include <maxscript/maxscript.h>

// Interface changed from MCHAR* to MSTR in Max 2025
#if MAX_RELEASE >= 26900
#define MCHAR_TO_MSTR MSTR
#define MCHAR_TO_MSTR_RET {}
#else
#define MCHAR_TO_MSTR const MCHAR*
#define MCHAR_TO_MSTR_RET nullptr
#endif

/**
 * \brief MAX_2022 Mock for 3ds Max's "Interface" interface.
 */
#if MAX_RELEASE >= 23900  // cover up to 2026+ (MAX_RELEASE >= 27900)
class MockCoreInterface : public Interface17
#endif
{
protected:
	/**
	 * The following properties are instantiated in order to return some data
	 * by reference by the Interface.
	 */
	MockLogSys SomeLog;
	MockViewExp MockViewExp;
	MSTR SomeMSTR{};
	BitmapInfo SomeBitmapInfo;
#if MAX_RELEASE < 26900
	MtlBaseLib SomeMtlBaseLib;
#else
	TypedSingleRefMaker<MtlBaseLib> SomeMtlBaseLib;
#endif
	MaxSDK::AssetManagement::AssetUser SomeAssetUser;
	Renderer* renderer{ new MockRenderer() };
	DllDir* dllDir{ new MockDllDirInternal() };

public:
	/**
	 * The following methods are used by the MockCoreInterface in order to
	 * control the behavior of the Interface as it is passed to tests.
	 */

	~MockCoreInterface()
	{
		if (renderer != nullptr)
		{
			delete renderer;
		}
		if (dllDir != nullptr)
		{
			delete dllDir;
		}
	}

	Renderer* GetRenderer(RenderSettingID /*renderSettingID*/, bool /*createRendererIfItDoesntExist*/ = true) override
	{
		return renderer;
	}

	void AssignProductionRenderer(Renderer* newRenderer) override
	{
		if (renderer != nullptr)
		{
			delete renderer;
		}
		renderer = newRenderer;
	}

	void* CreateInstance(SClass_ID superID, Class_ID classID) override
	{
		return nullptr;
	}

	DllDir& GetDllDir() override
	{
		return *dllDir;
	}

	DllDir* GetDllDirectory() override
	{
		return dllDir;
	}

public:
	/**
	 * The following members inherited from the Interface interface are not
	 * implemented. Their return values should not be considered, and can cause
	 * undefined side-effects.
	 */

	HFONT GetAppHFont() override
	{
		return nullptr;
	}
	void RedrawViews(TimeValue /*t*/, DWORD /*vpFlags*/ = REDRAW_NORMAL, ReferenceTarget* /*change*/ = NULL) override
	{
	}
	BOOL SetActiveViewport(HWND /*hwnd*/) override
	{
		return FALSE;
	}
	ViewExp& GetActiveViewExp() override
	{
		return MockViewExp;
	}
	void ForceCompleteRedraw(BOOL /*doDisabled*/ = TRUE) override
	{
	}
	IObjCreate* GetIObjCreate() override
	{
		return nullptr;
	}
	IObjParam* GetIObjParam() override
	{
		return nullptr;
	}
	HWND GetMAXHWnd() const override
	{
		return nullptr;
	}
	MaxSDK::QmaxMainWindow* GetQmaxMainWindow() const override
	{
		return nullptr;
	}
	BOOL DisplayActiveCameraViewWithMultiPassEffect() override
	{
		return FALSE;
	}
	BOOL SetActiveViewportTransparencyDisplay(int /*transType*/) override
	{
		return FALSE;
	}
	void DisableSceneRedraw() override
	{
	}
	void EnableSceneRedraw() override
	{
	}
	int IsSceneRedrawDisabled() override
	{
		return 0;
	}
	void RegisterRedrawViewsCallback(RedrawViewsCallback* /*cb*/) override
	{
	}
	void UnRegisterRedrawViewsCallback(RedrawViewsCallback* /*cb*/) override
	{
	}
	void RegisterSelectFilterCallback(SelectFilterCallback* /*cb*/) override
	{
	}
	void UnRegisterSelectFilterCallback(SelectFilterCallback* /*cb*/) override
	{
	}
	void RegisterDisplayFilterCallback(DisplayFilterCallback* /*cb*/) override
	{
	}
	void UnRegisterDisplayFilterCallback(DisplayFilterCallback* /*cb*/) override
	{
	}
	BOOL SetSelectionType(BOOL /*autoWinCross*/, int /*winCrossOrAutoDir*/) override
	{
		return FALSE;
	}
	void MakeExtendedViewportActive(HWND /*hWnd*/) override
	{
	}
	void PutUpViewMenu(HWND /*hWnd*/, POINT /*pt*/) override
	{
	}
	BOOL TrackViewPickDlg(HWND /*hParent*/, TrackViewPick* /*res*/, TrackViewFilter* /*filter*/ = NULL,
			DWORD /*pickTrackFlags*/ = 0) override
	{
		return FALSE;
	}
	BOOL TrackViewPickMultiDlg(HWND /*hParent*/, MaxSDK::Array<TrackViewPick>* /*res*/,
			TrackViewFilter* /*filter*/ = NULL, DWORD /*pickTrackFlags*/ = 0) override
	{
		return FALSE;
	}
	void PushCommandMode(CommandMode* /*m*/) override
	{
	}
	void SetCommandMode(CommandMode* /*m*/) override
	{
	}
	void PopCommandMode() override
	{
	}
	CommandMode* GetCommandMode() override
	{
		return nullptr;
	}
	void SetStdCommandMode(int /*cid*/) override
	{
	}
	void PushStdCommandMode(int /*cid*/) override
	{
	}
	void RemoveMode(CommandMode* /*m*/) override
	{
	}
	void DeleteMode(CommandMode* /*m*/) override
	{
	}
	PickModeCallback* GetCurPickMode() override
	{
		return nullptr;
	}
	BOOL DoHitByNameDialog(HitByNameDlgCallback* /*hbncb*/ = NULL) override
	{
		return FALSE;
	}
	void PushPrompt(const MCHAR* /*s*/) override
	{
	}
	void PopPrompt() override
	{
	}
	void ReplacePrompt(const MCHAR* /*s*/) override
	{
	}
	void DisplayTempPrompt(const MCHAR* /*s*/, int /*msec*/ = 1000) override
	{
	}
	void RemoveTempPrompt() override
	{
	}
	void DisableStatusXYZ() override
	{
	}
	void EnableStatusXYZ() override
	{
	}
	void SetStatusXYZ(Point3 /*xyz*/, int /*type*/) override
	{
	}
	void SetStatusXYZ(AngAxis /*aa*/) override
	{
	}
	void ChooseDirectory(HWND /*hWnd*/, const MCHAR* /*title*/, MCHAR* /*dir*/, MCHAR* /*desc*/ = NULL) override
	{
	}

#if MAX_RELEASE < 24900

	float GetAutoBackupTime() override
	{
		return 0.0f;
	}
	void SetAutoBackupTime(float /*minutes*/) override
	{
	}
	BOOL AutoBackupEnabled() override
	{
		return FALSE;
	}
	void EnableAutoBackup(BOOL /*onOff*/) override
	{
	}

	BOOL ProgressStart(const wchar_t*, BOOL, LPTHREAD_START_ROUTINE, LPVOID) override
	{
		return TRUE;
	}
	void ProgressUpdate(int /*pct*/, BOOL /*showPct*/ = TRUE, const MCHAR* /*title*/ = NULL) override
	{
	}
#endif

#if MAX_RELEASE >= 24900
	bool ProgressStart(const wchar_t*, bool, LPTHREAD_START_ROUTINE, LPVOID)
	{
		return TRUE;
	}

	bool ProgressStart(const wchar_t*, bool) override
	{
		return FALSE;
	}

	void ProgressUpdate(int, bool = true, const MCHAR* = nullptr) override
	{
	}
#endif

	void ProgressEnd() override
	{
	}
	BOOL GetCancel() override
	{
		return FALSE;
	}
	void SetCancel(BOOL /*sw*/) override
	{
	}
	void CreatePreview(PreviewParams* /*pvp*/ = NULL, MSTR* /*filename*/ = NULL, MSTR* /*snippet*/ = NULL,
			MAXScript::ScriptSource /*source*/ = MAXScript::ScriptSource::NotSpecified) override
	{
	}
	float GetGridSpacing() override
	{
		return 0.0f;
	}
	int GetGridMajorLines() override
	{
		return 0;
	}
	void SetExtendedDisplayMode(int /*vflags*/) override
	{
	}
	int GetExtendedDisplayMode() override
	{
		return 0;
	}
	void SetFlyOffTime(int /*msecs*/) override
	{
	}
	int GetFlyOffTime() override
	{
		return 0;
	}
	HCURSOR GetSysCursor(int /*id*/) override
	{
		return 0;
	}
	void SetCrossHairCur(BOOL /*onOff*/) override
	{
	}
	BOOL GetCrossHairCur() override
	{
		return FALSE;
	}
	void RealizeParamPanel() override
	{
	}
	float SnapAngle(float /*angleIn*/, BOOL /*fastSnap*/ = TRUE, BOOL /*forceSnap*/ = FALSE) override
	{
		return 0.0f;
	}
	float SnapPercent(float /*percentIn*/) override
	{
		return 0.0f;
	}
	BOOL GetSnapState() override
	{
		return FALSE;
	}
	int GetSnapMode() override
	{
		return 0;
	}
	BOOL SetSnapMode(int /*mode*/) override
	{
		return FALSE;
	}
	void SetPickMode(PickModeCallback* /*pCB*/) override
	{
	}
	void ClearPickMode() override
	{
	}
	INode* PickNode(HWND /*hWnd*/, IPoint2 /*pt*/, PickNodeCallback* /*filt*/ = NULL) override
	{
		return nullptr;
	}
	void BoxPickNode(ViewExp* /*vpt*/, IPoint2* /*pt*/, BOOL /*crossing*/, PickNodeCallback* /*filt*/ = NULL) override
	{
	}
	void CirclePickNode(
			ViewExp* /*vpt*/, IPoint2* /*pt*/, BOOL /*crossing*/, PickNodeCallback* /*filt*/ = NULL) override
	{
	}
	void FencePickNode(ViewExp* /*vpt*/, IPoint2* /*pt*/, BOOL /*crossing*/, PickNodeCallback* /*filt*/ = NULL) override
	{
	}
	void RegisterSubObjectTypes(const MCHAR** /*types*/, int /*count*/, int /*startIndex*/ = 0) override
	{
	}
	void AppendSubObjectNamedSelSet(const MCHAR* /*set*/) override
	{
	}
	void ClearSubObjectNamedSelSets() override
	{
	}
	void ClearCurNamedSelSet() override
	{
	}
	void SetCurNamedSelSet(const MCHAR* /*setName*/) override
	{
	}
	void NamedSelSetListChanged() override
	{
	}
	int GetSubObjectLevel() override
	{
		return 0;
	}
	void SetSubObjectLevel(int /*level*/, BOOL /*force*/ = FALSE) override
	{
	}
	int GetNumSubObjectLevels() override
	{
		return 0;
	}
	void PipeSelLevelChanged() override
	{
	}
	void GetPipelineSubObjLevel(DWORDTab& /*levels*/) override
	{
	}
	int SubObHitTest(
			TimeValue /*t*/, int /*type*/, int /*crossing*/, int /*vflags*/, IPoint2* /*p*/, ViewExp* /*vpt*/) override
	{
		return 0;
	}
	void GetModContexts(ModContextList& /*list*/, INodeTab& /*nodes*/) override
	{
	}
	BaseObject* GetCurEditObject() override
	{
		return nullptr;
	}
	BOOL SelectionFrozen() override
	{
		return FALSE;
	}
	void FreezeSelection() override
	{
	}
	void ThawSelection() override
	{
	}
	INode* GetSelNode(int /*i*/) override
	{
		return nullptr;
	}
	int GetSelNodeCount() override
	{
		return 0;
	}
	void EnableShowEndResult(BOOL /*enabled*/) override
	{
	}
	BOOL GetShowEndResult() override
	{
		return FALSE;
	}
	void SetShowEndResult(BOOL /*show*/) override
	{
	}
	BOOL GetCrossing() override
	{
		return FALSE;
	}
	void SetToolButtonState(int /*button*/, BOOL /*state*/) override
	{
	}
	BOOL GetToolButtonState(int /*button*/) override
	{
		return FALSE;
	}
	void EnableToolButton(int /*button*/, BOOL /*enable*/ = TRUE) override
	{
	}
	void EnableUndo(BOOL /*enable*/) override
	{
	}
	int GetCommandPanelTaskMode() override
	{
		return 0;
	}
	void SetCommandPanelTaskMode(int /*mode*/) override
	{
	}
	ViewExp& GetViewExp(HWND /*hwnd*/) override
	{
		return MockViewExp;
	}
	void EnableAnimateButton(BOOL /*enable*/) override
	{
	}
	BOOL IsAnimateEnabled() override
	{
		return FALSE;
	}
	void SetAnimateButtonState(BOOL /*onOff*/) override
	{
	}
	bool InProgressiveMode() override
	{
		return false;
	}
	void BeginProgressiveMode() override
	{
	}
	void EndProgressiveMode() override
	{
	}
	void RegisterAxisChangeCallback(AxisChangeCallback* /*cb*/) override
	{
	}
	void UnRegisterAxisChangeCallback(AxisChangeCallback* /*cb*/) override
	{
	}
	int GetAxisConstraints() override
	{
		return 0;
	}
	void SetAxisConstraints(int /*c*/) override
	{
	}
	void EnableAxisConstraints(int /*c*/, BOOL /*enabled*/) override
	{
	}
	void PushAxisConstraints(int /*c*/) override
	{
	}
	void PopAxisConstraints() override
	{
	}
	int GetCoordCenter() override
	{
		return 0;
	}
	void SetCoordCenter(int /*c*/) override
	{
	}
	void EnableCoordCenter(BOOL /*enabled*/) override
	{
	}
	int GetRefCoordSys() override
	{
		return 0;
	}
	void SetRefCoordSys(int /*c*/) override
	{
	}
	void EnableRefCoordSys(BOOL /*enabled*/) override
	{
	}
	int GetSelectFilter() override
	{
		return 0;
	}
	void SetSelectFilter(int /*c*/) override
	{
	}
	int GetNumberSelectFilters() override
	{
		return 0;
	}
	const MCHAR* GetSelectFilterName(int /*index*/) override
	{
		return nullptr;
	}
	BOOL GetDisplayFilter(int /*index*/) override
	{
		return FALSE;
	}
	void SetDisplayFilter(int /*index*/, BOOL /*on*/) override
	{
	}
	int GetNumberDisplayFilters() override
	{
		return 0;
	}
#if MAX_RELEASE < 25900
	BOOL DisplayFilterIsNodeVisible(int /*index*/, int /*sid*/, Class_ID /*cid*/, INode* /*node*/) override
#else
	BOOL DisplayFilterIsNodeHidden(int /*index*/, int /*sid*/, Class_ID /*cid*/, INode* /*node*/) override
#endif
	{
		return FALSE;
	}
	const MCHAR* GetDisplayFilterName(int /*index*/) override
	{
		return nullptr;
	}
	Matrix3 GetTransformAxis(INode* /*node*/, int /*subIndex*/, BOOL* /*local*/ = NULL) override
	{
		return Matrix3();
	}
	int GetNumAxis() override
	{
		return 0;
	}
	void LockAxisTripods(BOOL /*onOff*/) override
	{
	}
	BOOL AxisTripodLocked() override
	{
		return FALSE;
	}
	void RegisterDlgWnd(HWND /*hDlg*/) override
	{
	}
	int UnRegisterDlgWnd(HWND /*hDlg*/) override
	{
		return 0;
	}
	void RegisterAccelTable(HWND /*hWnd*/, HACCEL /*hAccel*/) override
	{
	}
	int UnRegisterAccelTable(HWND /*hWnd*/, HACCEL /*hAccel*/) override
	{
		return 0;
	}
	IActionManager* GetActionManager() override
	{
		return nullptr;
	}
#if MAX_RELEASE < 26900
	IMenuManager* GetMenuManager() override
	{
		return nullptr;
	}
#endif
#if MAX_RELEASE >= 26900
	MaxSDK::CUI::ICuiMenuManager* GetICuiMenuManager()
	{
		return nullptr;
	}
	MaxSDK::CUI::ICuiQuadMenuManager* GetICuiQuadMenuManager()
	{
		return nullptr;
	}
#endif
	HWND AddRollupPage(HINSTANCE /*hInst*/, const MCHAR* /*dlgTemplate*/, DLGPROC /*dlgProc*/, const MCHAR* /*title*/,
			LPARAM /*param*/ = 0, DWORD /*rollUpFlags*/ = 0, int /*category*/ = ROLLUP_CAT_STANDARD) override
	{
		return nullptr;
	}
	void AddRollupPage(QWidget& /*qtWidget*/, const MCHAR* /*title*/, DWORD /*rollupFlags*/ = 0,
			int /*category*/ = ROLLUP_CAT_STANDARD) override
	{
	}
	HWND AddRollupPage(HINSTANCE /*hInst*/, DLGTEMPLATE* /*dlgTemplate*/, DLGPROC /*dlgProc*/, const MCHAR* /*title*/,
			LPARAM /*param*/ = 0, DWORD /*rollUpFlags*/ = 0, int /*category*/ = ROLLUP_CAT_STANDARD) override
	{
		return nullptr;
	}
	void DeleteRollupPage(HWND /*hRollup*/) override
	{
	}
	void DeleteRollupPage(QWidget& /*qtWidget*/) override
	{
	}
	HWND ReplaceRollupPage(HWND /*hOldRollup*/, HINSTANCE /*hInst*/, const MCHAR* /*dlgTemplate*/, DLGPROC /*dlgProc*/,
			const MCHAR* /*title*/, LPARAM /*param*/ = 0, DWORD /*rollUpFlags*/ = 0,
			int /*category*/ = ROLLUP_CAT_STANDARD) override
	{
		return nullptr;
	}
	HWND ReplaceRollupPage(HWND /*hOldRollup*/, HINSTANCE /*hInst*/, DLGTEMPLATE* /*dlgTemplate*/, DLGPROC /*dlgProc*/,
			const MCHAR* /*title*/, LPARAM /*param*/ = 0, DWORD /*rollUpFlags*/ = 0,
			int /*category*/ = ROLLUP_CAT_STANDARD) override
	{
		return nullptr;
	}
	IRollupWindow* GetCommandPanelRollup() override
	{
		return nullptr;
	}
	void RollupMouseMessage(HWND /*hDlg*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/) override
	{
	}
	TimeValue GetTime() override
	{
		return 0;
	}
	void SetTime(TimeValue /*t*/, BOOL /*redraw*/ = TRUE) override
	{
	}
	Interval GetAnimRange() override
	{
		return NEVER;
	}
	void SetAnimRange(Interval /*range*/) override
	{
	}
	void RegisterTimeChangeCallback(TimeChangeCallback* /*tc*/) override
	{
	}
	void UnRegisterTimeChangeCallback(TimeChangeCallback* /*tc*/) override
	{
	}
	void RegisterCommandModeChangedCallback(CommandModeChangedCallback* /*cb*/) override
	{
	}
	void UnRegisterCommandModeChangedCallback(CommandModeChangedCallback* /*cb*/) override
	{
	}
	void RegisterViewportDisplayCallback(BOOL /*preScene*/, ViewportDisplayCallback* /*cb*/) override
	{
	}
	void UnRegisterViewportDisplayCallback(BOOL /*preScene*/, ViewportDisplayCallback* /*cb*/) override
	{
	}
	void NotifyViewportDisplayCallbackChanged(BOOL /*preScene*/, ViewportDisplayCallback* /*cb*/) override
	{
	}
	void RegisterExitMAXCallback(ExitMAXCallback* /*cb*/) override
	{
	}
	void UnRegisterExitMAXCallback(ExitMAXCallback* /*cb*/) override
	{
	}
#if MAX_RELEASE < 27900
	RightClickMenuManager* GetRightClickMenuManager() override
	{
		return nullptr;
	}
#endif
	void RegisterDeleteUser(EventUser* /*user*/) override
	{
	}
	void UnRegisterDeleteUser(EventUser* /*user*/) override
	{
	}
	void MakeNameUnique(MSTR& /*name*/) override
	{
	}
	INode* CreateObjectNode(Object* /*obj*/) override
	{
		return nullptr;
	}
	GenCamera* CreateCameraObject(int /*type*/) override
	{
		return nullptr;
	}
	Object* CreateTargetObject() override
	{
		return nullptr;
	}
	GenLight* CreateLightObject(int /*type*/) override
	{
		return nullptr;
	}
	int BindToTarget(INode* /*laNode*/, INode* /*targNode*/) override
	{
		return 0;
	}
	int IsCPEdgeOnInView() override
	{
		return 0;
	}
	unsigned int DeleteNode(INode* /*node*/, BOOL /*redraw*/ = TRUE, BOOL /*overrideSlaves*/ = FALSE) override
	{
		return 0;
	}
	INode* GetRootNode() override
	{
		return nullptr;
	}
#if MAX_RELEASE < 25900
	void SetNodeTMRelConstPlane(INode* /*node*/, Matrix3& /*mat*/) override
#else
	void SetNodeTMRelConstPlane(INode* /*node*/, const Matrix3& /*mat*/) override
#endif
	{
	}
	void SelectNode(INode* /*node*/, int /*clearSel*/ = 1) override
	{
	}
	void DeSelectNode(INode* /*node*/) override
	{
	}
	void SelectNodeTab(INodeTab& /*nodes*/, BOOL /*sel*/, BOOL /*redraw*/ = TRUE) override
	{
	}
	void ClearNodeSelection(BOOL /*redraw*/ = TRUE) override
	{
	}
	void AddLightToScene(INode* /*node*/) override
	{
	}
	float GetLightConeConstraint() override
	{
		return 0.0f;
	}
	void AddGridToScene(INode* /*node*/) override
	{
	}
	void SetActiveGrid(INode* /*node*/) override
	{
	}
	INode* GetActiveGrid() override
	{
		return nullptr;
	}
	void StopCreating() override
	{
	}
	Object* NonMouseCreate(Matrix3 /*tm*/) override
	{
		return nullptr;
	}
	void NonMouseCreateFinish(Matrix3 /*tm*/) override
	{
	}
	MCHAR_TO_MSTR GetDir(int /*which*/) override
	{
#if MAX_RELEASE >= 26900
		static MSTR dir(MaxUsd::UsdStringToMaxString(TestUtils::GetOutputDirectory()));
		return dir;
#else
		static WStr dir = MaxUsd::UsdStringToMaxString(TestUtils::GetOutputDirectory());
		return dir;
#endif
	}
	int GetPlugInEntryCount() override
	{
		return 0;
	}
	MCHAR_TO_MSTR GetPlugInDesc(int /*i*/) override
	{
		return MCHAR_TO_MSTR_RET;
	}
	MCHAR_TO_MSTR GetPlugInDir(int /*i*/) override
	{
		return MCHAR_TO_MSTR_RET;
	}
	int GetAssetDirCount(MaxSDK::AssetManagement::AssetType /*assetType*/) override
	{
		return 0;
	}
	MCHAR_TO_MSTR GetAssetDir(int /*i*/, MaxSDK::AssetManagement::AssetType /*assetType*/) override
	{
		return MCHAR_TO_MSTR_RET;
	}
	BOOL AddAssetDir(const MCHAR* /*dir*/, MaxSDK::AssetManagement::AssetType /*assetType*/) override
	{
		return FALSE;
	}
	BOOL AddSessionAssetDir(
			const MCHAR* /*dir*/, MaxSDK::AssetManagement::AssetType /*assetType*/, int /*update*/ = TRUE) override
	{
		return FALSE;
	}
	int GetSessionAssetDirCount(MaxSDK::AssetManagement::AssetType /*assetType*/) override
	{
		return 0;
	}
	MCHAR_TO_MSTR GetSessionAssetDir(int /*i*/, MaxSDK::AssetManagement::AssetType /*assetType*/) override
	{
		return MCHAR_TO_MSTR_RET;
	}
	BOOL DeleteSessionAssetDir(
			int /*i*/, MaxSDK::AssetManagement::AssetType /*assetType*/, int /*update*/ = TRUE) override
	{
		return FALSE;
	}
	int GetCurAssetDirCount(MaxSDK::AssetManagement::AssetType /*assetType*/) override
	{
		return 0;
	}
	MCHAR_TO_MSTR GetCurAssetDir(int /*i*/, MaxSDK::AssetManagement::AssetType /*assetType*/) override
	{
		return MCHAR_TO_MSTR_RET;
	}
	int DoExclusionListDialog(ExclList* /*nl*/, BOOL /*doShadows*/ = TRUE) override
	{
		return 0;
	}
	void ConvertNameTabToExclList(const NameTab* /*nt*/, ExclList* /*excList*/) override
	{
	}
	MtlBase* DoMaterialBrowseDlg(HWND /*hParent*/, DWORD /*vflags*/, BOOL& /*newMat*/, BOOL& /*cancel*/) override
	{
		return nullptr;
	}
	void PutMtlToMtlEditor(MtlBase* /*mb*/, int /*slot*/ = -1) override
	{
	}
	MtlBase* GetMtlSlot(int /*slot*/) override
	{
		return nullptr;
	}
	MtlBaseLib* GetSceneMtls() override
	{
		return nullptr;
	}
	BOOL OkMtlForScene(MtlBase* /*m*/) override
	{
		return FALSE;
	}
	MSTR& GetCurFileName() override
	{
		return SomeMSTR;
	}
	MSTR& GetCurFilePath() override
	{
		return SomeMSTR;
	}
	MCHAR_TO_MSTR GetMatLibFileName() override
	{
		return MCHAR_TO_MSTR_RET;
	}
	void FileOpen() override
	{
	}
	BOOL FileSave() override
	{
		return FALSE;
	}
	BOOL FileSaveAs() override
	{
		return FALSE;
	}
	void FileSaveSelected() override
	{
	}
	void FileReset(BOOL /*noPrompt*/ = FALSE) override
	{
	}
	void FileMerge() override
	{
	}
	void FileHold() override
	{
	}
	void FileFetch() override
	{
	}
	void FileOpenMatLib(HWND /*hWnd*/) override
	{
	}
	void FileSaveMatLib(HWND /*hWnd*/) override
	{
	}
	void FileSaveAsMatLib(HWND /*hWnd*/) override
	{
	}
	void LoadDefaultMatLib() override
	{
	}
	int LoadFromFile(const MCHAR* /*name*/, BOOL /*refresh*/ = TRUE) override
	{
		return 0;
	}
	int SaveToFile(const MCHAR* /*fname*/, BOOL /*clearNeedSaveFlag*/ = TRUE, BOOL /*useNewFile*/ = TRUE) override
	{
		return 0;
	}
	void FileSaveSelected(const MCHAR* /*fname*/) override
	{
	}
	void FileSaveNodes(INodeTab* /*nodes*/, const MCHAR* /*fname*/) override
	{
	}
	int LoadMaterialLib(const MCHAR* /*name*/, MtlBaseLib* /*lib*/ = NULL) override
	{
		return 0;
	}
	int SaveMaterialLib(const MCHAR* /*name*/, MtlBaseLib* /*lib*/ = NULL) override
	{
		return 0;
	}
#if MAX_RELEASE < 25900
	int MergeFromFile(const MCHAR* /*name*/, BOOL /*mergeAll*/ = FALSE, BOOL /*selMerged*/ = FALSE,
			BOOL /*refresh*/ = TRUE, int /*dupAction*/ = MERGE_DUPS_PROMPT, NameTab* /*mrgList*/ = NULL,
			int /*dupMtlAction*/ = MERGE_DUP_MTL_PROMPT, int /*reparentAction*/ = MERGE_REPARENT_PROMPT,
			BOOL /*includeFullGroup*/ = FALSE) override
#else
	int MergeFromFile(const MCHAR* /*name*/, BOOL /*mergeAll*/ = FALSE, BOOL /*selMerged*/ = FALSE,
			BOOL /*refresh*/ = TRUE, int /*dupAction*/ = MERGE_DUPS_PROMPT, NameTab* /*mrgList*/ = NULL,
			int /*dupMtlAction*/ = MERGE_DUP_MTL_PROMPT, int /*reparentAction*/ = MERGE_REPARENT_PROMPT,
			BOOL /*includeFullGroup*/ = FALSE, MaxSDK::Array<MaxRefEntryData>* /*dataList*/ = nullptr) override
#endif
	{
		return 0;
	}
	BOOL FileImport() override
	{
		return FALSE;
	}
	BOOL FileExport() override
	{
		return FALSE;
	}
	BOOL ImportFromFile(
			const MCHAR* /*name*/, BOOL /*suppressPrompts*/ = FALSE, Class_ID* /*importerID*/ = NULL) override
	{
		return FALSE;
	}
	BOOL ExportToFile(const MCHAR* /*name*/, BOOL /*suppressPrompts*/ = FALSE, DWORD /*options*/ = 0,
			Class_ID* /*exporterID*/ = NULL) override
	{
		return FALSE;
	}
	BOOL NodeColorPicker(HWND /*hWnd*/, DWORD& /*col*/) override
	{
		return FALSE;
	}
	INode* GroupNodes(INodeTab* /*nodes*/ = NULL, MSTR* /*name*/ = NULL, BOOL /*selGroup*/ = TRUE) override
	{
		return nullptr;
	}
	void UngroupNodes(INodeTab* /*nodes*/ = NULL) override
	{
	}
	void ExplodeNodes(INodeTab* /*nodes*/ = NULL) override
	{
	}
	void OpenGroup(INodeTab* /*nodes*/ = NULL, BOOL /*clearSel*/ = TRUE) override
	{
	}
	void CloseGroup(INodeTab* /*nodes*/ = NULL, BOOL /*selGroup*/ = TRUE) override
	{
	}
	bool AttachNodesToGroup(INodeTab& /*nodes*/, INode& /*pTargetNode*/) override
	{
		return false;
	}
	bool DetachNodesFromGroup(INodeTab& /*nodes*/) override
	{
		return false;
	}
	void FlashNodes(INodeTab* /*nodes*/) override
	{
	}
	void TranslateAndDispatchMAXMessage(MSG& /*msg*/) override
	{
	}
	BOOL CheckMAXMessages() override
	{
		return FALSE;
	}
	BOOL setBkgImageAsset(const MaxSDK::AssetManagement::AssetUser& /*asset*/) override
	{
		return FALSE;
	}
	const MaxSDK::AssetManagement::AssetUser& getBkgImageAsset(void) override
	{
		return SomeAssetUser;
	}
	void setBkgImageAspect(int /*t*/) override
	{
	}
	int getBkgImageAspect() override
	{
		return 0;
	}
	void setBkgImageAnimate(BOOL /*onOff*/) override
	{
	}
	int getBkgImageAnimate(void) override
	{
		return 0;
	}
	void setBkgFrameRange(int /*start*/, int /*end*/, int /*step*/ = 1) override
	{
	}
	int getBkgFrameRangeVal(int /*which*/) override
	{
		return 0;
	}
	void setBkgORType(int /*which*/, int /*type*/) override
	{
	}
	int getBkgORType(int /*which*/) override
	{
		return 0;
	}
	void setBkgStartTime(TimeValue /*t*/) override
	{
	}
	TimeValue getBkgStartTime() override
	{
		return 0;
	}
	void setBkgSyncFrame(int /*f*/) override
	{
	}
	int getBkgSyncFrame() override
	{
		return 0;
	}
	int getBkgFrameNum(TimeValue /*t*/) override
	{
		return 0;
	}
	BOOL GetRealTimePlayback() override
	{
		return FALSE;
	}
	void SetRealTimePlayback(BOOL /*realTime*/) override
	{
	}
	BOOL GetPlayActiveOnly() override
	{
		return FALSE;
	}
	void SetPlayActiveOnly(BOOL /*playActive*/) override
	{
	}
	void StartAnimPlayback(int /*selOnly*/ = FALSE) override
	{
	}
	void EndAnimPlayback() override
	{
	}
	BOOL IsAnimPlaying() override
	{
		return FALSE;
	}
	int GetPlaybackSpeed() override
	{
		return 0;
	}
	void SetPlaybackSpeed(int /*s*/) override
	{
	}
	BOOL GetPlaybackLoop() override
	{
		return FALSE;
	}
	void SetPlaybackLoop(BOOL /*loop*/) override
	{
	}
	void IncrementValidityToken() override
	{
	}
	unsigned int CurrentValidityToken() override
	{
		return 0;
	}
	int OpenCurRenderer(INode* /*camNode*/, ViewExp* /*view*/, RendType /*t*/ = RENDTYPE_NORMAL, int /*w*/ = 0,
			int /*h*/ = 0) override
	{
		return 0;
	}
	int OpenCurRenderer(ViewParams* /*vpar*/, RendType /*t*/ = RENDTYPE_NORMAL, int /*w*/ = 0, int /*h*/ = 0) override
	{
		return 0;
	}
	void CloseCurRenderer() override
	{
	}
	int CurRendererRenderFrame(TimeValue /*t*/, Bitmap* /*bm*/, RendProgressCallback* /*prog*/ = NULL,
			float /*frameDur*/ = 1.0f, ViewParams* /*vp*/ = NULL, RECT* /*regionRect*/ = NULL) override
	{
		return 0;
	}
	IScanRenderer* CreateDefaultScanlineRenderer() override
	{
		return nullptr;
	}
	Renderer* CreateDefaultRenderer(RenderSettingID /*renderSettingID*/) override
	{
		return nullptr;
	}
	Class_ID GetDefaultRendererClassID(RenderSettingID /*renderSettingID*/) override
	{
		return Class_ID(0x0, 0x0);
	}
	void SetDefaultRendererClassID(RenderSettingID /*renderSettingID*/, Class_ID /*classID*/) override
	{
	}
	int OpenRenderer(Renderer* /*pRenderer*/, INode* /*camNode*/, ViewExp* /*view*/,
			RendType /*type*/ = RENDTYPE_NORMAL, int /*w*/ = 0, int /*h*/ = 0) override
	{
		return 0;
	}
	int OpenRenderer(Renderer* /*pRenderer*/, ViewParams* /*vpar*/, RendType /*type*/ = RENDTYPE_NORMAL, int /*w*/ = 0,
			int /*h*/ = 0) override
	{
		return 0;
	}
	int RendererRenderFrame(Renderer* /*pRenderer*/, TimeValue /*t*/, Bitmap* /*bm*/,
			RendProgressCallback* /*prog*/ = NULL, float /*frameDur*/ = 1.0f, ViewParams* /*vp*/ = NULL,
			RECT* /*regionRect*/ = NULL) override
	{
		return 0;
	}
	void CloseRenderer(Renderer* /*pRenderer*/) override
	{
	}
	Renderer* GetCurrentRenderer(bool /*createRendererIfItDoesntExist*/ = true) override
	{
		return nullptr;
	}
	Renderer* GetProductionRenderer(bool /*createRendererIfItDoesntExist*/ = true) override
	{
		return nullptr;
	}
	Renderer* GetDraftRenderer(bool /*createRendererIfItDoesntExist*/ = true) override
	{
		return nullptr;
	}
	void AssignCurRenderer(Renderer* /*rend*/) override
	{
	}
	void AssignDraftRenderer(Renderer* /*rend*/) override
	{
	}
	void AssignRenderer(RenderSettingID /*renderSettingID*/, Renderer* /*rend*/) override
	{
	}
	void SetUseDraftRenderer(BOOL /*b*/) override
	{
	}
	BOOL GetUseDraftRenderer() override
	{
		return FALSE;
	}
	void ChangeRenderSetting(RenderSettingID /*renderSettingID*/) override
	{
	}
	RenderSettingID GetCurrentRenderSetting() override
	{
		return RenderSettingID::RS_Production;
	}
	Renderer* GetMEditRenderer(bool /*createRendererIfItDoesntExist*/ = true) override
	{
		return nullptr;
	}
	void AssignMEditRenderer(Renderer* /*renderer*/) override
	{
	}
	Renderer* GetActualMEditRenderer(bool /*createRendererIfItDoesntExist*/ = true) override
	{
		return nullptr;
	}
	bool GetMEditRendererLocked() override
	{
		return false;
	}
	void SetMEditRendererLocked(bool /*locked*/) override
	{
	}
	bool GetMEditRendererLocked_DefaultValue() override
	{
		return false;
	}
	void SetMEditRendererLocked_DefaultValue(bool /*locked*/) override
	{
	}
	IRenderElementMgr* GetCurRenderElementMgr() override
	{
		return nullptr;
	}
	IRenderElementMgr* GetRenderElementMgr(RenderSettingID /*renderSettingID*/) override
	{
		return nullptr;
	}
	void SetupRendParams(RendParams& /*rp*/, ViewExp* /*vpt*/, RendType /*t*/ = RENDTYPE_NORMAL) override
	{
	}
	void GetViewParamsFromNode(INode* /*vnode*/, ViewParams& /*vp*/, TimeValue /*t*/) override
	{
	}
	BOOL CheckForRenderAbort() override
	{
		return FALSE;
	}
	void AbortRender() override
	{
	}
	int GetRendTimeType() override
	{
		return 0;
	}
	void SetRendTimeType(int /*type*/) override
	{
	}
	TimeValue GetRendStart() override
	{
		return 0;
	}
	void SetRendStart(TimeValue /*start*/) override
	{
	}
	TimeValue GetRendEnd() override
	{
		return 0;
	}
	void SetRendEnd(TimeValue /*end*/) override
	{
	}
	int GetRendNThFrame() override
	{
		return 0;
	}
	void SetRendNThFrame(int /*n*/) override
	{
	}
	BOOL GetRendShowVFB() override
	{
		return FALSE;
	}
	void SetRendShowVFB(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendSaveFile() override
	{
		return FALSE;
	}
	void SetRendSaveFile(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendUseDevice() override
	{
		return FALSE;
	}
	void SetRendUseDevice(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendUseNet() override
	{
		return FALSE;
	}
	void SetRendUseNet(BOOL /*onOff*/) override
	{
	}
	BitmapInfo& GetRendFileBI() override
	{
		return SomeBitmapInfo;
	}
	BitmapInfo& GetRendDeviceBI() override
	{
		return SomeBitmapInfo;
	}
	int GetRendWidth() override
	{
		return 0;
	}
	void SetRendWidth(int /*w*/) override
	{
	}
	int GetRendHeight() override
	{
		return 0;
	}
	void SetRendHeight(int /*h*/) override
	{
	}
#if MAX_RELEASE < 25900
	float GetRendApect() override
#else
	float GetRendPixelAspect() override
#endif
	{
		return 0.0f;
	}
#if MAX_RELEASE < 25900
	void SetRendAspect(float /*a*/) override
#else
	void SetRendPixelAspect(float /*a*/) override
#endif
	{
	}
	float GetRendImageAspect() override
	{
		return 0.0f;
	}
	float GetRendApertureWidth() override
	{
		return 0.0f;
	}
	void SetRendApertureWidth(float /*aw*/) override
	{
	}
	BOOL GetRendFieldRender() override
	{
		return FALSE;
	}
	void SetRendFieldRender(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendColorCheck() override
	{
		return FALSE;
	}
	void SetRendColorCheck(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendSuperBlack() override
	{
		return FALSE;
	}
	void SetRendSuperBlack(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendHidden() override
	{
		return FALSE;
	}
	void SetRendHidden(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendForce2Side() override
	{
		return FALSE;
	}
	void SetRendForce2Side(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendAtmosphere() override
	{
		return FALSE;
	}
	void SetRendAtmosphere(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendEffects() override
	{
		return FALSE;
	}
	void SetRendEffects(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendDisplacement() override
	{
		return FALSE;
	}
	void SetRendDisplacement(BOOL /*onOff*/) override
	{
	}
	MSTR& GetRendPickFramesString() override
	{
		return SomeMSTR;
	}
	BOOL GetRendDitherTrue() override
	{
		return FALSE;
	}
	void SetRendDitherTrue(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendDither256() override
	{
		return FALSE;
	}
	void SetRendDither256(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendMultiThread() override
	{
		return FALSE;
	}
	void SetRendMultiThread(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendNThSerial() override
	{
		return FALSE;
	}
	void SetRendNThSerial(BOOL /*onOff*/) override
	{
	}
	int GetRendVidCorrectMethod() override
	{
		return 0;
	}
	void SetRendVidCorrectMethod(int /*m*/) override
	{
	}
	int GetRendFieldOrder() override
	{
		return 0;
	}
	void SetRendFieldOrder(int /*fo*/) override
	{
	}
	int GetRendNTSC_PAL() override
	{
		return 0;
	}
	void SetRendNTSC_PAL(int /*np*/) override
	{
	}
	int GetRendSuperBlackThresh() override
	{
		return 0;
	}
	void SetRendSuperBlackThresh(int /*sb*/) override
	{
	}
	int GetRendFileNumberBase() override
	{
		return 0;
	}
	void SetRendFileNumberBase(int /*n*/) override
	{
	}
	BOOL GetSkipRenderedFrames() override
	{
		return FALSE;
	}
	void SetSkipRenderedFrames(BOOL /*onOff*/) override
	{
	}
	BOOL GetRendSimplifyAreaLights() override
	{
		return FALSE;
	}
	void SetRendSimplifyAreaLights(BOOL /*onOff*/) override
	{
	}
	DWORD GetHideByCategoryFlags() override
	{
		return 0;
	}
	void SetHideByCategoryFlags(DWORD /*f*/) override
	{
	}
	int GetViewportLayout() override
	{
		return 0;
	}
	void SetViewportLayout(int /*layout*/) override
	{
	}
	BOOL IsViewportMaxed() override
	{
		return FALSE;
	}
	void SetViewportMax(BOOL /*max*/) override
	{
	}
	void ViewportZoomExtents(BOOL /*doAll*/, BOOL /*skipPersp*/ = FALSE) override
	{
	}
	void ZoomToBounds(BOOL /*doAll*/, Box3 /*box*/) override
	{
	}
	void GetSelectionWorldBox(TimeValue /*t*/, Box3& /*box*/) override
	{
	}
	INode* GetINodeByName(const MCHAR* /*name*/) override
	{
		return nullptr;
	}
	INode* GetINodeByHandle(ULONG /*handle*/) override
	{
		return nullptr;
	}
	INode* GetINodeFromRenderID(UWORD /*id*/) override
	{
		return nullptr;
	}
	void ExecuteMAXCommand(int /*id*/) override
	{
	}
	NameMaker* NewNameMaker(BOOL /*initFromScene*/ = TRUE) override
	{
		return nullptr;
	}
	void SetViewportBGColor(const Point3& /*color*/) override
	{
	}
	Point3 GetViewportBGColor() override
	{
		return nullptr;
	}
	Texmap* GetEnvironmentMap() override
	{
		return nullptr;
	}
	void SetEnvironmentMap(Texmap* /*map*/) override
	{
	}
	BOOL GetUseEnvironmentMap() override
	{
		return FALSE;
	}
	void SetUseEnvironmentMap(BOOL /*onOff*/) override
	{
	}
	Point3 GetAmbient(TimeValue /*t*/, Interval& /*valid*/) override
	{
		return nullptr;
	}
	void SetAmbient(TimeValue /*t*/, Point3 /*col*/) override
	{
	}
	Control* GetAmbientController() override
	{
		return nullptr;
	}
	void SetAmbientController(Control* /*c*/) override
	{
	}
	Point3 GetLightTint(TimeValue /*t*/, Interval& /*valid*/) override
	{
		return nullptr;
	}
	void SetLightTint(TimeValue /*t*/, Point3 /*col*/) override
	{
	}
	Control* GetLightTintController() override
	{
		return nullptr;
	}
	void SetLightTintController(Control* /*c*/) override
	{
	}
	float GetLightLevel(TimeValue /*t*/, Interval& /*valid*/) override
	{
		return 0.0f;
	}
	void SetLightLevel(TimeValue /*t*/, float /*lev*/) override
	{
	}
	Control* GetLightLevelController() override
	{
		return nullptr;
	}
	void SetLightLevelController(Control* /*c*/) override
	{
	}
	int NumAtmospheric() override
	{
		return 0;
	}
	Atmospheric* GetAtmospheric(int /*i*/) override
	{
		return nullptr;
	}
	void SetAtmospheric(int /*i*/, Atmospheric* /*a*/) override
	{
	}
	void AddAtmosphere(Atmospheric* /*atmos*/) override
	{
	}
	void DeleteAtmosphere(int /*i*/) override
	{
	}
	void EditAtmosphere(Atmospheric* /*a*/, INode* /*gizmo*/ = NULL) override
	{
	}
	Point3 GetBackGround(TimeValue /*t*/, Interval& /*valid*/) override
	{
		return nullptr;
	}
	void SetBackGround(TimeValue /*t*/, Point3 /*col*/) override
	{
	}
	Control* GetBackGroundController() override
	{
		return nullptr;
	}
	void SetBackGroundController(Control* /*c*/) override
	{
	}
	int NumEffects() override
	{
		return 0;
	}
	Effect* GetEffect(int /*i*/) override
	{
		return nullptr;
	}
	void SetEffect(int /*i*/, Effect* /*e*/) override
	{
	}
	void AddEffect(Effect* /*eff*/) override
	{
	}
	void DeleteEffect(int /*i*/) override
	{
	}
	void EditEffect(Effect* /*e*/, INode* /*gizmo*/ = NULL) override
	{
	}
	SoundObj* GetSoundObject() override
	{
		return nullptr;
	}
	void SetSoundObject(SoundObj* /*snd*/) override
	{
	}
	IOsnapManager* GetOsnapManager() override
	{
		return nullptr;
	}
	MouseManager* GetMouseManager() override
	{
		return nullptr;
	}
	void InvalidateOsnapdraw() override
	{
	}
	MtlBaseLib& GetMaterialLibrary() override
	{
#if MAX_RELEASE < 26900
		return SomeMtlBaseLib;
#else
		return *SomeMtlBaseLib;
#endif
	}
	void AssignNewName(Mtl* /*m*/) override
	{
	}
	void AssignNewName(Texmap* /*m*/) override
	{
	}
	bool IsNetworkRenderServer() const override
	{
		return false;
	}
	LogSys* Log() override
	{
		return &SomeLog;
	}
	INT_PTR Execute(int /*cmd*/, ULONG_PTR /*arg1*/ = 0, ULONG_PTR /*arg2*/ = 0, ULONG_PTR /*arg3*/ = 0,
			ULONG_PTR /*arg4*/ = 0, ULONG_PTR /*arg5*/ = 0, ULONG_PTR /*arg6*/ = 0) override
	{
		return 0;
	}
	void* GetInterface(DWORD /*id*/) override
	{
		return nullptr;
	}
	BaseInterface* GetInterface(Interface_ID /*id*/) override
	{
		return nullptr;
	}
	ReferenceTarget* GetScenePointer() override
	{
		return nullptr;
	}
	ITrackViewNode* GetTrackViewRootNode() override
	{
		return nullptr;
	}
	void FreeSceneBitmaps() override
	{
	}
	void EnumAuxFiles(AssetEnumCallback& /*assetEnum*/, DWORD /*vflags*/) override
	{
	}
	void RenderTexmap(Texmap* /*tex*/, Bitmap* /*bm*/, float /*scale3d*/ = 1.0f, BOOL /*filter*/ = FALSE,
			BOOL /*display*/ = FALSE, float /*z*/ = 0.0f, TimeValue /*t*/ = GetCOREInterface()->GetTime(),
			bool /*bake*/ = false) override
	{
	}
#if MAX_RELEASE >= 26900 
	void RescaleWorldUnits(float /*f*/, BOOL /*selected*/, Tab<INode*>* /*nodes*/) override
#else
	void RescaleWorldUnits(float /*f*/, BOOL /*selected*/) override
#endif
	{
	}
	int InitSnapInfo(SnapInfo* /*info*/) override
	{
		return 0;
	}
	BOOL GetKeyStepsSelOnly() override
	{
		return FALSE;
	}
	void SetKeyStepsSelOnly(BOOL /*onOff*/) override
	{
	}
	BOOL GetKeyStepsUseTrans() override
	{
		return FALSE;
	}
	void SetKeyStepsUseTrans(BOOL /*onOff*/) override
	{
	}
	BOOL GetKeyStepsPos() override
	{
		return FALSE;
	}
	void SetKeyStepsPos(BOOL /*onOff*/) override
	{
	}
	BOOL GetKeyStepsRot() override
	{
		return FALSE;
	}
	void SetKeyStepsRot(BOOL /*onOff*/) override
	{
	}
	BOOL GetKeyStepsScale() override
	{
		return FALSE;
	}
	void SetKeyStepsScale(BOOL /*onOff*/) override
	{
	}
	BOOL GetKeyStepsUseTrackBar() override
	{
		return FALSE;
	}
	void SetKeyStepsUseTrackBar(BOOL /*onOff*/) override
	{
	}
	BOOL GetUseTransformGizmo() override
	{
		return FALSE;
	}
	void SetUseTransformGizmo(BOOL /*onOff*/) override
	{
	}
	void SetTransformGizmoRestoreAxis(BOOL /*bOnOff*/) override
	{
	}
	BOOL GetTransformGizmoRestoreAxis() override
	{
		return FALSE;
	}
	BOOL GetConstantAxisRestriction() override
	{
		return FALSE;
	}
	void SetConstantAxisRestriction(BOOL /*onOff*/) override
	{
	}
	int HitTestTransformGizmo(IPoint2* /*p*/, ViewExp* /*vpt*/, int /*axisFlags*/) override
	{
		return 0;
	}
	void DeactivateTransformGizmo() override
	{
	}
	int ConfigureBitmapPaths() override
	{
		return 0;
	}
	BOOL DoSpaceArrayDialog(SpaceArrayCallback* /*sacb*/ = NULL) override
	{
		return FALSE;
	}
	int AddClass(ClassDesc* /*pCD*/) override
	{
		return 0;
	}
	int DeleteClass(ClassDesc* /*pCD*/) override
	{
		return 0;
	}
	int GetCommandStackSize() override
	{
		return 0;
	}
	CommandMode* GetCommandStackEntry(int /*entry*/) override
	{
		return nullptr;
	}
	void AddSFXRollupPage(ULONG /*vflags*/ = 0) override
	{
	}
	void DeleteSFXRollupPage() override
	{
	}
	void RefreshSFXRollupPage() override
	{
	}
	int GetNumProperties(int /*PropertySet*/) override
	{
		return 0;
	}
	int FindProperty(int /*PropertySet*/, const PROPSPEC* /*propspec*/) override
	{
		return 0;
	}
	const PROPVARIANT* GetPropertyVariant(int /*PropertySet*/, int /*idx*/) override
	{
		return nullptr;
	}
	const PROPSPEC* GetPropertySpec(int /*PropertySet*/, int /*idx*/) override
	{
		return nullptr;
	}
	void AddProperty(int /*PropertySet*/, const PROPSPEC* /*propspec*/, const PROPVARIANT* /*propvar*/) override
	{
	}
	void DeleteProperty(int /*PropertySet*/, const PROPSPEC* /*propspec*/) override
	{
	}
	BOOL RegisterViewWindow(ViewWindow* /*vw*/) override
	{
		return FALSE;
	}
	BOOL UnRegisterViewWindow(ViewWindow* /*vw*/) override
	{
		return FALSE;
	}
	ShadowType* GetGlobalShadowGenerator() override
	{
		return nullptr;
	}
	void SetGlobalShadowGenerator(ShadowType* /*st*/) override
	{
	}
	BOOL GetImportZoomExtents() override
	{
		return FALSE;
	}
	void SetImportZoomExtents(BOOL /*onOff*/) override
	{
	}
	bool CanImportFile(const MCHAR* /*filename*/) override
	{
		return false;
	}
	ITrackBar* GetTrackBar() override
	{
		return nullptr;
	}
	void SetIncludeXRefsInHierarchy(BOOL /*onOff*/) override
	{
	}
	BOOL GetIncludeXRefsInHierarchy() override
	{
		return FALSE;
	}
	BOOL IsXRefAutoUpdateSuspended() override
	{
		return FALSE;
	}
	void SetXRefAutoUpdateSuspended(BOOL /*onOff*/) override
	{
	}
	BOOL IsSceneXRefNode(INode* /*node*/) override
	{
		return FALSE;
	}
	MacroRecorder* GetMacroRecorder() override
	{
		return nullptr;
	}
	void UpdateMtlEditorBrackets() override
	{
	}
	bool IsTrialLicense() override
	{
		return false;
	}
	bool IsNetworkLicense() override
	{
		return false;
	}
	BOOL CheckForSave() override
	{
		return FALSE;
	}
	void SetMAXFileOpenDlg(MAXFileOpenDialog* /*dlg*/) override
	{
	}
	void SetMAXFileSaveDlg(MAXFileSaveDialog* /*dlg*/) override
	{
	}
	void RAMPlayer(HWND /*hWndParent*/, const MCHAR* /*szChanA*/ = NULL, const MCHAR* /*szChanB*/ = NULL) override
	{
	}
	void FlushUndoBuffer() override
	{
	}
	bool DeferredPluginLoadingEnabled() override
	{
		return false;
	}
	void EnableDeferredPluginLoading(bool /*onOff*/) override
	{
	}
	bool IsMaxFile(const MCHAR* /*filename*/) override
	{
		return false;
	}
	bool IsInternetCachedFile(const MCHAR* /*filename*/) override
	{
		return false;
	}
	bool CanImportBitmap(const MCHAR* /*filename*/) override
	{
		return false;
	}
	bool CaptureSubObjectRegistration(bool /*OnOff*/, Class_ID /*cid*/) override
	{
		return false;
	}
	bool DownloadUrl(
			HWND /*hwnd*/, const MCHAR* /*url*/, const MCHAR* /*filename*/, DWORD /*downloadFlags*/ = 0) override
	{
		return false;
	}
	INode* GetImportCtxNode(void) override
	{
		return nullptr;
	}
	ITreeView* CreateTreeViewChild(ReferenceTarget* /*root*/, HWND /*hParent*/, DWORD /*style*/ = 0, ULONG /*id*/ = 0,
			int /*open*/ = OPENTV_SPECIAL) override
	{
		return nullptr;
	}
	void ConvertMtl(TimeValue /*t*/, Material& /*gm*/, Mtl* /*mtl*/, BOOL /*doTex*/, int /*subNum*/, float /*vis*/,
			BOOL& /*needDecal*/, INode* /*node*/, BitArray* /*needTex*/, GraphicsWindow* /*gw*/) override
	{
	}
	bool CloneNodes(INodeTab& /*nodes*/, Point3& /*offset*/, bool /*expandHierarchies*/ = true,
			CloneType /*cloneType*/ = NODE_COPY, INodeTab* /*resultSource*/ = NULL,
			INodeTab* /*resultTarget*/ = NULL) override
	{
		return false;
	}
	void CollapseNode(INode* /*node*/, BOOL /*noWarning*/ = FALSE) override
	{
	}
	BOOL CollapseNodeTo(INode* /*node*/, int /*modIndex*/, BOOL /*noWarning*/ = FALSE) override
	{
		return FALSE;
	}
	BOOL ConvertNode(INode* /*node*/, Class_ID& /*cid*/) override
	{
		return FALSE;
	}
	IRenderPresetsManager* GetRenderPresetsManager() override
	{
		return nullptr;
	}
	DefaultActionSys* DefaultActions() override
	{
		return nullptr;
	}
	BOOL SetQuietMode(BOOL /*onOff*/) override
	{
		return FALSE;
	}
	BOOL GetQuietMode(BOOL /*checkServerMode*/ = TRUE) override
	{
		return TRUE;
	}
	void SetAutoGridEnable(bool /*sw*/ = true) override
	{
	}
	bool GetAutoGridEnable() override
	{
		return false;
	}
	bool GetAutoGridState() override
	{
		return false;
	}
	void SetAutoGridState(bool /*sw*/ = true) override
	{
	}

	virtual void RenderTexmapRange(Texmap* tex, Bitmap* bm, FBox2* range, TimeValue t, float scale3d = 1.0f,
			BOOL filter = FALSE, BOOL display = FALSE, bool bake = false, const MCHAR* name = NULL, float z = 0.0f,
			BOOL mono = false, bool disableBitmapProxies = false) override{};
	virtual void SetPlayPreviewWhenDone(BOOL play) override{};
	virtual BOOL GetPlayPreviewWhenDone() override
	{
		return TRUE;
	}

	virtual bool ArchiveSceneFile(const MCHAR* archiveFileName, unsigned long saveAsVersion = MAX_RELEASE) override
	{
		return false;
	}

	virtual bool GetSceneFileCompressOnSave() override
	{
		return false;
	}
	virtual void SetSceneFileCompressOnSave(bool compress, bool persist) override
	{
	}


	virtual void SetEnableTMCache(bool value, bool commitToInit) override
	{
	}
	virtual bool GetEnableTMCache() const override
	{
		return false;
	}

	virtual bool InNonInteractiveMode(bool checkTestMode = false) const override
	{
		return true;
	}
	virtual void SetInNonInteractiveTestMode() const override
	{
	}

	virtual void SetInSecureMode() override
	{
	}
	virtual bool InSecureMode() const override
	{
		return false;
	}

	virtual bool IsRibbonSupportEnabled() const override
	{
		return false;
	}

	virtual void BreakSelectedKeyTangent() override
	{
	}

	virtual void UnifySelectedKeyTangent() override
	{
	}
	virtual void SetSelectedKeyTangentType(int type) override
	{
	}
	virtual PathColoringType GetTrajectoryPathGradation() const override
	{
		return kNoGradation;
	}
	virtual void SetTrajectoryPathGradation(PathColoringType type) override
	{
	}

	virtual bool GetUnselTrajectoryDrawFrameTicks() const override
	{
		return false;
	}
	virtual void SetUnselTrajectoryDrawFrameTicks(bool draw) override
	{
	}
	virtual bool GetUnselTrajectoryDrawKeys() const override
	{
		return false;
	}
	virtual void SetUnselTrajectoryDrawKeys(bool draw) override
	{
	}
	virtual bool TrimTrajectories() const override
	{
		return false;
	}
	virtual void SetTrimTrajectories(bool trim) override
	{
	}
	virtual bool IsFixedTrimRange() const override
	{
		return false;
	}
	virtual void SetFixedTrimRange(bool fixedRange) override
	{
	}
	virtual int TrimTrajectoriesOffset() const override
	{
		return 0;
	}
	virtual void SetTrimTrajectoriesOffset(int range) override
	{
	}
	virtual int GetTrimStart() const override
	{
		return 0;
	}
	virtual void SetTrimStart(int startFrame) override
	{
	}
	virtual int GetTrimEnd() const override
	{
		return 0;
	}
	virtual void SetTrimEnd(int endFrame) override
	{
	}

	virtual bool GetSelTrajectoryDrawAllTangents() const override
	{
		return false;
	}
	virtual void SetSelTrajectoryDrawAllTangents(bool draw) override
	{
	}
	virtual bool GetSelTrajectoryDrawFrameTicks() const override
	{
		return false;
	}
	virtual void SetSelTrajectoryDrawFrameTicks(bool draw) override
	{
	}
	virtual bool GetSelTrajectoryDrawGradientTicks() const override
	{
		return false;
	}
	virtual void SetSelTrajectoryDrawGradientTicks(bool draw) override
	{
	}
	virtual bool GetTrajectoryDrawKeyTimes() const override
	{
		return false;
	}
	virtual void SetTrajectoryDrawKeyTimes(bool draw) override
	{
	}
	virtual bool GetSelTrajectoryDrawKeys() const override
	{
		return false;
	}
	virtual void SetSelTrajectoryDrawKeys(bool draw) override
	{
	}
	virtual void UpdateSceneMaterialLib() override
	{
	}

	virtual int GetRendViewID(const RenderSettingID renderSettingID) override
	{
		return 0;
	}
	virtual void SetRendViewID(const RenderSettingID renderSettingID, const int viewportID) override
	{
	}
	virtual ViewExp* GetCurrentRenderView() override
	{
		return nullptr;
	}
	virtual void SetShowWorldAxis(BOOL state) override
	{
	}
	virtual BOOL GetShowWorldAxis() override
	{
		return FALSE;
	}
	virtual void SetOverrideRenderSettingMtl(MtlBase* pOverrideRenderSettingMtl) override
	{
	}
	virtual MtlBase* GetOverrideRenderSettingMtl() const override
	{
		return nullptr;
	}

	virtual DWORD GetMainThreadID() override
	{
		return 0;
	}
	virtual MSTR& GetCurTemplateAssetPath() override
	{
		static MSTR str;
		return str;
	}

	virtual ViewExp& GetViewExpByID(int viewID) override
	{
		return MockViewExp;
	}
	virtual int GetRendViewID() override
	{
		return 0;
	}
	virtual void SetRendViewID(int id) override
	{
	}
	virtual PreviewParams GetPreviewParams() const
	{
		return PreviewParams();
	}
	virtual bool OverrideLanguageSpecifiedInSceneFile() const override
	{
		return FALSE;
	}
	virtual void SetOverrideLanguageSpecifiedInSceneFile(bool overrideFileLangID) override
	{
	}
	virtual bool UseCodePageSpecifiedInSceneFile() const override
	{
		return FALSE;
	}
	virtual void SetUseCodePageSpecifiedInSceneFile(bool useFileCodePage) override
	{
	}
	virtual LANGID LanguageToUseForFileIO() const override
	{
		return 0;
	}
	virtual bool SetLanguageToUseForFileIO(LANGID id) override
	{
		return FALSE;
	}
	virtual UINT CodePageForLanguage(LANGID id) const override
	{
		return 0;
	}
	virtual bool LegacyFilesCanBeStoredUsingUTF8() const override
	{
		return FALSE;
	}
	virtual void SetLegacyFilesCanBeStoredUsingUTF8(bool allowUTF8) override
	{
	}
	virtual void ConvertAppDataChunksContainingStringToUTF8(Animatable* anim, const Class_ID& classID,
			SClass_ID superClassID, Tab<DWORD>& subIDs, UINT codePage) override
	{
	}
	virtual UINT DefaultTextLoadCodePage() override
	{
		return 0;
	}
	virtual UINT DefaultTextSaveCodePage(bool allow_utf8 = false) override
	{
		return 0;
	}


	virtual ClassDesc* DoPickClassDlg(
			HWND hParent, const MCHAR* title, SClass_ID super, PickClassCallback* pPickClassCB = NULL) override
	{
		return nullptr;
	}
	virtual int DoMultiplePickClassDlg(HWND hParent, const MCHAR* title, SClass_ID super,
			PickClassCallback* pPickClassCB, Tab<ClassDesc*>* classDescTab) override
	{
		return 0;
	}
	virtual BOOL GetRendUseIterative() override
	{
		return FALSE;
	}
	virtual void SetRendUseIterative(BOOL b) override
	{
	}
	virtual bool SaveSceneAsVersion(const MCHAR* fname, bool clearNeedSaveFlag = true, bool useNewFile = true,
			unsigned long saveAsVersion = MAX_RELEASE) override
	{
		return FALSE;
	}
	virtual bool SaveNodesAsVersion(
			const MCHAR* fname, const INodeTab* nodes, unsigned long saveAsVersion = MAX_RELEASE) override
	{
		return FALSE;
	}
	virtual bool SaveSelectedNodesAsVersion(const MCHAR* fname, unsigned long saveAsVersion = MAX_RELEASE) override
	{
		return FALSE;
	}
	virtual ILayerManager* GetLayerManager() override
	{
		return nullptr;
	}
	virtual int GetMtlDlgMode() override
	{
		return 0;
	}
	virtual void SetMtlDlgMode(int mode) override
	{
	}
	virtual BOOL IsMtlDlgShowing(int mode) override
	{
		return FALSE;
	}
	virtual void OpenMtlDlg(int mode) override
	{
	}
	virtual void CloseMtlDlg(int mode) override
	{
	}
	virtual void SetNameSuffixLength(unsigned short suffixLength) override
	{
	}
	virtual unsigned short GetNameSuffixLength() const override
	{
		return 0;
	}


	virtual RECT GetMainWindowClientRect() const override
	{
		return RECT();
	}

	virtual HRESULT OpenMAXStorageFile(const WCHAR* filename, IStorage** pIStorage) override
	{
		return 0;
	}
	virtual BOOL GetRendUseActiveView() override
	{
		return FALSE;
	}
	virtual void SetRendUseActiveView(BOOL useActiveView) override
	{
	}
	virtual int GetRenderPresetMRUListCount() override
	{
		return 0;
	}
	virtual const MCHAR* GetRenderPresetMRUListDisplayName(int i) override
	{
		return nullptr;
	}
	virtual const MCHAR* GetRenderPresetMRUListFileName(int i) override
	{
		return nullptr;
	}
	virtual Matrix3 GetTransformGizmoTM() const override
	{
		return Matrix3();
	}
	virtual void DisplayViewportConfigDialogPage(int i) override
	{
	}

	virtual bool GetTrajectoryMode() const override
	{
		return FALSE;
	}
	virtual void SetTrajectoryMode(bool flag) override
	{
	}
	virtual bool GetTrajectoryKeySubMode() const override
	{
		return FALSE;
	}
	virtual void SetTrajectoryKeySubMode(bool flag) override
	{
	}
	virtual bool GetTrajectoryAddKeyMode() const override
	{
		return FALSE;
	}
	virtual void SetTrajectoryAddKeyMode(bool flag) override
	{
	}
	virtual void DeleteSelectedTrajectoryKey() override
	{
	}
	virtual BOOL GetAutoKeyDefaultKeyOn() const override
	{
		return FALSE;
	}
	virtual void SetAutoKeyDefaultKeyOn(BOOL setKey) override
	{
	}
	virtual TimeValue GetAutoKeyDefaultKeyTime() const override
	{
		return 0;
	}
	virtual void SetAutoKeyDefaultKeyTime(TimeValue t) override
	{
	}
	virtual void FindNodes(const Tab<INode*>& templateNodes, Tab<INode*>& foundNodes,
			const unsigned int nodePropsToMatch = kNodeProp_All) override
	{
	}
	virtual MSTR GetPrompt() override
	{
		return L"";
	}
	virtual void FormatRenderTime(DWORD msec, MSTR& str, BOOL hundredths = FALSE) override
	{
	}

	virtual bool DoMaxBrowseForFolder(HWND aOwner, const MSTR& aInstructions, MSTR& aDir) override
	{
		return FALSE;
	}

	virtual unsigned int DeleteNodes(
			INodeTab& aNodes, bool aKeepChildTM = true, bool aRedraw = true, bool overrideSlaveTM = false) override
	{
		return 0;
	}


	virtual int QuickRender(
			TimeValue t = TIME_PosInfinity, Bitmap* rendbm = NULL, RendProgressCallback* prog = NULL) override
	{
		return 0;
	}
	virtual void GetRendFrameList(IntTab& frameNums) override
	{
	}
	virtual RendProgressCallback* GetRendProgressCallback() override
	{
		return nullptr;
	}
	virtual void SetRendProgressCallback(RendProgressCallback* prog) override
	{
	}
	virtual INode* GetRendCamNode() override
	{
		return nullptr;
	}
	virtual void SetRendCamNode(INode* camNode) override
	{
	}
	virtual BOOL GetRendUseImgSeq() override
	{
		return FALSE;
	}
	virtual void SetRendUseImgSeq(BOOL onOff) override
	{
	}
	virtual int GetRendImgSeqType() override
	{
		return 0;
	}
	virtual void SetRendImgSeqType(int type) override
	{
	}
	virtual void CreateRendImgSeq(CreateRendImgSeqCallback* cb = NULL) override
	{
	}
	virtual const MaxSDK::AssetManagement::AssetUser& GetPreRendScriptAsset()
	{
		return SomeAssetUser;
	}
	virtual void SetPreRendScriptAsset(const MaxSDK::AssetManagement::AssetUser& script) override
	{
	}
	virtual BOOL GetUsePreRendScript() override
	{
		return FALSE;
	}
	virtual void SetUsePreRendScript(BOOL onOff) override
	{
	}
	virtual BOOL GetLocalPreRendScript() override
	{
		return FALSE;
	}
	virtual void SetLocalPreRendScript(BOOL onOff) override
	{
	}
	virtual const MaxSDK::AssetManagement::AssetUser& GetPostRendScriptAsset()
	{
		return SomeAssetUser;
	}
	virtual void SetPostRendScriptAsset(const MaxSDK::AssetManagement::AssetUser& script) override
	{
	}
	virtual BOOL GetUsePostRendScript() override
	{
		return FALSE;
	}
	virtual void SetUsePostRendScript(BOOL onOff) override
	{
	}
	virtual BOOL GetControllerOverrideRangeDefault() override
	{
		return FALSE;
	}
	virtual void SetControllerOverrideRangeDefault(BOOL override) override
	{
	}
	virtual void GetDefaultTangentType(int& dfltInTangentType, int& dfltOutTangentType) override
	{
	}
	virtual void SetDefaultTangentType(
			int dfltInTangentType, int dfltOutTangentType, BOOL writeInCfgFile = TRUE) override
	{
	}
	virtual BOOL GetSpringQuickEditMode() const override
	{
		return FALSE;
	}
	virtual void SetSpringQuickEditMode(BOOL in_quickEdit) override
	{
	}
	virtual void SetSpringRollingStart(int in_start) override
	{
	}
	virtual int GetSpringRollingStart() const override
	{
		return 0;
	}
	virtual void ColorById(DWORD id, Color& c) override
	{
	}
	virtual void RegisterExitMAXCallback(ExitMAXCallback2* cb) override
	{
	}
	virtual void UnRegisterExitMAXCallback(ExitMAXCallback2* cb) override
	{
	}
	virtual bool DoMaxSaveAsDialog(
			HWND parentWnd, const MSTR& title, MSTR& filename, MSTR& initialDir, FilterList& extensionList) override
	{
		return FALSE;
	}
	virtual bool DoMaxOpenDialog(
			HWND parentWnd, const MSTR& title, MSTR& filename, MSTR& initialDir, FilterList& extensionList) override
	{
		return FALSE;
	}
	virtual void RegisterModelessRenderWindow(HWND hWnd) override
	{
	}
	virtual void UnRegisterModelessRenderWindow(HWND hWnd) override
	{
	}
	virtual bool IsRegisteredModelessRenderWindow(HWND hWnd) override
	{
		return FALSE;
	}
	virtual bool IsSavingToFile() override
	{
		return FALSE;
	}
	virtual bool LoadFromFile(const MCHAR* szFilename, unsigned long lFlags) override
	{
		return FALSE;
	}
	virtual bool RevealInExplorer(const MSTR& path) override
	{
		return FALSE;
	}


	virtual void GetSelNodeTab(INodeTab& selectedNodes) const override
	{
	}
	virtual PivotMode GetPivotMode() const override
	{
		return kPIV_NONE;
	}
	virtual void SetPivotMode(PivotMode pivMode) override
	{
	}
	virtual bool GetAffectChildren() const override
	{
		return FALSE;
	}
	virtual void SetAffectChildren(bool bAffectChildren) override
	{
	}

	virtual void GetCurRefCoordSysName(MSTR& name) const override
	{
	}

	virtual void AddRefCoordNode(INode* node) override
	{
	}
	virtual INode* GetRefCoordNode() override
	{
		return nullptr;
	}

	virtual bool ShouldContinueRenderOnError() const override
	{
		return FALSE;
	}

	virtual void SetupFrameRendParams(FrameRendParams& frp, RendParams& rp, ViewExp* vx, RECT* r) override
	{
	}
	virtual void SetupFrameRendParams_MXS(
			FrameRendParams& frp, RendParams& rp, ViewExp* vx, RECT* r, bool useSelBox) override
	{
	}

#if MAX_RELEASE < 25900
	// Signature changed in 2024+
	virtual int InitDefaultLights(DefaultLight* dl, int maxn, BOOL applyGlobalLevel = FALSE, ViewExp* vx = NULL,
		BOOL forRenderer = FALSE) override
	{
		return 0;
	}
#else
	virtual int Interface7::InitDefaultLights(DefaultLight*, int, BOOL, ViewExp*) override
	{
		return 0;
	}
#endif

	virtual void IncrRenderActive() override
	{
	}
	virtual void DecrRenderActive() override
	{
	}
	virtual BOOL IsRenderActive() override
	{
		return FALSE;
	}

	virtual BOOL XRefRenderBegin() override
	{
		return FALSE;
	}
	virtual void XRefRenderEnd() override
	{
	}

	virtual void OpenRenderDialog() override
	{
	}
	virtual void CancelRenderDialog() override
	{
	}
	virtual void CloseRenderDialog() override
	{
	}
	virtual void CommitRenderDialogParameters() override
	{
	}
	virtual void UpdateRenderDialogParameters() override
	{
	}
	virtual BOOL RenderDialogOpen() override
	{
		return FALSE;
	}

	virtual Bitmap* GetLastRenderedImage() override
	{
		return nullptr;
	}

	virtual HWND GetStatusPanelHWnd() override
	{
		return nullptr;
	}
	virtual void SetListenerMiniHWnd(HWND wnd) override
	{
	}
	virtual HWND GetListenerMiniHWnd() override
	{
		return nullptr;
	}

	virtual int MAXScriptHelp(const MCHAR* keyword = NULL)
	{
		return 0;
	}

	virtual MAXScriptPrefs& GetMAXScriptPrefs() override
	{
		static MAXScriptPrefs s;
		return s;
	}

	virtual BOOL OpenTrackViewWindow(const MCHAR* tv_name, const MCHAR* layoutName = NULL,
			Point2 pos = Point2(-1.0f, -1.0f), int width = -1, int height = -1, int dock = TV_FLOAT) override
	{
		return FALSE;
	}

	virtual BOOL BringTrackViewWindowToTop(const MCHAR* tv_name) override
	{
		return FALSE;
	}

	virtual BOOL TrackViewZoomSelected(const MCHAR* tv_name) override
	{
		return FALSE;
	}
	virtual BOOL TrackViewZoomOn(const MCHAR* tv_name, Animatable* parent, int subNum) override
	{
		return FALSE;
	}
	virtual BOOL CloseTrackView(const MCHAR* tv_name) override
	{
		return FALSE;
	}
	virtual int NumTrackViews()
	{
		return 0;
	}
	virtual const MCHAR* GetTrackViewName(int i) override
	{
		return nullptr;
	}
	virtual BOOL SetTrackViewFilter(const MCHAR* tv_name, DWORD mask, int which, BOOL redraw = TRUE) override
	{
		return FALSE;
	}
	virtual BOOL ClearTrackViewFilter(const MCHAR* tv_name, DWORD mask, int which, BOOL redraw = TRUE) override
	{
		return FALSE;
	}
	virtual DWORD TestTrackViewFilter(const MCHAR* tv_name, DWORD mask, int which)
	{
		return 0;
	}
	virtual void FlushAllTrackViewWindows() override
	{
	}
	virtual void UnFlushAllTrackViewWindows() override
	{
	}
	virtual void CloseAllTrackViewWindows() override
	{
	}

	virtual void SetCurEditObject(BaseObject* obj, INode* hintNode = NULL) override
	{
	}
	virtual void AddModToSelection(Modifier* mod) override
	{
	}
	virtual void InvalidateObCache(INode* node) override
	{
	}
#if MAX_RELEASE >= 26900
	virtual Tab<INode*> FindNodesFromBaseObject(ReferenceTarget* obj, bool allowXRefNodes = true,
			bool allowNonSceneNodes = true, bool allowScriptedPlugins = false, bool allowXrefObjects = false,
			bool handleBranching = false) override
	{
		return Tab<INode*>();
	}
#endif
#if MAX_RELEASE >= 26900
	virtual INode* FindNodeFromBaseObject(ReferenceTarget* obj, bool allowXRefNodes = true,
			bool allowNonSceneNodes = true, bool allowScriptedPlugins = false, bool allowXrefObjects = false,
			bool handleBranching = false, bool preferSelected = false) override
#else
	virtual INode* FindNodeFromBaseObject(ReferenceTarget* obj) override
#endif
	{
		return nullptr;
	}
	virtual void SelectedHistoryChanged() override
	{
	}
	virtual BOOL CmdPanelOpen() override
	{
		return FALSE;
	}
	virtual void CmdPanelOpen(BOOL openClose) override
	{
	}

	virtual void SuspendEditing(DWORD whichPanels = (1 << TASK_MODE_MODIFY), BOOL alwaysSuspend = FALSE) override
	{
	}
	virtual void ResumeEditing(DWORD whichPanels = (1 << TASK_MODE_MODIFY), BOOL alwaysSuspend = FALSE) override
	{
	}
	virtual void SuspendMotionEditing() override
	{
	}
	virtual void ResumeMotionEditing() override
	{
	}
	virtual int AddClass(ClassDesc* cdesc, int dllNum = -1, int index = -1, bool load = true) override
	{
		return 0;
	}
	virtual void ReBuildSuperList() override
	{
	}
	virtual BOOL IsEditing() override
	{
		return FALSE;
	}
	virtual void ChangeHistory(int upDown) override
	{
	}

	virtual void StartCreatingObject(ClassDesc* pCD) override
	{
	}
	virtual BOOL IsCreatingObject(const Class_ID& id) override
	{
		return FALSE;
	}
	virtual BOOL IsCreatingObject() override
	{
		return FALSE;
	}
	virtual void UpdateLockCheckObjectCounts() override
	{
	}
	virtual INode* CreateObjectNode(Object* obj, const MCHAR* name) override
	{
		return nullptr;
	}

	virtual BOOL SetDir(int which, const MCHAR* dir) override
	{
		return FALSE;
	}
	virtual BOOL AddAssetDir(const MCHAR* dir, MaxSDK::AssetManagement::AssetType assetType, int update = TRUE) override
	{
		return FALSE;
	}
	virtual BOOL DeleteAssetDir(int i, MaxSDK::AssetManagement::AssetType assetType, int update = TRUE) override
	{
		return FALSE;
	}
	virtual void UpdateAssetSection(MaxSDK::AssetManagement::AssetType assetType) override
	{
	}

	virtual BOOL AppendToCurFilePath(const MCHAR* toAppend) override
	{
		return FALSE;
	}

	virtual MSTR GetMAXIniFile() override
	{
		return L"";
	}

	virtual BOOL OpenSchematicViewWindow(const MCHAR* sv_name) override
	{
		return FALSE;
	}
	virtual BOOL SchematicViewZoomSelected(const MCHAR* sv_name) override
	{
		return FALSE;
	}
	virtual BOOL CloseSchematicView(const MCHAR* sv_name) override
	{
		return FALSE;
	}
	virtual int NumSchematicViews()
	{
		return 0;
	}
	virtual const MCHAR* GetSchematicViewName(int i) override
	{
		return nullptr;
	}
	virtual void CloseAllSchematicViewWindows() override
	{
	}
	virtual void FlushAllSchematicViewWindows() override
	{
	}
	virtual void UnFlushAllSchematicViewWindows() override
	{
	}

	virtual BOOL DrawingEnabled() override
	{
		return FALSE;
	}
	virtual void EnableDrawing(BOOL onOff) override
	{
	}
	virtual BOOL SceneResetting() override
	{
		return FALSE;
	}
	virtual BOOL QuitingApp() override
	{
		return FALSE;
	}

	virtual BOOL GetHideFrozen() override
	{
		return FALSE;
	}
	virtual void SetSceneDisplayFlag(DWORD flag, BOOL onOff, BOOL updateUI = TRUE) override
	{
	}
	virtual BOOL GetSceneDisplayFlag(DWORD flag) override
	{
		return FALSE;
	}

	virtual IScene* GetScene() override
	{
		return nullptr;
	}

	virtual void SetMtlSlot(int i, MtlBase* m) override
	{
	}
	virtual int GetActiveMtlSlot() override
	{
		return 0;
	}
	virtual void SetActiveMtlSlot(int i) override
	{
	}
	virtual int NumMtlSlots() override
	{
		return 0;
	}
	virtual void FlushMtlDlg() override
	{
	}
	virtual void UnFlushMtlDlg() override
	{
	}
	virtual BOOL IsMtlInstanced(MtlBase* m) override
	{
		return FALSE;
	}

	virtual Mtl* FindMtlNameInScene(MSTR& name) override
	{
		return nullptr;
	}
	virtual void PutMaterial(MtlBase* mtl, MtlBase* oldMtl, BOOL delOld = 1, RefMakerHandle skipThis = 0) override
	{
	}
	virtual BOOL IsMtlDlgShowing() override
	{
		return FALSE;
	}
	virtual void OpenMtlDlg() override
	{
	}
	virtual void CloseMtlDlg() override
	{
	}

	virtual HWND GetViewPanelHWnd() override
	{
		return nullptr;
	}
	virtual int getActiveViewportIndex() override
	{
		return 0;
	}
	virtual BOOL setActiveViewport(int index) override
	{
		return FALSE;
	}
	virtual int getNumViewports() override
	{
		return 0;
	}
	virtual ViewExp& getViewExp(int i) override
	{
		return MockViewExp;
	}

	virtual void resetAllViews() override
	{
	}

	/// Viewport name access
	virtual const MCHAR* getActiveViewportLabel() override
	{
		return nullptr;
	}
	virtual const MCHAR* getViewportLabel(int index) override
	{
		return nullptr;
	}

	virtual void SetRegionRect(int index, Rect r) override
	{
	}
	virtual Rect GetRegionRect(int index) override
	{
		return Rect();
	}
	virtual void SetBlowupRect(int index, Rect r) override
	{
	}
	virtual Rect GetBlowupRect(int index) override
	{
		return Rect();
	}
	virtual void SetRegionRect2(int index, Rect r) override
	{
	}
	virtual Rect GetRegionRect2(int index) override
	{
		return Rect();
	}
	virtual void SetBlowupRect2(int index, Rect r) override
	{
	}
	virtual Rect GetBlowupRect2(int index) override
	{
		return Rect();
	}
	virtual int GetRenderType() override
	{
		return 0;
	}
	virtual void SetRenderType(int rtype) override
	{
	}
	virtual BOOL GetLockImageAspRatio() override
	{
		return FALSE;
	}
	virtual void SetLockImageAspRatio(BOOL on) override
	{
	}
	virtual float GetImageAspRatio() override
	{
		return 0;
	}
	virtual void SetImageAspRatio(float on) override
	{
	}
	virtual BOOL GetLockPixelAspRatio() override
	{
		return FALSE;
	}
	virtual void SetLockPixelAspRatio(BOOL on) override
	{
	}
	virtual float GetPixelAspRatio() override
	{
		return 0;
	}
	virtual void SetPixelAspRatio(float on) override
	{
	}

	virtual void SetViewportGridVisible(int index, BOOL state) override
	{
	}
	virtual BOOL GetViewportGridVisible(int index) override
	{
		return FALSE;
	}

	virtual void ViewportInvalidate(int index) override
	{
	}
	virtual void ViewportInvalidateBkgImage(int index) override
	{
	}
	virtual void InvalidateAllViewportRects() override
	{
	}

	virtual void RedrawViewportsNow(TimeValue t, DWORD vpFlags = VP_DONT_SIMPLIFY) override
	{
	}
	virtual void RedrawViewportsLater(TimeValue t, DWORD vpFlags = VP_DONT_SIMPLIFY) override
	{
	}
	virtual void SetActiveViewportRenderLevel(int level) override
	{
	}
	virtual int GetActiveViewportRenderLevel() override
	{
		return 0;
	}
	virtual void SetActiveViewportShowEdgeFaces(BOOL show) override
	{
	}
	virtual BOOL GetActiveViewportShowEdgeFaces() override
	{
		return FALSE;
	}
	virtual void SetActiveViewportTransparencyLevel(int level) override
	{
	}
	virtual int GetActiveViewportTransparencyLevel() override
	{
		return 0;
	}
	virtual BOOL GetDualPlanes() override
	{
		return FALSE;
	}
	virtual void SetDualPlanes(BOOL b) override
	{
	}
	virtual void SetTestOnlyFrozen(int onOff) override
	{
	}
	virtual void SetScaleMode(int mode) override
	{
	}
	virtual void SetCenterMode(int mode) override
	{
	}
	virtual BOOL InManipMode() override
	{
		return FALSE;
	}
	virtual void StartManipulateMode() override
	{
	}
	virtual void EndManipulateMode() override
	{
	}
	virtual BOOL IsViewportCommandMode(CommandMode* m) override
	{
		return FALSE;
	}
	virtual void ConvertFlagedNodesToXRefs(
			const MaxSDK::AssetManagement::AssetUser& fname, INode* rootNode, Tab<INode*>& nodes, int xFlags) override
	{
	}
	virtual void XRefSceneSetIgnoreFlag(int index, DWORD f, BOOL onOff) override
	{
	}
	virtual void UpdateSceneXRefState() override
	{
	}

	virtual BOOL GetSnapActive() override
	{
		return FALSE;
	}
	virtual void SetSnapActive(BOOL onOff) override
	{
	}
	virtual int GetSnapType() override
	{
		return 0;
	}
	virtual void SetSnapType(int type) override
	{
	}
	virtual void ToggleASnap() override
	{
	}
	virtual int ASnapStatus() override
	{
		return 0;
	}
	virtual void TogglePSnap() override
	{
	}
	virtual int PSnapStatus() override
	{
		return 0;
	}
	virtual void SetGridSpacing(float newVal) override
	{
	}
	virtual void SetGridMajorLines(float newVal) override
	{
	}
	virtual float GetSnapAngle() override
	{
		return 0;
	}
	virtual void SetSnapAngle(float newVal) override
	{
	}
	virtual float GetSnapPercent() override
	{
		return 0;
	}
	virtual void SetSnapPercent(float newVal) override
	{
	}

	virtual void SetNodeAttribute(INodeTab& nodes, int whatAttrib, int onOff) override
	{
	}
	virtual void SetNodeAttribute(INode* node, int whatAttrib, int onOff) override
	{
	}
	virtual void SetExpertMode(int onOff) override
	{
	}
	virtual int GetExpertMode() override
	{
		return 0;
	}
	virtual void LoadCUIConfig(const MCHAR* fileName) override
	{
	}
	virtual void WriteCUIConfig() override
	{
	}
	virtual void SaveCUIAs() override
	{
	}
	virtual void LoadCUI() override
	{
	}
	virtual void RevertToBackupCUI() override
	{
	}
	virtual void ResetToFactoryDefaultCUI() override
	{
	}

	virtual int GetDefaultImageListBaseIndex(SClass_ID sid, Class_ID cid) override
	{
		return 0;
	}
	virtual MSTR* GetDefaultImageListFilePrefix(SClass_ID sid, Class_ID cid) override
	{
		return nullptr;
	}

	virtual float GetGridIntens() override
	{
		return 0;
	}
	virtual void SetGridIntens(float f) override
	{
	}
	virtual BOOL GetWhiteOrigin() override
	{
		return FALSE;
	}
	virtual void SetWhiteOrigin(BOOL b) override
	{
	}
	virtual BOOL GetUseGridColor() override
	{
		return FALSE;
	}
	virtual void SetUseGridColor(BOOL b) override
	{
	}
	virtual void UpdateColors(BOOL useGridColor, int gridIntensity, BOOL whiteOrigin) override
	{
	}
#if MAX_RELEASE < 26900
	virtual IMenu* GetIMenu() override
	{
		return nullptr;
	}
	virtual IMenuItem* GetIMenuItem() override
	{
		return nullptr;
	}
#endif
	virtual void RepaintTimeSlider() override
	{
	}

	virtual MSTR GetTabPageTitle(ITabPage* page) override
	{
		return L"";
	}
	virtual BOOL DoMaxFileSaveAsDlg(MSTR& fileName, BOOL setAsCurrent = TRUE) override
	{
		return FALSE;
	}
	virtual BOOL DoMaxFileOpenDlg(MSTR& fileName, MSTR* defDir, MSTR* defFile) override
	{
		return FALSE;
	}
	virtual BOOL DoMaxFileMergeDlg(MSTR& fileName, MSTR* defDir, MSTR* defFile) override
	{
		return FALSE;
	}
	virtual BOOL DoMaxFileXRefDlg(MSTR& fileName, MSTR* defDir, MSTR* defFile) override
	{
		return FALSE;
	}
	virtual void StartAnimPlayback2(int selOnly) override
	{
	}
	virtual BOOL IsValidModForSelection(ClassEntry* ce) override
	{
		return FALSE;
	}
	virtual BOOL IsValidModifier(INode& node, Modifier& mod) override
	{
		return FALSE;
	}
	virtual ResCode AddModifier(INode& node, Modifier& mod, int beforeIdx = 0) override
	{
		return kRES_SUCCESS;
	}
	virtual ResCode DeleteModifier(INode& node, Modifier& mod) override
	{
		return kRES_SUCCESS;
	}
	virtual ResCode DeleteModifier(INode& node, int modIdx) override
	{
		return kRES_SUCCESS;
	}
	virtual IDerivedObject* FindModifier(INode& node, Modifier& mod, int& modStackIdx, int& derivedObjIdx) override
	{
		return nullptr;
	}
	virtual IDerivedObject* FindModifier(INode& node, int modIdx, int& idx, Modifier*& mod) override
	{
		return nullptr;
	}
	virtual IDerivedObject* FindModifier(
			INode& node, Modifier& mod, ModContext& mc, int& modStackIdx, int& dobjidx) override
	{
		return nullptr;
	}

	virtual ResCode DoDeleteModifier(INode& node, IDerivedObject& dobj, int idx) override
	{
		return kRES_SUCCESS;
	}
	virtual Object* GetReplaceableObjRef(INode& node) override
	{
		return nullptr;
	}
	virtual void OpenEnvEffectsDialog() override
	{
	}
	virtual void CloseEnvEffectsDialog() override
	{
	}
	virtual BOOL EnvEffectsDialogOpen() override
	{
		return FALSE;
	}
#if MAX_RELEASE >= 25900
	virtual void SetRendFormatToCustom() override
	{
	}
	virtual void FileSaveMatLib(HWND hWnd, MtlBaseLib* lib, const MCHAR* seedName = nullptr,
			MSTR* savedFileName = nullptr) override
		{}

	virtual void GetSaveMatLibFilterList(FilterList& filterList) override
	{
	}

	virtual int GetSaveAsVersionFromOFN(const OPENFILENAME& ofn) override
	{
		return 0;
	}

	virtual bool SaveMaterialLib(const MCHAR* name, MtlBaseLib* lib, int saveAsVersion) override
	{
		return true;
	}

	virtual bool IsValidSaveAsVersion(int saveAsVersion) override
	{
		return true;
	}

	void EnableViewportStatisticsRefresh(bool enable) override
	{
	}

	void RefreshViewportStatistics() override
	{
	}
#if MAX_RELEASE >= 26900
	void EnableViewportStatistics(bool enable)
	{
	}

	void GlobalScaleTime(Interval oldSegment, Interval newSegment, BOOL modifyTimeline, BOOL scaleToWholeFrames)
	{
	}

	virtual BOOL IsSceneNode(INode* node) override
	{
		return FALSE;
	}
#endif
#endif

#if MAX_RELEASE >= 27900
	virtual bool GetViewportFromScreenCoord(const POINT&, int&, int&, int&, POINT&, HWND&, INode**) const override
	{
		return false;
	}

	virtual void UpdateOsnapDlg() const override { }

	virtual void SetASnapStatus(BOOL enable) override {};
 
	virtual void SetPSnapStatus(BOOL enable) override {};

	virtual void DoUICustomization(CUIDialogPage pageId) override
	{
	}

	MSTR GetSceneFileUserName(void) const override { return L""; }

	void SetSceneFileUserName(const MSTR&, bool) override { }

	MSTR GetSceneFileComputerName(void) const override { return L""; }

	void SetSceneFileComputerName(const MSTR&, bool) override { }

#else
	virtual void DoUICustomization(int startPage) override
	{
	}
#endif
};