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
#include "wrapUSDSceneBuilderOptions.h"

#include "pythonObjectRegistry.h"

#include <MaxUsd/Builders/USDSceneBuilderOptions.h>
#include <MaxUsd/Utilities/Logging.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/OptionUtils.h>

#include <pxr/base/tf/api.h>
#include <pxr/base/tf/pyContainerConversions.h>
#include <pxr/base/tf/pyEnum.h>
#include <pxr/base/tf/pyPolymorphic.h>
#include <pxr/base/tf/pyResultConversions.h>
#include <pxr/base/tf/registryManager.h>

#include <QByteArray>
#include <boost/python/class.hpp>
#include <boost/python/return_value_policy.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

USDSceneBuilderOptionsWrapper::USDSceneBuilderOptionsWrapper(
    const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    SetOptions(exportArgs);
}

USDSceneBuilderOptionsWrapper::USDSceneBuilderOptionsWrapper(const std::string& json)
{
    const QByteArray jsonBytes(json.c_str());
    SetOptions(USDSceneBuilderOptions(MaxUsd::OptionUtils::DeserializeOptionsFromJson(jsonBytes)));
}

const TfToken& USDSceneBuilderOptionsWrapper::GetChannelPrimvarName(int channel) const
{
    // Will throw on unmapped channels.
    const auto& config = GetValidPrimvarConfig(channel);
    return config.GetPrimvarName();
}

void USDSceneBuilderOptionsWrapper::SetChannelPrimvarName(int channel, const pxr::TfToken& name)
{
    auto       meshConversionOptions = GetMeshConversionOptions();
    const auto current = meshConversionOptions.GetChannelPrimvarConfig(channel);
    MaxUsd::MappedAttributeBuilder::Config cfg { name,
                                                 current.GetPrimvarType(),
                                                 current.IsAutoExpandType() };
    meshConversionOptions.SetChannelPrimvarConfig(channel, cfg);
    SetMeshConversionOptions(meshConversionOptions);
}

MaxUsd::MappedAttributeBuilder::Type
USDSceneBuilderOptionsWrapper::GetChannelPrimvarType(int channel) const
{
    // Will throw on unmapped channels.
    const auto& config = GetValidPrimvarConfig(channel);
    return config.GetPrimvarType();
}

void USDSceneBuilderOptionsWrapper::SetChannelPrimvarType(
    int                                  channel,
    MaxUsd::MappedAttributeBuilder::Type type)
{
    auto       meshConversionOptions = GetMeshConversionOptions();
    const auto current = meshConversionOptions.GetChannelPrimvarConfig(channel);
    MaxUsd::MappedAttributeBuilder::Config cfg { current.GetPrimvarName(),
                                                 type,
                                                 current.IsAutoExpandType() };
    meshConversionOptions.SetChannelPrimvarConfig(channel, cfg);
    SetMeshConversionOptions(meshConversionOptions);
}

bool USDSceneBuilderOptionsWrapper::GetChannelPrimvarAutoExpandType(int channel) const
{
    // Will throw on unmapped channels.
    const auto& config = GetValidPrimvarConfig(channel);
    return config.IsAutoExpandType();
}

void USDSceneBuilderOptionsWrapper::SetChannelPrimvarAutoExpandType(int channel, bool autoExpand)
{
    auto       meshConversionOptions = GetMeshConversionOptions();
    const auto current = meshConversionOptions.GetChannelPrimvarConfig(channel);
    MaxUsd::MappedAttributeBuilder::Config cfg { current.GetPrimvarName(),
                                                 current.GetPrimvarType(),
                                                 autoExpand };
    meshConversionOptions.SetChannelPrimvarConfig(channel, cfg);
    SetMeshConversionOptions(meshConversionOptions);
}

bool USDSceneBuilderOptionsWrapper::GetBakeObjectOffsetTransform() const
{
    return GetMeshConversionOptions().GetBakeObjectOffsetTransform();
}

void USDSceneBuilderOptionsWrapper::SetBakeObjectOffsetTransform(bool bakeObjectOffset)
{
    auto meshConversionOptions = GetMeshConversionOptions();
    meshConversionOptions.SetBakeObjectOffsetTransform(bakeObjectOffset);
    SetMeshConversionOptions(meshConversionOptions);
}

bool USDSceneBuilderOptionsWrapper::GetPreserveEdgeOrientation() const
{
    return GetMeshConversionOptions().GetPreserveEdgeOrientation();
}
void USDSceneBuilderOptionsWrapper::SetPreserveEdgeOrientation(bool preserveEdgeOrientation)
{
    auto meshConversionOptions = GetMeshConversionOptions();
    meshConversionOptions.SetPreserveEdgeOrientation(preserveEdgeOrientation);
    SetMeshConversionOptions(meshConversionOptions);
}

