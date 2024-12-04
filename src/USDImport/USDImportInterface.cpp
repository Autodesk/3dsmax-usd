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

#include "USDImport.h"

#include <MaxUsd/CurveConversion/CurveConverter.h>
#include <MaxUsd/Interfaces/IUSDImportOptions.h>
#include <MaxUsd/USDSceneController.h>
#include <MaxUsd/Utilities/OptionUtils.h>

#include <pxr/usd/usdGeom/basisCurves.h>

#include <maxscript/foundation/3dmath.h>
#include <maxscript/foundation/DataPair.h>
#include <maxscript/maxscript.h>
#include <maxscript/maxwrapper/mxsmaterial.h>
#include <maxscript/util/listener.h>

#define ScriptPrint (the_listener->edit_stream->_tprintf)

class USDImportInterface : public FPStaticInterface
{
public:
    DECLARE_DESCRIPTOR(USDImportInterface)

    void SetUIOptions(FPInterface* options)
    {
        if (options && options->GetID() == IUSDImportOptions_INTERFACE_ID) {
            MaxUsd::IUSDImportOptions* importOptions
                = dynamic_cast<MaxUsd::IUSDImportOptions*>(options);
            if (!importOptions) {
                throw MAXException(MSTR(L"Invalid Import Options object."));
            }
            USDImporter::SetUIOptions((*importOptions));
        }
    }

    FPInterface* GetUIOptions() { return &USDImporter::GetUIOptions(); }

    static int ImportFile(const MCHAR* filePath, FPInterface* usdImportOptions)
    {
        if (usdImportOptions) {
            MaxUsd::IUSDImportOptions* importOptions
                = dynamic_cast<MaxUsd::IUSDImportOptions*>(usdImportOptions);
            if (!importOptions) {
                throw MAXException(MSTR(L"Invalid Import Options object."));
            }
            return USDImporter::ImportFile(filePath, (*importOptions), true);
        }
        MaxUsd::IUSDImportOptions importOptions;
        importOptions.SetDefaults();
        return USDImporter::ImportFile(filePath, importOptions, true);
    }

    static int ImportFromCache(INT64 cacheId, FPInterface* usdImportOptions)
    {
        const MaxUsd::UsdStageSource cachedStage { pxr::UsdStageCache::Id::FromLongInt(
            static_cast<long>(cacheId)) };
        if (usdImportOptions) {
            MaxUsd::IUSDImportOptions* importOptions
                = dynamic_cast<MaxUsd::IUSDImportOptions*>(usdImportOptions);
            if (!importOptions) {
                throw MAXException(MSTR(L"Invalid Import Options object."));
            }
            if (!USDImporter::ValidateImportOptions(*importOptions)) {
                return IMPEXP_FAIL;
            }
            return MaxUsd::GetUSDSceneController()->Import(cachedStage, *importOptions);
        }
        MaxUsd::MaxSceneBuilderOptions defaultOptions;
        defaultOptions.SetDefaults();
        return MaxUsd::GetUSDSceneController()->Import(cachedStage, defaultOptions);
    }

