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
#include "USDStageObject.h"

#include "CreateCallbacks/CreateAtPosition.h"
#include "USDStageObjectIcon.h"
#include "USDStageObjectclassDesc.h"
#include "USDTransformControllers.h"
#include "UsdCameraObject.h"

#include <MaxUsdObjects/DLLEntry.h>
#include <MaxUsdObjects/MaxUsdUfe/MaxUfeUndoableCommandMgr.h>
#include <MaxUsdObjects/MaxUsdUfe/QmaxUsdUfeAttributesWidget.h>
#include <MaxUsdObjects/MaxUsdUfe/StageObjectMap.h>
#include <MaxUsdObjects/MaxUsdUfe/UfeUtils.h>
#include <MaxUsdObjects/Objects/SubobjectManips.h>
#include <MaxUsdObjects/USDAssetAccessor.h>
#include <MaxUsdObjects/USDExplorer.h>
#include <MaxUsdObjects/USDPickingRenderer.h>
#include <MaxUsdObjects/resource.h>

#include <RenderDelegate/HdMaxConsolidator.h>
#include <RenderDelegate/HdMaxDisplayPreferences.h>
#include <UFEUI/ReplaceSelectionCommand.h>

#include <MaxUsd/Utilities/DiagnosticDelegate.h>
#include <MaxUsd/Utilities/MathUtils.h>
#include <MaxUsd/Utilities/MeshUtils.h>
#include <MaxUsd/Utilities/MxsUtils.h>
#include <MaxUsd/Utilities/OptionUtils.h>
#include <MaxUsd/Utilities/PluginUtils.h>
#include <MaxUsd/Utilities/ScopeGuard.h>
#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/TypeUtils.h>

#include <usdUfe/ufe/UsdSceneItem.h>
#include <usdUfe/utils/loadRules.h>

#include <pxr/usd/kind/registry.h>
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usd/stageCacheContext.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/metrics.h>

#include <Graphics/CustomRenderItemHandle.h>
#include <Graphics/IViewportViewSetting.h>
#include <Graphics/Utilities/SplineRenderItem.h>
#include <Graphics/ViewSystem/ViewParameter.h>
#include <Qt/QMaxParamBlockWidget.h>
#include <Qt/QmaxRollup.h>
#include <Qt/QmaxSpinBox.h>
#include <maxscript/maxscript.h>
#include <maxscript/mxsplugin/mxsplugin.h>
#include <ufe/attributes.h>
#include <ufe/globalSelection.h>
#include <ufe/observableSelection.h>
#include <ufe/selection.h>
#include <ufe/selectionNotification.h>
#include <ufe/undoableCommandMgr.h>

#include <IParamm2.h>
#include <IPathConfigMgr.h>
#include <QtWidgets/QtWidgets>
#include <iparamb2.h>
#include <irollupsettings.h>

Class_ID USDSTAGEOBJECT_CLASS_ID(0x24ce4724, 0x14d2486b);

// Bump this version number when saved data changes.
static int       USD_OBJECT_DATA_SAVE_VERSION = 1;
constexpr USHORT SAVE_VERSION_CHUNK_ID = 100;
constexpr USHORT PRIMVAR_MAPPING_NAME_CHUNK_ID = 200;
constexpr USHORT PRIMVAR_MAPPING_CHANNELS_CHUNK_ID = 300;
constexpr USHORT SESSION_LAYER_CHUNK_ID = 400;
constexpr USHORT PAYLOAD_RULES_CHUNK_ID = 500;

SelectModBoxCMode*  USDStageObject::selectMode = nullptr;
MoveModBoxCMode*    USDStageObject::moveMode = nullptr;
RotateModBoxCMode*  USDStageObject::rotateMode = nullptr;
UScaleModBoxCMode*  USDStageObject::uScaleMode = nullptr;
NUScaleModBoxCMode* USDStageObject::nuScaleMode = nullptr;
SquashModBoxCMode*  USDStageObject::squashMode = nullptr;

bool                    USDStageObject::primAttributeRollupOpenStatesLoaded = false;
std::map<QString, bool> USDStageObject::primAttributeRollupStates;
std::map<QString, bool> USDStageObject::loadedPrimAttributeRollupStates;
const QString           USDStageObject::rollupCategory = "USDStageObjectRollups";

void USDStageObject::USDPBAccessor::PreSet(
    PB2Value&       v,
    ReferenceMaker* owner,
    ParamID         id,
    int             tabIndex,
    TimeValue       t)
{
    auto setIfInBounds =
        [](PB2Value& v, IParamBlock2* pb, ParamID id, TimeValue t, int lowerBound, int upperBound) {
            int      currVar;
            Interval valid = FOREVER;
            pb->GetValue(id, t, currVar, valid);
            int newVal = v.i;
            if (newVal >= lowerBound && newVal <= upperBound) {
                v.i = newVal;
            } else {
                v.i = currVar;
            }
        };

    USDStageObject* stageObj = static_cast<USDStageObject*>(owner);
    IParamBlock2*   pblock2 = stageObj->GetParamBlockByID(0);
    if (pblock2) {
        switch (id) {
        case DisplayMode: {
            setIfInBounds(v, pblock2, id, t, 0, 2);
            break;
        }
        case AnimationMode: {
            setIfInBounds(v, pblock2, id, t, 0, 3);
            break;
        }
        }
    }
}

void USDStageObject::USDPBAccessor::Get(
    PB2Value&       v,
    ReferenceMaker* owner,
    ParamID         id,
    int             tabIndex,
    TimeValue       t,
    Interval&       valid)
{
    auto saveString = [](const std::wstring& s) {
        TCHAR* dest = new TCHAR[s.size() + 1]; // Size + 1 to null-terminate..
        std::copy(s.begin(), s.end(), dest);
        dest[s.size()] = '\0';
        return dest;
    };

    USDStageObject* stageObj = static_cast<USDStageObject*>(owner);
    IParamBlock2*   pblock2 = stageObj->GetParamBlockByID(0);

    switch (id) {
    case AxisAndUnitTransform: {
        *v.m = MaxUsd::ToMaxMatrix3(stageObj->GetStageRootTransform());
        break;
    }
    case CacheId: {
        v.i = stageObj->GetStageCacheId();
        break;
    }
    case Guid: {
        auto       guid = MaxUsd::UsdStringToMaxString(stageObj->GetGuid());
        const auto guidStr = guid.ToMCHAR();
        v.s = saveString(guidStr);
        break;
    }
    case SourceMetersPerUnit: {
        auto stage = stageObj->GetUSDStage();
        if (!stage) {
            v.f = 0;
        } else {
            const float usdMetersPerUnit
                = static_cast<float>(pxr::UsdGeomGetStageMetersPerUnit(stage));
            v.f = usdMetersPerUnit;
        }
        break;
    }
    case SourceUpAxis: {
        auto stage = stageObj->GetUSDStage();

        if (!stage) {
            v.s = saveString(L"N/A");
        } else {
            if (MaxUsd::IsStageUsingYUpAxis(stage)) {
                v.s = saveString(L"Y");
            } else {
                v.s = saveString(L"Z");
            }
        }
        break;
    }
    case PointedPrim: {
        // PointedPrim must evaluate to the path of the prim currently under the cursor.
        // We force a call to the ::HitTest() method via PickNode(). This also allows us
        // make sure that we are indeed hovering over the stage (just looking that
        // lastHit.primPath is not empty is not enough - as if the cursor is outside the
        // bounding box, the object's HitTest() code is not even run.
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(MAXScript_interface->GetActiveViewExp().GetHWnd(), &pt);
        const IPoint2 point(pt.x, pt.y);
        const auto    pickedNode
            = GetCOREInterface()->PickNode(GetCOREInterface()->GetActiveViewExp().GetHWnd(), point);
        if (!pickedNode || pickedNode->GetObjectRef() != stageObj) {
            v.s = saveString(L"");
        } else {
            v.s = saveString(MaxUsd::UsdStringToMaxString(
                                 stageObj->hitTestingCache[pickedNode].hit.primPath.GetString())
                                 .ToMCHAR());
        }
        break;
    }
    case SourceAnimationStartTimeCode: {
        auto stage = stageObj->GetUSDStage();

        v.f = stage ? stage->GetStartTimeCode() : 0.0f;
        break;
    }
    case SourceAnimationEndTimeCode: {
        auto stage = stageObj->GetUSDStage();

        v.f = stage ? stage->GetEndTimeCode() : 0.0f;
        break;
    }
    case SourceAnimationTPS: {
        auto stage = stageObj->GetUSDStage();

        v.f = stage ? stage->GetTimeCodesPerSecond() : 0.0f;
        break;
    }
    case MaxAnimationStartFrame: {
        auto stage = stageObj->GetUSDStage();

        if (!stage) {
            v.f = 0.f;
        } else {
            int      animMode;
            Interval valid = FOREVER;
            pblock2->GetValue(AnimationMode, t, animMode, valid);
            float customStartFrame;
            pblock2->GetValue(CustomAnimationStartFrame, t, customStartFrame, valid);

            auto stageStartCode = stage->GetStartTimeCode();
            if (animMode == AnimationMode::OriginalRange) {
                auto computedValue = MaxUsd::GetMaxFrameFromUsdTimeCode(stage, stageStartCode);
                v.f = computedValue;
            } else if (animMode == AnimationMode::CustomTimeCodePlayback) {
                v.f = stageStartCode;
            } else {
                v.f = customStartFrame;
            }
        }
        break;
    }
    case MaxAnimationEndFrame: {
        auto stage = stageObj->GetUSDStage();

        if (!stage) {
            v.f = 0.f;
        } else {
            int      animMode;
            Interval valid = FOREVER;
            pblock2->GetValue(AnimationMode, t, animMode, valid);
            float customStartFrame;
            pblock2->GetValue(CustomAnimationStartFrame, t, customStartFrame, valid);
            float customEndFrame;
            pblock2->GetValue(CustomAnimationEndFrame, t, customEndFrame, valid);
            float customSpeed;
            pblock2->GetValue(CustomAnimationSpeed, t, customSpeed, valid);

            auto stageStartCode = stage->GetStartTimeCode();
            auto stageEndCode = stage->GetEndTimeCode();
            if (animMode == AnimationMode::OriginalRange) {
                auto computedValue = MaxUsd::GetMaxFrameFromUsdTimeCode(stage, stageEndCode);
                v.f = computedValue;
            } else if (animMode == AnimationMode::CustomTimeCodePlayback) {
                v.f = stageEndCode;
            } else if (animMode == AnimationMode::CustomRange) {
                v.f = customEndFrame;
            } else {
                auto stageAnimeLengthInTimeCodes = stageEndCode - stageStartCode;
                auto computedValue = customStartFrame;
                if (customSpeed != 0.f) {
                    computedValue = customStartFrame
                        + (MaxUsd::GetMaxFrameFromUsdTimeCode(stage, stageAnimeLengthInTimeCodes)
                           / customSpeed);
                }

                v.f = computedValue;
            }
        }
        break;
    }
    case RenderUsdTimeCode: {
        auto stage = stageObj->GetUSDStage();

        if (!stage) {
            v.f = 0.f;
        } else {
            // If the animation is playing, render at the beginning of the current frame,
            // to ease caching
            TimeValue time = t;
            if (GetCOREInterface()->IsAnimPlaying()) {
                time = time - time % GetTicksPerFrame();
            }

            auto timeCodeSample = stageObj->ResolveRenderTimeCode(time).GetValue();
            v.f = timeCodeSample;
        }
    }
    }
}