std::string USDSceneBuilderOptionsWrapper::GetLogPath() const
{
    return GetLogOptions().path.u8string();
}

void USDSceneBuilderOptionsWrapper::SetLogPath(const std::string& logPath)
{
    auto logOptions = GetLogOptions();
    logOptions.path = logPath;
    SetLogOptions(logOptions);
}

MaxUsd::Log::Level USDSceneBuilderOptionsWrapper::GetLogLevel() const
{
    return GetLogOptions().level;
}

void USDSceneBuilderOptionsWrapper::SetLogLevel(MaxUsd::Log::Level logLevel)
{
    auto logOptions = GetLogOptions();
    logOptions.level = logLevel;
    SetLogOptions(logOptions);
}

const MaxUsd::MappedAttributeBuilder::Config
USDSceneBuilderOptionsWrapper::GetValidPrimvarConfig(int channel) const
{
    // Validation.
    if (!MaxUsd::IsValidChannel(channel)) {
        const auto errorMsg
            = std::to_string(channel)
            + std::string(
                  " is not a valid map channel. Valid channels are from -2 to 99 inclusively.");
        throw std::runtime_error(errorMsg.c_str());
    }
    return GetMeshConversionOptions().GetChannelPrimvarConfig(channel);
}

boost::python::dict USDSceneBuilderOptionsWrapper::GetAllChaserArgs() const
{
    boost::python::dict allChaserArgs;
    for (auto&& perChaser : USDSceneBuilderOptions::GetAllChaserArgs()) {
        auto perChaserDict = boost::python::dict();
        for (auto&& perItem : perChaser.second) {
            perChaserDict[perItem.first] = perItem.second;
        }
        allChaserArgs[perChaser.first] = perChaserDict;
    }
    return allChaserArgs;
}

void USDSceneBuilderOptionsWrapper::SetAllChaserArgsFromDict(boost::python::dict args)
{
    std::map<std::string, ChaserArgs> allArgs;
    try {
        auto items = args.items();
        for (boost::python::ssize_t i = 0; i < boost::python::len(items); ++i) {
            std::string chaserKey = boost::python::extract<std::string>(items[i][0]);

            ChaserArgs chaserArgs;

            auto paramDict = boost::python::dict { items[i][1] };

            auto params = paramDict.items();
            for (boost::python::ssize_t i = 0; i < boost::python::len(params); ++i) {
                std::string name = boost::python::extract<std::string>(params[i][0]);
                std::string value = boost::python::extract<std::string>(params[i][1]);
                chaserArgs.insert({ name, value });
            }
            allArgs.insert({ chaserKey, chaserArgs });
        }
    } catch (...) {
        throw std::invalid_argument(
            std::string("Badly formed dictionary. Expecting the form : {'chaser' : {'param' : "
                        "'val', 'param1' : 'val2'}, 'chaser2' : {'param2' : 'val3'}}."));
    }
    USDSceneBuilderOptions::SetAllChaserArgs(allArgs);
}

void USDSceneBuilderOptionsWrapper::SetAllChaserArgsFromList(boost::python::list args)
{
    static const std::string badArgMsg(
        "Badly formed list. Expecting 3 elements per argument entry (<chaser>, <key>, <value>).");

    if (boost::python::len(args) % 3) {
        throw std::invalid_argument(badArgMsg);
    }

    std::map<std::string, ChaserArgs> allArgs;
    try {

        for (int i = 0; i < len(args); i = i + 3) {
            const std::string chaser = boost::python::extract<std::string>(args[i]);
            const std::string param = boost::python::extract<std::string>(args[i + 1]);
            const std::string value = boost::python::extract<std::string>(args[i + 2]);
            ChaserArgs&       chaserArgs = allArgs[chaser];
            chaserArgs[param] = value;
        }
    } catch (...) {
        throw std::invalid_argument(badArgMsg);
    }
    MaxUsd::USDSceneBuilderOptions::SetAllChaserArgs(allArgs);
}

void USDSceneBuilderOptionsWrapper::SetAllMaterialConversions(
    const std::set<std::string>& materialConversions)
{
    std::set<pxr::TfToken> conversionTokens;
    for (const auto& conv : materialConversions) {
        conversionTokens.insert(pxr::TfToken(conv));
    }
    MaxUsd::USDSceneBuilderOptions::SetAllMaterialConversions(conversionTokens);
}

std::string USDSceneBuilderOptionsWrapper::GetMaterialLayerPath() const
{
    return MaxUsd::USDSceneBuilderOptions::GetMaterialLayerPath();
}

