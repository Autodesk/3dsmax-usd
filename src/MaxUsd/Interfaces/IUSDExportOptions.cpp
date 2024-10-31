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
// AUTHOR: Autodesk Inc.
//
#include "IUSDExportOptions.h"

#include <MaxUsd/Chaser/ExportChaserRegistry.h>
#include <MaxUsd/Translators/ShadingModeRegistry.h>
#include <MaxUsd/Utilities/OptionUtils.h>

#include <maxscript/foundation/MXSDictionaryValue.h>
#include <maxscript/foundation/numbers.h>
#include <maxscript/foundation/structs.h>
#include <maxscript/maxscript.h>
#include <maxscript/maxwrapper/mxsobjects.h>

namespace MAXUSD_NS_DEF {

// clang-format off
FPInterfaceDesc IUSDExportOptionsDesc(
	IUSDExportOptions_INTERFACE_ID, _T("IUSDExportOptions"), 0, NULL, FP_MIXIN,
	// Functions
	IUSDExportOptions::fnIdReset, _T("Reset"), "Reset to defaults export options", TYPE_VOID, FP_NO_REDRAW, 0,
	IUSDExportOptions::fnIdSetChannelPrimvarMappingDefaults, _T("SetChannelPrimvarMappingDefaults"), "Resets channel to primvar mappings.", TYPE_VOID, FP_NO_REDRAW, 0,
	IUSDExportOptions::fnIdSetChannelPrimvarMapping, _T("SetChannelPrimvarMapping"), "Sets a channel to primvar mapping.", TYPE_VOID, FP_NO_REDRAW, 4,
		_T("channel"), 0, TYPE_INT,
		_T("targetPrimvar"), 0, TYPE_VALUE,
		_T("type"), 0, TYPE_ENUM, IUSDExportOptions::eIdPrimvarType, f_keyArgDefault, MaxUsd::MappedAttributeBuilder::Type::Float3Array,
		_T("autoExpandType"), 0, TYPE_bool, f_keyArgDefault, FALSE,
	IUSDExportOptions::fnIdGetChannelPrimvarType, _T("GetChannelPrimvarType"), "Returns the type of the primvar associated with this channel.", TYPE_ENUM, IUSDExportOptions::eIdPrimvarType, FP_NO_REDRAW, 1,
		_T("channel"), 0, TYPE_INT,
	IUSDExportOptions::fnIdGetChannelPrimvarName, _T("GetChannelPrimvarName"), "Returns the name of the primvar associated with this channel.", TYPE_STRING, FP_NO_REDRAW, 1,
		_T("channel"), 0, TYPE_INT,
	IUSDExportOptions::fnIdGetChannelPrimvarAutoExpandType, _T("GetChannelPrimvarAutoExpandType"), "Returns whether the type should auto expand if need.", TYPE_BOOL, FP_NO_REDRAW, 1,
		_T("channel"), 0, TYPE_INT,
	IUSDExportOptions::fnIdGetAvailableMaterialConversions, _T("AvailableMaterialTargets"), "Returns an array of all available USD material target types", TYPE_TSTR_TAB_BV, FP_NO_REDRAW, 0,
	IUSDExportOptions::fnIdGetAvailableChasers, _T("AvailableChasers"), "Returns an array of all available export chasers", TYPE_TSTR_TAB_BV, FP_NO_REDRAW, 0,
	IUSDExportOptions::fidSerialize, _T("Serialize"), "Serialize the options to a JSON formatted string.", TYPE_STRING, FP_NO_REDRAW, 0,
	// Properties
	properties,
	IUSDExportOptions::fnIdGetTranslateMeshes, IUSDExportOptions::fnIdSetTranslateMeshes, _T("Meshes"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetTranslateShapes, IUSDExportOptions::fnIdSetTranslateShapes, _T("Shapes"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetTranslateLights, IUSDExportOptions::fnIdSetTranslateLights, _T("Lights"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetTranslateCameras, IUSDExportOptions::fnIdSetTranslateCameras, _T("Cameras"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetTranslateSkin, IUSDExportOptions::fnIdSetTranslateSkin, _T("Skin"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetTranslateMorpher, IUSDExportOptions::fnIdSetTranslateMorpher, _T("Morpher"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetTranslateMaterials, IUSDExportOptions::fnIdSetTranslateMaterials, _T("Materials"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetShadingMode, IUSDExportOptions::fnIdSetShadingMode, _T("ShadingMode"), 0, TYPE_STRING,
	IUSDExportOptions::fnIdGetAllMaterialConversions, IUSDExportOptions::fnIdSetAllMaterialConversions, _T("AllMaterialTargets"), 0, TYPE_TSTR_TAB_BV,
	IUSDExportOptions::fnIdGetUsdStagesAsReferences, IUSDExportOptions::fnIdSetUsdStagesAsReferences, _T("UsdStagesAsReferences"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetTranslateHidden, IUSDExportOptions::fnIdSetTranslateHidden, _T("HiddenObjects"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetUseUSDVisibility, IUSDExportOptions::fnIdSetUseUSDVisibility, _T("UseUSDVisibility"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetAllowNestedGprims, IUSDExportOptions::fnIdSetAllowNestedGprims, _T("AllowNestedGprims"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetFileFormat, IUSDExportOptions::fnIdSetFileFormat, _T("FileFormat"), 0, TYPE_ENUM, IUSDExportOptions::eIdFileFormat,
	IUSDExportOptions::fnIdGetNormals, IUSDExportOptions::fnIdSetNormals, _T("Normals"), 0, TYPE_ENUM, IUSDExportOptions::eIdNormalsMode,
	IUSDExportOptions::fnIdGetMeshFormat, IUSDExportOptions::fnIdSetMeshFormat, _T("MeshFormat"), 0, TYPE_ENUM, IUSDExportOptions::eIdMeshFormat,
	IUSDExportOptions::fnIdGetTimeMode, IUSDExportOptions::fnIdSetTimeMode, _T("TimeMode"), FP_NO_REDRAW, TYPE_ENUM, IUSDExportOptions::eIdTimeMode,
	IUSDExportOptions::fid_GetStartFrame, IUSDExportOptions::fid_SetStartFrame, _T("StartFrame"), FP_NO_REDRAW, TYPE_DOUBLE,
	IUSDExportOptions::fid_GetEndFrame, IUSDExportOptions::fid_SetEndFrame, _T("EndFrame"), FP_NO_REDRAW, TYPE_DOUBLE,
	IUSDExportOptions::fid_GetSamplesPerFrame, IUSDExportOptions::fid_SetSamplesPerFrame, _T("SamplesPerFrame"), FP_NO_REDRAW, TYPE_DOUBLE,
	IUSDExportOptions::fnIdGetUpAxis, IUSDExportOptions::fnIdSetUpAxis, _T("UpAxis"), 0, TYPE_ENUM, IUSDExportOptions::eIdUpAxis,
	IUSDExportOptions::fnIdGetBakeObjectOffsetTransform, IUSDExportOptions::fnIdSetBakeObjectOffsetTransform, _T("BakeObjectOffsetTransform"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetPreserveEdgeOrientation, IUSDExportOptions::fnIdSetPreserveEdgeOrientation, _T("PreserveEdgeOrientation"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetRootPrimPath, IUSDExportOptions::fnIdSetRootPrimPath, _T("RootPrimPath"), FP_NO_REDRAW, TYPE_STRING,
	IUSDExportOptions::fnIdGetBonesPrimName, IUSDExportOptions::fnIdSetBonesPrimName, _T("BonesPrimName"), FP_NO_REDRAW, TYPE_STRING,
	IUSDExportOptions::fnIdGetAnimationsPrimName, IUSDExportOptions::fnIdSetAnimationsPrimName, _T("AnimationsPrimName"), FP_NO_REDRAW, TYPE_STRING,
	IUSDExportOptions::fnIdGetLogPath, IUSDExportOptions::fnIdSetLogPath, _T("LogPath"), FP_NO_REDRAW, TYPE_STRING,
	IUSDExportOptions::fnIdGetLogLevel, IUSDExportOptions::fnIdSetLogLevel, _T("LogLevel"), FP_NO_REDRAW, TYPE_ENUM, IUSDExportOptions::eIdLogLevel,
	IUSDExportOptions::fnIdGetOpenInUsdview, IUSDExportOptions::fnIdSetOpenInUsdview, _T("OpenInUsdview"), 0, TYPE_BOOL,
	IUSDExportOptions::fnIdGetChaserNames, IUSDExportOptions::fnIdSetChaserNames, _T("ChaserNames"), 0, TYPE_TSTR_TAB_BV,
	IUSDExportOptions::fnIdGetAllChaserArgs, IUSDExportOptions::fnIdSetAllChaserArgs, _T("AllChaserArgs"), 0, TYPE_VALUE,
	IUSDExportOptions::fnIdGetContextNames, IUSDExportOptions::fnIdSetContextNames, _T("ContextNames"), 0, TYPE_TSTR_TAB_BV,
#ifdef IS_MAX2024_OR_GREATER
	IUSDExportOptions::fnIdGetMtlSwitcherExportStyle, IUSDExportOptions::fnIdSetMtlSwitcherExportStyle, _T("MtlSwitcherExportStyle"), FP_NO_REDRAW, TYPE_ENUM, IUSDExportOptions::eIdMtlSwitcherExportStyle,
#endif
	IUSDExportOptions::fnIdGetUseProgressBar, IUSDExportOptions::fnIdSetUseProgressBar, _T("UseProgressBar"), 0, TYPE_BOOL, 
	IUSDExportOptions::fnIdGetMaterialLayerPath, IUSDExportOptions::fnIdSetMaterialLayerPath, _T("MaterialLayerPath"), FP_NO_REDRAW, TYPE_STRING, 
	IUSDExportOptions::fnIdGetMaterialPrimPath,	IUSDExportOptions::fnIdSetMaterialPrimPath, _T("MaterialPrimPath"), FP_NO_REDRAW, TYPE_STRING,
	IUSDExportOptions::fnIdGetUseSeparateMaterialLayer, IUSDExportOptions::fnIdSetUseSeparateMaterialLayer, _T("UseSeparateMaterialLayer"), FP_NO_REDRAW, TYPE_BOOL,
	IUSDExportOptions::fnIdGetUseLastResortUSDPreviewSurfaceWriter, IUSDExportOptions::fnIdSetUseLastResortUSDPreviewSurfaceWriter, _T("UseLastResortUSDPreviewSurfaceWriter"), FP_NO_REDRAW, TYPE_BOOL,
	// Enums
	enums,
	IUSDExportOptions::eIdFileFormat, 2,
		_T("ascii"), USDSceneBuilderOptions::FileFormat::ASCII,
		_T("binary"), USDSceneBuilderOptions::FileFormat::Binary,
	IUSDExportOptions::eIdUpAxis, 2,
		_T("y"), USDSceneBuilderOptions::UpAxis::Y,
		_T("z"), USDSceneBuilderOptions::UpAxis::Z,
	IUSDExportOptions::eIdPrimvarType, 6,
		_T("texCoord2fArray"), MaxUsd::MappedAttributeBuilder::Type::TexCoord2fArray,
		_T("texCoord3fArray"), MaxUsd::MappedAttributeBuilder::Type::TexCoord3fArray,
		_T("floatArray"), MaxUsd::MappedAttributeBuilder::Type::FloatArray,
		_T("float2Array"), MaxUsd::MappedAttributeBuilder::Type::Float2Array,
		_T("float3Array"), MaxUsd::MappedAttributeBuilder::Type::Float3Array,
		_T("color3fArray"), MaxUsd::MappedAttributeBuilder::Type::Color3fArray,
	IUSDExportOptions::eIdLogLevel, 4,
		_T("off"), MaxUsd::Log::Level::Off,
		_T("info"), MaxUsd::Log::Level::Info,
		_T("warn"), MaxUsd::Log::Level::Warn,
		_T("error"), MaxUsd::Log::Level::Error,
	IUSDExportOptions::eIdNormalsMode, 3,
		_T("none"), MaxMeshConversionOptions::NormalsMode::None,
		_T("asAttribute"), MaxMeshConversionOptions::NormalsMode::AsAttribute,
		_T("asPrimvar"), MaxMeshConversionOptions::NormalsMode::AsPrimvar,
	IUSDExportOptions::eIdMeshFormat, 3,
		_T("fromScene"), MaxMeshConversionOptions::MeshFormat::FromScene,
		_T("polyMesh"), MaxMeshConversionOptions::MeshFormat::PolyMesh,
		_T("triMesh"), MaxMeshConversionOptions::MeshFormat::TriMesh,
	IUSDExportOptions::eIdTimeMode, 4, 
		_T("current"), USDSceneBuilderOptions::TimeMode::CurrentFrame,
		_T("explicit"), USDSceneBuilderOptions::TimeMode::ExplicitFrame,
		_T("animationRange"), USDSceneBuilderOptions::TimeMode::AnimationRange,
		_T("frameRange"), USDSceneBuilderOptions::TimeMode::FrameRange,
#ifdef IS_MAX2024_OR_GREATER
	IUSDExportOptions::eIdMtlSwitcherExportStyle, 2, 
		_T("asVariantSets"), USDSceneBuilderOptions::MtlSwitcherExportStyle::AsVariantSets,
		_T("activeMaterial"), USDSceneBuilderOptions::MtlSwitcherExportStyle::ActiveMaterialOnly,
#endif
	p_end
);
// clang-format on

IUSDExportOptions::IUSDExportOptions() { SetDefaults(); }

IUSDExportOptions::IUSDExportOptions(const IUSDExportOptions& options) { SetOptions(options); }

IUSDExportOptions::IUSDExportOptions(const USDSceneBuilderOptions& options) { SetOptions(options); }

const IUSDExportOptions& IUSDExportOptions::operator=(const IUSDExportOptions& options)
{
    SetOptions(options);
    return *this;
}

void IUSDExportOptions::SetFileFormat(int saveFormat)
{
    const auto format = USDSceneBuilderOptions::FileFormat(saveFormat);
    if (format != FileFormat::ASCII && format != FileFormat::Binary) {
        WStr errorMsg(L"Incorrect FileFormat value. Accepted values are #ascii and #binary.");
        throw RuntimeError(errorMsg.data());
    }

    USDSceneBuilderOptions::SetFileFormat(format);
}

void IUSDExportOptions::SetUpAxis(int upAxis)
{
    const auto axis = USDSceneBuilderOptions::UpAxis(upAxis);
    if (axis != UpAxis::Y && axis != UpAxis::Z) {
        WStr errorMsg(L"Incorrect UpAxis value. Accepted values are #y and #z.");
        throw RuntimeError(errorMsg.data());
    }

    USDSceneBuilderOptions::SetUpAxis(axis);
}

void IUSDExportOptions::SetNormalsMode(int normalsMode)
{
    const auto normals = MaxMeshConversionOptions::NormalsMode(normalsMode);
    if (normals != MaxMeshConversionOptions::NormalsMode::AsAttribute
        && normals != MaxMeshConversionOptions::NormalsMode::AsPrimvar
        && normals != MaxMeshConversionOptions::NormalsMode::None) {
        WStr errorMsg(
            L"Incorrect Normals value. Accepted values are #asAttribute, #asPrimvar and #none.");
        throw RuntimeError(errorMsg.data());
    }

    USDSceneBuilderOptions::SetNormalsMode(normals);
}

void IUSDExportOptions::SetMeshFormat(int meshFormatValue)
{
    const auto meshFormat = MaxMeshConversionOptions::MeshFormat(meshFormatValue);
    if (meshFormat != MaxMeshConversionOptions::MeshFormat::FromScene
        && meshFormat != MaxMeshConversionOptions::MeshFormat::PolyMesh
        && meshFormat != MaxMeshConversionOptions::MeshFormat::TriMesh) {
        WStr errorMsg(
            L"Incorrect MeshFormat value. Accepted values are #fromScene, #polyMesh and #triMesh.");
        throw RuntimeError(errorMsg.data());
    }

    USDSceneBuilderOptions::SetMeshFormat(meshFormat);
}

void IUSDExportOptions::SetTimeMode(int timeMode)
{
    const auto mode = static_cast<TimeMode>(timeMode);
    // Validation.
    if (mode != TimeMode::CurrentFrame && mode != TimeMode::ExplicitFrame
        && mode != TimeMode::AnimationRange && mode != TimeMode::FrameRange) {
        WStr errorMsg(L"Incorrect TimeMode value. Accepted values are #current, #explicit, "
                      L"#animationRange or #frameRange.");
        throw RuntimeError(errorMsg.data());
    }
    USDSceneBuilderOptions::SetTimeMode(mode);
}

void IUSDExportOptions::SetSamplesPerFrame(double samplesPerFrame)
{
    if (samplesPerFrame < USDSceneBuilderOptions::MIN_SAMPLES_PER_FRAME
        || samplesPerFrame > USDSceneBuilderOptions::MAX_SAMPLES_PER_FRAME) {
        WStr errorMsg = WStr(L"The given SamplesPerFrame is outside of the allowed range [")
            + std::to_wstring(MIN_SAMPLES_PER_FRAME).c_str() + WStr(L",")
            + std::to_wstring(MAX_SAMPLES_PER_FRAME).c_str() + WStr(L"].");
        throw RuntimeError(errorMsg.data());
    }
    USDSceneBuilderOptions::SetSamplesPerFrame(samplesPerFrame);
}

bool IUSDExportOptions::GetBakeObjectOffsetTransform() const
{
    return GetMeshConversionOptions().GetBakeObjectOffsetTransform();
}

void IUSDExportOptions::SetBakeObjectOffsetTransform(bool value)
{
    auto meshConversionOptions = GetMeshConversionOptions();
    meshConversionOptions.SetBakeObjectOffsetTransform(value);
    SetMeshConversionOptions(meshConversionOptions);
}

bool IUSDExportOptions::GetPreserveEdgeOrientation() const
{
    return GetMeshConversionOptions().GetPreserveEdgeOrientation();
}

void IUSDExportOptions::SetPreserveEdgeOrientation(bool preserve)
{
    auto meshConversionOptions = GetMeshConversionOptions();
    meshConversionOptions.SetPreserveEdgeOrientation(preserve);
    SetMeshConversionOptions(meshConversionOptions);
}

void IUSDExportOptions::SetChannelPrimvarMappingDefaults()
{
    auto meshConversionOptions = GetMeshConversionOptions();
    meshConversionOptions.SetDefaultChannelPrimvarMappings();
    SetMeshConversionOptions(meshConversionOptions);
}

void IUSDExportOptions::SetChannelPrimvarMapping(
    int    channel,
    Value* nameValue,
    int    type,
    bool   autoExpandType)
{
    // Validation.
    if (!MaxUsd::IsValidChannel(channel)) {
        const auto errorMsg
            = std::to_wstring(channel)
            + std::wstring(
                  L" is not a valid map channel. Valid channels are from -2 to 99 inclusively.");
        throw RuntimeError(errorMsg.c_str());
    }

    const MaxUsd::MappedAttributeBuilder::Type attributeType
        = MaxUsd::MappedAttributeBuilder::Type(type);
    if (attributeType != MappedAttributeBuilder::Type::TexCoord2fArray
        && attributeType != MappedAttributeBuilder::Type::TexCoord3fArray
        && attributeType != MappedAttributeBuilder::Type::FloatArray
        && attributeType != MappedAttributeBuilder::Type::Float2Array
        && attributeType != MappedAttributeBuilder::Type::Float3Array
        && attributeType != MappedAttributeBuilder::Type::Color3fArray) {
        WStr errorMsg(L"Unsupported primvar type. Accepted values are #TexCoord2fArray, "
                      L"#TexCoord3fArray, #FloatArray, "
                      L"#Float2Array, #Float3Array and #Color3fArray.");
        throw RuntimeError(errorMsg.data());
    }

    // If "undefined" was passed as name this means we do not want to export this channel.
    // Internally we will keep track of this with an empty string.
    std::string primvarName;
    if (nameValue != &undefined) {
        // Make sure the given primvar name is supported...
        const wchar_t*    nameWstring = nameValue->to_string();
        const std::string primvarNameStr = MaxUsd::MaxStringToUsdString(nameWstring);
        primvarName = pxr::TfMakeValidIdentifier(primvarNameStr);
        if (wcscmp(nameWstring, MaxUsd::UsdStringToMaxString(primvarName).data()) != 0) {
            const auto errorMsg = nameWstring
                + std::wstring(L" is not a valid primvar name. The name must start with a letter "
                               L"or underscore, and "
                               L"must contain only letters, underscores, and numerals..");
            throw RuntimeError(errorMsg.c_str());
        }
    }

    const MaxUsd::MappedAttributeBuilder::Config config(
        pxr::TfToken(primvarName), attributeType, autoExpandType);

    auto meshConversionOptions = GetMeshConversionOptions();
    meshConversionOptions.SetChannelPrimvarConfig(channel, config);
    SetMeshConversionOptions(meshConversionOptions);
}

const MaxUsd::MappedAttributeBuilder::Config IUSDExportOptions::GetValidPrimvarConfig(int channel)
{
    // Validation.
    if (!MaxUsd::IsValidChannel(channel)) {
        const auto errorMsg
            = std::to_wstring(channel)
            + std::wstring(
                  L" is not a valid map channel. Valid channels are from -2 to 99 inclusively.");
        throw RuntimeError(errorMsg.c_str());
    }
    return GetMeshConversionOptions().GetChannelPrimvarConfig(channel);
}

Value* IUSDExportOptions::GetChannelPrimvarName(int channel)
{
    // Will throw on unmapped channels.
    const auto& config = GetValidPrimvarConfig(channel);
    const WStr  name = MaxUsd::UsdStringToMaxString(config.GetPrimvarName());
    if (name.Length() == 0) {
        return &undefined;
    }
    // Garbage collected string (inherits from Collectable via Value)
    return new String(name);
}

int IUSDExportOptions::GetChannelPrimvarType(int channel)
{
    // Will throw on unmapped channels.
    const auto& config = GetValidPrimvarConfig(channel);
    return static_cast<int>(config.GetPrimvarType());
}

int IUSDExportOptions::GetChannelPrimvarAutoExpandType(int channel)
{
    // Will throw on unmapped channels.
    const auto& config = GetValidPrimvarConfig(channel);
    return static_cast<int>(config.IsAutoExpandType());
}

const wchar_t* IUSDExportOptions::GetRootPrimPath() const
{
    return UsdStringToMaxString(USDSceneBuilderOptions::GetRootPrimPath().GetString()).data();
}

const wchar_t* IUSDExportOptions::GetBonesPrimName() const
{
    return UsdStringToMaxString(USDSceneBuilderOptions::GetBonesPrimName().GetString()).data();
}

void IUSDExportOptions::SetBonesPrimName(const wchar_t* bonesPrim)
{
    const std::string bonesPrimString = MaxStringToUsdString(bonesPrim);
    if (!pxr::TfIsValidIdentifier(bonesPrimString)) {
        const auto errorMsg = std::wstring(L"The bones prim name could not be set. This is not a "
                                           L"valid USD prim identifier : ")
                                  .append(bonesPrim);
        throw RuntimeError(errorMsg.c_str());
    }
    USDSceneBuilderOptions::SetBonesPrimName(pxr::TfToken(bonesPrimString));
}

const wchar_t* IUSDExportOptions::GetAnimationsPrimName() const
{
    return UsdStringToMaxString(USDSceneBuilderOptions::GetAnimationsPrimName().GetString()).data();
}

void IUSDExportOptions::SetAnimationsPrimName(const wchar_t* animationsPrim)
{
    const std::string animationsPrimString = MaxStringToUsdString(animationsPrim);
    if (!pxr::TfIsValidIdentifier(animationsPrimString)) {
        const auto errorMsg = std::wstring(L"The Animations prim name could not be set. This is "
                                           L"not a valid USD prim identifier : ")
                                  .append(animationsPrim);
        throw RuntimeError(errorMsg.c_str());
    }
    USDSceneBuilderOptions::SetAnimationsPrimName(pxr::TfToken(animationsPrimString));
}

void IUSDExportOptions::SetRootPrimPath(const wchar_t* rootPath)
{
    const std::wstring rootPathString = rootPath;
    const auto path = pxr::SdfPath(MaxUsd::MaxStringToUsdString(rootPathString.c_str()).c_str());
    if (!rootPathString.empty() && (!path.IsAbsolutePath() || !path.IsAbsoluteRootOrPrimPath())) {
        const auto errorMsg = std::wstring(L"The root prim path could not be set. This is not a "
                                           L"valid absolute USD prim path : ")
                                  .append(rootPath);
        throw RuntimeError(errorMsg.c_str());
    }
    USDSceneBuilderOptions::SetRootPrimPath(path);
}

#ifdef IS_MAX2024_OR_GREATER
void IUSDExportOptions::SetMtlSwitcherExportStyle(int exportStyle)
{
    const auto style = USDSceneBuilderOptions::MtlSwitcherExportStyle(exportStyle);
    // Validation.
    if (style != MtlSwitcherExportStyle::AsVariantSets
        && style != MtlSwitcherExportStyle::ActiveMaterialOnly) {
        WStr errorMsg(L"Incorrect MtlSwitcherExportStyle value. Accepted values are #asVariantSets "
                      L"or #activeMaterial.");
        throw RuntimeError(errorMsg.data());
    }
    USDSceneBuilderOptions::SetMtlSwitcherExportStyle(style);
}
#endif

FPInterfaceDesc* IUSDExportOptions::GetDesc() { return &IUSDExportOptionsDesc; }

void IUSDExportOptions::SetShadingMode(const wchar_t* shadingMode)
{
    BaseClass::SetShadingMode(pxr::TfToken(MaxUsd::MaxStringToUsdString(shadingMode)));
}

const wchar_t* IUSDExportOptions::GetShadingMode() const
{
    return (MaxUsd::UsdStringToMaxString(BaseClass::GetShadingMode())).data();
}

Tab<TSTR*> IUSDExportOptions::GetAllMaterialTargets() const
{
    Tab<TSTR*> materialArray;
    for (const auto& material : GetAllMaterialConversions()) {
        TSTR* name = new TSTR(MaxUsd::UsdStringToMaxString(material.GetString()).data());
        materialArray.Append(1, &name);
    }
    return materialArray;
}

void IUSDExportOptions::SetAllMaterialTargets(const Tab<TSTR*>& materialArray)
{
    std::set<pxr::TfToken> materialSet;
    pxr::TfTokenVector     availableConversions
        = pxr::MaxUsdShadingModeRegistry::ListMaterialConversions();
    for (int i = 0; i < materialArray.Count(); ++i) {
        auto elementToInsert = pxr::TfToken(MaxUsd::MaxStringToUsdString(materialArray[i]->data()));
        if (std::find(availableConversions.begin(), availableConversions.end(), elementToInsert)
            != availableConversions.end()) {
            materialSet.insert(elementToInsert);
        } else {
            const auto errorMsg = materialArray[i]->data()
                + std::wstring(L" is not a valid material target type. See available types with "
                               L"'AvailalbleMaterialTargets'.");
            throw RuntimeError(errorMsg.c_str());
        }
    }
    SetAllMaterialConversions(materialSet);
}

Tab<TSTR*> IUSDExportOptions::GetAvailableMaterialTargets() const
{
    Tab<TSTR*>         materialArray;
    pxr::TfTokenVector availableConversions
        = pxr::MaxUsdShadingModeRegistry::ListMaterialConversions();
    for (const auto& material : availableConversions) {
        auto const& info = pxr::MaxUsdShadingModeRegistry::GetMaterialConversionInfo(material);
        if (info.hasExporter) {
            auto mxsCopy = new TSTR(UsdStringToMaxString(material.GetString()));
            materialArray.Append(1, &mxsCopy);
        }
    }
    return materialArray;
}

Tab<TSTR*> IUSDExportOptions::GetAvailableChasers() const
{
    Tab<TSTR*> chaserArray;
    auto       chasers = pxr::MaxUsdExportChaserRegistry::GetAllRegisteredChasers();
    for (const auto& chaser : chasers) {
        auto mxsCopy = new TSTR(UsdStringToMaxString(chaser.GetString()));
        chaserArray.Append(1, &mxsCopy);
    }
    return chaserArray;
}

Tab<TSTR*> IUSDExportOptions::GetChaserNamesMxs() const
{
    Tab<TSTR*> chaserArray;
    for (const auto& chaserName : USDSceneBuilderOptions::GetChaserNames()) {
        auto mxsCopy = new TSTR(UsdStringToMaxString(chaserName));
        chaserArray.Append(1, &mxsCopy);
    }
    return chaserArray;
}

void IUSDExportOptions::SetChaserNamesMxs(const Tab<TSTR*>& chaserArray)
{
    std::vector<std::string> chaserNames;
    for (int i = 0; i < chaserArray.Count(); ++i) {
        chaserNames.push_back(MaxUsd::MaxStringToUsdString(chaserArray[i]->data()));
    }
    SetChaserNames(chaserNames);
}

void IUSDExportOptions::SetAllChaserArgs(Value* chaserArgsValue)
{
    auto allChaserArgs = USDSceneBuilderOptions::GetAllChaserArgs();

    if (is_dictionary(chaserArgsValue)) {
        MXSDictionaryValue* dict = dynamic_cast<MXSDictionaryValue*>(chaserArgsValue);
        Array*              chasers = dict->getKeys();

        for (int i = 0; i < chasers->size; ++i) {
            Value* chaserName = (*chasers)[i];
            Value* chaserArgs = dict->get(chaserName);
            if (!is_dictionary(chaserArgs)) {
                throw RuntimeError(
                    L"Badly formed dictionary entry. Expecting Dictionary to for arguments.");
            }

            ChaserArgs          args;
            MXSDictionaryValue* dictArgs = dynamic_cast<MXSDictionaryValue*>(chaserArgs);
            Array*              argKeys = dictArgs->getKeys();

            for (int j = 0; j < argKeys->size; ++j) {
                Value* argKey = (*argKeys)[j];
                Value* argValue = dictArgs->get(argKey);

                args[MaxUsd::MaxStringToUsdString(argKey->to_string())]
                    = MaxUsd::MaxStringToUsdString(argValue->to_string());
            }
            allChaserArgs[MaxStringToUsdString(chaserName->to_string())] = args;
        }
        USDSceneBuilderOptions::SetAllChaserArgs(allChaserArgs);
    } else if (is_array(chaserArgsValue)) {
        Array* argsArray = dynamic_cast<Array*>(chaserArgsValue);
        if (argsArray->size % 3) {
            throw RuntimeError(L"Badly formed Array. Expecting 3 elements per argument entry "
                               L"(<chaser>, <key>, <value>).");
        }
        for (int i = 0; i < argsArray->size; i = i + 3) {
            Value* chaserName = (*argsArray)[i];
            Value* argKey = (*argsArray)[i + 1];
            Value* argValue = (*argsArray)[i + 2];

            ChaserArgs& chaserArgs = allChaserArgs[MaxStringToUsdString(chaserName->to_string())];
            chaserArgs[MaxUsd::MaxStringToUsdString(argKey->to_string())]
                = MaxUsd::MaxStringToUsdString(argValue->to_string());
        }
        USDSceneBuilderOptions::SetAllChaserArgs(allChaserArgs);
    } else {
        throw RuntimeError(
            L"Invalid parameter type provided. Expecting a maxscript Dictionary or Array.");
    }
}

Value* IUSDExportOptions::GetAllChaserArgs()
{
    if (allChaserArgsMxsHolder.get_value() == nullptr) {
        allChaserArgsMxsHolder.set_value(new MXSDictionaryValue(MXSDictionaryValue::key_string));
    }

    ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;
    MAXScript_TLS*                   _tls = scopedMaxScriptEvaluationContext.Get_TLS();
    five_typed_value_locals_tls(
        MXSDictionaryValue * allChaserArgsDict,
        MXSDictionaryValue * argsDict,
        Value * chaserName,
        Value * argKey,
        Value * argValue)

        vl.allChaserArgsDict
        = dynamic_cast<MXSDictionaryValue*>(allChaserArgsMxsHolder.get_value());
    // remove the previous args if any
    vl.allChaserArgsDict->free();
    for (const auto& chaserArgs : USDSceneBuilderOptions::GetAllChaserArgs()) {
        vl.argsDict = new MXSDictionaryValue(MXSDictionaryValue::key_string);
        vl.chaserName = new String(MaxUsd::UsdStringToMaxString(chaserArgs.first).data());
        for (const auto& chaserArgsValue : chaserArgs.second) {
            vl.argKey = new String(MaxUsd::UsdStringToMaxString(chaserArgsValue.first).data());
            vl.argValue = new String(MaxUsd::UsdStringToMaxString(chaserArgsValue.second).data());
            vl.argsDict->put(vl.argKey, vl.argValue);
        }
        vl.allChaserArgsDict->put(vl.chaserName, vl.argsDict);
    }
    return allChaserArgsMxsHolder.get_value();
}

Tab<TSTR*> IUSDExportOptions::GetContextNamesMxs() const
{
    Tab<TSTR*> contextArray;
    for (const auto& contextName : USDSceneBuilderOptions::GetContextNames()) {
        auto mxsCopy = new TSTR(UsdStringToMaxString(contextName));
        contextArray.Append(1, &mxsCopy);
    }
    return contextArray;
}

void IUSDExportOptions::SetContextNamesMxs(const Tab<TSTR*>& contextArray)
{
    std::set<std::string> contextNames;
    for (int i = 0; i < contextArray.Count(); ++i) {
        contextNames.insert(MaxUsd::MaxStringToUsdString(contextArray[i]->data()));
    }
    SetContextNames(contextNames);
}

void IUSDExportOptions::SetMaterialLayerPath(const wchar_t* mtlPath)
{
    const auto& mtlPathStr = MaxUsd::MaxStringToUsdString(mtlPath);
    if (!MaxUsd::HasUnicodeCharacter(mtlPathStr)) {
        BaseClass::SetMaterialLayerPath(mtlPathStr);
    } else {
        throw RuntimeError(L"The path used is not valid. USD does not support unicode characters "
                           L"in its file paths.");
    }
}

const wchar_t* IUSDExportOptions::GetMaterialLayerPath() const
{
    return MaxUsd::UsdStringToMaxString(BaseClass::GetMaterialLayerPath()).data();
}

void IUSDExportOptions::SetMaterialPrimPath(const wchar_t* mtlPath)
{
    auto        str = MaxUsd::MaxStringToUsdString(mtlPath);
    std::string err;
    if (pxr::SdfPath::IsValidPathString(str, &err)) {
        pxr::SdfPath primPath(str);
        if (primPath.IsAbsoluteRootOrPrimPath()) {
            BaseClass::SetMaterialPrimPath(primPath);
        } else {
            throw RuntimeError(L"The path used is not valid.");
        }
    } else {
        err = "The path used is not valid : " + err;
        throw RuntimeError(UsdStringToMaxString(err));
    }
}

const wchar_t* IUSDExportOptions::GetMaterialPrimPath() const
{
    return MaxUsd::UsdStringToMaxString(BaseClass::GetMaterialPrimPath().GetAsString()).data();
}

const MCHAR* IUSDExportOptions::Serialize() const
{
    auto        jsonString = OptionUtils::SerializeOptionsToJson(*this);
    static WStr maxString;
    maxString = UsdStringToMaxString(jsonString);
    return maxString.data();
}

} // namespace MAXUSD_NS_DEF