namespace {

static USDStageObject::USDPBAccessor pbAccessor;

// clang-format off
static FPInterfaceDesc usdStageInterface(
	IUSDStageProvider_ID, _T("usdStageOps"), 0, GetUSDStageObjectClassDesc(), FP_MIXIN,
	fnIdReload, _T("Reload"), "Reload the Stage's layers from disk.", TYPE_VOID, 0, 0,
	fnIdClearSessionLayer, _T("ClearSessionLayer"), "Clears the session layer.", TYPE_VOID, 0, 0,
	fnIdOpenInUsdExplorer, _T("OpenInUsdExplorer"), "Open the stage in the USD Explorer.", TYPE_VOID, 0, 0,
	fnIdCloseInUsdExplorer, _T("CloseInUsdExplorer"), "Close the stage in the USD Explorer.", TYPE_VOID, 0, 0,
	fnIdSetRootLayer, _T("SetRootLayer"), "Sets the USD Stage's root layer and mask", TYPE_VOID, 0, 3,
		_T("rootLayer"), 0, TYPE_STRING,
		_T("stageMask"), 0, TYPE_STRING, f_keyArgDefault, _T("/"),
		_T("payloadsLoaded"), 0, TYPE_BOOL, f_keyArgDefault, TRUE,
	fnIdGetUsdPreviewSurfaceMaterials, _T("GetUsdPreviewSurfaceMaterials"), "Returns the MultiMaterial carrying converted UsdPreviewSurface materials, which can be used for offline rendering if applied to the UsdStage node.", TYPE_MTL, 0, 1,
		_T("sync"), 0, TYPE_BOOL, f_keyArgDefault, TRUE,

	fnIdSetPrimvarChannelMappingDefaults, _T("SetPrimvarChannelMappingDefaults"), "Reset to defaults primvar to channel mappings.", TYPE_VOID, FP_NO_REDRAW, 0,
	fnIdSetPrimvarChannelMapping, _T("SetPrimvarChannelMapping"), "Sets a primvar to channel mapping", TYPE_VOID, FP_NO_REDRAW, 2,
		_T("primvar"), 0, TYPE_STRING,
		_T("targetChannel"), 0, TYPE_VALUE,
	fnIdGetPrimvarChannel, _T("GetPrimvarChannel"), "Returns the channel the given primvar should map too.", TYPE_VALUE, FP_NO_REDRAW, 1,
		_T("primvar"), 0, TYPE_STRING,
	fnIdIsMappedPrimvar, _T("IsMappedPrimvar"), "Returns whether this primvar is mapped to a channel.", TYPE_BOOL, FP_NO_REDRAW, 1,
		_T("primvar"), 0, TYPE_STRING,
	fnIdGetMappedPrimvars, _T("GetMappedPrimvars"), "Returns the list of currently mapped primvars.", TYPE_STRING_TAB_BV, FP_NO_REDRAW, 0,
	fnIdClearMappedPrimvars, _T("ClearMappedPrimvars"), "Clears all primvar to channel mappings.", TYPE_VOID, FP_NO_REDRAW, 0,
	fnIdGenerateDrawModes, _T("GenerateDrawModes"), "Regenerate USD Draw Modes.", TYPE_VOID, 0, 0,
	p_end
);

ParamBlockDesc2 propertiesParamblock(PBLOCK_REF, // The parameter block ID.
	_M("USDStageObjectParamBlock"), // The internal name exposed to MaxScript
	IDS_USDSTAGEOBJECT_ROLL_OUT, // The resource ID of a particular string in the string table
	GetUSDStageObjectClassDesc(), // The address of your plug-in descriptor
	// flags
	P_AUTO_CONSTRUCT | P_AUTO_UI_QT | P_MULTIMAP, // This flag makes 3ds Max automatically create the paramblock for this object
	PBLOCK_REF, // To satisfy P_AUTO_CONSTRUCT
	// Define the multiple rollups we need.
	6,
	// The order matters, it is the default order in the UI.
	// Use a different order for 2022 - in 2022 we cannot use the general
	// progress bar, so we have an embedded one in the general rollout. 
	// To increase the odds that this progress bar is visible on screen
	// when materials are generated, bring the render settings window closer
	// to the general rollout. In 2023+, the render settings is all the way 
	// at the bottom.
#ifdef IS_MAX2023_OR_GREATER
	ParamMapID::UsdStageGeneral,
	ParamMapID::UsdStageSelection,
	ParamMapID::UsdStageViewportDisplay,
	ParamMapID::UsdStageAnimation,
	ParamMapID::UsdStageRenderSettings,
	ParamMapID::UsdStageViewportPerformance,
#else 
	ParamMapID::UsdStageGeneral,
	ParamMapID::UsdStageSelection,
	ParamMapID::UsdStageRenderSettings,
	ParamMapID::UsdStageViewportDisplay,
	ParamMapID::UsdStageAnimation,
	ParamMapID::UsdStageViewportPerformance,
#endif
	// Parameters
	StageFile, _M("FilePath"), TYPE_FILENAME, P_RESET_DEFAULT | P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_FILEPATH,
		p_default, _T(""),
		p_assetTypeID, MaxSDK::AssetManagement::kOtherAsset, 
		p_end,
	StageMask, _M("StageMask"), TYPE_STRING, P_RESET_DEFAULT | P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_STAGE_MASK,
		p_default, _T("/"),
		p_end,
	CacheId, _M("CacheId"), TYPE_INT, P_INVISIBLE | P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_CACHEID, 
		p_accessor, &pbAccessor, p_range, 0, LONG_MAX,
		p_end,
	Guid, _M("Guid"), TYPE_STRING, P_INVISIBLE | P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_GUID, 
		p_accessor, &pbAccessor,
		p_end,
	IsOpenInExplorer, _M("IsOpenInExplorer"), TYPE_BOOL, P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_IS_OPEN_IN_EXPLORER, 
		p_default, FALSE,
		p_end,
	AxisAndUnitTransform, _M("AxisAndUnitTransform"), TYPE_MATRIX3, P_INVISIBLE | P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_AXIS_AND_UNIT_TRANSFORM, 
		p_accessor, &pbAccessor, 
		p_end,
	DisplayProxy, _M("DisplayProxy"), TYPE_BOOL, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_DISPLAY_PROXY, 
		p_default, TRUE, 
		p_end,
	DisplayGuide, _M("DisplayGuide"), TYPE_BOOL, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_DISPLAY_GUIDE, 
		p_default, FALSE, 
		p_end,
	DisplayRender, _M("DisplayRender"), TYPE_BOOL, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_DISPLAY_RENDER, 
		p_default, FALSE, 
		p_end,
	DisplayMode, _M("DisplayMode"), TYPE_INT, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_DISPLAY_MODE, 
		p_default, 0,
		p_accessor, &pbAccessor,
		p_end,
	GeneratePointInstancesDrawModes, _M("GeneratePointInstancesDrawModes"), TYPE_BOOL, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_GEN_POINT_INSTANCE_DRAW_MODES, 
		p_default, TRUE,	
		p_end,
	PointInstancesDrawMode, _M("PointInstancesDrawMode"), TYPE_INT, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_POINT_INSTANCE_DRAW_MODE,
		p_default, DrawMode::BoxCards,
		p_end,
	ShowIcon, _M("ShowIcon"), TYPE_BOOL, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_SHOW_ICON, 
		p_default, TRUE, 
		p_end, 
	GenerateCameras, _M("GenerateCameras"), TYPE_BOOL, 0,
		IDS_USDSTAGEOBJECT_ROLL_OUT_GENERATE_CAMERAS,
		p_default, TRUE, 
		p_end,
	LoadPayloads, _M("LoadPayloads"), TYPE_BOOL, P_OBSOLETE | P_READ_ONLY | P_INVISIBLE, IDS_USDSTAGEOBJECT_ROLL_OUT_LOAD_PAYLOADS, 
		p_default, TRUE,
		p_end,
	SourceMetersPerUnit, _M("SourceMetersPerUnit"), TYPE_FLOAT, P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_SOURCE_METERS_PER_UNIT, 
		p_accessor, &pbAccessor,
		p_end,
	SourceUpAxis, _M("SourceUpAxis"), TYPE_STRING, P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_SOURCE_UP_AXIS,
		p_accessor, &pbAccessor,
		p_end, 
	IconScale, _M("IconScale"), TYPE_FLOAT, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_ICON_SCALE, 
		p_default, 1.0f,
		p_range, 0.f, 999999999.f,
		p_end,
	MeshMergeMode, _M("MeshMergeMode"), TYPE_INT, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_CONSOLIDATION_MODE,
		p_default, HdMaxConsolidator::Strategy::Static, 
		p_end,
	MeshMergeDiagnosticView, _M("MeshMergeDiagnosticView"), TYPE_BOOL, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_CONSOLIDATION_DIAGNOSTICS, 
		p_default, FALSE,
		p_end,
	MeshMergeMaxTriangles, _M("MeshMergeMaxTriangles"), TYPE_INT, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_CONSOLIDATION_MAX_TRIANGLES,
		p_default, 20000,
		p_range, 0, 999999999,
		p_end,
	MeshMergeMaxInstances, _M("MeshMergeMaxInstances"), TYPE_INT, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_CONSOLIDATION_MAX_INSTANCE_COUNT, 
		p_default, 20,
		p_range, 0, 999999999,
		p_end,
	MaxMergedMeshTriangles, _M("MaxMergedMeshTriangles"), TYPE_INT, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_CONSOLIDATION_MAX_CELL_SIZE,
		p_default, 200000,
		p_range, 0, 999999999,
		p_end,
	PointedPrim, _M("PointedPrim"), TYPE_STRING, P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_POINTED_PRIM,
		p_accessor, &pbAccessor,
		p_end,
	CustomAnimationStartFrame, _M("CustomAnimationStartFrame"), TYPE_FLOAT, P_ANIMATABLE | P_RESET_DEFAULT, IDS_USDSTAGEOBJECT_ROLL_OUT_CUSTOM_ANIM_START_FRAME, 
		p_default, 0.0f,
		p_range, -9999999.f, 9999999.f,
		p_end,
	CustomAnimationSpeed, _M("CustomAnimationSpeed"), TYPE_FLOAT, P_ANIMATABLE | P_RESET_DEFAULT, IDS_USDSTAGEOBJECT_ROLL_OUT_CUSTOM_ANIM_SPEED, 
		p_default, 1.0f,
		p_range, -9999999.f, 9999999.f,
		p_end,
	CustomAnimationEndFrame, _M("CustomAnimationEndFrame"), TYPE_FLOAT, P_ANIMATABLE | P_RESET_DEFAULT, IDS_USDSTAGEOBJECT_ROLL_OUT_CUSTOM_ANIM_END_FRAME, 
		p_default, 0.0f,
		p_range, -9999999.f, 9999999.f,
		p_end,
	CustomAnimationPlaybackTimecode, _M("CustomAnimationPlaybackTimecode"), TYPE_FLOAT, P_ANIMATABLE | P_RESET_DEFAULT, IDS_USDSTAGEOBJECT_ROLL_OUT_CUSTOM_ANIM_PLAYBACK_TIMECODE, 
		p_default, 0.0f,
		p_range, 0.f, 9999999.f,
		p_end,
	AnimationMode, _M("AnimationMode"), TYPE_INT, 0, IDS_USDSTAGEOBJECT_ROLL_OUT_ANIM_MODE, 
		p_default, 0,
		p_accessor, &pbAccessor,
		p_end,
	SourceAnimationStartTimeCode, _M("SourceAnimationStartTimeCode"), TYPE_FLOAT, P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_SOURCE_ANIM_START_TIME_CODE, 
		p_accessor, &pbAccessor,
		p_end,
	SourceAnimationEndTimeCode, _M("SourceAnimationEndTimeCode"), TYPE_FLOAT, P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_SOURCE_ANIM_END_TIME_CODE,
		p_accessor, &pbAccessor,
		p_end, 
	SourceAnimationTPS, _M("SourceAnimationTPS"), TYPE_FLOAT, P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_SOURCE_ANIM_TPS,
		p_accessor, &pbAccessor,
		p_end,
	MaxAnimationStartFrame, _M("MaxAnimationStartFrame"), TYPE_FLOAT, P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_MAX_ANIM_START_FRAME, 
		p_accessor, &pbAccessor,
		p_end,
	MaxAnimationEndFrame, _M("MaxAnimationEndFrame"), TYPE_FLOAT, P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_MAX_ANIM_END_FRAME,
		p_accessor, &pbAccessor,
		p_end,
	RenderUsdTimeCode, _M("RenderUsdTimeCode"), TYPE_FLOAT, P_READ_ONLY, IDS_USDSTAGEOBJECT_ROLL_OUT_MAX_ANIM_RENDER_USD_TIMECODE,
		p_accessor, &pbAccessor,
		p_end,
	KindSelection, _M("KindSelection"), TYPE_STRING, P_RESET_DEFAULT, IDS_USDSTAGEOBJECT_ROLL_OUT_KIND_SELECTION,
		p_default, _T(""),
		p_end,
	p_end
);
// clang-format on

inline std::vector<pxr::TfType> GetAllAncestorSchemaTypes(const pxr::UsdPrim& usdPrim)
{
    std::vector<pxr::TfType> result;
    if (usdPrim) {
        auto& info = usdPrim.GetPrimTypeInfo();
        auto& type = info.GetSchemaType();

        static std::map<pxr::TfType, std::vector<pxr::TfType>> cache;
        auto                                                   it = cache.find(type);
        if (it != cache.end()) {
            return it->second;
        }

        std::vector<pxr::TfType> types;
        type.GetAllAncestorTypes(&types);

        for (auto& t : types) {
            if (!type.IsA<pxr::UsdSchemaBase>()) {
                continue;
            }
            result.push_back(t);
        }

        cache[type] = result;
    }
    return result;
}

} // namespace

FPInterfaceDesc* USDStageObject::GetDesc() { return &usdStageInterface; }

static void NotifyTimeRangeChanged(void* param, NotifyInfo* /*info*/)
{
    USDStageObject* usdStageObject = static_cast<USDStageObject*>(param);
    // Timeline FPS may have changed, clear the bounding box cache.
    usdStageObject->ClearBoundingBoxCache();
}

static void NotifySelectionHighlightConfigChanged(void* param, NotifyInfo* /*info*/)
{
    const auto usdStageObject = static_cast<USDStageObject*>(param);
    usdStageObject->DirtySelectionDisplay();
}

static void NotifyUnitsChanged(void* param, NotifyInfo* /*info*/)
{
    // The hydra render depends on the transform to adjust for the USD axis and units VS the 3dsMax
    // units. If units change, the render is no longer valid, the cached bounding boxes neither.
    USDStageObject* usdStageObject = static_cast<USDStageObject*>(param);
    usdStageObject->ClearBoundingBoxCache();
    // As the USD Stage is a reference, changing the 3dsMax system units will change the stage's
    // representation in the viewport (scaled up or down, depending on the stage's units). So we
    // tell the object to redraw itself.
    usdStageObject->Redraw();
}

static void NotifyNodeDeleted(void* param, NotifyInfo* info)
{
    if (!info->callParam) {
        return;
    }
    const auto deletedNode = static_cast<INode*>(info->callParam);
    const auto usdStageObject = static_cast<USDStageObject*>(param);
    if (!usdStageObject || deletedNode->GetObjectRef() != usdStageObject) {
        return;
    }

    // Remove any entry for this node in the hitTesting cache.
    usdStageObject->hitTestingCache.erase(deletedNode);
    // Cleanup any cameras associated with this stage object.
    usdStageObject->DeleteCameraNodes(deletedNode);
}

static void NotifyNodeCreated(void* param, NotifyInfo* info)
{
    if (!info->callParam) {
        return;
    }
    const auto addedNode = static_cast<INode*>(info->callParam);
    const auto usdStageObject = static_cast<USDStageObject*>(param);
    if (!usdStageObject || addedNode->GetObjectRef() != usdStageObject) {
        return;
    }

    // While cloning, node created notification are sent for the transient objects that
    // are created in the scene to preview the clone. No need to react to those (and it would
    // actually lead to issues - as those transient nodes are created outside of holds).
    if (usdStageObject->inCloneOperation) {
        return;
    }

    usdStageObject->BuildCameraNodes(addedNode);
}

static void NotifyNodePreClone(void* param, NotifyInfo* info)
{
    if (!info->callParam) {
        return;
    }
    const auto usdStageObject = static_cast<USDStageObject*>(param);
    if (!usdStageObject) {
        return;
    }
    usdStageObject->inCloneOperation = true;
}

static void NotifyNodePostClone(void* param, NotifyInfo* info)
{
    if (!info->callParam) {
        return;
    }

    const auto usdStageObject = static_cast<USDStageObject*>(param);
    if (!usdStageObject) {
        return;
    }
    usdStageObject->inCloneOperation = false;

    const auto cloneInfo = static_cast<MaxSDKSupport::NotifyPostNodesCloned*>(info->callParam);
    if (!cloneInfo) {
        return;
    }

    auto clonedNodes = MaxSDKSupport::GetClonedNodes(cloneInfo);
    for (int i = 0; i < clonedNodes->Count(); ++i) {
        auto node = (*clonedNodes)[i];
        if (dynamic_cast<USDStageObject*>(node->GetObjectRef())) {
            usdStageObject->BuildCameraNodes(node);
            // Force a refresh of the cameras.
            Interval valid = FOREVER;
            usdStageObject->ForceNotify(valid);
        }
    }
}

bool GetParamBlockBool(IParamBlock2* paramBlock, PBParameterIds id)
{
    BOOL     value = false;
    Interval valid;
    paramBlock->GetValue(id, GetCOREInterface()->GetTime(), value, valid);
    return static_cast<bool>(value);
}

int GetParamBlockInt(IParamBlock2* paramBlock, PBParameterIds id)
{
    int      value = 0;
    Interval valid;
    paramBlock->GetValue(id, GetCOREInterface()->GetTime(), value, valid);
    return value;
}

float GetParamBlockFloat(IParamBlock2* paramBlock, PBParameterIds id)
{
    float    value = 0.0;
    Interval valid;
    paramBlock->GetValue(id, GetCOREInterface()->GetTime(), value, valid);
    return value;
}

void USDStageObject::CleanupPrimAttributeWidgets()
{
    for (const auto& w : primAttributeWidgets) {
        if (w) {
            if (!w->property("ignoreRollupOpenState").toBool()) {
                if (auto rollup = dynamic_cast<MaxSDK::QmaxRollup*>(w->parentWidget())) {
                    primAttributeRollupStates[rollup->title()] = rollup->isOpen();
                }
            }

            if (ip) {
                ip->DeleteRollupPage(*w);
            } else {
                w->deleteLater();
            }
        }
    }
    primAttributeWidgets.clear();
}

void USDStageObject::AdjustAttributeRollupsForSelection()
{
    // We need to remember the categories aka order of the common rollups,
    // before removing the ones we won't show (based on the sub-object level and
    // the current selection).
    auto sid = this->SuperClassID();
    auto cid = this->ClassID();

    if (auto rollupSettings = GetIRollupSettings()) {
        if (auto catReg = rollupSettings->GetCatReg()) {
            std::vector<std::pair<MaxSDK::QmaxRollup*, int>> default_rollups;

            if (auto pb = this->GetParamBlockByID(0)) {
                for (auto mapID : { ParamMapID::UsdStageGeneral,
                                    ParamMapID::UsdStageSelection,
                                    ParamMapID::UsdStageViewportDisplay,
                                    ParamMapID::UsdStageAnimation,
                                    ParamMapID::UsdStageRenderSettings,
                                    ParamMapID::UsdStageViewportPerformance }) {
                    if (auto map = pb->GetMap(mapID)) {
                        if (auto widget = map->GetQWidget()) {
                            if (auto rollup
                                = dynamic_cast<MaxSDK::QmaxRollup*>(widget->parentWidget())) {
                                default_rollups.push_back(
                                    { rollup,
                                      catReg->GetCat(
                                          sid,
                                          cid,
                                          rollup->title().toStdWString().c_str(),
                                          rollup->category()) });
                                if (mapID != ParamMapID::UsdStageSelection) {
                                    const bool stageLevel = subObjectLevel == 0;
                                    rollup->setVisible(stageLevel);
                                    // The command panel relies on the "sizeHint" of the rollups to
                                    // calculate on what column to display the rollups. When hidden,
                                    // the rollups conserves the same sizeHint, therefor, trick QT
                                    // into returning a (0,0) size hint, by also hiding the widget
                                    // inside the rollup, and removing margins.
                                    widget->setVisible(stageLevel);
                                    static const auto contentMargins = rollup->contentsMargins();
                                    rollup->setContentsMargins(
                                        stageLevel ? contentMargins : QMargins { 0, 0, 0, 0 });
                                }
                            }
                        }
                    }
                }
            }

            std::stable_sort(
                default_rollups.begin(), default_rollups.end(), [](const auto& a, const auto& b) {
                    return a.second < b.second;
                });

            // Clear all the data - this will remove all the entries for the
            // current selection as well.
            catReg->DeleteList(sid, cid);

            // Add the default rollups back in the user-specified order.
            int category = ROLLUP_CAT_STANDARD;

#if MAX_VERSION_MAJOR >= 26 // 2024+
            if (auto catReg2 = dynamic_cast<ICatRegistry2*>(catReg)) {
                for (const auto& r : default_rollups) {
                    catReg2->UpdateCat(
                        sid,
                        cid,
                        r.first->title().toStdWString().c_str(),
                        category++,
                        r.first->isOpen());
                }
            } else {
#endif
                for (const auto& r : default_rollups) {
                    catReg->UpdateCat(
                        sid, cid, r.first->title().toStdWString().c_str(), category++);
                }
#if MAX_VERSION_MAJOR >= 26 // 2024+
            }
#endif
            catReg->Save();
        }
    }

    // remove old rollups
    CleanupPrimAttributeWidgets();

    if (subObjectLevel != 0 && ip) {
        Ufe::Selection filteredSelection;
        for (const auto& sceneItem : *Ufe::GlobalSelection::get()) {
            if (sceneItem && sceneItem->path().startsWith(MaxUsd::ufe::getUsdStageObjectPath(this))
                && !MaxUsd::ufe::isPointInstance(
                    sceneItem)) // No rollout for point instances for now.
            {
                filteredSelection.append(sceneItem);
            }
        }
        if (filteredSelection.empty()) {
            return;
        }

        // add rollups
        int category = 10000;

        auto w = new QWidget();
        auto l = new QGridLayout(w);

        auto label = new QLabel(QApplication::translate("USDStageObject", "Name"));
        auto textEdit = new QLineEdit();
        textEdit->setReadOnly(true);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setBuddy(textEdit);
        l->addWidget(label, 0, 0);
        l->addWidget(textEdit, 0, 1);

        if (filteredSelection.size() == 1) {
            auto selected_prim = filteredSelection.front();
            textEdit->setText(QString::fromStdString(selected_prim->nodeName()));
        } else {
            textEdit->setText(QApplication::translate("USDStageObject", "Multiple prims selected"));
            textEdit->setDisabled(true);
        }

        std::unordered_set<std::string> primTypes;
        for (const auto& prim : filteredSelection) {
            primTypes.insert(prim->nodeType());
        }

        label = new QLabel(QApplication::translate("USDStageObject", "Type"));
        if (primTypes.size() == 1) {
            textEdit = new QLineEdit(QString::fromStdString(*primTypes.begin()));
        } else {
            textEdit = new QLineEdit(
                QApplication::translate("USDStageObject", "Multiple types selected"));
            textEdit->setDisabled(true);
        }
        textEdit->setReadOnly(true);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setBuddy(textEdit);
        l->addWidget(label, 1, 0);
        l->addWidget(textEdit, 1, 1);

        l->setColumnStretch(0, 1);
        l->setColumnStretch(1, 2);

        ip->AddRollupPage(
            *w,
            QApplication::translate("USDStageObject", "General").toStdWString().c_str(),
            0,
            category++);
        primAttributeWidgets.push_back(w);

        std::vector<pxr::TfType> commonAncestors;
        if (primTypes.size() == 1) {
            auto usdPrim = MaxUsd::ufe::ufePathToPrim(filteredSelection.front()->path());
            commonAncestors = GetAllAncestorSchemaTypes(usdPrim);
        } else {
            bool firstOne = true;
            for (const auto& item : filteredSelection) {
                auto usdPrim = MaxUsd::ufe::ufePathToPrim(item->path());
                auto ancestors = GetAllAncestorSchemaTypes(usdPrim);
                if (firstOne) {
                    commonAncestors = ancestors;
                    firstOne = false;
                } else {
                    // remove from common if not in ancestors
                    commonAncestors.erase(
                        std::remove_if(
                            commonAncestors.begin(),
                            commonAncestors.end(),
                            [&ancestors](const auto& it) {
                                return std::find(ancestors.begin(), ancestors.end(), it)
                                    == ancestors.end();
                            }),
                        commonAncestors.end());
                }
            }
        }

        std::set<std::string> handledAttributeNames;

        bool firstOne = true;

        auto addRollup = [this, &firstOne, &category](std::unique_ptr<QWidget> widget) {
            if (widget) {
                auto w = widget.release();
                auto rollupTitle = w->objectName();
                int  rollupFlags = 0;

                if (firstOne) {
                    w->setProperty("ignoreRollupOpenState", true);
                    firstOne = false;
                } else {
                    auto it = primAttributeRollupStates.find(rollupTitle);
                    if (it != primAttributeRollupStates.end()) {
                        if (!it->second) {
                            rollupFlags = APPENDROLL_CLOSED;
                        }
                    }
                }

                ip->AddRollupPage(*w, rollupTitle.toStdWString().c_str(), rollupFlags, category++);
                primAttributeWidgets.push_back(w);
            }
        };

        // Type schemas.
        for (const auto& t : commonAncestors) {
            auto widget = MaxUsd::ufe::QmaxUsdUfeAttributesWidget::create(
                filteredSelection, t, handledAttributeNames);
            addRollup(std::move(widget));
        }

        // Applied schemas.
        pxr::TfHashSet<pxr::TfToken, pxr::TfToken::HashFunctor> commonAppliedSchemas;
        bool                                                    firstItem = true;
        for (const auto& sel : filteredSelection) {
            auto usdPrim = MaxUsd::ufe::ufePathToPrim(sel->path());
            auto schemas = usdPrim.GetAppliedSchemas();

            if (firstItem) {
                for (const auto& schema : schemas) {
                    commonAppliedSchemas.insert(schema);
                }
                firstItem = false;
                continue;
            }

            for (const auto& cs : commonAppliedSchemas) {
                if (std::find(schemas.begin(), schemas.end(), cs) == schemas.end()) {
                    commonAppliedSchemas.erase(cs);
                }
            }
        }
        for (const auto& as : commonAppliedSchemas) {

            const auto& type = pxr::UsdSchemaRegistry::GetTypeFromName(as);
            addRollup(MaxUsd::ufe::QmaxUsdUfeAttributesWidget::create(
                filteredSelection, type, handledAttributeNames));
        }

        // Catch all rollup, any attribute that not part of a schema, or that we missed.
        // Only display this rollup on single selection.
        if (filteredSelection.size() == 1) {
            const auto usdPrim = MaxUsd::ufe::ufePathToPrim(filteredSelection.front()->path());

            std::vector<std::string> extraAttrNames;
            for (const auto& attr : usdPrim.GetAuthoredAttributes()) {
                const auto& name = attr.GetName();
                // Attribute is authored and not yet handled, we want it!
                if (handledAttributeNames.find(name) == handledAttributeNames.end()) {

                    extraAttrNames.push_back(name);
                }
            }

            if (auto widget = MaxUsd::ufe::QmaxUsdUfeAttributesWidget::create(
                    filteredSelection, extraAttrNames, handledAttributeNames)) {
                widget->setObjectName(
                    QApplication::translate("USDStageObject", "Extra Attributes"));
                addRollup(std::move(widget));
            }
        }

        addRollup(MaxUsd::ufe::QmaxUsdUfeAttributesWidget::createMetaData(
            filteredSelection, handledAttributeNames));
    }
}