    static Value*
    ConvertUsdMesh(INT64 stageCacheId, const wchar_t* path, FPInterface* usdImportOptions)
    {
        MaxUsd::IUSDImportOptions importOptions;
        importOptions.SetDefaults();
        if (usdImportOptions) {
            auto specifiedImportOptions
                = dynamic_cast<MaxUsd::IUSDImportOptions*>(usdImportOptions);
            if (!specifiedImportOptions) {
                throw RuntimeError(MSTR(L"Invalid Import Options object."));
            }
            importOptions = *specifiedImportOptions;
        }

        const MaxUsd::UsdStageSource cachedStage { pxr::UsdStageCache::Id::FromLongInt(
            static_cast<long>(stageCacheId)) };
        const auto                   stage = cachedStage.LoadStage(importOptions);
        if (!stage) {
            throw RuntimeError(MSTR(L"Unable to fetch the stage from the global cache using the "
                                    L"given stage cache id."));
        }

        const auto prim = stage->GetPrimAtPath(pxr::SdfPath { MaxUsd::MaxStringToUsdString(path) });
        if (!prim.IsValid() || !prim.IsA<pxr::UsdGeomMesh>()) {
            throw RuntimeError(MSTR(L"The given path does not point to a USD Mesh prim."));
        }

        MNMesh                mesh;
        MaxUsd::MeshConverter converter;

        const auto timeMode = static_cast<MaxUsd::MaxSceneBuilderOptions::ImportTimeMode>(
            importOptions.GetTimeMode());
        const auto                 timeConfig = importOptions.GetResolvedTimeConfig(stage);
        auto                       timeCodeValue = pxr::UsdTimeCode::Default();
        bool                       validOptions = USDImporter::ValidateImportOptions(importOptions);
        MultiMtl*                  materialBind = nullptr;
        std::map<int, std::string> channelNames;

        if (!validOptions) {
            throw RuntimeError(MSTR(
                L"The import configuration is not valid. Set a valid mode and/or time range."));
        }

        switch (timeMode) {
        case MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::StartTime: {
            timeCodeValue = stage->GetStartTimeCode();
            break;
        }
        case MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::EndTime: {
            timeCodeValue = stage->GetEndTimeCode();
            break;
        }
        case MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::CustomRange: {
            timeCodeValue = timeConfig.GetStartTimeCode();
            if (timeCodeValue != timeConfig.GetEndTimeCode()) {
                MaxUsd::Log::Warn(
                    "#customRange TimeMode not supported for USDImporter.ConvertUsdMesh, the "
                    "conversion will be performed at the configured start time {}.",
                    std::to_string(timeCodeValue.GetValue()));
            }
            break;
        }
        case MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::AllRange: {
            timeCodeValue = stage->GetStartTimeCode();
            MaxUsd::Log::Warn(
                "#allrange TimeMode not supported for USDImporter.ConvertUsdMesh, the "
                "conversion will be performed at the stage's start time code {}.",
                std::to_string(timeCodeValue.GetValue()));
            break;
        }
        default: DbgAssert("unsupported TimeCode for USDImporter.ConvertUsdMesh");
        }

        converter.ConvertToMNMesh(
            pxr::UsdGeomMesh(prim),
            mesh,
            importOptions.GetPrimvarMappingOptions(),
            channelNames,
            &materialBind,
            timeCodeValue);

        Mesh* triMesh = new Mesh();
        mesh.OutToTri(*triMesh);

        three_typed_value_locals(DataPair * dataPair, MeshValue * mesh, Value * materialBind);
        vl.mesh = new MeshValue { triMesh };
        vl.materialBind = MAXMaterial::intern(materialBind);
        vl.dataPair = new DataPair(
            vl.mesh, vl.materialBind, n_mesh, Name::intern(_T("usdGeomSubsetsBindMaterial")));

        return vl.dataPair;
    }