std::string USDSceneBuilderOptionsWrapper::GetMaterialPrimPath() const
{
    return MaxUsd::USDSceneBuilderOptions::GetMaterialPrimPath().GetAsString();
}

std::string USDSceneBuilderOptionsWrapper::Serialize()
{
    return MaxUsd::OptionUtils::SerializeOptionsToJson(*this);
}

TF_REGISTRY_FUNCTION(TfEnum)
{
    TF_ADD_ENUM_NAME(MaxUsd::MappedAttributeBuilder::Type::Color3fArray);
    TF_ADD_ENUM_NAME(MaxUsd::MappedAttributeBuilder::Type::FloatArray);
    TF_ADD_ENUM_NAME(MaxUsd::MappedAttributeBuilder::Type::Float2Array);
    TF_ADD_ENUM_NAME(MaxUsd::MappedAttributeBuilder::Type::Float3Array);
    TF_ADD_ENUM_NAME(MaxUsd::MappedAttributeBuilder::Type::TexCoord2fArray);
    TF_ADD_ENUM_NAME(MaxUsd::MappedAttributeBuilder::Type::TexCoord3fArray);

    TF_ADD_ENUM_NAME(MaxUsd::MaxMeshConversionOptions::NormalsMode::None);
    TF_ADD_ENUM_NAME(MaxUsd::MaxMeshConversionOptions::NormalsMode::AsAttribute);
    TF_ADD_ENUM_NAME(MaxUsd::MaxMeshConversionOptions::NormalsMode::AsPrimvar);

    TF_ADD_ENUM_NAME(MaxUsd::MaxMeshConversionOptions::MeshFormat::FromScene);
    TF_ADD_ENUM_NAME(MaxUsd::MaxMeshConversionOptions::MeshFormat::TriMesh);
    TF_ADD_ENUM_NAME(MaxUsd::MaxMeshConversionOptions::MeshFormat::PolyMesh);

    TF_ADD_ENUM_NAME(MaxUsd::Log::Level::Off);
    TF_ADD_ENUM_NAME(MaxUsd::Log::Level::Error);
    TF_ADD_ENUM_NAME(MaxUsd::Log::Level::Warn);
    TF_ADD_ENUM_NAME(MaxUsd::Log::Level::Info);

    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::ContentSource::RootNode);
    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::ContentSource::Selection);
    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::ContentSource::NodeList);

    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::UpAxis::Y);
    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::UpAxis::Z);

    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::FileFormat::Binary);
    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::FileFormat::ASCII);

    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::TimeMode::AnimationRange);
    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::TimeMode::CurrentFrame);
    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::TimeMode::ExplicitFrame);
    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::TimeMode::FrameRange);

#ifdef IS_MAX2024_OR_GREATER
    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::AsVariantSets);
    TF_ADD_ENUM_NAME(MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle::ActiveMaterialOnly);
#endif
}