void USDStageObject::DirtySelectionDisplay() { isSelectionDisplayDirty = true; }

void USDStageObject::UpdatePrimSelectionDisplay()
{
    if (!isSelectionDisplayDirty) {
        return;
    }
    isSelectionDisplayDirty = false;

    // If disabled, or at object level : we do not want to display prim selection at all.
    if (!HdMaxDisplayPreferences::GetInstance().GetSelectionHighlightEnabled()
        || subObjectLevel == 0) {
        hydraEngine->SetSelection({});
        return;
    }

    // Otherwise, configure the hydra selection display from the UFE selection.
    const auto&               globalSelection = Ufe::GlobalSelection::get();
    std::vector<pxr::SdfPath> paths;
    auto                      relevant = [this](const Ufe::Path& path) {
        return path.startsWith(MaxUsd::ufe::getUsdStageObjectPath(this));
    };

    std::unordered_map<pxr::SdfPath, pxr::VtIntArray, pxr::SdfPath::Hash> newSelection;

    for (const auto& item : *globalSelection) {
        const auto& path = item->path();
        if (relevant(path)) {
            const UsdUfe::UsdSceneItem::Ptr usdItem
                = std::dynamic_pointer_cast<UsdUfe::UsdSceneItem>(item);
            if (usdItem && usdItem->isPointInstance()) {
                newSelection[usdItem->prim().GetPath()].push_back(usdItem->instanceIndex());
            } else {
                newSelection[usdItem->prim().GetPath()];
            }
        }
    }
    hydraEngine->SetSelection(newSelection);
}

/**
 * \brief Observes a UFE subject and update the selection display in the VP.
 */
class SelectionObserver : public Ufe::Observer
{
public:
    SelectionObserver(USDStageObject* stageObject) { this->stageObject = stageObject; }

    void operator()(const Ufe::Notification& notification) override
    {
        if (const auto sc = dynamic_cast<const Ufe::SelectionChanged*>(&notification)) {
            stageObject->AdjustAttributeRollupsForSelection();
            stageObject->DirtySelectionDisplay();
            stageObject->Redraw();
        }
    }

private:
    USDStageObject* stageObject = nullptr;
};

USDStageObject::USDStageObject()
    : usdMaterials { NewDefaultMultiMtl() }
{
    hydraEngine = std::make_unique<HdMaxEngine>();

    CreateParameterBlock2(&propertiesParamblock, this);

    // Register ourselves as a listener for USD stage change notifications. Another USD client could
    // be changing the scene.
    pxr::TfWeakPtr<USDStageObject> me(this);
    onStageChangeNotice = pxr::TfNotice::Register(me, &USDStageObject::OnStageChange);

    RegisterNotification(NotifyTimeRangeChanged, this, NOTIFY_TIMERANGE_CHANGE);
    RegisterNotification(NotifyUnitsChanged, this, NOTIFY_UNITS_CHANGE);
    RegisterNotification(NotifyNodeDeleted, this, NOTIFY_SCENE_PRE_DELETED_NODE);
    RegisterNotification(NotifyNodeCreated, this, NOTIFY_NODE_CREATED);
    RegisterNotification(NotifyNodePreClone, this, NOTIFY_PRE_NODES_CLONED);
    RegisterNotification(NotifyNodePostClone, this, NOTIFY_POST_NODES_CLONED);

    RegisterNotification(
        NotifySelectionHighlightConfigChanged, this, NOTIFY_SELECTION_HIGHLIGHT_ENABLED_CHANGED);

    nodeEventCallbackKey = GetISceneEventManager()->RegisterCallback(&nodeEventCallback);

    // Init viewport icon display
    UpdateViewportStageIcon();

    guid = MaxUsd::GenerateGUID();

    selectionObserver = std::make_shared<SelectionObserver>(this);
    Ufe::GlobalSelection::get()->addObserver(selectionObserver);
}

USDStageObject::~USDStageObject()
{
    UnRegisterNotification(NotifyTimeRangeChanged, this, NOTIFY_TIMERANGE_CHANGE);
    UnRegisterNotification(NotifyUnitsChanged, this, NOTIFY_UNITS_CHANGE);
    UnRegisterNotification(NotifyNodeDeleted, this, NOTIFY_SCENE_PRE_DELETED_NODE);
    UnRegisterNotification(
        NotifySelectionHighlightConfigChanged, this, NOTIFY_SELECTION_HIGHLIGHT_ENABLED_CHANGED);
    UnRegisterNotification(NotifyNodeCreated, this, NOTIFY_NODE_CREATED);
    UnRegisterNotification(NotifyNodePreClone, this, NOTIFY_PRE_NODES_CLONED);
    UnRegisterNotification(NotifyNodePostClone, this, NOTIFY_POST_NODES_CLONED);

    if (stage) {
        // If the stage is currently opened in the explorer, close it.
        USDExplorer::Instance()->CloseStage(this);
        pxr::UsdUtilsStageCache::Get().Erase(stageCacheId);
        stageCacheId = {};

        StageObjectMap::GetInstance()->Remove(this);
    }

    pxr::TfNotice::Revoke(onStageChangeNotice);
    stage = pxr::TfNullPtr;

    GetISceneEventManager()->UnRegisterCallback(nodeEventCallbackKey);
}

void USDStageObject::SetReference(int i, RefTargetHandle rtarg)
{
    pb = dynamic_cast<IParamBlock2*>(rtarg);
}

int USDStageObject::NumRefs() { return 1; }

ReferenceTarget* USDStageObject::GetReference(int i) { return (i == 0) ? pb : nullptr; }

int USDStageObject::NumParamBlocks() { return 1; }

IParamBlock2* USDStageObject::GetParamBlock(int i) { return (i == 0) ? pb : nullptr; }

IParamBlock2* USDStageObject::GetParamBlockByID(BlockID id)
{
    if (pb && pb->ID() == id) {
        return pb;
    }
    return nullptr;
}

int USDStageObject::NumSubs() { return 1; }

Animatable* USDStageObject::SubAnim(int i) { return pb; }

TSTR USDStageObject::SubAnimName(int i, bool localized)
{
    return localized ? GetString(IDS_PARAMS) : _T("Parameters");
}

int USDStageObject::SubNumToRefNum(int subNum)
{
    if (subNum == PBLOCK_REF) {
        return subNum;
    }
    return -1;
}

void USDStageObject::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
    this->ip = ip;
    if (flags & BEGIN_EDIT_CREATE) {
        isInCreateMode = true;
    } else {
        isInCreateMode = false;
        selectMode = new SelectModBoxCMode(this, ip);
        moveMode = new MoveModBoxCMode(this, ip);
        rotateMode = new RotateModBoxCMode(this, ip);
        uScaleMode = new UScaleModBoxCMode(this, ip);
        nuScaleMode = new NUScaleModBoxCMode(this, ip);
        squashMode = new SquashModBoxCMode(this, ip);
    }
    GetUSDStageObjectClassDesc()->BeginEditParams(ip, this, flags, prev);

    if (!primAttributeRollupOpenStatesLoaded) {
        primAttributeRollupOpenStatesLoaded = true;
        primAttributeRollupStates = loadedPrimAttributeRollupStates
            = MaxUsd::OptionUtils::LoadRollupStates(rollupCategory);
    }
    AdjustAttributeRollupsForSelection();
}

void USDStageObject::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
    if (!isInCreateMode) {
        ip->DeleteMode(selectMode);
        delete selectMode;
        selectMode = nullptr;

        ip->DeleteMode(moveMode);
        delete moveMode;
        moveMode = nullptr;

        ip->DeleteMode(rotateMode);
        delete rotateMode;
        rotateMode = nullptr;

        ip->DeleteMode(uScaleMode);
        delete uScaleMode;
        uScaleMode = nullptr;

        ip->DeleteMode(nuScaleMode);
        delete nuScaleMode;
        nuScaleMode = nullptr;

        ip->DeleteMode(squashMode);
        delete squashMode;
        squashMode = nullptr;
    } else {
        isInCreateMode = false;
    }
    GetUSDStageObjectClassDesc()->EndEditParams(ip, this, flags, next);

    CleanupPrimAttributeWidgets();

    this->ip = nullptr;

    HdMaxDisplayPreferences::GetInstance().Save();

    // Save the rollup states if needed.
    bool primAttributeRollupStatesChanged = false;
    if (primAttributeRollupStates.size() == loadedPrimAttributeRollupStates.size()) {
        for (const auto& it : primAttributeRollupStates) {
            auto it2 = loadedPrimAttributeRollupStates.find(it.first);
            if (it2 == loadedPrimAttributeRollupStates.end() || it2->second != it.second) {
                primAttributeRollupStatesChanged = true;
                break;
            }
        }
    } else {
        primAttributeRollupStatesChanged = true;
    }
    if (primAttributeRollupStatesChanged) {
        MaxUsd::OptionUtils::SaveRollupStates(rollupCategory, primAttributeRollupStates);
        loadedPrimAttributeRollupStates = primAttributeRollupStates;
    }
}

void USDStageObject::ClearRenderCache() { renderCache = {}; }

void USDStageObject::ClearBoundingBoxCache() { boundingBoxCache.clear(); }

void USDStageObject::FullStageReset()
{
    pickingRenderer.reset();

    // If the stage is currently opened in the explorer, close it.
    // TODO : This behavior is not what we will want in the end. It will be reviewed
    // along with the work to "save" an stage's open/closed state.
    USDExplorer::Instance()->CloseStage(this);
    StageObjectMap::GetInstance()->Remove(this);

    // Clear any selection belonging to this stage in the global UFE selection.
    const auto selection = Ufe::GlobalSelection::get();

    Ufe::Selection newSelection { *selection };
    const auto     objectPath = MaxUsd::ufe::getUsdStageObjectPath(this);
    for (const auto& item : newSelection) {
        if (!item->path().startsWith(objectPath)) {
            continue;
        }
        newSelection.remove(item);
    }
    // Replace the global selection all at once, to avoid sending many notifications.
    selection->replaceWith(newSelection);

    // Clear the current stage.
    pxr::UsdUtilsStageCache::Get().Erase(stageCacheId);
    stage = pxr::TfNullPtr;
    stageCacheId = {};

    // Reset the engine, to make sure we don't hold onto any state.
    hydraEngine = std::make_unique<HdMaxEngine>();

    if (auto owner = dynamic_cast<Object*>(pb->GetOwner())) {
        Interval valid = FOREVER;
        owner->ForceNotify(valid);
    }
    ClearAllCaches();
}

void USDStageObject::GenerateDrawModes()
{
    if (!stage) {
        return;
    }

    // Draw modes are generated in an anonymous sublayer of the session layer.
    auto              session = stage->GetSessionLayer();
    auto              subLayers = session->GetSubLayerPaths();
    const std::string reservedName = "MaxUsd_DrawModes_Reserved";

    pxr::SdfLayerRefPtr drawModesLayer;
    int                 layerIndex;

    for (layerIndex = 0; layerIndex < subLayers.size(); ++layerIndex) {
        std::string layerPath = subLayers[layerIndex];
        if (layerPath.find(reservedName) != std::string::npos) {
            // Identified this is our layer, however, if we are loading the max scene from disk, it
            // could no longer exist...
            if (auto layer = pxr::SdfLayer::FindOrOpen(subLayers[layerIndex])) {
                drawModesLayer = layer;
                break;
            }
            // Remove the "dead" layer. We will generate a new one below if required.
            session->RemoveSubLayerPath(layerIndex);
            break;
        }
    }

    if (!GetParamBlockBool(pb, GeneratePointInstancesDrawModes)) {
        // No existing draw modes layer, nothing to do...
        if (!drawModesLayer) {
            return;
        }
        // Clear the layer and remove it from the session layer.
        drawModesLayer->Clear();
        session->RemoveSubLayerPath(layerIndex);
        return;
    }

    if (!drawModesLayer) {
        drawModesLayer = pxr::SdfLayer::CreateAnonymous(reservedName);
        if (!drawModesLayer) {
            return;
        }
        session->InsertSubLayerPath(drawModesLayer->GetIdentifier());
    }

    pxr::TfToken drawMode;
    pxr::TfToken cardsGeom;
    const auto   activeMode = GetParamBlockInt(pb, PointInstancesDrawMode);

    switch (activeMode) {
    case 0: {
        drawMode = pxr::UsdGeomTokens->default_;
        cardsGeom = pxr::UsdGeomTokens->cross; // default, not used.
        break;
    }
    case 1: {
        drawMode = pxr::UsdGeomTokens->cards;
        cardsGeom = pxr::UsdGeomTokens->box;
        break;
    }
    case 2: {
        drawMode = pxr::UsdGeomTokens->cards;
        cardsGeom = pxr::UsdGeomTokens->cross;
        break;
    }
    }

    pxr::UsdEditContext editContext(stage, drawModesLayer);

    // Setup draw modes for the prototypes of all instancers in the scene.
    for (pxr::UsdPrim prim : stage->TraverseAll()) {
        if (!prim.IsA<pxr::UsdGeomPointInstancer>()) {
            continue;
        }

        const auto instancer = pxr::UsdGeomPointInstancer(prim);

        pxr::SdfPathVector targets;
        instancer.GetPrototypesRel().GetTargets(&targets);

        for (const auto& protoPath : targets) {
            auto prototype = stage->GetPrimAtPath(protoPath);

            if (!prototype.IsValid()) {
                continue;
            }

            const auto usdModelApi = pxr::UsdModelAPI(prototype);
            const auto geomModelApi = pxr::UsdGeomModelAPI::Apply(prototype);

            // Draw modes apply by default on "component" kind prims. If no kind is authored
            // on the prototype, we just set it as a component to get the Draw Mode going. A
            // Kind needs to be setup for the Draw Mode to work at all. However, if a Kind is
            // already authored, we preserve it, and instead explicitely specify that we want
            // the draw mode to run at this prim's level (unless if it is a component already,
            // in that case, no need to do anything more).
            pxr::TfToken kind;
            if (!usdModelApi.GetKind(&kind)) {
                usdModelApi.SetKind(pxr::KindTokens->component);
            } else {
                if (kind != pxr::KindTokens->component) {
                    geomModelApi.CreateModelApplyDrawModeAttr().Set(true);
                }
            }

            geomModelApi.CreateModelDrawModeAttr().Set(drawMode);
            geomModelApi.CreateModelCardGeometryAttr().Set(cardsGeom);

            // Make sure all ancestors have proper model kinds defined.
            auto current = prototype.GetParent();
            while (current.IsValid()) {
                auto         usdModelApi = pxr::UsdModelAPI(current);
                pxr::TfToken kind;
                if (!usdModelApi.GetKind(&kind)
                    || (kind != pxr::KindTokens->assembly && kind != pxr::KindTokens->group)) {
                    usdModelApi.SetKind(pxr::KindTokens->assembly);
                }
                current = current.GetParent();
            }
        }
    }
}