    static Value* ConvertUsdBasisCurve(
        INT64          stageCacheId,
        const wchar_t* path,
        bool           asBezierShape,
        FPInterface*   usdImportOptions)
    {
        MaxUsd::IUSDImportOptions importOptions;
        importOptions.SetDefaults();
        if (usdImportOptions) {
            auto specifiedImportOptions
                = dynamic_cast<MaxUsd::IUSDImportOptions*>(usdImportOptions);
            if (!specifiedImportOptions) {
                throw RuntimeError(MSTR(L"Invalid Import Options object."));
            }
            importOptions = *specifiedImportOptions;
        }

        const MaxUsd::UsdStageSource cachedStage { pxr::UsdStageCache::Id::FromLongInt(
            static_cast<long>(stageCacheId)) };
        const auto                   stage = cachedStage.LoadStage(importOptions);
        if (!stage) {
            throw RuntimeError(MSTR(L"Unable to fetch the stage from the global cache using the "
                                    L"given stage cache id."));
        }

        const auto prim = stage->GetPrimAtPath(pxr::SdfPath { MaxUsd::MaxStringToUsdString(path) });
        if (!prim.IsValid() || !prim.IsA<pxr::UsdGeomBasisCurves>()) {
            throw RuntimeError(MSTR(L"The given path does not point to a USD BasisCurve prim."));
        }

        const auto timeMode = static_cast<MaxUsd::MaxSceneBuilderOptions::ImportTimeMode>(
            importOptions.GetTimeMode());
        const auto                 timeConfig = importOptions.GetResolvedTimeConfig(stage);
        auto                       timeCodeValue = pxr::UsdTimeCode::Default();
        bool                       validOptions = USDImporter::ValidateImportOptions(importOptions);
        MultiMtl*                  materialBind = nullptr;
        std::map<int, std::string> channelNames;

        if (!validOptions) {
            throw RuntimeError(MSTR(
                L"The import configuration is not valid. Set a valid mode and/or time range."));
        }

        switch (timeMode) {
        case MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::StartTime: {
            timeCodeValue = stage->GetStartTimeCode();
            break;
        }
        case MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::EndTime: {
            timeCodeValue = stage->GetEndTimeCode();
            break;
        }
        case MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::CustomRange: {
            timeCodeValue = timeConfig.GetStartTimeCode();
            if (timeCodeValue != timeConfig.GetEndTimeCode()) {
                MaxUsd::Log::Warn(
                    "#customRange TimeMode not supported for USDImporter.ConvertUsdBasisCurve, the "
                    "conversion will be performed at the configured start time {}.",
                    std::to_string(timeCodeValue.GetValue()));
            }
            break;
        }
        case MaxUsd::MaxSceneBuilderOptions::ImportTimeMode::AllRange: {
            timeCodeValue = stage->GetStartTimeCode();
            MaxUsd::Log::Warn(
                "#allrange TimeMode not supported for USDImporter.ConvertUsdBasisCurve, the "
                "conversion will be performed at the stage's start time code {}.",
                std::to_string(timeCodeValue.GetValue()));
            break;
        }
        default: DbgAssert("unsupported TimeCode for USDImporter.ConvertUsdBasisCurve");
        }

        const pxr::UsdGeomBasisCurves    basisCurvesPrim(prim);
        TypedSingleRefMaker<SplineShape> shapeObj(
            (SplineShape*)GetCOREInterface()->CreateInstance(SHAPE_CLASS_ID, splineShapeClassID));

        MaxUsd::CurveConverter::ConvertToSplineShape(basisCurvesPrim, *shapeObj, timeCodeValue);

        one_typed_value_local(Value * res);
        if (asBezierShape) {
            BezierShape* bshape = new BezierShape { shapeObj->GetShape() };
            vl.res = new BezierShapeValue(bshape, 1);
        } else {
            INode* shapeNode = GetCOREInterface()->CreateObjectNode(shapeObj);
            vl.res = MAXNode::intern(shapeNode);
        }
        return_value(vl.res);
    }

    FPInterface* CreateOptions()
    {
        auto options = new MaxUsd::IUSDImportOptions;
        options->SetDefaults();
        return options;
    }

    void Log(int messageType, const std::wstring& message)
    {
        MaxUsd::Log::Message(MaxUsd::Log::Level(messageType), message);
    }

    FPInterface* CreateOptionsFromJsonString(const MCHAR* jsonString)
    {
        if (jsonString) {
            MaxUsd::MaxSceneBuilderOptions options(MaxUsd::OptionUtils::DeserializeOptionsFromJson(
                QString::fromWCharArray(jsonString).toUtf8()));
            return new MaxUsd::IUSDImportOptions(options);
        }
        throw MAXException(L"Invalid JSON string");
    }

    enum
    {
        eid_LogLevel
    };

    enum
    {
        fid_SetUIOptions,
        fid_GetUIOptions,
        fid_ImportFile,
        fid_ImportFromCache,
        fid_CreateOptions,
        fid_ConvertUsdMesh,
        fid_ConvertUsdBasisCurve,
        fid_Log,
        fid_SetMaterialParamByName,
        fid_SetTexmapParamByName,
        fid_CreateOptionsFromJsonString
    };