void wrapUsdSceneBuilderOptions()
{
    using namespace boost::python;

    TfPyWrapEnum<MaxUsd::MappedAttributeBuilder::Type>("PrimvarType");
    TfPyWrapEnum<MaxUsd::MaxMeshConversionOptions::NormalsMode>("NormalsMode");
    TfPyWrapEnum<MaxUsd::MaxMeshConversionOptions::MeshFormat>("MeshFormat");
    TfPyWrapEnum<MaxUsd::USDSceneBuilderOptions::ContentSource>("ContentSource");
    TfPyWrapEnum<MaxUsd::USDSceneBuilderOptions::UpAxis>("UpAxis");
    TfPyWrapEnum<MaxUsd::USDSceneBuilderOptions::FileFormat>("FileFormat");
    TfPyWrapEnum<MaxUsd::USDSceneBuilderOptions::TimeMode>("TimeMode");
#ifdef IS_MAX2024_OR_GREATER
    TfPyWrapEnum<MaxUsd::USDSceneBuilderOptions::MtlSwitcherExportStyle>("MtlSwitcherExportStyle");
#endif
    TfPyWrapEnum<MaxUsd::Log::Level>("LogLevel");

    boost::python::class_<USDSceneBuilderOptionsWrapper> c(
        "USDSceneBuilderOptions",
        "The class USDSceneBuilderOptions which exposes the export arguments from the current "
        "export context.");
    c.def(init<const USDSceneBuilderOptionsWrapper&>())
        .def(init<const std::string&>())
        .def(
            "GetContentSource",
            &MaxUsd::USDSceneBuilderOptions::GetContentSource,
            (boost::python::arg("self")),
            "Gets the 3ds Max content source from which to build the USD scene.")
        .def(
            "SetContentSource",
            &MaxUsd::USDSceneBuilderOptions::SetContentSource,
            (boost::python::args("self", "contentSource")),
            "Sets the 3ds Max content source from which to build the USD scene.")
        .def(
            "GetAllMaterialConversions",
            &MaxUsd::USDSceneBuilderOptions::GetAllMaterialConversions,
            return_value_policy<pxr::TfPySequenceToSet>(),
            (boost::python::arg("self")),
            "Gets the set of targeted materials for material conversion.")
        .def(
            "SetAllMaterialConversions",
            &USDSceneBuilderOptionsWrapper::SetAllMaterialConversions,
            return_value_policy<pxr::TfPySequenceToSet>(),
            (boost::python::args("self", "materialConversions")),
            "Sets the set of targeted materials for material conversion.")
        .def(
            "GetShadingMode",
            &MaxUsd::USDSceneBuilderOptions::GetShadingMode,
            return_value_policy<return_by_value>(),
            (boost::python::arg("self")),
            "Gets the shading schema (mode) to use for material export.")
        .def(
            "SetShadingMode",
            &MaxUsd::USDSceneBuilderOptions::SetShadingMode,
            return_value_policy<return_by_value>(),
            (boost::python::args("self", "shadingMode")),
            "Sets the shading schema (mode) to use for material export.")
        .def(
            "GetConvertMaterialsTo",
            &MaxUsd::USDSceneBuilderOptions::GetConvertMaterialsTo,
            return_value_policy<return_by_value>(),
            (boost::python::arg("self")),
            "Returns a token identifier of the USD material type targeted to convert the 3ds Max "
            "materials (to which USD material are we exporting to).")
        .def(
            "GetTranslateMeshes",
            &MaxUsd::USDSceneBuilderOptions::GetTranslateMeshes,
            (boost::python::arg("self")),
            "Check if 3ds Max meshes should be translated into USD meshes.")
        .def(
            "SetTranslateMeshes",
            &MaxUsd::USDSceneBuilderOptions::SetTranslateMeshes,
            (boost::python::args("self", "translateMeshes")),
            "Sets whether 3ds Max meshes should be translated into USD meshes.")
        .def(
            "GetTranslateShapes",
            &MaxUsd::USDSceneBuilderOptions::GetTranslateShapes,
            (boost::python::arg("self")),
            "Check if 3ds Max shapes should be translated into USD meshes.")
        .def(
            "SetTranslateShapes",
            &MaxUsd::USDSceneBuilderOptions::SetTranslateShapes,
            (boost::python::args("self", "translateShapes")),
            "Sets whether 3ds Max shapes should be translated into USD meshes.")
        .def(
            "GetTranslateLights",
            &MaxUsd::USDSceneBuilderOptions::GetTranslateLights,
            (boost::python::arg("self")),
            "Check if 3ds Max lights should be translated into USD lights.")
        .def(
            "SetTranslateLights",
            &MaxUsd::USDSceneBuilderOptions::SetTranslateLights,
            (boost::python::args("self", "translateLights")),
            "Sets whether 3ds Max lights should be translated into USD lights.")
        .def(
            "GetTranslateCameras",
            &MaxUsd::USDSceneBuilderOptions::GetTranslateCameras,
            (boost::python::arg("self")),
            "Check if 3ds Max cameras should be translated into USD cameras.")
        .def(
            "SetTranslateCameras",
            &MaxUsd::USDSceneBuilderOptions::SetTranslateCameras,
            (boost::python::args("self", "translateCameras")),
            "Sets whether 3ds Max cameras should be translated into USD cameras.")
        .def(
            "GetTranslateMaterials",
            &MaxUsd::USDSceneBuilderOptions::GetTranslateMaterials,
            (boost::python::arg("self")),
            "Check if materials should be translated.")
        .def(
            "SetTranslateMaterials",
            &MaxUsd::USDSceneBuilderOptions::SetTranslateMaterials,
            (boost::python::args("self", "translateMaterials")),
            "Sets whether materials should be translated.")
        .def(
            "GetTranslateSkin",
            &MaxUsd::USDSceneBuilderOptions::GetTranslateSkin,
            (boost::python::arg("self")),
            "Check if skin and skeletons should be translated.")
        .def(
            "SetTranslateSkin",
            &MaxUsd::USDSceneBuilderOptions::SetTranslateSkin,
            (boost::python::args("self", "translateSkin")),
            "Sets whether skin and skeletons should be translated.")
        .def(
            "GetTranslateMorpher",
            &MaxUsd::USDSceneBuilderOptions::GetTranslateMorpher,
            (boost::python::arg("self")),
            "Check if morpher modifiers should be translated.")
        .def(
            "SetTranslateMorpher",
            &MaxUsd::USDSceneBuilderOptions::SetTranslateMorpher,
            (boost::python::args("self", "translateMorpher")),
            "Sets whether morpher modifiers should be translated.")
        .def(
            "GetChannelPrimvarType",
            &USDSceneBuilderOptionsWrapper::GetChannelPrimvarType,
            (boost::python::args("self", "channel")),
            "Gets the primvar type associated with a given max channel on export "
            "(maxUsd.MappedAttributeBuilder.Type).")
        .def(
            "SetChannelPrimvarType",
            &USDSceneBuilderOptionsWrapper::SetChannelPrimvarType,
            (boost::python::args("self", "channel", "type")),
            "Sets the primvar type associated with a given max channel on export "
            "(maxUsd.MappedAttributeBuilder.Type).")
        .def(
            "GetChannelPrimvarName",
            &USDSceneBuilderOptionsWrapper::GetChannelPrimvarName,
            boost::python::return_value_policy<boost::python::return_by_value>(),
            (boost::python::arg("self"), boost::python::arg("channel")),
            "Gets the primvar name of a given channel.")
        .def(
            "SetChannelPrimvarName",
            &USDSceneBuilderOptionsWrapper::SetChannelPrimvarName,
            boost::python::return_value_policy<boost::python::return_by_value>(),
            (boost::python::arg("self"), boost::python::args("channel", "name")),
            "Sets the primvar name associated with a given map channel.")
        .def(
            "GetChannelPrimvarAutoExpandType",
            &USDSceneBuilderOptionsWrapper::GetChannelPrimvarAutoExpandType,
            (boost::python::arg("self")),
            "Gets whether to auto-expand the primvar type based on the data.")
        .def(
            "SetChannelPrimvarAutoExpandType",
            &USDSceneBuilderOptionsWrapper::SetChannelPrimvarAutoExpandType,
            (boost::python::args("self", "autoExpandType")),
            "Sets whether to auto-expand the primvar type based on the data.")
        .def(
            "GetUsdStagesAsReferences",
            &MaxUsd::USDSceneBuilderOptions::GetUsdStagesAsReferences,
            (boost::python::arg("self")),
            "Checks if USD Stage Objects should be exported as USD References.")
        .def(
            "SetUsdStagesAsReferences",
            &MaxUsd::USDSceneBuilderOptions::SetUsdStagesAsReferences,
            (boost::python::args("self", "UsdStagesAsReferences")),
            "Sets whether USD Stage Objects should be exported as USD References.")
        .def(
            "GetTranslateHidden",
            &MaxUsd::USDSceneBuilderOptions::GetTranslateHidden,
            (boost::python::arg("self")),
            "Check if hidden objects should be translated.")
        .def(
            "SetTranslateHidden",
            &MaxUsd::USDSceneBuilderOptions::SetTranslateHidden,
            (boost::python::args("self", "translateHidden")),
            "Sets whether hidden objects should be translated.")
        .def(
            "GetUseUSDVisibility",
            &MaxUsd::USDSceneBuilderOptions::GetUseUSDVisibility,
            (boost::python::arg("self")),
            "Check if we should attempt to match the Hidden state in Max with the USD visibility "
            "attribute.")
        .def(
            "SetUseUSDVisibility",
            &MaxUsd::USDSceneBuilderOptions::SetUseUSDVisibility,
            (boost::python::args("self", "useUSDVisibility")),
            "Sets whether we should attempt to match the Hidden state in Max with the USD "
            "visibility attribute.")
        .def(
            "GetAllowNestedGprims",
            &MaxUsd::USDSceneBuilderOptions::GetAllowNestedGprims,
            (boost::python::arg("self")),
            "Check if the exporter is allowed to nest Gprims. While technically illegal, nesting "
            "Gprims may still work in many cases while improving scene performance by limiting the "
            "number of Prims.")
        .def(
            "SetAllowNestedGprims",
            &MaxUsd::USDSceneBuilderOptions::SetAllowNestedGprims,
            (boost::python::args("self", "allowNestedGprims")),
            "Sets if the exporter is allowed to nest Gprims. While technically illegal, nesting "
            "Gprims may still work in many cases while improving scene performance by limiting the "
            "number of Prims.")
        .def(
            "GetFileFormat",
            &MaxUsd::USDSceneBuilderOptions::GetFileFormat,
            (boost::python::arg("self")),
            "Returns the format of the file to export (maxUsd.FileFormat).")
        .def(
            "SetFileFormat",
            &MaxUsd::USDSceneBuilderOptions::SetFileFormat,
            (boost::python::args("self", "fileFormat")),
            "Sets the format of the file to export (maxUsd.FileFormat).")
        .def(
            "GetNormalsMode",
            &USDSceneBuilderOptionsWrapper::GetNormalsMode,
            (boost::python::arg("self")),
            "Returns how normals should be exported (maxUsd.NormalsMode).")
        .def(
            "SetNormalsMode",
            &USDSceneBuilderOptionsWrapper::SetNormalsMode,
            (boost::python::args("self", "normalsMode")),
            "Sets normals should be exported (maxUsd.NormalsMode).")
        .def(
            "GetMeshFormat",
            &MaxUsd::USDSceneBuilderOptions::GetMeshFormat,
            (boost::python::arg("self")),
            "Returns how meshes should be exported (maxUsd.MeshFormat).")
        .def(
            "SetMeshFormat",
            &MaxUsd::USDSceneBuilderOptions::SetMeshFormat,
            (boost::python::args("self", "meshFormat")),
            "Sets how meshes should be exported (maxUsd.MeshFormat).")
        .def(
            "GetTimeMode",
            &MaxUsd::USDSceneBuilderOptions::GetTimeMode,
            (boost::python::arg("self")),
            "Gets the time mode to be used for export (maxUsd.TimeMode).")
        .def(
            "SetTimeMode",
            &MaxUsd::USDSceneBuilderOptions::SetTimeMode,
            (boost::python::arg("self")),
            "Sets the time mode to be used for export (maxUsd.TimeMode).")
        .def(
            "GetStartFrame",
            &MaxUsd::USDSceneBuilderOptions::GetStartFrame,
            (boost::python::arg("self")),
            "Gets the first frame from which to export, only used if the time mode is configured "
            "as ExplicitFrame or FrameRange.")
        .def(
            "SetStartFrame",
            &MaxUsd::USDSceneBuilderOptions::SetStartFrame,
            (boost::python::arg("self")),
            "Sets the first frame from which to export, only used if the time mode is configured "
            "as ExplicitFrame or FrameRange.")
        .def(
            "GetEndFrame",
            &MaxUsd::USDSceneBuilderOptions::GetEndFrame,
            (boost::python::arg("self")),
            "Gets the last frame from which to export, only used if the time mode is configured as "
            "FrameRange.")
        .def(
            "SetEndFrame",
            &MaxUsd::USDSceneBuilderOptions::SetEndFrame,
            (boost::python::arg("self")),
            "Sets the last frame from which to export, only used if the time mode is configured as "
            "FrameRange.")
        .def(
            "GetSamplesPerFrame",
            &MaxUsd::USDSceneBuilderOptions::GetSamplesPerFrame,
            (boost::python::arg("self")),
            "Gets the number of samples to be exported to USD, per frame.")
        .def(
            "SetSamplesPerFrame",
            &MaxUsd::USDSceneBuilderOptions::SetSamplesPerFrame,
            (boost::python::arg("self")),
            "Sets the number of samples to be exported to USD, per frame.")
        .def(
            "GetUpAxis",
            &MaxUsd::USDSceneBuilderOptions::GetUpAxis,
            (boost::python::arg("self")),
            "Returns the \"up axis\" of the USD Stage produced from the translation of the 3ds Max "
            "content (maxUsd.UpAxis).")
        .def(
            "SetUpAxis",
            &MaxUsd::USDSceneBuilderOptions::SetUpAxis,
            (boost::python::args("self", "upAxis")),
            "Sets the \"up axis\" of the USD Stage produced from the translation of the 3ds Max "
            "content (maxUsd.UpAxis).")
        .def(
            "GetBakeObjectOffsetTransform",
            &USDSceneBuilderOptionsWrapper::GetBakeObjectOffsetTransform,
            (boost::python::arg("self")),
            "Gets whether or not the Object - offset transform should be baked into the geometry.")
        .def(
            "SetBakeObjectOffsetTransform",
            &USDSceneBuilderOptionsWrapper::SetBakeObjectOffsetTransform,
            (boost::python::args("self", "bakeObjectOffsetTransform")),
            "Sets whether or not the Object - offset transform should be baked into the geometry.")
        .def(
            "GetPreserveEdgeOrientation",
            &USDSceneBuilderOptionsWrapper::GetPreserveEdgeOrientation,
            (boost::python::arg("self")),
            "Gets whether or not to preserve max edge orientation.")
        .def(
            "SetPreserveEdgeOrientation",
            &USDSceneBuilderOptionsWrapper::SetPreserveEdgeOrientation,
            (boost::python::args("self", "preserveEdgeOrientation")),
            "Sets whether or not to preserve max edge orientation.")
        .def(
            "GetRootPrimPath",
            &MaxUsd::USDSceneBuilderOptions::GetRootPrimPath,
            return_value_policy<return_by_value>(),
            (boost::python::arg("self")),
            "Gets the configured root prim path")
        .def(
            "SetRootPrimPath",
            &MaxUsd::USDSceneBuilderOptions::SetRootPrimPath,
            return_value_policy<return_by_value>(),
            (boost::python::args("self", "rootPrimPath")),
            "Sets the configured root prim path")
        .def(
            "GetLogPath",
            &USDSceneBuilderOptionsWrapper::GetLogPath,
            (boost::python::arg("self")),
            "Gets the path to the log file.")
        .def(
            "SetLogPath",
            &USDSceneBuilderOptionsWrapper::SetLogPath,
            (boost::python::args("self", "logPath")),
            "Sets the path to the log file.")
        .def(
            "GetLogLevel",
            &USDSceneBuilderOptionsWrapper::GetLogLevel,
            (boost::python::arg("self")),
            "Gets the log level (maxUsd.Log.Level).")
        .def(
            "SetLogLevel",
            &USDSceneBuilderOptionsWrapper::SetLogLevel,
            (boost::python::args("self", "logLevel")),
            "Sets the log level (maxUsd.Log.Level).")
        .def(
            "GetOpenInUsdview",
            &MaxUsd::USDSceneBuilderOptions::GetOpenInUsdview,
            (boost::python::arg("self")),
            "Check if the produced USD file should be opened in USDVIEW at the end of the export.")
        .def(
            "SetOpenInUsdview",
            &MaxUsd::USDSceneBuilderOptions::SetOpenInUsdview,
            (boost::python::args("self", "openInUsdView")),
            "Sets whether if the produced USD file should be opened in USDVIEW at the end of the "
            "export.")
        .def(
            "GetChaserNames",
            &MaxUsd::USDSceneBuilderOptions::GetChaserNames,
            return_value_policy<TfPySequenceToSet>(),
            (boost::python::arg("self")),
            "Gets the list of export chasers to be called at USD export.")
        .def(
            "SetChaserNames",
            &MaxUsd::USDSceneBuilderOptions::SetChaserNames,
            return_value_policy<TfPySequenceToSet>(),
            (boost::python::args("self", "chaserNames")),
            "Sets the list of export chasers to be called at USD export.")
        .def(
            "GetAllChaserArgs",
            &USDSceneBuilderOptionsWrapper::GetAllChaserArgs,
            (boost::python::arg("self")),
            "Gets the dictionary of export chasers with their specified arguments.")
        .def(
            "SetAllChaserArgs",
            &USDSceneBuilderOptionsWrapper::SetAllChaserArgsFromDict,
            (boost::python::args("self", "allChaserArgs")),
            "Sets the dictionary of export chasers with their specified arguments, from a "
            "dictionary.")
        .def(
            "SetAllChaserArgs",
            &USDSceneBuilderOptionsWrapper::SetAllChaserArgsFromList,
            (boost::python::args("self", "allChaserArgs")),
            "Sets the dictionary of export chasers with their specified arguments, from a list.")
        .def(
            "GetContextNames",
            &MaxUsd::USDSceneBuilderOptions::GetContextNames,
            return_value_policy<TfPySequenceToSet>(),
            (boost::python::arg("self")),
            "Gets the list of export contexts being used at USD export.")
        .def(
            "SetContextNames",
            &MaxUsd::USDSceneBuilderOptions::SetContextNames,
            (boost::python::args("self", "contexts")),
            "Sets the list of export contexts being used at USD export.")
#ifdef IS_MAX2024_OR_GREATER
        .def(
            "GetMtlSwitcherExportStyle",
            &MaxUsd::USDSceneBuilderOptions::GetMtlSwitcherExportStyle,
            (boost::python::arg("self")),
            "Gets the Material Switcher export style to be used for export "
            "(maxUsd.MtlSwitcherExportStyle).")
        .def(
            "SetMtlSwitcherExportStyle",
            &MaxUsd::USDSceneBuilderOptions::SetMtlSwitcherExportStyle,
            (boost::python::args("self", "exportStyle")),
            "Sets the Material Switcher export style to be used for export "
            "(maxUsd.MtlSwitcherExportStyle).")
#endif
        .def(
            "GetUseProgressBar",
            &MaxUsd::USDSceneBuilderOptions::GetUseProgressBar,
            (boost::python::arg("self")),
            "Check if the 3ds Max progress bar should be used during export.")
        .def(
            "SetUseProgressBar",
            &MaxUsd::USDSceneBuilderOptions::SetUseProgressBar,
            (boost::python::args("self", "useProgressBar")),
            "Sets if the 3ds Max progress bar should be used during export.")
        .def(
            "GetMaterialLayerPath",
            &USDSceneBuilderOptionsWrapper::GetMaterialLayerPath,
            (boost::python::arg("self")),
            "Gets the path used for the Material Layer.")
        .def(
            "SetMaterialLayerPath",
            &MaxUsd::USDSceneBuilderOptions::SetMaterialLayerPath,
            (boost::python::args("self", "matLayerPath")),
            "Sets the path used for the Material Layer.")
        .def(
            "GetUseSeparateMaterialLayer",
            &MaxUsd::USDSceneBuilderOptions::GetUseSeparateMaterialLayer,
            (boost::python::arg("self")),
            "Check if material should be exported to a separate layer.")
        .def(
            "SetUseSeparateMaterialLayer",
            &MaxUsd::USDSceneBuilderOptions::SetUseSeparateMaterialLayer,
            (boost::python::args("self", "useSeparateMaterialLayer")),
            "Sets if material should be exported to a separate layer.")
        .def(
            "GetMaterialPrimPath",
            &USDSceneBuilderOptionsWrapper::GetMaterialPrimPath,
            (boost::python::arg("self")),
            "Gets the prim path where materials are exported to.")
        .def(
            "SetMaterialPrimPath",
            &MaxUsd::USDSceneBuilderOptions::SetMaterialPrimPath,
            (boost::python::args("self", "matPrimPath")),
            "Sets the prim path to export materials to.")
        .def(
            "GetUseLastResortUSDPreviewSurfaceWriter",
            &MaxUsd::USDSceneBuilderOptions::GetUseLastResortUSDPreviewSurfaceWriter,
            (boost::python::arg("self")),
            "Checks if the USD Preview Surface Material target should use the last resort shader "
            "writer if no writer can handle the conversion from a material type to "
            "UsdPreviewSurface,"
            "the last resort writer will just look at the Diffuse color of the material, which is "
            "part of the base material interface, and setup a UsdPreviewSurface with that diffuse "
            "color.")
        .def(
            "SetUseLastResortUSDPreviewSurfaceWriter",
            &MaxUsd::USDSceneBuilderOptions::SetUseLastResortUSDPreviewSurfaceWriter,
            (boost::python::args("self", "useLastResortUSDFallbackMaterial")),
            "Sets if the USD Preview Surface Material target should use the last resort shader "
            "writer if no writer can handle the conversion from a material type to "
            "UsdPreviewSurface,"
            "the last resort writer will just look at the Diffuse color of the material, which is "
            "part of the base material interface, and setup a UsdPreviewSurface with that diffuse "
            "color.")
        .def(
            "SetDefaults",
            &MaxUsd::USDSceneBuilderOptions::SetDefaults,
            (boost::python::arg("self")),
            "Sets default options.")
        .def(
            "GetJobContextOptions",
            &MaxUsd::USDSceneBuilderOptions::GetJobContextOptions,
            return_value_policy<TfPyMapToDictionary>(),
            (boost::python::arg("self"), boost::python::arg("jobContext")),
            "Gets the job context options for the given job context.")
        .def(
            "GetAnimationsPrimName",
            &MaxUsd::USDSceneBuilderOptions::GetAnimationsPrimName,
            return_value_policy<return_by_value>(),
            (boost::python::arg("self")),
            "Gets the name of the prim that will contain the animations.")
        .def(
            "SetAnimationsPrimName",
            &MaxUsd::USDSceneBuilderOptions::SetAnimationsPrimName,
            return_value_policy<return_by_value>(),
            (boost::python::args("self", "animationsPrimName")),
            "Sets the name of the prim that will contain the animations.")
        .def(
            "GetBonesPrimName",
            &MaxUsd::USDSceneBuilderOptions::GetBonesPrimName,
            return_value_policy<return_by_value>(),
            (boost::python::arg("self")),
            "Gets the name of the prim that will contain the bones.")
        .def(
            "SetBonesPrimName",
            &MaxUsd::USDSceneBuilderOptions::SetBonesPrimName,
            return_value_policy<return_by_value>(),
            (boost::python::args("self", "bonesPrimName")),
            "Sets the name of the prim that will contain the bones.")
        .def(
            "Serialize",
            &USDSceneBuilderOptionsWrapper::Serialize,
            boost::python::arg("self"),
            "Serialize the options to JSON format");
}