RefResult USDStageObject::NotifyRefChanged(
    const Interval& changeInt,
    RefTargetHandle hTarget,
    PartID&         partID,
    RefMessage      message,
    BOOL            propagate)
{
    switch (message) {
    case REFMSG_CHANGE: {
        auto pb = dynamic_cast<IParamBlock2*>(hTarget);
        if (!pb) {
            return REF_DONTCARE;
        }

        int     tabIndex = -1;
        ParamID pID = pb->LastNotifyParamID(tabIndex);
        switch (pID) {
            // There is auto-binding for the stage file, but the tooltip also needs to be set.
        case StageFile:
            // There is no auto-binding with the stage mask. Trigger an update manually.
        case StageMask: {
            if (const auto parametersMap = pb->GetMap(ParamMapID::UsdStageGeneral)) {
                parametersMap->UpdateUI(GetCOREInterface()->GetTime());
            }
            break;
        }
        case DisplayGuide: {
            displayPurposeUpdated = true;
            ClearAllCaches();
            Redraw();
            break;
        }
        case DisplayProxy: {
            displayPurposeUpdated = true;
            ClearAllCaches();
            Redraw();
            break;
        }
        case DisplayRender: {
            displayPurposeUpdated = true;
            ClearAllCaches();
            Redraw();
            break;
        }
        case CustomAnimationStartFrame:
        case CustomAnimationSpeed:
        case CustomAnimationEndFrame:
        case CustomAnimationPlaybackTimecode:
        case AnimationMode: {
            if (const auto usdStageAnimationParamsMap = pb->GetMap(ParamMapID::UsdStageAnimation)) {
                usdStageAnimationParamsMap->Invalidate(MaxAnimationStartFrame);
                usdStageAnimationParamsMap->Invalidate(MaxAnimationEndFrame);
            }
            // With the animation playback config changed, the rendered UsdTimeCode might have
            // changed, clear the bounding box cache.
            ClearBoundingBoxCache();
            Redraw();
            break;
        }
        case MeshMergeMode:
        case MeshMergeDiagnosticView:
        case MaxMergedMeshTriangles:
        case MeshMergeMaxTriangles:
        case MeshMergeMaxInstances:
        case DisplayMode: {
            Redraw();
            break;
        }
        case ShowIcon: {
            // The icon influences the bounding box.
            ClearBoundingBoxCache();
            Redraw();
            break;
        }
        case IconScale: {
            UpdateViewportStageIcon();
            // The icon influences the bounding box.
            ClearBoundingBoxCache();
            if (GetParamBlockBool(pb, ShowIcon)) {
                Redraw();
            }
            break;
        }
        case KindSelection: {
            // Notify the UI it should change. We do not use automatic binding for this parameter.
            if (const auto vpSelectionMap = pb->GetMap(ParamMapID::UsdStageSelection)) {
                vpSelectionMap->UpdateUI(GetCOREInterface()->GetTime());
            }
            break;
        }
        case GenerateCameras: {
            BuildCameraNodes();
        }
        case GeneratePointInstancesDrawModes:
        case PointInstancesDrawMode: {
            GenerateDrawModes();
            Redraw();
        }
        }
        break;
    }
    }
    return REF_SUCCEED;
}

int USDStageObject::NumSubObjTypes() { return 1; }

ISubObjType* USDStageObject::GetSubObjType(int i)
{
    if (i == 0) {
        static GenSubObjType subobjType(L"Prim", nullptr, 0);
        return &subobjType;
    }
    return nullptr;
}

void USDStageObject::ActivateSubobjSel(int level, XFormModes& modes)
{
    subObjectLevel = level;
    if (level) {
        modes = XFormModes(moveMode, rotateMode, nuScaleMode, uScaleMode, squashMode, selectMode);
        NotifyDependents(FOREVER, PART_SUBSEL_TYPE | PART_DISPLAY, REFMSG_CHANGE);
        GetCOREInterface()->PipeSelLevelChanged();
    }
    AdjustAttributeRollupsForSelection();
    DirtySelectionDisplay();
    Redraw();
}

void USDStageObject::SelectSubComponent(HitRecord* hitRec, BOOL selected, BOOL all, BOOL invert)
{
    const auto usdHit = dynamic_cast<UsdHitData*>(hitRec->hitData);
    if (!usdHit) {
        return;
    }

    const auto& hits = usdHit->GetHits();

    // Update the UFE global selection.
    Ufe::Selection newSelection(*Ufe::GlobalSelection::get());
    bool           selectionChanged = false;
    for (const auto& hit : hits) {
        auto resolvedPath = hit.primPath;
        auto prim = stage->GetPrimAtPath(hit.primPath);
        prim = MaxUsd::GetFirstNonInstanceProxyPrimAncestor(prim);
        resolvedPath = prim.GetPath();

        // Resolve selection based on the current mode.
        const MCHAR* kindSelectionPb = nullptr;
        Interval     valid = FOREVER;
        pb->GetValue(KindSelection, GetCOREInterface()->GetTime(), kindSelectionPb, valid);

        const auto kindSelection = pxr::TfToken { MaxUsd::MaxStringToUsdString(kindSelectionPb) };

        const auto subobjLevel = GetSubObjectLevel();
        if (static_cast<SelectionMode>(subobjLevel) == SelectionMode::Prim
            && !kindSelection.IsEmpty()) {
            const auto kindPrim = MaxUsd::GetPrimOrAncestorWithKind(prim, kindSelection);
            if (kindPrim) {
                resolvedPath = kindPrim.GetPath();
            }
        }

        // Update the UFE global selection.
        Ufe::Path ufePath;

        // If we resolved the selection to a path above in the hierarchy, don't consider the
        // instance index.
        if (resolvedPath == hit.primPath) {
            ufePath = MaxUsd::ufe::getUsdPrimUfePath(this, resolvedPath, hit.instanceIdx);
        } else {
            ufePath = MaxUsd::ufe::getUsdPrimUfePath(this, resolvedPath);
        }

        const auto sceneItem = Ufe::Hierarchy::createItem(ufePath);
        if (!sceneItem) {
            continue;
        }

        if (selected) {
            selectionChanged |= newSelection.append(sceneItem);
        } else {
            selectionChanged |= newSelection.remove(sceneItem);
        }
    }

    if (selectionChanged) {
        if (theHold.RestoreOrRedoing()) {
            Ufe::GlobalSelection::get()->replaceWith(newSelection);
        } else {
            Ufe::UndoableCommandMgr::instance().executeCmd(
                std::make_shared<UfeUi::ReplaceSelectionCommand>(newSelection));
        }
    }
}

void USDStageObject::ClearSelection(int level)
{
    if (!level) {
        return;
    }

    // Clear the UFE global selection.
    if (!Ufe::GlobalSelection::get()->empty()) {
        if (theHold.RestoreOrRedoing()) {
            Ufe::GlobalSelection::get()->clear();
        } else {
            Ufe::UndoableCommandMgr::instance().executeCmd(
                std::make_shared<UfeUi::ReplaceSelectionCommand>(Ufe::Selection({})));
        }
    }
}

void USDStageObject::GetSubObjectCenters(
    SubObjAxisCallback* cb,
    TimeValue           t,
    INode*              node,
    ModContext* /*mc*/)
{
    const auto transformables = GetTransformablesFromSelection();
    if (transformables.empty()) {
        return;
    }

    // Average out the positions of the Transformables we need to transform.
    Point3 avgCenter;
    size_t count = 0;
    for (auto& transformable : transformables) {
        // Point instances.
        if (!transformable.instanceIndices.empty()) {
            const auto instanceTransforms = GetMaxScenePointInstancesTransforms(
                node, transformable.prim, transformable.instanceIndices, t);
            for (const auto& mat : instanceTransforms) {
                avgCenter += mat.GetTrans();
            }
            count += instanceTransforms.size();
            continue;
        }
        // Xformable Prims.
        const auto primSceneTransform = GetMaxScenePrimTransform(node, transformable.prim, t, true);
        avgCenter += primSceneTransform.GetTrans();
        count++;
    }
    avgCenter /= static_cast<float>(count);
    cb->Center(avgCenter, 0);
}

void USDStageObject::GetSubObjectTMs(
    SubObjAxisCallback* cb,
    TimeValue           t,
    INode*              node,
    ModContext* /*mc*/)
{
    const auto transformables = GetTransformablesFromSelection();
    if (transformables.empty()) {
        return;
    }

    // Average out the positions and normals to find the sub-object TM when there are
    // multi-selections (which are used when transforming things in non-world coord systems).
    Point3 avgNormal;
    Point3 avgCenter;

    const auto addCenterAndNormal = [&avgCenter, &avgNormal](const Matrix3& transform) {
        avgCenter += transform.GetTrans();
        auto noTrans = transform;
        noTrans.SetTrans({});
        const auto up = noTrans * Point3 { 0.f, 0.f, 1.f };
        avgNormal += up;
    };

    size_t count = 0;
    for (int i = 0; i < transformables.size(); ++i) {
        const auto& transformable = transformables[i];

        // Point instances.
        if (!transformable.instanceIndices.empty()) {
            const auto instanceTransforms = GetMaxScenePointInstancesTransforms(
                node, transformable.prim, transformable.instanceIndices, t);
            for (const auto& t : instanceTransforms) {
                addCenterAndNormal(t);
            }
            count += instanceTransforms.size();
            continue;
        }
        // Xformable prims.
        auto primSceneTransform = GetMaxScenePrimTransform(node, transformable.prim, t, true);
        addCenterAndNormal(primSceneTransform);
        count++;
    }

    avgNormal /= static_cast<float>(count);
    avgCenter /= static_cast<float>(count);

    Matrix3 tm;
    avgNormal = avgNormal.Normalize();

    // Equivalent to SetMatrixFromNormal(), which is only available in the SDK in 2024+.
    Point3 vx;
    vx.z = .0f;
    vx.x = -avgNormal.y;
    vx.y = avgNormal.x;
    if (vx.x == .0f && vx.y == .0f) {
        vx.x = 1.0f;
    }
    tm.SetRow(0, vx);
    tm.SetRow(1, avgNormal ^ vx);
    tm.SetRow(2, avgNormal);
    tm.SetTrans(Point3(0, 0, 0));
    tm.NoScale();

    tm.SetTrans(avgCenter);
    cb->TM(tm, 0);
}

void USDStageObject::Move(
    TimeValue t,
    Matrix3&  partm,
    Matrix3&  tmAxis,
    Point3&   val,
    BOOL /*localOrigin*/)
{
    if (!stage || subObjectManips.empty()) {
        return;
    }

    Matrix3 translation;
    translation.SetTranslate(val);
    TransformInteractive(partm, tmAxis, translation);
}

void USDStageObject::Rotate(
    TimeValue t,
    Matrix3&  partm,
    Matrix3&  tmAxis,
    Quat&     val,
    BOOL /*localOrigin*/)
{
    if (!stage || subObjectManips.empty()) {
        return;
    }

    Matrix3 rotation;
    val.MakeMatrix(rotation);
    TransformInteractive(partm, tmAxis, rotation);
}

void USDStageObject::Scale(
    TimeValue t,
    Matrix3&  partm,
    Matrix3&  tmAxis,
    Point3&   val,
    BOOL /*localOrigin*/)
{
    if (!stage || subObjectManips.empty()) {
        return;
    }
    const auto scaling = ScaleMatrix(val);
    TransformInteractive(partm, tmAxis, scaling);
}

void USDStageObject::TransformStart(TimeValue t)
{
    // Setup a diagnostic delegate to log any errors in the listener.
    const auto del
        = MaxUsd::Diagnostics::ScopedDelegate::Create<MaxUsd::Diagnostics::ListenerDelegate>();

    const auto transformables = GetTransformablesFromSelection();
    if (transformables.empty()) {
        return;
    }

    // We read USD values at the current time code, but we will author at the default time code.
    const auto timeCode = MaxUsd::GetUsdTimeCodeFromMaxTime(stage, t);

    for (auto& transformable : transformables) {
        // Point instances.
        if (!transformable.instanceIndices.empty()) {
            const auto instancer = pxr::UsdGeomPointInstancer(transformable.prim);
            subObjectManips.push_back(std::make_unique<PointInstanceManip>(
                instancer, transformable.instanceIndices, timeCode));
            continue;
        }
        // Xformable prims.
        const auto xformable = pxr::UsdGeomXformable(transformable.prim);
        subObjectManips.push_back(std::make_unique<XformableManip>(xformable, timeCode));
    }
}

void USDStageObject::TransformFinish(TimeValue t)
{
    if (subObjectManips.empty()) {
        return;
    }

    // Use a composite command to properly support undo when transforming from a multi-selection.
    const auto compositeCmd = Ufe::CompositeUndoableCommand::create({});
    for (const auto& manip : subObjectManips) {
        if (auto cmd = manip->BuildTransformCmd()) {
            compositeCmd->append(cmd);
        }
    }
    if (!compositeCmd->cmdsList().empty()) {
        // UndoableCommandMgr will setup a diagnostics delegate.
        Ufe::UndoableCommandMgr::instance().executeCmd(MaxUfeUndoableCommandMgr::named(
            compositeCmd,
            QApplication::translate("USDStageObject", "Change USD transform").toStdString()));
    }
    subObjectManips.clear();
}

Matrix3 USDStageObject::GetMaxScenePrimTransform(
    INode*              stageNode,
    const pxr::UsdPrim& prim,
    const TimeValue&    time,
    bool                includePivot)
{
    const auto stageObject = dynamic_cast<USDStageObject*>(stageNode->GetObjectRef());
    if (!stageObject) {
        return {};
    }

    const auto timeCode = stageObject->ResolveRenderTimeCode(time);

    const auto imageable = pxr::UsdGeomImageable { prim };
    auto       usdWorldMatrix = imageable.ComputeLocalToWorldTransform(timeCode);
    if (includePivot) {
        const auto pivot = MaxUsd::GetPivotTransform(pxr::UsdGeomXformable { prim }, timeCode);
        usdWorldMatrix = pivot * usdWorldMatrix;
    }

    const auto objTm = MaxUsd::ToUsd(stageNode->GetObjectTM(time));
    const auto fullPrimMatrix = usdWorldMatrix * stageObject->GetStageRootTransform() * objTm;

    return MaxUsd::ToMaxMatrix3(fullPrimMatrix);
}

std::vector<Matrix3> USDStageObject::GetMaxScenePointInstancesTransforms(
    INode*                  stageNode,
    const pxr::UsdPrim&     instancerPrim,
    const std::vector<int>& instanceIndices,
    const TimeValue&        time)
{
    std::vector<Matrix3> maxSceneInstanceTransforms;

    const auto stageObject = dynamic_cast<USDStageObject*>(stageNode->GetObjectRef());
    if (!stageObject) {
        return maxSceneInstanceTransforms;
    }

    const auto timeCode = stageObject->ResolveRenderTimeCode(time);

    auto pointInstancer = pxr::UsdGeomPointInstancer { instancerPrim };

    // Compute the instance transfoms in "parent" space, i.e. relative to the instancer.
    pxr::VtArray<pxr::GfMatrix4d> instanceTransforms;
    pointInstancer.ComputeInstanceTransformsAtTime(&instanceTransforms, timeCode, timeCode);

    const auto      imageable = pxr::UsdGeomImageable { instancerPrim };
    pxr::GfMatrix4d instancerWorld = imageable.ComputeLocalToWorldTransform(timeCode);

    for (const auto& idx : instanceIndices) {

        pxr::GfMatrix4d instanceWorld;
        if (instanceTransforms.size() < static_cast<size_t>(idx + 1)) {
            // Can happen if none of the PRS attributes are authored, assume identity instance
            // transform.
            instanceWorld = instancerWorld;
        } else {
            instanceWorld = instanceTransforms[idx] * instancerWorld;
        }
        const auto objTm = MaxUsd::ToUsd(stageNode->GetObjectTM(time));
        const auto fullPrimMatrix = instanceWorld * stageObject->GetStageRootTransform() * objTm;
        maxSceneInstanceTransforms.push_back(MaxUsd::ToMaxMatrix3(fullPrimMatrix));
    }
    return maxSceneInstanceTransforms;
}