    /**
     * \brief Sets a paramblock parameter by name. Setting PB params from MXS is slow, using the C++ implementation
     * is useful to bypass the slowness. The function will look at all paramblocks until it finds a
     * matching param, it is then set using to the passed Value.
     * \param mtlBase The base material object, for which to set a param.
     * \param name The name of the param.
     * \param value The value to set.
     */
    static void SetMtlBaseParamByName(MtlBase* mtlBase, const wchar_t* name, Value* value)
    {
        if (!mtlBase) {
            ScriptPrint(_T("ERROR : Undefined material passed to SetMtlBaseParamByName().\n"));
            return;
        }

        const bool isUndefined = value == &undefined;

        for (int i = 0; i < mtlBase->NumParamBlocks(); ++i) {
            const auto pb = mtlBase->GetParamBlock(i);
            const auto idx = pb->GetDesc()->NameToIndex(name);

            if (idx == -1) {
                continue;
            }

            // We found the paramblock for this parameter
            const auto paramDef = pb->GetDesc()->GetParamDefByIndex(idx);
            const auto paramId = paramDef->ID;

            switch (paramDef->type) {
            case TYPE_FLOAT: pb->SetValue(paramId, 0, value->to_float()); break;
            case TYPE_INT: pb->SetValue(paramId, 0, value->to_int()); break;
            case TYPE_BOOL: pb->SetValue(paramId, 0, value->to_bool()); break;
            case TYPE_RGBA:
            case TYPE_RGBA_BV: pb->SetValue(paramId, 0, value->to_point3()); break;
            case TYPE_STRING:
            case TYPE_FILENAME:
                pb->SetValue(paramId, 0, isUndefined ? nullptr : value->to_string());
                break;
            case TYPE_INODE:
                pb->SetValue(paramId, 0, isUndefined ? nullptr : value->to_node());
                break;
            case TYPE_REFTARG:
                pb->SetValue(paramId, 0, isUndefined ? nullptr : value->to_reftarg());
                break;
            case TYPE_TEXMAP:
                pb->SetValue(paramId, 0, isUndefined ? nullptr : value->to_texmap());
                break;
            case TYPE_BITMAP: {
                if (isUndefined) {
                    pb->SetValue(paramId, 0, static_cast<PBBitmap*>(nullptr));
                } else {
                    FPValue val;
                    value->to_fpvalue(val);
                    pb->SetValue(paramId, 0, val.bm);
                }
                break;
            }
            case TYPE_MTL: pb->SetValue(paramId, 0, isUndefined ? nullptr : value->to_mtl()); break;
            case TYPE_FRGBA_BV:
            case TYPE_FRGBA: pb->SetValue(paramId, 0, value->to_acolor()); break;
            case TYPE_POINT2: pb->SetValue(paramId, 0, value->to_point2()); break;
            case TYPE_POINT3: pb->SetValue(paramId, 0, value->to_point3()); break;
            case TYPE_POINT4: pb->SetValue(paramId, 0, value->to_point4()); break;
            default:
                DbgAssert(false);
                ScriptPrint(
                    _T("ERROR : Unsupported parameter type for SetMtlBaseParamByName().\n"));
                break;
            }

            break;
        }
    }

    // clang-format off
	BEGIN_FUNCTION_MAP
		PROP_FNS(fid_GetUIOptions, GetUIOptions, fid_SetUIOptions, SetUIOptions, TYPE_INTERFACE);
		FN_2(fid_ImportFile, TYPE_INT, ImportFile, TYPE_STRING, TYPE_INTERFACE);
		FN_2(fid_ImportFromCache, TYPE_INT, ImportFromCache, TYPE_INT64, TYPE_INTERFACE);
		FN_3(fid_ConvertUsdMesh, TYPE_VALUE, ConvertUsdMesh, TYPE_INT64, TYPE_STRING, TYPE_INTERFACE);
		FN_4(fid_ConvertUsdBasisCurve, TYPE_VALUE, ConvertUsdBasisCurve, TYPE_INT64, TYPE_STRING, TYPE_BOOL, TYPE_INTERFACE);
		FN_0(fid_CreateOptions, TYPE_INTERFACE, CreateOptions);
		FN_1(fid_CreateOptionsFromJsonString, TYPE_INTERFACE, CreateOptionsFromJsonString, TYPE_STRING);
		VFN_2(fid_Log, Log, TYPE_ENUM, TYPE_STRING);
		VFN_3(fid_SetMaterialParamByName, SetMtlBaseParamByName, TYPE_MTL, TYPE_STRING, TYPE_VALUE);
		VFN_3(fid_SetTexmapParamByName, SetMtlBaseParamByName, TYPE_TEXMAP, TYPE_STRING, TYPE_VALUE);
	END_FUNCTION_MAP
    // clang-format on
};