std::vector<USDStageObject::Transformable> USDStageObject::GetTransformablesFromSelection()
{
    auto& globalSelection = Ufe::GlobalSelection::get();
    if (globalSelection->empty()) {
        return {};
    }

    std::unordered_set<Ufe::Path> transformableItems;

    // First, gather all transformable USD entities in the selection.
    const auto& selection = *globalSelection;
    for (const auto& item : selection) {
        // Make sure the selected prim belongs to this object.
        const auto& path = item->path();
        if (!path.startsWith(MaxUsd::ufe::getUsdStageObjectPath(this))) {
            continue;
        }

        if (MaxUsd::ufe::isPointInstance(item)) {
            transformableItems.insert(path);
            continue;
        }

        const auto prim = MaxUsd::ufe::ufePathToPrim(path);
        // If the prim is not xformable, get out now. This way the transform gizmos will not be
        // showed at all.
        if (!prim.IsValid() || !prim.IsA<pxr::UsdGeomXformable>()) {
            continue;
        }
        transformableItems.insert(path);
    }

    // Next, find the root-most transformable entities. I.e. only keep the prims/instances that do
    // not have an ancestor that is selected and itself transformable.
    std::vector<Ufe::SceneItem::Ptr> rootMostTransformableItems;
    // Cache paths that are known to be descendants of selected transformables.
    std::unordered_set<Ufe::Path> transformableDescendants;

    for (const auto& path : transformableItems) {
        if (transformableDescendants.find(path) != transformableDescendants.end()) {
            continue;
        }

        // Go up the hierarchy looking for a transformable.
        auto                   current = path.pop();
        bool                   hasSelectedTransformableAncestor = false;
        std::vector<Ufe::Path> descendants;

        while (!current.empty()) {
            // Current parent was itself already found to be a transformable descendant, or is a
            // selected transformable.
            if (transformableDescendants.find(current) != transformableDescendants.end()
                || transformableItems.find(current) != transformableItems.end()) {
                hasSelectedTransformableAncestor = true;
                break;
            }
            descendants.push_back(current);
            current = current.pop();
        }

        if (hasSelectedTransformableAncestor) {
            for (const auto& descendant : descendants) {
                transformableDescendants.insert(descendant);
            }
            continue;
        }

        rootMostTransformableItems.push_back(Ufe::Hierarchy::createItem(path));
    }

    // Finally, build the Transformable objects to return to the caller,
    // here we aggregate instances that belong to the same instancers together.
    std::vector<Transformable> transformables;

    pxr::TfHashMap<pxr::SdfPath, std::vector<int>, pxr::SdfPath::Hash> pointInstancers;
    for (const auto& item : rootMostTransformableItems) {
        auto         usdItem = std::dynamic_pointer_cast<UsdUfe::UsdSceneItem>(item);
        pxr::UsdPrim prim = usdItem->prim();
        int          instanceIdx = usdItem->instanceIndex();
        if (instanceIdx >= 0) {
            pointInstancers[prim.GetPath()].push_back(instanceIdx);
            continue;
        }
        transformables.push_back({ prim, {} });
    }
    for (const auto& instancer : pointInstancers) {
        transformables.push_back({ stage->GetPrimAtPath(instancer.first), instancer.second });
    }
    return transformables;
}

void USDStageObject::TransformInteractive(
    const Matrix3& partm,
    const Matrix3& tmAxis,
    const Matrix3& transform) const
{
    // Setup a diagnostic delegate to log any errors in the listener.
    const auto del
        = MaxUsd::Diagnostics::ScopedDelegate::Create<MaxUsd::Diagnostics::ListenerDelegate>();
    for (const auto& manip : subObjectManips) {
        manip->TransformInteractive(GetStageRootTransform(), partm, tmAxis, transform);
    }
}

void USDStageObject::SetRootLayerMXS(
    const wchar_t* rootLayer,
    const wchar_t* stageMask,
    bool           payloadsLoaded)
{
    // Make sure the given paths are valid.
    const std::string filename = MaxUsd::MaxStringToUsdString(rootLayer);
    if (filename.empty() || MaxUsd::HasUnicodeCharacter(filename)
        || !pxr::UsdStage::IsSupportedFile(filename)) {
        auto errorMsg = std::wstring(L"rootLayer could not be set. Invalid file path found : ");
        if (rootLayer) {
            errorMsg.append(rootLayer);
        } else {
            errorMsg.append(L"undefined");
        }
        throw RuntimeError(errorMsg.c_str());
    }

    const auto primPath = pxr::SdfPath(MaxUsd::MaxStringToUsdString(stageMask));
    if (!primPath.IsAbsolutePath() || !primPath.IsAbsoluteRootOrPrimPath()) {
        const auto errorMsg
            = std::wstring(L"stageMask could not be set. Invalid USD absolute prim path found : ")
                  .append(stageMask);
        throw RuntimeError(errorMsg.c_str());
    }
    SetRootLayer(rootLayer, stageMask, payloadsLoaded);
}

void USDStageObject::SetRootLayer(
    const wchar_t* rootLayer,
    const wchar_t* stageMask,
    bool           payloadsLoaded)
{

    class StageRestoreObj
        : public RestoreObj
        , private SingleRefMaker
    {
    public:
        StageRestoreObj(
            USDStageObject*     object,
            IParamBlock2*       pb,
            const std::wstring& oldRootLayer,
            const std::wstring& oldStageMask,
            const std::wstring& newRootLayer,
            const std::wstring& newStageMask)
            : oldRootLayer { oldRootLayer }
            , oldStageMask { oldStageMask }
            , newRootLayer { newRootLayer }
            , newStageMask { newStageMask }
            , pb { pb }
            , object { object }
        {
            // Keep a reference on the stage object, to make sure it's not garbage collected.
            this->SetRef(object);
            this->SetAutoDropRefOnShutdown(AutoDropRefOnShutdown::PrePluginShutdown);

            oldStageRef = object->GetUSDStage();
        }
        void Restore(int isUndo) override
        {
            newStageRef = object->GetUSDStage();

            pb->SetValue(
                PBParameterIds::StageFile, GetCOREInterface()->GetTime(), oldRootLayer.c_str());
            pb->SetValue(
                PBParameterIds::StageMask, GetCOREInterface()->GetTime(), oldStageMask.c_str());
            object->LoadUSDStage(oldStageRef);
        }
        void Redo() override
        {
            pb->SetValue(
                PBParameterIds::StageFile, GetCOREInterface()->GetTime(), newRootLayer.c_str());
            pb->SetValue(
                PBParameterIds::StageMask, GetCOREInterface()->GetTime(), newStageMask.c_str());
            object->LoadUSDStage(newStageRef);
        }
        int Size() override
        {
            return sizeof(oldStageMask) + sizeof(newStageMask) + sizeof(oldRootLayer)
                + sizeof(newRootLayer) + sizeof(pb) + sizeof(object);
        }
        TSTR Description() override { return TSTR(_T("USD Stage Object root layer restore.")); }

    private:
        std::wstring        oldRootLayer;
        std::wstring        oldStageMask;
        std::wstring        newRootLayer;
        std::wstring        newStageMask;
        IParamBlock2*       pb;
        USDStageObject*     object;
        pxr::UsdStageRefPtr oldStageRef;
        pxr::UsdStageRefPtr newStageRef;
    };

    const MCHAR* stageFilepathValue = nullptr;
    Interval     valid = FOREVER;
    pb->GetValue(StageFile, GetCOREInterface()->GetTime(), stageFilepathValue, valid);
    int cmpResRootLayer = wcscmp(stageFilepathValue, rootLayer);

    const MCHAR* stageMaskValue = nullptr;
    pb->GetValue(StageMask, GetCOREInterface()->GetTime(), stageMaskValue, valid);
    int cmpResStageMask = wcscmp(stageMaskValue, stageMask);

    if (cmpResRootLayer != 0 || cmpResStageMask != 0) {
        auto configureAndLoadStage = [this, rootLayer, stageMask, payloadsLoaded]() {
            pb->SetValue(StageFile, GetCOREInterface()->GetTime(), rootLayer);
            pb->SetValue(StageMask, GetCOREInterface()->GetTime(), stageMask);
            LoadUSDStage(nullptr, payloadsLoaded);
        };

        // Insert the StageResoreObj in the undo stack, to allow undoing the stage change.
        if (!theHold.Holding()) {
            theHold.Begin();
            theHold.Put(new StageRestoreObj(
                this, pb, stageFilepathValue, stageMaskValue, rootLayer, stageMask));
            configureAndLoadStage();
            theHold.Accept(L"Set Root Layer and Mask");
        } else {
            if (!theHold.IsSuspended()) {
                theHold.Put(new StageRestoreObj(
                    this, pb, stageFilepathValue, stageMaskValue, rootLayer, stageMask));
            }
            configureAndLoadStage();
        }
    }
}

void USDStageObject::OnStageChange(pxr::UsdNotice::ObjectsChanged const& notice)
{
    if (notice.GetStage() != GetUSDStage()) {
        return;
    }

    // Invalidate the source parameters so that fresh values for these params
    // will be fetched by the accessors from components that depend on them, such
    // as the UI
    InvalidateParams();

    ClearRenderCache();
    ClearBoundingBoxCache();

    // We might need to re-populate the hydra selection, for example if prims were
    // added, or removed, or some instance indices changed.
    DirtySelectionDisplay();

    // If we have resync'ed paths, there were structural changes to the stage, and we may have
    // deleted or added cameras.
    if (!notice.GetResyncedPaths().empty()) {
        BuildCameraNodes();
    }

    // Notify that the object may have changed, so that it is flagged for redraw.
    Interval valid = FOREVER;
    this->ForceNotify(valid);
}

void USDStageObject::GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box)
{
    GetLocalBoundBox(t, inode, vp, box);
    if (!box.IsEmpty()) {
        box = box * inode->GetNodeTM(t);
    }
}

void USDStageObject::GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vp, Box3& box)
{
    const auto stage = GetUSDStage();
    const bool showIcon = GetParamBlockBool(pb, ShowIcon);
    if (!vp || !vp->IsAlive() || !stage && !showIcon) {
        box.Init();
        return;
    }

    if (showIcon) {
        shapeIcon.GetLocalBoundBox(t, inode, vp, box);
        if (!stage) {
            return;
        }
    }

    box += GetStageBoundingBox(GetStageRootTransform(), t, inode);
}

Class_ID USDStageObject::ClassID() { return USDSTAGEOBJECT_CLASS_ID; }

void USDStageObject::GetClassName(MSTR& s, bool localized) const
{
    UNUSED_PARAM(localized);
    s = GetString(IDS_USDSTAGEOBJECT_CLASS_NAME);
}

inline int USDStageObject::IsRenderable() { return TRUE; }

BaseInterface* USDStageObject::GetInterface(Interface_ID id)
{
    if (id == IUSDStageProvider_ID) {
        return this;
    }
    return Object::GetInterface(id);
}

pxr::UsdStageWeakPtr USDStageObject::GetUSDStage() const
{
    if (stage) {
        return stage;
    }

    return pxr::TfNullPtr;
}

pxr::UsdStageWeakPtr
USDStageObject::LoadUSDStage(const pxr::UsdStageRefPtr& fromStage, bool loadPayloads)
{
    const auto camGenerationGuard
        = MaxUsd::MakeScopeGuard([]() {}, [this]() { BuildCameraNodes(); });

    if (stage) {
        FullStageReset();
    }

    Interval valid;

    if (fromStage) {
        stage = fromStage;
    } else {
        // Block stage cache population for now. If the same file is referenced several times, we
        // want different stages.
        pxr::UsdStageCacheContext stageCacheContext {
            pxr::UsdStageCacheContextBlockType::UsdBlockStageCachePopulation
        };
        const MCHAR* stageFilepathValue = nullptr;

        pb->GetValue(StageFile, GetCOREInterface()->GetTime(), stageFilepathValue, valid);
        if ((!stageFilepathValue) || (!stageFilepathValue[0])) {
            return nullptr;
        }

        const std::string filename = MaxUsd::MaxStringToUsdString(stageFilepathValue);
        if (filename.empty() || MaxUsd::HasUnicodeCharacter(filename)) {
            // NOTE: need better error reporting here
            return nullptr;
        }

        if (!pxr::UsdStage::IsSupportedFile(filename)) {
            return nullptr;
        }

        const MCHAR* stageMaskValue = nullptr;
        pb->GetValue(StageMask, GetCOREInterface()->GetTime(), stageMaskValue, valid);
        if ((!stageMaskValue) || (!stageMaskValue[0])) {
            return nullptr;
        }

        pxr::UsdStagePopulationMask stageMask;
        pxr::SdfPath                stageMaskSdfPath(MaxUsd::MaxStringToUsdString(stageMaskValue));
        stageMask.Add(stageMaskSdfPath);

        const auto rootLayer = pxr::SdfLayer::FindOrOpen(filename);

        pxr::UsdStage::InitialLoadSet initialLoadSet;
        // Depending on whether we have a session layer that was loaded from the 3dsMax file or not,
        // we need to call different signatures to open the stage. Indeed, if we pass a null session
        // layer, none will be created for us. The signature which does not pass a session layer,
        // will create a session layer for us under the hood - this is what we want.
        if (sessionLayerFromMaxScene) {
            // was the file saved with the obsolete PB value for 'LoadPayloads'
            // in case the 'LoadNone' was set it will use that value, otherwise,
            // whether the value is set or not, it uses the default 'LoadAll'
            initialLoadSet = GetParamBlockBool(pb, LoadPayloads) ? pxr::UsdStage::LoadAll
                                                                 : pxr::UsdStage::LoadNone;

            stage = pxr::UsdStage::OpenMasked(
                rootLayer, sessionLayerFromMaxScene, stageMask, initialLoadSet);
            // No need to hold onto the layer once it is passed to the stage.
            sessionLayerFromMaxScene = nullptr;
        } else {
            initialLoadSet = loadPayloads ? pxr::UsdStage::LoadAll : pxr::UsdStage::LoadNone;
            stage = pxr::UsdStage::OpenMasked(rootLayer, stageMask, initialLoadSet);
        }

        if (!stage) {
            return nullptr;
        }

        if (initialLoadSet == pxr::UsdStage::LoadNone) {
            SaveStageLoadRules();
        } else {
            // set the payload rules that apply
            stage->SetLoadRules(UsdUfe::createLoadRulesFromText(savedPayloadRules));
        }
    }

    // Default the target to the session layer.
    stage->SetEditTarget(stage->GetSessionLayer());

    // Insert the stage into the cache, and expose the CacheId so that it is accessible from
    // Maxscript.
    stageCacheId = pxr::UsdUtilsStageCache::Get().Insert(stage);

    StageObjectMap::GetInstance()->Set(this);

    auto sourceAnimationLength = stage->GetEndTimeCode() - stage->GetStartTimeCode();
    auto animationLength = MaxUsd::GetMaxFrameFromUsdTimeCode(stage, sourceAnimationLength);

    // The following segment of code is for setting the "End Frame" ui field to the length of the
    // animation that is being referenced for convenience. The check here is being performed to
    // ensure that it is in fact a newly added reference and in order to not override previously set
    // values for these param block parameters after loading a .max file which contains a
    // UsdStageObject object with these values having been set manually.
    float customStartFrame;
    pb->GetValue(CustomAnimationStartFrame, GetCOREInterface()->GetTime(), customStartFrame, valid);
    float customEndFrame;
    pb->GetValue(CustomAnimationEndFrame, GetCOREInterface()->GetTime(), customEndFrame, valid);
    if (customStartFrame == 0.f && customEndFrame == 0.f) {
        pb->SetValue(
            CustomAnimationEndFrame, GetCOREInterface()->GetTime(), (float)animationLength);
    }
    Redraw();

    InvalidateParams();

    if (GetParamBlockBool(pb, IsOpenInExplorer)) {
        USDExplorer::Instance()->OpenStage(this);
    }

    GenerateDrawModes();

    return stage;
}

pxr::GfMatrix4d USDStageObject::GetStageRootTransform() const
{
    pxr::GfMatrix4d rootTransform;
    const auto      stage = GetUSDStage();
    if (!stage) {
        rootTransform.SetIdentity();
        return rootTransform;
    }

    const auto rescaleFactor = MaxUsd::GetUsdToMaxScaleFactor(stage);
    rootTransform.SetScale(rescaleFactor);

    if (MaxUsd::IsStageUsingYUpAxis(stage)) {
        MaxUsd::MathUtils::ModifyTransformYToZUp(rootTransform);
    }
    return rootTransform;
}

void USDStageObject::EnumAuxFiles(AssetEnumCallback& nameEnum, DWORD flags)
{
    using namespace MaxSDK::AssetManagement;

    if ((flags & FILE_ENUM_CHECK_AWORK1) && this->TestAFlag(A_WORK1))
        return;
    // This flag means the callback object passed through is a IEnumAuxAssetsCallback derived object
    if (flags & FILE_ENUM_ACCESSOR_INTERFACE) {
        USDAssetAccessor accessor(this);
        if (accessor.GetAsset().GetId() != kInvalidId) {
            IEnumAuxAssetsCallback& callback = static_cast<IEnumAuxAssetsCallback&>(nameEnum);
            callback.DeclareAsset(accessor);
        }
    } else {
        AssetUser assetFile(
            pb ? pb->GetAssetUser(PBParameterIds::StageFile)
               : MaxSDK::AssetManagement::AssetUser());
        IPathConfigMgr* pathConf = IPathConfigMgr::GetPathConfigMgr();
        if (assetFile.GetId() != kInvalidId && pathConf) {
            pathConf->RecordInputAsset(assetFile, nameEnum, flags);
        }
    }
    // Must be called in order to perform default behavior
    this->ReferenceMaker::EnumAuxFiles(nameEnum, flags);
}