#define USDIMPORT_INTERFACE Interface_ID(0x2b240ddb, 0x61f331e8)

// clang-format off
static USDImportInterface usdImportInterface(
	USDIMPORT_INTERFACE, _T("USDImport"), 0, GetUSDImporterDesc(), 0,
	// Functions
	USDImportInterface::fid_ImportFile, _T("ImportFile"), "Import USD file with custom options.", TYPE_INT, FP_NO_REDRAW, 2,
		_T("filePath"), 0, TYPE_STRING,
		_T("importOptions"), 0, TYPE_INTERFACE, f_keyArgDefault, NULL,
	USDImportInterface::fid_ImportFromCache, _T("ImportFromCache"), "Import USD stage from cache with custom options.", TYPE_INT, FP_NO_REDRAW, 2,
		_T("stageCacheId"), 0, TYPE_INT,
		_T("importOptions"), 0, TYPE_INTERFACE, f_keyArgDefault, NULL,

	USDImportInterface::fid_ConvertUsdMesh, _T("ConvertUsdMesh"), "Converts a USD mesh to a TriMesh.", TYPE_VALUE, FP_NO_REDRAW, 3,
		_T("stageCacheId"), 0, TYPE_INT,
		_T("path"), 0, TYPE_STRING,
		_T("options"), 0, TYPE_INTERFACE, f_keyArgDefault, NULL,

	USDImportInterface::fid_ConvertUsdBasisCurve, _T("ConvertUsdBasisCurve"), "Converts a USD basis curve to a 3dsMax Editable Spline. If 'asBezierShape:true' is specified, then the curve will be converted to a BezierShape.", TYPE_VALUE, FP_NO_REDRAW, 4,
		_T("stageCacheId"), 0, TYPE_INT,
		_T("path"), 0, TYPE_STRING,
		_T("asBezierShape"), 0, TYPE_BOOL, f_keyArgDefault, false,
		_T("options"), 0, TYPE_INTERFACE, f_keyArgDefault, NULL,
		
	USDImportInterface::fid_SetMaterialParamByName, _T("SetMaterialParamByName"), "Sets a material parameter, by name, and fast.", TYPE_VOID, FP_NO_REDRAW, 3,
		_T("material"), 0, TYPE_MTL,
		_T("paramName"), 0, TYPE_STRING,
		_T("value"), 0, TYPE_VALUE,

	USDImportInterface::fid_SetTexmapParamByName, _T("SetTexmapParamByName"), "Sets a texture map parameter, by name, and fast.", TYPE_VOID, FP_NO_REDRAW, 3,
		_T("texmap"), 0, TYPE_TEXMAP,
		_T("paramName"), 0, TYPE_STRING,
		_T("value"), 0, TYPE_VALUE,
			
	USDImportInterface::fid_CreateOptions, _T("CreateOptions"), "Create a new set of import options filled with default values", TYPE_INTERFACE, FP_NO_REDRAW, 0,

	USDImportInterface::fid_CreateOptionsFromJsonString, _T("CreateOptionsFromJson"), "Creates import options from a JSON formatted string.", TYPE_INTERFACE, FP_NO_REDRAW, 1,
		_T("jsonString"), 0, TYPE_STRING,

	USDImportInterface::fid_Log, _T("Log"), "Log info, warning, and error messages to USD import logs from USD import callbacks.", TYPE_VOID, FP_NO_REDRAW, 2,
		_T("logLevel"), 0, TYPE_ENUM, USDImportInterface::eid_LogLevel,
		_T("message"), 0, TYPE_STRING, properties,
	properties, 
	USDImportInterface::fid_GetUIOptions, USDImportInterface::fid_SetUIOptions, _T("UIOptions"), 0, TYPE_INTERFACE,
	enums,
	USDImportInterface::eid_LogLevel, 3,
		_T("info"), MaxUsd::Log::Level::Info,
		_T("warn"), MaxUsd::Log::Level::Warn,
		_T("error"), MaxUsd::Log::Level::Error,
	p_end
);
// clang-format on