IOResult USDStageObject::Save(ISave* iSave)
{
    ULONG nb = 0;

    // Save the version first - if the saved format changes, we need to know what we are reading..
    iSave->BeginChunk(SAVE_VERSION_CHUNK_ID);
    iSave->Write(&USD_OBJECT_DATA_SAVE_VERSION, sizeof(USD_OBJECT_DATA_SAVE_VERSION), &nb);
    iSave->EndChunk();

    const auto& mappings
        = hydraEngine->GetRenderDelegate()->GetPrimvarMappingOptions().GetPrimvarMappings();
    const size_t numMappings = mappings.size();

    // Save primvar->channel mappings.

    // Save all primvar names. Can only have one string per chunk, no way
    // to read different strings from one chunk, everything is read as one string.
    for (auto kvPair : mappings) {
        iSave->BeginChunk(PRIMVAR_MAPPING_NAME_CHUNK_ID);
        const auto primvarName = MaxUsd::UsdStringToMaxString(kvPair.first);
        iSave->WriteWString(primvarName.ToACP());
        iSave->EndChunk();
    }

    // Now save the target channels. Can use a single chunk for this.
    iSave->BeginChunk(PRIMVAR_MAPPING_CHANNELS_CHUNK_ID);
    iSave->Write(&numMappings, sizeof(numMappings), &nb);
    for (auto kvPair : mappings) {
        int channel = kvPair.second.Get<int>();
        iSave->Write(&channel, sizeof(int), &nb);
    }
    iSave->EndChunk();

    if (const auto stage = GetUSDStage()) {
        // Save the session layer, if it exists.
        if (const auto sessionLayer = stage->GetSessionLayer()) {
            iSave->BeginChunk(SESSION_LAYER_CHUNK_ID);
            std::string sessionLayerStr;
            const bool  sessionExpResult = sessionLayer->ExportToString(&sessionLayerStr);
            // If there is an error, log it, but do not fail the entire max scene save.
            if (!sessionExpResult) {
                const auto msg = _T("UsdStageObject save error. Unable to serialize the session ")
                                 _T("layer to a string.");
                DbgAssert(0 && msg);
                GetCOREInterface()->Log()->LogEntry(SYSLOG_ERROR, NO_DIALOG, nullptr, msg);
                sessionLayerStr.clear();
            }
            const auto storageStr = MaxUsd::UsdStringToMaxString(sessionLayerStr);
            iSave->WriteWString(storageStr.ToACP());
            iSave->EndChunk();
        }

        // save the payload rules applied on the stage
        iSave->BeginChunk(PAYLOAD_RULES_CHUNK_ID);
        iSave->WriteWString(MaxUsd::UsdStringToMaxString(savedPayloadRules).ToACP());
        iSave->EndChunk();
    }
    return IO_OK;
}

IOResult USDStageObject::Load(ILoad* iLoad)
{
    iLoad->RegisterPostLoadCallback(new USDItemPostLoadCB(this));

    IOResult res = IO_OK;
    ULONG    nb = 0;

    res = iLoad->OpenChunk();

    // Nothing to load. Could be a StageObject in an earlier version of the plugin.
    if (res == IO_END) {
        return IO_OK;
    }

    if (res != IO_OK) {
        DbgAssert(0 && _T("Problem in loading saved data UsdStageObject."));
        return res;
    }

    if (iLoad->CurChunkID() != SAVE_VERSION_CHUNK_ID) {
        DbgAssert(iLoad->CurChunkID() == SAVE_VERSION_CHUNK_ID); // Should always be first
        return IO_ERROR;
    }

    // Read save model version
    int loadedVersion = -1;
    res = iLoad->Read(&loadedVersion, sizeof(loadedVersion), &nb);
    iLoad->CloseChunk();
    if (IO_OK != res) {
        DbgAssert(0 && _T("Problem in loading version of the UsdStageObject"));
        return res;
    }

    // For now don't do anything. In the future there are actually multiple versions, we will
    // need to deal with them individually...
    if (loadedVersion != USD_OBJECT_DATA_SAVE_VERSION) {
        return IO_OK;
    }

    std::vector<std::string> primvarNames;
    std::vector<int>         primvarChannels;

    while (IO_OK == (res = iLoad->OpenChunk())) {
        switch (iLoad->CurChunkID()) {
        // We will probably get multiple of these, as we can only have one string
        // per chunk.
        case PRIMVAR_MAPPING_NAME_CHUNK_ID: {
            TCHAR*     primvarWstring = NULL;
            const auto strRes = iLoad->ReadWStringChunk(&primvarWstring);
            if (strRes != IO_OK) {
                DbgAssert(0 && _T("Error reading string in primvar mapping for UsdStageObject."));
                return strRes;
            }

            primvarNames.push_back(MaxUsd::MaxStringToUsdString(primvarWstring));
            break;
        }
        case PRIMVAR_MAPPING_CHANNELS_CHUNK_ID: {
            // All channels are in the same chunk.
            size_t count = 0;
            res = iLoad->Read(&count, sizeof(count), &nb);
            if (IO_OK != res) {
                DbgAssert(0 && _T("Error in reading count of primvar mappings."));
                return res;
            }
            if (count > 0) {
                primvarChannels.reserve(count);
                for (int i = 0; i < count; ++i) {
                    int        channel = 0;
                    const auto intRes = iLoad->Read(&channel, sizeof(channel), &nb);
                    if (intRes != IO_OK) {
                        DbgAssert(
                            0 && _T("Error reading int in primvar mapping for UsdStageObject."));
                        return intRes;
                    }
                    primvarChannels.push_back(channel);
                }
            }
            break;
        }
        case SESSION_LAYER_CHUNK_ID: {
            TCHAR*     sessionLayerRaw = NULL;
            const auto strRes = iLoad->ReadWStringChunk(&sessionLayerRaw);
            if (strRes != IO_OK) {
                DbgAssert(0 && _T("Error reading session layer from UsdStageObject."));
                return strRes;
            }
            // The passed string is just a tag. The extension is important as SdfLayer will
            // use it to determine the used file format for the layer.
            sessionLayerFromMaxScene
                = pxr::SdfLayer::CreateAnonymous("3dsmax_usd_session_layer.usd");
            const bool layerImportRes = sessionLayerFromMaxScene->ImportFromString(
                MaxUsd::MaxStringToUsdString(sessionLayerRaw));
            // If there is an error, log it, but do not fail the entire max scene load.
            if (!layerImportRes) {
                const auto msg = _T("UsdStageObject load error. Unable to load the session layer ")
                                 _T("from the max file.");
                DbgAssert(0 && msg);
                GetCOREInterface()->Log()->LogEntry(SYSLOG_ERROR, NO_DIALOG, nullptr, msg);
            }
            break;
        }
        case PAYLOAD_RULES_CHUNK_ID: {
            TCHAR*     payloadRulesRaw = NULL;
            const auto strRes = iLoad->ReadWStringChunk(&payloadRulesRaw);
            if (strRes != IO_OK) {
                DbgAssert(0 && _T("Error reading payload rules from UsdStageObject."));
                return strRes;
            }
            savedPayloadRules = MaxUsd::MaxStringToUsdString(payloadRulesRaw);
            break;
        }
        default: break;
        }
        iLoad->CloseChunk();
    }

    // We should always find the same number of names/channels.
    if (primvarNames.size() != primvarChannels.size()) {
        DbgAssert(primvarNames.size() == primvarChannels.size());
        return IO_ERROR;
    }

    // Setup the primvar mappings from the data that we read.
    auto& primvarMappingOptions = hydraEngine->GetRenderDelegate()->GetPrimvarMappingOptions();
    primvarMappingOptions.ClearMappedPrimvars();
    for (int i = 0; i < primvarNames.size(); ++i) {
        // Already know here that both vectors are of the same size.
        primvarMappingOptions.SetPrimvarChannelMapping(primvarNames[i], primvarChannels[i]);
    }
    return IO_OK;
}

bool USDStageObject::GetDisplayPurpose(pxr::TfToken purpose) const
{
    PBParameterIds id;
    if (purpose == pxr::TfToken("guide")) {
        id = DisplayGuide;
    } else if (purpose == pxr::TfToken("render")) {
        id = DisplayRender;
    } else if (purpose == pxr::TfToken("proxy")) {
        id = DisplayProxy;
    } else {
        return false;
    }

    BOOL     display;
    Interval valid;
    pb->GetValue(id, GetCOREInterface()->GetTime(), display, valid);
    return display;
}

void USDStageObject::CheckRenderCache(TimeValue time, const pxr::TfTokenVector& renderTags)
{
    if (renderCache.IsValid(time, renderTags)) {
        return;
    }
    // Cache is invalid.
    ClearRenderCache();
}

void USDStageObject::ClearAllCaches()
{
    ClearRenderCache();
    ClearBoundingBoxCache();
}

pxr::UsdTimeCode USDStageObject::ResolveRenderTimeCode(TimeValue time) const
{
    auto animMode = GetParamBlockInt(pb, AnimationMode);
    auto resolvedAnimationModeIndex
        = std::min(static_cast<int>(AnimationMode::CustomTimeCodePlayback), std::max(0, animMode));

    pxr::UsdTimeCode timeCodeSample;
    if (resolvedAnimationModeIndex == AnimationMode::OriginalRange) {
        timeCodeSample = MaxUsd::GetUsdTimeCodeFromMaxTime(stage, time);
    } else if (resolvedAnimationModeIndex == AnimationMode::CustomStartAndSpeed) {
        auto customAnimStartFrame = GetParamBlockFloat(pb, CustomAnimationStartFrame);
        auto customAnimSpeed = GetParamBlockFloat(pb, CustomAnimationSpeed);

        auto stageStartCode = stage->GetStartTimeCode();
        auto stageEndCode = stage->GetEndTimeCode();
        auto sourceAnimLength = stageEndCode - stageStartCode;
        auto maxAnimLength = MaxUsd::GetMaxFrameFromUsdTimeCode(stage, sourceAnimLength);

        if (customAnimSpeed != 0.f) {
            auto scaledAnimLength = maxAnimLength / customAnimSpeed;
            timeCodeSample
                = MaxUsd::GetOffsetTimeCode(stage, time, customAnimStartFrame, scaledAnimLength);
        }
    } else if (resolvedAnimationModeIndex == AnimationMode::CustomRange) {
        auto customAnimStartFrame = GetParamBlockFloat(pb, CustomAnimationStartFrame);
        auto customAnimEndFrame = GetParamBlockFloat(pb, CustomAnimationEndFrame);

        timeCodeSample = MaxUsd::GetOffsetTimeCode(
            stage, time, customAnimStartFrame, customAnimEndFrame - customAnimStartFrame);
    } else if (resolvedAnimationModeIndex == AnimationMode::CustomTimeCodePlayback) {
        auto customAnimPlaybackTimecode = GetParamBlockFloat(pb, CustomAnimationPlaybackTimecode);
        timeCodeSample = pxr::UsdTimeCode(customAnimPlaybackTimecode);
    }

    return MaxUsd::MathUtils::RoundToSignificantDigit(timeCodeSample.GetValue(), 4);
}

namespace {
class FindUsdCameraDependentsProc : public DependentEnumProc
{
public:
    INodeTab result;

    int proc(ReferenceMaker* rmaker) override
    {
        const auto node = dynamic_cast<INode*>(rmaker);
        if (!node) {
            return DEP_ENUM_CONTINUE;
        }

        const auto object = node->GetObjectRef();
        if (!object) {
            return DEP_ENUM_CONTINUE;
        }

        if (dynamic_cast<USDCameraObject*>(object->FindBaseObject())) {
            result.AppendNode(node);
            return DEP_ENUM_CONTINUE;
        }
        return DEP_ENUM_CONTINUE;
    }
};
} // namespace

void USDStageObject::BuildCameraNodes()
{
    const auto nodes = MaxUsd::GetReferencingNodes(this);
    for (int i = 0; i < nodes.Count(); ++i) {
        BuildCameraNodes(nodes[i]);
    }
}

void USDStageObject::BuildCameraNodes(INode* stageNode) const
{
    if (theHold.RestoreOrRedoing()) {
        return;
    }

    auto checkFlushUndo = []() {
        // It is not safe to create/delete nodes outside of a hold, as it may lead to situations
        // where the undo stack points to objects that do not exist anymore. For example:
        // 1- Create a camera in hold/undo
        // 2- Rename the camera in a hold/undo
        // 3- Delete the camera NOT IN A HOLD / UNDO
        // 4- Undo -> Attempts to undo the name change -> camera does not exist -> crash.
        // Here, the only safe thing we can do is flush the undo buffer.
        if (!theHold.Holding() || theHold.IsSuspended()) {
            GetCOREInterface()->FlushUndoBuffer();
        }
    };

    FindUsdCameraDependentsProc cameraFinder;
    ;
    stageNode->DoEnumDependents(&cameraFinder);

    if (!stage || !GetParamBlockBool(pb, GenerateCameras)) {
        if (cameraFinder.result.Count() > 0) {
            // Set overrideDrivenTM to true - we generally block those cameras being deleted. But
            // here we do want to delete.
            GetCOREInterface10()->DeleteNodes(cameraFinder.result, true, true, true);
            checkFlushUndo();
        }
        return;
    }

    bool needFlushUndoCheck = false;

    pxr::TfHashMap<pxr::SdfPath, std::pair<INode*, bool>, pxr::SdfPath::Hash> generatedCameras;

    for (const auto& cam : cameraFinder.result) {
        const auto camObject
            = dynamic_cast<USDCameraObject*>(cam->GetObjectRef()->FindBaseObject());
        const MCHAR* primPathStr = nullptr;
        Interval     valid = FOREVER;
        camObject->GetParamBlock(0)->GetValue(
            USDCameraParams_PrimPath, GetCOREInterface()->GetTime(), primPathStr, valid);
        const auto path = pxr::SdfPath { MaxUsd::MaxStringToUsdString(primPathStr) };
        generatedCameras.insert({ path, { cam, false } });
    }

    for (const auto& prim : stage->Traverse()) {
        if (prim.IsA<pxr::UsdGeomCamera>() && prim.IsActive()) {
            // Do we already have a max camera generated for this usd camera?
            auto it = generatedCameras.find(prim.GetPath());
            if (it != generatedCameras.end()) {
                it->second.second = true;
                continue;
            }

            const auto cameraObject
                = GetCOREInterface17()->CreateInstance(CAMERA_CLASS_ID, USDCAMERAOBJECT_CLASS_ID);

            USDCameraObject* camera = static_cast<USDCameraObject*>(cameraObject);

            // Setup the camera with the stage reference and camera path.
            const auto paramBlock = camera->GetParamBlock(0);

            const auto pathStr = MaxUsd::UsdStringToMaxString(prim.GetPath().GetString());
            paramBlock->SetValue(USDCameraParams_PrimPath, 0, pathStr);

            auto node = GetCOREInterface()->CreateObjectNode(camera);
            paramBlock->SetValue(USDCameraParams_USDStage, 0, stageNode);

            camera->Eval(GetCOREInterface()->GetTime());

            USDXformableController* transformController = new USDXformableController();

            const auto controllerPb = transformController->GetParamBlock(0);

            controllerPb->SetValue(USDControllerParams_USDStage, 0, stageNode);
            controllerPb->SetValue(USDControllerParams_Path, 0, pathStr);
            controllerPb->SetValue(USDControllerParams_PreventNodeDeletion, 0, TRUE);

            node->SetTMController(transformController);
            node->InvalidateTM();

            auto name = MaxUsd::UsdStringToMaxString(prim.GetName().GetString());
            node->SetName(name);

            // Do we need this with the controller?
            node->SetTransformLock(INODE_LOCKROT, INODE_LOCK_X, TRUE);
            node->SetTransformLock(INODE_LOCKROT, INODE_LOCK_Y, TRUE);
            node->SetTransformLock(INODE_LOCKROT, INODE_LOCK_Z, TRUE);
            node->SetTransformLock(INODE_LOCKPOS, INODE_LOCK_X, TRUE);
            node->SetTransformLock(INODE_LOCKPOS, INODE_LOCK_Y, TRUE);
            node->SetTransformLock(INODE_LOCKPOS, INODE_LOCK_Z, TRUE);
            node->SetTransformLock(INODE_LOCKSCL, INODE_LOCK_X, TRUE);
            node->SetTransformLock(INODE_LOCKSCL, INODE_LOCK_Y, TRUE);
            node->SetTransformLock(INODE_LOCKSCL, INODE_LOCK_Z, TRUE);

            stageNode->AttachChild(node);
            needFlushUndoCheck |= true;
        }
    }

    // Check if we have any previously generated camera that is no longer needed.
    for (const auto& entry : generatedCameras) {
        if (!entry.second.second) {
            // Set overrideDriven to true - we generally block those cameras being deleted.
            // But here we do want to delete.
            GetCOREInterface()->DeleteNode(entry.second.first, TRUE, TRUE);
            needFlushUndoCheck |= true;
        }
    }

    if (needFlushUndoCheck) {
        checkFlushUndo();
    }
}

void USDStageObject::DeleteCameraNodes(INode* stageNode) const
{
    if (theHold.RestoreOrRedoing()) {
        return;
    }

    // It is not safe to delete nodes outside of a hold, as it may lead to situations
    // where the undo stack points to objects that do not exist anymore. For example:
    // 1- Create a camera in hold/undo
    // 2- Rename the camera in a hold/undo
    // 3- Delete the camera NOT IN A HOLD / UNDO
    // 4- Undo -> Attempts to undo the name change -> camera does not exist -> crash.
    // Here, the only safe thing we can do is flush the undo buffer.
    if (!theHold.Holding() || theHold.IsSuspended()) {
        GetCOREInterface()->FlushUndoBuffer();
    }

    FindUsdCameraDependentsProc cameraFinder;
    stageNode->DoEnumDependents(&cameraFinder);
    // Set overrideDrivenTM to true - we generally block those cameras being deleted. But here we do
    // want to delete.
    GetCOREInterface10()->DeleteNodes(cameraFinder.result, true, true, true);
}

void USDStageObject::SetupRenderDelegateDisplaySettings(INode* node) const
{
    // Setup the display settings.
    auto& displaySettings = hydraEngine->GetRenderDelegate()->GetDisplaySettings();
    auto& tracker = hydraEngine->GetChangeTracker();
    // Clamp the display mode int to the enum bounds [0, USDPreviewSurface]
    auto resolvedDisplayModeIndex = std::min(
        static_cast<int>(HdMaxDisplaySettings::DisplayMode::USDPreviewSurface),
        std::max(0, GetParamBlockInt(pb, DisplayMode)));
    displaySettings.SetDisplayMode(
        static_cast<HdMaxDisplaySettings::DisplayMode>(resolvedDisplayModeIndex), tracker);
    displaySettings.SetWireColor(Color(node->GetWireColor()), tracker);
}

void USDStageObject::SetPrimvarChannelMappingDefaults()
{
    hydraEngine->GetRenderDelegate()->GetPrimvarMappingOptions().SetDefaultPrimvarChannelMappings();
    OnPrimvarMappingChanged();
}

void USDStageObject::SetPrimvarChannelMapping(const wchar_t* primvarName, Value* channel)
{
    MaxUsd::mxs::SetPrimvarChannelMapping(
        hydraEngine->GetRenderDelegate()->GetPrimvarMappingOptions(), primvarName, channel);
    OnPrimvarMappingChanged();
}

void USDStageObject::ClearMappedPrimvars()
{
    hydraEngine->GetRenderDelegate()->GetPrimvarMappingOptions().ClearMappedPrimvars();
    OnPrimvarMappingChanged();
}

void USDStageObject::OpenInUsdExplorer()
{
    USDExplorer::Instance()->OpenStage(this);
    pb->SetValue(IsOpenInExplorer, GetCOREInterface()->GetTime(), TRUE);
}

void USDStageObject::CloseInUsdExplorer()
{
    USDExplorer::Instance()->CloseStage(this);
    pb->SetValue(IsOpenInExplorer, GetCOREInterface()->GetTime(), FALSE);
}

const std::string& USDStageObject::GetGuid() const { return guid; }

void USDStageObject::SaveStageLoadRules()
{
    savedPayloadRules = UsdUfe::convertLoadRulesToText(GetUSDStage()->GetLoadRules());
}

HdMaxEngine* USDStageObject::GetHydraEngine() const { return hydraEngine.get(); }

Value* USDStageObject::GetPrimvarChannel(const wchar_t* primvarName)
{
    return MaxUsd::mxs::GetPrimvarChannel(
        hydraEngine->GetRenderDelegate()->GetPrimvarMappingOptions(), primvarName);
}

Tab<const wchar_t*> USDStageObject::GetMappedPrimvars() const
{
    return MaxUsd::mxs::GetMappedPrimvars(
        hydraEngine->GetRenderDelegate()->GetPrimvarMappingOptions());
}

void USDStageObject::OnPrimvarMappingChanged()
{
    hydraEngine->GetChangeTracker().MarkAllRprimsDirty(
        pxr::HdChangeTracker::DirtyPrimvar | pxr::HdChangeTracker::DirtyMaterialId);
    hydraEngine->GetRenderDelegate()
        ->GetMaterialCollection()
        ->Clear(); // Force a rebuild of materials.
    ClearRenderCache();
}

bool USDStageObject::IsMappedPrimvar(const wchar_t* primvarName)
{
    return MaxUsd::mxs::IsMappedPrimvar(
        hydraEngine->GetRenderDelegate()->GetPrimvarMappingOptions(), primvarName);
}

bool USDStageObject::UpdatePerViewItems(
    const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext,
    MaxSDK::Graphics::UpdateNodeContext&          nodeContext,
    MaxSDK::Graphics::UpdateViewContext&          viewContext,
    MaxSDK::Graphics::IRenderItemContainer&       targetRenderItemContainer)
{
    {
        // Workaround for the instance render item perf drop in 2023.1/2023.2.
        // Previously held render items should have been cleared from the render node,
        // and they are not - so do it ourselves.
        // However, we must avoid removing render items that may have been built for
        // the "display as box" and "selection brackets". This is tricky, as there is no
        // direct exposure of these render items in the SDK.
        MaxSDK::Graphics::RenderNodeHandle& renderNodeHandle = nodeContext.GetRenderNode();
        bool                                displayAsBox = false;
        for (int i = static_cast<int>(renderNodeHandle.GetNumberOfRenderItems()) - 1; i >= 0; i--) {
            auto       ri = renderNodeHandle.GetRenderItem(i);
            const auto consolidationKey = ri.GetConsolidationData().Key;

            // The "display as box" mode uses a BoxStyleItem, it has consolidation data.
            if (consolidationKey) {
                // Do not SplineItems, which is what we generate for the Icon.
                const auto splineItemKey
                    = dynamic_cast<MaxSDK::Graphics::Utilities::SplineItemKey*>(consolidationKey);
                if (!splineItemKey) {
                    // Not a spline, assume a BoxStyleItem
                    displayAsBox = true;
                    continue;
                }
            }

            // The selection brackets use a legacy render item type, not exposed in the SDK.
            // This render item is always present, whether or not brackets end up actually
            // displayed. However, we are able to detect these items via their visibility group, in
            // most cases the visibility group is set to shader, wireframe, or gizmo in 3dsMax. The
            // items we generate are set to shaded/wireframe.
            if (ri.GetVisibilityGroup() == MaxSDK::Graphics::RenderItemVisible_Unknown) {
                continue;
            }
            renderNodeHandle.RemoveRenderItem(i);
        }
        // At this point we should only have one render item, the legacy render item - or two, if we
        // are in box display.
        DbgAssert(renderNodeHandle.GetNumberOfRenderItems() == (displayAsBox ? 2 : 1));
    }

    const bool showIcon = GetParamBlockBool(pb, ShowIcon);
    if (showIcon) {
        shapeIcon.UpdatePerNodeItems(updateDisplayContext, nodeContext, targetRenderItemContainer);
    }

    const auto stage = GetUSDStage();
    if (!stage) {
        return showIcon;
    }

    // If the animation is playing, render at the beginning of the current frame,
    // to ease caching
    TimeValue time = updateDisplayContext.GetDisplayTime();
    if (GetCOREInterface()->IsAnimPlaying()) {
        time = time - time % GetTicksPerFrame();
    }

    auto node = nodeContext.GetRenderNode().GetMaxNode();
    SetupRenderDelegateDisplaySettings(node);

    auto view = viewContext.GetView();

    // Figure out which "representations" we will need. Shaded, wireframe, or both.
    pxr::TfTokenVector reprs;
    // We need the shaded render items if we are in a shaded view, or if we are in a wireframe
    // view and that the animation is not playing. Indeed, shaded items are required to get the
    // selection highlighting in the Max viewport.
    if (!view->IsWire() || !GetCOREInterface()->IsAnimPlaying()) {
        reprs.push_back(pxr::HdReprTokens->smoothHull);
    }
    const auto vpSettings = static_cast<MaxSDK::Graphics::IViewportViewSetting*>(
        view->GetInterface(IVIEWPORT_SETTINGS_INTERFACE_ID));
    if (view->IsWire() || vpSettings->GetShowEdgedFaces()) {
        reprs.push_back(pxr::HdReprTokens->wire);
    }

    if (isSelectionDisplayDirty) {
        UpdatePrimSelectionDisplay();
        isSelectionDisplayDirty = false;
    }

    auto axisAndUnitTransform = GetStageRootTransform();

    HdMaxConsolidator::Config consolidationConfig;
    consolidationConfig.strategy
        = static_cast<HdMaxConsolidator::Strategy>(GetParamBlockInt(pb, MeshMergeMode));
    consolidationConfig.visualize = GetParamBlockBool(pb, MeshMergeDiagnosticView);
    consolidationConfig.maxCellSize = GetParamBlockInt(pb, MaxMergedMeshTriangles);
    consolidationConfig.maxInstanceCount = GetParamBlockInt(pb, MeshMergeMaxInstances);
    consolidationConfig.maxTriangles = GetParamBlockInt(pb, MeshMergeMaxTriangles);
    consolidationConfig.displaySettings = hydraEngine->GetRenderDelegate()->GetDisplaySettings();

    const auto timeCodeSample = ResolveRenderTimeCode(time);

    // HACK	: If a render purpose is already set, and a new one is enabled, for some reason
    // hydra does not flag dirty the meshes that now need a Sync() to be displayed. It works as
    // expected when going from 0 to N purposes enabled, but not when going from 1 to N purposes.
    // Work around this with an additional render, where no render purposes are enabled at all. It
    // is pretty cheap to do so as nothing meaningful should get flagged dirty between these to
    // renders, so we don't do that much more work than with a single call.
    if (displayPurposeUpdated) {
        hydraEngine->Render(
            stage->GetPseudoRoot(),
            axisAndUnitTransform,
            targetRenderItemContainer,
            timeCodeSample,
            updateDisplayContext,
            nodeContext,
            reprs,
            { pxr::HdTokens->geometry },
            usdMaterials.GetAs<MultiMtl>(),
            consolidationConfig,
            view,
            buildOfflineRenderMaterial,
            progressReporter);
        displayPurposeUpdated = false;
    }

    hydraEngine->Render(
        stage->GetPseudoRoot(),
        axisAndUnitTransform,
        targetRenderItemContainer,
        timeCodeSample,
        updateDisplayContext,
        nodeContext,
        reprs,
        GetRenderTags(),
        usdMaterials.GetAs<MultiMtl>(),
        consolidationConfig,
        view,
        buildOfflineRenderMaterial,
        progressReporter);

    buildOfflineRenderMaterial = false;

    // Setup the object's box for node level culling.
    auto renderNode = nodeContext.GetRenderNode();
    Box3 localBoundingBox;
    GetLocalBoundBox(time, node, viewContext.GetView(), localBoundingBox);
    renderNode.SetObjectBox(localBoundingBox);
    return true;
}

int USDStageObject::GetStageCacheId()
{
    // Force the stage to load from the currently set layer, if not already.
    GetUSDStage();
    return this->stageCacheId.ToLongInt();
}

unsigned long USDStageObject::GetObjectDisplayRequirement() const
{
    return MaxSDK::Graphics::ObjectDisplayRequireUpdatePerViewItems;
}

Interval USDStageObject::ObjectValidity(TimeValue time)
{
    // Object invalidates when time changes. The USD stage can be animated.
    return Interval(time, time);
}

int USDStageObject::HitTest(
    TimeValue t,
    INode*    iNode,
    int       type,
    int       crossing,
    int       flags,
    IPoint2*  p,
    ViewExp*  vpt)
{
    auto gw = vpt->getGW();
    bool showIcon = GetParamBlockBool(pb, ShowIcon);

    const auto& stage = GetUSDStage();
    if (!stage && !showIcon) {
        return false;
    }

    const bool selectedOnly = flags & HIT_SELONLY;
    const bool unselectedOnly = flags & HIT_UNSELONLY;
    const bool selectAny = !selectedOnly && !unselectedOnly;

    if (stage) {
        HitRegion hitRegion;
        MakeHitRegion(hitRegion, type, crossing, 1 /*epsilon*/, p);

        bool isPointHitTest = hitRegion.type == POINT_RGN;

        // We maintain a cache for point hit testing. If we are point hit testing, first look
        // at the cache...
        // Consider that if the picking point has not changed since last picking computation, we can
        // reuse previously computed target. There are many cases where this could break (scene
        // update, animation, procedural camera move...) Ideally, this should be precomputed each
        // time a new pick action is initiated (sadly, no notification is available for that).
        bool                                                   useCachedPointHit = isPointHitTest;
        std::unordered_map<INode*, HitTestCacheData>::iterator cachedHit;
        if (useCachedPointHit) {
            cachedHit = hitTestingCache.find(iNode);
            useCachedPointHit
                = cachedHit != hitTestingCache.end() && cachedHit->second.cursorPos == *p;
        }

        std::vector<USDPickingRenderer::HitInfo> hitInfos;

        if (!useCachedPointHit) {
            hitInfos = PickStage(
                vpt,
                iNode,
                hitRegion,
                vpt->IsWire() ? pxr::UsdImagingGLDrawMode::DRAW_WIREFRAME
                              : pxr::UsdImagingGLDrawMode::DRAW_GEOM_ONLY,
                pxr::HdxPickTokens->pickPrimsAndInstances,
                t,
                {});

            // Cache point hitTesting.
            if (isPointHitTest) {
                HitTestCacheData hitData;
                hitData.cursorPos = *p;
                if (hitInfos.empty()) {
                    hitData.hit = {};
                } else {
                    hitData.hit = hitInfos[0];
                }
                hitTestingCache[iNode] = hitData;
            }
        } else {
            if (!cachedHit->second.hit.primPath.IsEmpty()) {
                hitInfos = { cachedHit->second.hit };
            }
        }

        if (!hitInfos.empty()) {
            // Use the first hit prim for the hit distance.
            // The hit point is in world space, so make sure the GW transform is the identity.
            // Reapply the transform after we are done.
            auto currentGwTransform = gw->getTransform();
            gw->setTransform(Matrix3::Identity);
            // Calculate native devices coordinates. The z component of the output point
            // is the depth we are interested in for hit testing.
            IPoint3 out;
            gw->hTransPoint(&hitInfos[0].hitPoint, &out);
            gw->setTransform(currentGwTransform);
            gw->setHitDistance(static_cast<DWORD>(out.z));

            // Log the hit, necessary for sub object selection.
            std::vector<UsdHitData::Hit> usdHit;
            for (int i = 0; i < hitInfos.size(); ++i) {
                // If an instancer is defined, we treat things a bit differently whether the
                // instancer is generated on the fly from scene graph instancing or if it is a point
                // instancer. Point instances do not have actual paths, for not we just show all
                // instances selected, so we use the instancer path itself.
                auto instancerPrim = stage->GetPrimAtPath(hitInfos[i].instancerPath);
                if (instancerPrim.IsValid() && instancerPrim.IsA<pxr::UsdGeomPointInstancer>()) {
                    usdHit.push_back({ hitInfos[i].instancerPath, hitInfos[i].instanceIndex });
                } else {
                    usdHit.push_back({ hitInfos[i].primPath, -1 });
                }
            }

            // Need to consider hit flags VS selection. This is important for sub-object
            // select/transform to behave correctly.
            if (!selectAny) {
                const auto& globalSelection = Ufe::GlobalSelection::get();
                bool        hasHit = false;
                for (const auto& path : usdHit) {
                    const auto& ufePath = MaxUsd::ufe::getUsdPrimUfePath(this, path.primPath);
                    const bool  isSelected = globalSelection->contains(ufePath);
                    if ((selectedOnly && isSelected) || (unselectedOnly && !isSelected)) {
                        hasHit = true;
                        break;
                    }
                }
                if (!hasHit) {
                    return false;
                }
            }

            // In sub-object mode, 3dsmax will perform hit testing a few times. In one of the
            // passes, it's trying to figure out if it should switch the axis we are transforming
            // against (like, move in X,Y or Z) when dragging. Don't log the hit.
            if (!(flags & HIT_SWITCH_GIZMO)) {
                vpt->LogHit(iNode, nullptr, static_cast<DWORD>(out.z), 0, new UsdHitData(usdHit));
            }
            return true;
        }
    }
    if (showIcon) {
        // This Hit test also set the GraphicsWindow Hit Distance.
        if (shapeIcon.HitTest(t, iNode, type, crossing, flags, p, vpt)) {
            return true;
        }
    }
    return false;
}

int USDStageObject::HitTest(
    TimeValue   t,
    INode*      iNode,
    int         type,
    int         crossing,
    int         flags,
    IPoint2*    p,
    ViewExp*    vpt,
    ModContext* mc)
{
    return HitTest(t, iNode, type, crossing, flags, p, vpt);
}

std::vector<USDPickingRenderer::HitInfo> USDStageObject::PickStage(
    ViewExp*                  viewport,
    INode*                    node,
    const HitRegion&          hitRegion,
    pxr::UsdImagingGLDrawMode drawMode,
    const pxr::TfToken&       pickTarget,
    TimeValue                 time,
    const pxr::SdfPathVector& excludedPaths)
{
    if (!stage) {
        return {};
    }

    if (!pickingRenderer) {
        pickingRenderer = std::make_unique<USDPickingRenderer>(stage);
    }

    Matrix3                    viewMatrixInv;
    MaxSDK::Graphics::Matrix44 viewProjectionMatrix;

    auto  gw = viewport->getGW();
    int   persp;
    float hither, yon;
    gw->getCameraMatrix(viewProjectionMatrix.m, &viewMatrixInv, &persp, &hither, &yon);

    MaxSDK::Graphics::Matrix44 viewMatrix;
    MaxSDK::Graphics::MaxWorldMatrixToMatrix44(viewMatrix, Inverse(viewMatrixInv));

    MaxSDK::Graphics::Matrix44 projectionMatrix;
    {
        const pxr::GfMatrix4d vmi = MaxUsd::ToUsd(viewMatrixInv);
        const pxr::GfMatrix4d proj = vmi * MaxUsd::ToUsd(viewProjectionMatrix);
        projectionMatrix = MaxUsd::ToMax(proj);
    }

    MaxSDK::Graphics::ViewParameter viewParameter;
    viewParameter.SetViewExp(viewport);
    const MaxSDK::Graphics::RectangleSize size
        = { std::size_t(gw->getWinSizeX()), std::size_t(gw->getWinSizeY()) };
    viewParameter.SetSize(size);
    MaxSDK::Graphics::CameraPtr camera = MaxSDK::Graphics::ICamera::Create();
    camera->SetProjectionMatrix(projectionMatrix);
    camera->SetViewMatrix(viewMatrix);
    camera->SetTargetDistance(viewport->GetFocalDist());

    const auto stageTransform = GetStageRootTransform() * MaxUsd::ToUsd(node->GetObjectTM(time));

    auto hits = pickingRenderer->Pick(
        stageTransform,
        camera,
        size,
        hitRegion,
        drawMode,
        GetDisplayPurpose(pxr::TfToken("proxy")),
        GetDisplayPurpose(pxr::TfToken("guide")),
        GetDisplayPurpose(pxr::TfToken("render")),
        pickTarget,
        ResolveRenderTimeCode(time),
        excludedPaths);

    return hits;
}

CreateMouseCallBack* USDStageObject::GetCreateMouseCallBack()
{
    static CreateAtPosition createMouseCallback {};
    return &createMouseCallback;
}

void USDStageObject::InitNodeName(MSTR& name) { name = _M("UsdStage"); }

const wchar_t* USDStageObject::GetObjectName(bool localized) const { return _M("UsdStage"); }

ObjectState USDStageObject::Eval(TimeValue t) { return ObjectState(this); }

RefTargetHandle USDStageObject::Clone(RemapDir& remap)
{
    USDStageObject* newStage = new USDStageObject();
    newStage->ReplaceReference(0, remap.CloneRef(pb));
    BaseClone(this, newStage, remap);
    newStage->savedPayloadRules = savedPayloadRules;
    // Manually trigger stage loading in the cloned object, as we don't setup
    // the root layer in the usual way.
    newStage->LoadUSDStage();
    return (newStage);
}

void USDStageObject::GetDeformBBox(TimeValue t, Box3& box, Matrix3* tm, BOOL useSel)
{
    box = GetStageBoundingBox(GetStageRootTransform(), t, nullptr, useSel);
    if (tm && !box.IsEmpty()) {
        box = box * *tm;
    }
}

BOOL USDStageObject::PolygonCount(TimeValue t, int& numFaces, int& numVerts)
{
    if (t != GetCOREInterface()->GetTime()) {
        return FALSE;
    }
    numFaces = int(this->numFaces);
    numVerts = int(this->numVerts);
    return TRUE;
}

Mesh* USDStageObject::GetRenderMesh(TimeValue t, INode* inode, View& view, BOOL& needDelete)
{
    // Keep control of the lifetime of the meshes we produce.
    needDelete = false;

    // Some renderers do not like receiving null meshes from GetRenderMesh() in some cases. Playing
    // nice...
    static Mesh emptyMesh;

    const auto stage = GetUSDStage();
    if (!stage || stage.IsInvalid()) {
        return &emptyMesh;
    }

    const auto renderTags = GetRenderTags();

    // Warning : some renderers hold on to the mesh pointer that we return (scanline), so we
    // must make sure that the mesh survives the whole render call. If a day comes where the mesh
    // returned must be different per-node, we will need to make sure that any previously generated
    // mesh for another instance is kept alive somehow.
    CheckRenderCache(t, renderTags);

    if (renderCache.fullMesh) {
        return renderCache.fullMesh.get();
    }

    // Setup the display settings. For offline rendering via the generic apis, use
    // UsdPreviewSurface.
    auto& displaySettings = hydraEngine->GetRenderDelegate()->GetDisplaySettings();
    auto& changeTracker = hydraEngine->GetChangeTracker();
    displaySettings.SetDisplayMode(HdMaxDisplaySettings::USDPreviewSurface, changeTracker);

    const auto timeCodeSample = ResolveRenderTimeCode(t);

    if (renderCache.meshes.empty()) {
        hydraEngine->RenderToMeshes(
            inode,
            stage->GetPseudoRoot(),
            GetStageRootTransform(),
            renderCache.meshes,
            renderCache.transforms,
            timeCodeSample,
            renderTags);
    }

    if (renderCache.meshes.empty()) {
        return &emptyMesh;
    }

    renderCache.fullMesh = std::make_unique<Mesh>();
    MaxUsd::MeshUtils::AttachAll(renderCache.meshes, renderCache.transforms, *renderCache.fullMesh);

    renderCache.SetValidity(t, renderTags, inode->GetMtl());
    return renderCache.fullMesh.get();
}

pxr::TfTokenVector USDStageObject::GetRenderTags() const
{
    pxr::TfTokenVector renderTags { pxr::HdTokens->geometry };
    if (GetDisplayPurpose(pxr::TfToken("proxy"))) {
        renderTags.push_back(pxr::HdRenderTagTokens->proxy);
    }
    if (GetDisplayPurpose(pxr::TfToken("guide"))) {
        renderTags.push_back(pxr::HdRenderTagTokens->guide);
    }
    if (GetDisplayPurpose(pxr::TfToken("render"))) {
        renderTags.push_back(pxr::HdRenderTagTokens->render);
    }
    return renderTags;
}

void USDStageObject::UpdateViewportStageIcon()
{
    UsdStageObjectIcon::GetIcon(shapeIcon.shape);
    const float iconScale = GetParamBlockFloat(pb, IconScale);
    if (!MaxUsd::MathUtils::IsAlmostZero(abs(iconScale - 1.0f))) {
        Matrix3 scaleTM;
        scaleTM.Scale(Point3(iconScale, iconScale, 1.0f));
        shapeIcon.shape.Transform(scaleTM);
    }
}

void USDStageObject::RegisterProgressReporter(const MaxUsd::ProgressReporter& reporter)
{
    progressReporter = reporter;
}

void USDStageObject::UnregisterProgressReporter() { progressReporter = {}; }

Mtl* USDStageObject::GetUsdPreviewSurfaceMaterials(bool sync)
{
    // Done on the next render loop.
    if (sync) {
        buildOfflineRenderMaterial = true;
        // Complete redraw, to make sure the material is generated immediately as we want to return
        // it right away.
        Redraw(true);
    }
    return usdMaterials.GetAs<Mtl>();
}

int USDStageObject::NumberOfRenderMeshes()
{
    const auto stage = GetUSDStage();
    if (!stage || stage.IsInvalid()) {
        return 0;
    }

    return int(hydraEngine->GetNumRenderPrim(GetRenderTags()));
}

Mesh* USDStageObject::GetMultipleRenderMesh(
    TimeValue t,
    INode*    inode,
    View&     view,
    BOOL&     needDelete,
    int       meshNumber)
{
    // Keep control of the lifetime of the meshes we produce.
    needDelete = false;

    const auto stage = GetUSDStage();
    if (!stage || stage.IsInvalid()) {
        nullptr;
    }

    const auto renderTags = GetRenderTags();
    CheckRenderCache(t, renderTags);
    if (!renderCache.meshes.empty()) {
        return renderCache.meshes[meshNumber].get();
    }

    // Setup the display settings. For rendering, go for the highest quality.
    auto& displaySettings = hydraEngine->GetRenderDelegate()->GetDisplaySettings();
    auto& changeTracker = hydraEngine->GetChangeTracker();
    displaySettings.SetDisplayMode(HdMaxDisplaySettings::USDPreviewSurface, changeTracker);

    const auto timeCodeSample = ResolveRenderTimeCode(t);

    hydraEngine->RenderToMeshes(
        inode,
        stage->GetPseudoRoot(),
        GetStageRootTransform(),
        renderCache.meshes,
        renderCache.transforms,
        timeCodeSample,
        renderTags);
    renderCache.SetValidity(t, renderTags, inode->GetMtl());
    return renderCache.meshes[meshNumber].get();
}

void USDStageObject::GetMultipleRenderMeshTM(
    TimeValue t,
    INode*    inode,
    View&     view,
    int       meshNumber,
    Matrix3&  meshTM,
    Interval& meshTMValid)
{
    const auto stage = GetUSDStage();
    if (!stage || stage.IsInvalid()) {
        return;
    }
    const auto renderTags = GetRenderTags();
    CheckRenderCache(t, renderTags);

    if (!renderCache.transforms.empty()) {
        meshTM = Inverse(inode->GetObjectTM(t)) * renderCache.transforms[meshNumber]
            * inode->GetObjectTM(t);
        return;
    }
    const auto timeCodeSample = ResolveRenderTimeCode(t);
    hydraEngine->RenderToMeshes(
        inode,
        stage->GetPseudoRoot(),
        GetStageRootTransform(),
        renderCache.meshes,
        renderCache.transforms,
        timeCodeSample,
        renderTags);
    renderCache.SetValidity(t, renderTags, inode->GetMtl());
    meshTM = Inverse(inode->GetObjectTM(t)) * renderCache.transforms[meshNumber]
        * inode->GetObjectTM(t);
}

void USDStageObject::Redraw(bool completeRedraw)
{
    // Notify that the object has changed and force a redraw.
    Interval valid = FOREVER;
    this->ForceNotify(valid);
    if (completeRedraw) {
        GetCOREInterface()->ForceCompleteRedraw();
        return;
    }
    GetCOREInterface()->RedrawViews(GetCOREInterface()->GetTime());
}

void USDStageObject::InvalidateParams()
{
    const auto usdStageGeneralParamsMap = pb->GetMap(ParamMapID::UsdStageGeneral);
    if (usdStageGeneralParamsMap) {
        usdStageGeneralParamsMap->Invalidate(SourceMetersPerUnit);
        usdStageGeneralParamsMap->Invalidate(SourceUpAxis);
    }
    const auto usdStageAnimationParamsMap = pb->GetMap(ParamMapID::UsdStageAnimation);
    if (usdStageAnimationParamsMap) {
        usdStageAnimationParamsMap->Invalidate(MaxAnimationStartFrame);
        usdStageAnimationParamsMap->Invalidate(MaxAnimationEndFrame);
        usdStageAnimationParamsMap->Invalidate(SourceAnimationStartTimeCode);
        usdStageAnimationParamsMap->Invalidate(SourceAnimationEndTimeCode);
        usdStageAnimationParamsMap->Invalidate(SourceAnimationTPS);
    }
}

bool USDStageObject::PrepareDisplay(
    const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext)
{
    return shapeIcon.PrepareDisplay(prepareDisplayContext);
}

void USDStageObject::WireColorChanged(Color newColor)
{
    // Only need to redraw if we are displaying using the wire color.
    if (GetParamBlockInt(pb, DisplayMode) == int(HdMaxDisplaySettings::DisplayMode::WireColor)) {
        Redraw();
    }
}

void USDStageObject::Reload()
{
    const auto stage = GetUSDStage();
    if (!stage) {
        return;
    }
    stage->Reload();
    Redraw();
}

void USDStageObject::ClearSessionLayer()
{
    const auto stage = GetUSDStage();
    if (!stage) {
        return;
    }
    stage->GetSessionLayer()->Clear();
    // We still want to respect the current configuration of draw mode generation.
    GenerateDrawModes();
    Redraw();
}

USDStageObject::NodeEventCallback::NodeEventCallback(USDStageObject* stageObject)
    : object(stageObject)
{
}

void USDStageObject::NodeEventCallback::WireColorChanged(NodeKeyTab& nodes)
{
    for (int i = 0; i < nodes.Count(); ++i) {
        INode* node = NodeEventNamespace::GetNodeByKey(nodes[i]);
        if (node->GetObjectRef() == object) {
            object->WireColorChanged(Color(node->GetWireColor()));
            return;
        }
    }
}

Box3 USDStageObject::GetStageBoundingBox(
    pxr::GfMatrix4d rootTransform,
    TimeValue       time,
    INode*          node,
    bool            useSel)
{
    Box3 boundingBox;
    if (!stage) {
        return boundingBox;
    }

    // If the animation is playing, compute the bounding box at the beginning of
    // the current frame, to ease caching.
    auto evaluationTime = time;
    if (GetCOREInterface()->IsAnimPlaying()) {
        evaluationTime = evaluationTime - evaluationTime % GetTicksPerFrame();
    }

    // If we are not in selection mode, check if this frame's bounding box is already in the cache.
    // (i.e boundingBox needs to be recomputed based on selection at any given evaluationTime)
    const auto it = boundingBoxCache.find(evaluationTime);
    if (!useSel && it != boundingBoxCache.end()) {
        return it->second;
    }

    // Compute the bounding box.
    const auto timeCode = ResolveRenderTimeCode(evaluationTime);

    const auto stage = GetUSDStage();
    if (!stage || stage.IsInvalid()) {
        return boundingBox;
    }

    if (node) {
        SetupRenderDelegateDisplaySettings(node);
        hydraEngine->UpdateRootPrim(stage->GetPseudoRoot(), node->GetMtl());
    } else {
        hydraEngine->UpdateRootPrim(stage->GetPseudoRoot());
    }
    hydraEngine->HydraRender(rootTransform, timeCode, GetRenderTags());

    auto includedPurposes = GetRenderTags();
    includedPurposes.insert(includedPurposes.end(), pxr::UsdGeomTokens->default_);

    std::vector<HdMaxRenderData*> visibleData;
    hydraEngine->GetRenderDelegate()->GetVisibleRenderData(includedPurposes, visibleData);

    pxr::GfBBox3d totalBoundingBox;

    numVerts = 0;
    numFaces = 0;

    for (const auto data : visibleData) {
        if (useSel) {
            auto selState
                = GetHydraEngine()->GetRenderDelegate()->GetSelectionStatus(data->rPrimPath);
            if (!selState || (!selState->fullySelected && selState->instanceIndices.empty())) {
                continue;
            }
        }

        // It is a good time to compute the stats. Max requests the bounding box before anything
        // else, and this is the earlier we actually can have this info. We also don't need to
        // recompute the stats each frame, only when things change, similar to the bounding box
        // (which only has a few more cases in which we need to recompute).
        const auto numInstances = std::max(1, int(data->instancer->GetNumInstances()));
        numVerts += data->sourceNumPoints * numInstances;
        numFaces += data->sourceNumFaces * numInstances;

        pxr::GfBBox3d bboxToUse;
        if (data->IsInstanced() && useSel) {
            bboxToUse = data->instancer->ComputeSelectionBoundingBox(data->extent);
        } else {
            bboxToUse = data->boundingBox;
        }

        // Ignore empty bounding boxes, or obscenely large ones.
        const auto& range = bboxToUse.GetRange();
        if (range.IsEmpty() || range.GetSize().GetLength() > FLT_MAX) {
            continue;
        }
        totalBoundingBox = pxr::GfBBox3d::Combine(totalBoundingBox, bboxToUse);
    }

    auto extent = totalBoundingBox.GetRange();
    if (extent.IsEmpty() && !useSel) {
        if (visibleData.empty()) {
            return boundingBox;
        }

        // We prefer to compute the bounding box from the data that is visible. However, it is
        // possible for geometry to not have the extent attribute setup at all. In this case, the
        // result of our computation could be empty. If this happens, fallback to a full compute of
        // the world bounds. This may not match exactly the visible data (some things we may not be
        // supported our delegate), but it is better than nothing.
        pxr::UsdGeomBBoxCache tmpCache { timeCode, { includedPurposes }, true };
        totalBoundingBox = tmpCache.ComputeWorldBound(stage->GetPseudoRoot());
        extent = totalBoundingBox.GetRange();

        // If the extent is still empty, give up
        if (extent.IsEmpty()) {
            return boundingBox;
        }

        // Transform to 3dsmax's axis/unit,
        const auto   rootXform = GetStageRootTransform();
        const auto&  min = extent.GetMin();
        const auto&  max = extent.GetMax();
        auto         tMin = rootXform.Transform(min);
        auto         tMax = rootXform.Transform(max);
        pxr::GfVec3d bbMin = { std::min(tMin[0], tMax[0]),
                               std::min(tMin[1], tMax[1]),
                               std::min(tMin[2], tMax[2]) };
        pxr::GfVec3d bbMax = { std::max(tMin[0], tMax[0]),
                               std::max(tMin[1], tMax[1]),
                               std::max(tMin[2], tMax[2]) };

        extent = { bbMin, bbMax };
    }

    // If we are in selection mode, and the extent is empty, return bounding box.
    if (extent.IsEmpty() && useSel) {
        return boundingBox;
    }

    boundingBox = Box3(MaxUsd::ToMax(extent.GetMin()), MaxUsd::ToMax(extent.GetMax()));

    // Only cache when we are not in selection mode since the bounding box can change for a given
    // evaluationTime based on what is selected, thus needing a recompute.
    if (!useSel) {
        boundingBoxCache[evaluationTime] = boundingBox;
    }

    return boundingBox;
}
