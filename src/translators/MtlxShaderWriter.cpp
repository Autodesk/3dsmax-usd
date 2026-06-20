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
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#ifdef IS_MAX2025_OR_GREATER
#include <MaxUsd/Translators/ShaderWriter.h>
#include <MaxUsd/Translators/ShaderWriterRegistry.h>
#include <MaxUsd/Utilities/ListenerUtils.h>

#include <usdufe/base/tokens.h>

#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>
#include <pxr/usd/usdMtlx/utils.h>
#if PXR_VERSION > 2411
#include <pxr/usd/usdMtlx/tokens.h>
#endif
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/nodeGraph.h>
#include <pxr/usd/usdUI/nodeGraphNodeAPI.h>

#include <maxscript/maxscript.h>
#include <maxscript/util/listener.h>

#include <MaterialXFormat/XmlIo.h>

#include <cmath>
#include <sstream>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// Returns the static float value of a MaterialX input if it is not connected.
// On a connection (node/nodegraph/output), reports the input as "not statically known".
bool _TryGetStaticFloat(const MaterialX::InputPtr& input, float& outValue)
{
    if (!input) {
        return false;
    }
    if (input->hasNodeName() || input->hasNodeGraphString() || input->hasOutputString()) {
        return false;
    }
    const std::string& valStr = input->getValueString();
    if (valStr.empty()) {
        return false;
    }
    try {
        outValue = std::stof(valStr);
    } catch (...) {
        return false;
    }
    return true;
}

// Parses a MaterialX color3 value string ("r, g, b" or "r g b") when the
// input is not connected. Tolerates commas, semicolons, and whitespace
// between components so we read whatever the MaterialX serializer wrote.
// Returns false on a connection, on a value string with fewer/more than 3
// numeric components, or on any parse failure.
bool _TryGetStaticColor3(
    const MaterialX::InputPtr& input,
    float (&outValue)[3])
{
    if (!input) {
        return false;
    }
    if (input->hasNodeName() || input->hasNodeGraphString() || input->hasOutputString()) {
        return false;
    }
    const std::string& valStr = input->getValueString();
    if (valStr.empty()) {
        return false;
    }
    // Replace any non-numeric separators with spaces so a single istringstream
    // walk extracts the three components regardless of MaterialX serialization
    // style.
    std::string normalized;
    normalized.reserve(valStr.size());
    for (char c : valStr) {
        if (c == ',' || c == ';' || c == '\t' || c == '\n') {
            normalized.push_back(' ');
        } else {
            normalized.push_back(c);
        }
    }
    std::istringstream iss(normalized);
    int parsed = 0;
    for (int i = 0; i < 3; ++i) {
        if (!(iss >> outValue[i])) {
            return false;
        }
        ++parsed;
    }
    // Reject trailing tokens (e.g. a fourth component) so we don't silently
    // accept color4 strings as color3.
    std::string trailing;
    if (iss >> trailing) {
        return false;
    }
    return parsed == 3;
}

// MAX-MAT-002 workaround: 3ds Max's MtlxIOUtil bridge emits emission = 1.0
// paired with emission_color = (0, 0, 0) on every standard_surface node,
// regardless of whether the source PhysicalMaterial authored any emission.
// The MaterialX standard_surface nodedef defaults are emission = 0.0 and
// emission_color = (1, 1, 1). The buggy pair multiplies to (0, 0, 0) so it
// has no visual effect, but it is semantically wrong: any downstream
// override that bumps emission_color away from black would unexpectedly
// turn emission on at full strength. Strip both inputs when they exactly
// match the buggy pattern so the exported USD falls back to the nodedef
// defaults (which also evaluate to zero emission, with the right meaning).
// Connected inputs and any non-matching values are left untouched.
void _NormalizeStandardSurfaceEmissionDefault(const MaterialX::DocumentPtr& doc)
{
    if (!doc) {
        return;
    }
    for (const auto& node : doc->getNodes("standard_surface")) {
        auto emissionInput = node->getInput("emission");
        auto emissionColorInput = node->getInput("emission_color");
        if (!emissionInput || !emissionColorInput) {
            continue;
        }
        float emissionValue = 0.f;
        if (!_TryGetStaticFloat(emissionInput, emissionValue)) {
            continue;
        }
        if (std::fabs(emissionValue - 1.0f) > 1e-6f) {
            continue;
        }
        float emissionColor[3] = { 0.f, 0.f, 0.f };
        if (!_TryGetStaticColor3(emissionColorInput, emissionColor)) {
            continue;
        }
        if (std::fabs(emissionColor[0]) > 1e-6f
            || std::fabs(emissionColor[1]) > 1e-6f
            || std::fabs(emissionColor[2]) > 1e-6f) {
            continue;
        }
        node->removeInput("emission");
        node->removeInput("emission_color");
    }
}

// MAX-MAT-001 workaround: 3ds Max's MtlxIOUtil bridge emits
// specular_rotation = 0.25 on every standard_surface node, regardless of the
// source PhysicalMaterial's anisotropy settings. The MaterialX standard_surface
// nodedef defaults specular_rotation to 0.0, and the value has no visual effect
// when specular_anisotropy is zero. Strip the spurious value so the exported
// USD matches the nodedef default for the common (anisotropy == 0) case.
// Inputs that are connected, non-zero, or carry a non-0.25 authored value are
// left untouched.
void _NormalizeStandardSurfaceSpecularRotation(const MaterialX::DocumentPtr& doc)
{
    if (!doc) {
        return;
    }
    for (const auto& node : doc->getNodes("standard_surface")) {
        auto rotationInput = node->getInput("specular_rotation");
        if (!rotationInput) {
            continue;
        }
        float rotationValue = 0.f;
        if (!_TryGetStaticFloat(rotationInput, rotationValue)) {
            continue;
        }
        // Match the buggy 3ds Max hardcoded value (0.25), tolerant of float noise.
        if (std::fabs(rotationValue - 0.25f) > 1e-6f) {
            continue;
        }
        // Only strip when specular_anisotropy is provably zero (or absent).
        // If anisotropy is connected, the user may genuinely want a rotation,
        // so we conservatively keep the input.
        auto anisotropyInput = node->getInput("specular_anisotropy");
        if (anisotropyInput) {
            float anisotropyValue = 0.f;
            if (!_TryGetStaticFloat(anisotropyInput, anisotropyValue)) {
                continue;
            }
            if (std::fabs(anisotropyValue) > 1e-6f) {
                continue;
            }
        }
        node->removeInput("specular_rotation");
    }
}

} // namespace

bool _IsWellFormedPath(const fs::path& p)
{
    try {
        // Attempt to create an absolute path to check its validity
        fs::path absPath = fs::absolute(p);
        return true;
    } catch (const fs::filesystem_error&) {
        return false;
    }
}

// Retrieves the standard library document for MaterialX.
MaterialX::ConstDocumentPtr _GetStandardLibraryDocument()
{
    static auto standardDoc = UsdMtlxGetDocument("");
    return standardDoc;
}

// Gets the node definition string for a given node.
// If the Node already has it's nodeDefString set, use that.
std::string _GetNodeDefString(const MaterialX::NodePtr& node)
{
    // For now the 3dsmax MaterialX component guarantees that the nodeDefString is set.
    // If we ever need to support nodes without nodeDefString, add a fallback here.
    auto nodeDefString = node->getNodeDefString();
    if (!nodeDefString.empty()) {
        return nodeDefString;
    }
    return std::string();
}

// Retrieves the node definition for a given node.
MaterialX::ConstNodeDefPtr _GetNodeDef(const MaterialX::NodePtr& node)
{
    auto nodeDefName = _GetNodeDefString(node);
    auto nodeDefPtr = node->getDocument()->getNodeDef(nodeDefName);
    if (!nodeDefPtr) {
        // Get the standard library document and check that.
        nodeDefPtr = _GetStandardLibraryDocument()->getNodeDef(nodeDefName);
    }
    if (!nodeDefPtr) {
        TF_WARN("Could not find nodeDef for node '%s'", node->getName().c_str());
    }
    return nodeDefPtr;
}

// Sets shader info:id attribute on a USD ShadeShader based on a MaterialX node.
void _SetShaderInfoAttributes(const MaterialX::NodePtr& node, UsdShadeShader& usdShader)
{
    auto nodeDefString = _GetNodeDefString(node);
    usdShader.CreateIdAttr(VtValue(TfToken(nodeDefString)));
}

// Checks if the input type supports color space.
bool _TypeSupportsColorSpace(const MaterialX::InputPtr& mxElem)
{
    // ColorSpaces are supported on
    //  - inputs of type color3 or color4
    //  - filename inputs on image nodes with color3 or color4 outputs
    const std::string& type = mxElem->getType();
    const bool         colorInput = type == "color3" || type == "color4";

    bool colorImageNode = false;
    if (type == "filename") {
        // verify the output is color3 or color4
        auto node = mxElem->getParent()->asA<MaterialX::Node>();
        if (auto parentNodeDef = _GetNodeDef(node)) {
            for (const MaterialX::OutputPtr& output : parentNodeDef->getOutputs()) {
                const std::string& type = output->getType();
                colorImageNode |= type == "color3" || type == "color4";
            }
        }
    }
    return colorInput || colorImageNode;
}

// Sets UI attributes for a USD input based on a MaterialX input.
void _SetInputUIAttributes(const MaterialX::InputPtr mtlxInput, UsdShadeInput& usdInput)
{
    auto attr = usdInput.GetAttr();
    for (const auto& key : UsdUfe::MetadataTokens->allTokens) {
        if (mtlxInput->hasAttribute(key.GetString())) {
            auto value = mtlxInput->getAttribute(key);
            if (key == UsdUfe::MetadataTokens->UIDoc) {
                attr.SetDocumentation(value);
            } else if (key == UsdUfe::MetadataTokens->UIEnumLabels) {
                const auto       enumStrings = UsdUfe::TfStringSplit(value, ",");
                VtArray<TfToken> allowedTokens;
                allowedTokens.reserve(enumStrings.size());
                for (const auto& tokenString : enumStrings) {
                    allowedTokens.push_back(TfToken(TfStringTrim(tokenString, " ")));
                }
                attr.SetMetadata(SdfFieldKeys->AllowedTokens, allowedTokens);
            } else if (key == UsdUfe::MetadataTokens->UIFolder) {
                std::replace(value.begin(), value.end(), '/', ':');
                attr.SetDisplayGroup(value);
            } else if (key == UsdUfe::MetadataTokens->UIName) {
                attr.SetDisplayName(value);
            } else if (SdfSchema::GetInstance().IsRegistered(key)) {
                attr.SetMetadata(key, VtValue(value));
            } else {
                attr.SetCustomDataByKey(key, VtValue(value));
            }
        }
    }
}

// Gets the output name of the mxNode connected to a portElement
// If the portElement has an outputString, use that.
// Otherwise, look for the output name in the NodeDef
// If that fails, use the default output name.
std::string
_GetOutputName(const MaterialX::PortElementPtr& portElement, const MaterialX::NodePtr& mxNode)
{
    std::string outputName;
    if (portElement->hasOutputString()) {
        outputName = portElement->getOutputString();
    } else if (auto nodeDef = _GetNodeDef(mxNode)) {
        auto outVec = nodeDef->getOutputs();
        if (!outVec.empty()) {
            outputName = outVec[0]->getName();
        }
    }
    if (outputName.empty()) {
        outputName = UsdMtlxTokens->DefaultOutputName.GetString();
    }
    return outputName;
}

// Connects a USD input to a node output based on a MaterialX input.
void _ConnectToNode(
    const MaterialX::InputPtr& input,
    UsdShadeInput&             usdInput,
    const SdfPath&             parentPath,
    const UsdStagePtr&         stage)
{
    auto connectedNode = input->getConnectedNode();

    std::string outputName = _GetOutputName(input, connectedNode);
    auto        nodeOutput = UsdShadeShader(stage->GetPrimAtPath(
                                         parentPath.AppendPath(SdfPath(connectedNode->getName()))))
                          .GetOutput(TfToken(outputName));
    if (nodeOutput.IsDefined()) {
        usdInput.ConnectToSource(nodeOutput);
    }
}

// Connect a USD input to a NodeGraph input based on a MaterialX input.
void _ConnectToInterfaceInput(
    const MaterialX::InputPtr& interfaceInput,
    UsdShadeInput&             usdInput,
    const SdfPath&             parentPath,
    const UsdStagePtr&         stage)
{
    auto interfaceInputNode = interfaceInput->getParent();
    auto interfaceInputNodePrim = stage->GetPrimAtPath(parentPath);
    if (interfaceInputNode->getName() == interfaceInputNodePrim.GetName()) {
        auto interfaceInputNodeGraph = UsdShadeNodeGraph(interfaceInputNodePrim);
        auto valType = UsdMtlxGetUsdType(interfaceInput->getType());
        auto interfaceInputNodeOutput = interfaceInputNodeGraph.CreateInput(
            TfToken(interfaceInput->getName()), valType.valueTypeName);
        if (interfaceInputNodeOutput.IsDefined()) {
            usdInput.ConnectToSource(interfaceInputNodeOutput);
        }
    }
}

// Connects a USD input to a node graph output based on a MaterialX input.
void _ConnectToNodeGraph(
    const MaterialX::InputPtr& input,
    UsdShadeInput&             usdInput,
    const SdfPath&             parentPath,
    const UsdStagePtr&         stage)
{
    auto output = input->getConnectedOutput();
    auto nodeGraphName = output->getParent()->getName();
    auto nodeGraphPrim = stage->GetPrimAtPath(parentPath.AppendPath(SdfPath(nodeGraphName)));
    if (nodeGraphPrim.IsDefined()) {
        auto nodeGraph = UsdShadeNodeGraph(nodeGraphPrim);
        auto usdOutput = nodeGraph.GetOutput(TfToken(output->getName()));
        if (usdOutput.IsDefined()) {
            usdInput.ConnectToSource(usdOutput);
        }
    }
}

// Sets the value of a USD input based on a MaterialX input.
void _SetInputValue(const MaterialX::InputPtr& input, UsdShadeInput& usdInput)
{
    usdInput.Set(UsdMtlxGetUsdValue(input));
    if (_TypeSupportsColorSpace(input)) {
        auto colorSpace = input->getActiveColorSpace();
        if (!colorSpace.empty()) {
            usdInput.GetAttr().SetColorSpace(TfToken(colorSpace));
        }
    }
}

// Adds a USD input based on a MaterialX input.
void _AddInput(
    const MaterialX::InputPtr& input,
    UsdShadeInput&             usdInput,
    const SdfPath&             parentPath,
    const UsdStagePtr&         stage)
{
    if (input->hasNodeGraphString()) {
        _ConnectToNodeGraph(input, usdInput, parentPath, stage);
    } else if (input->hasNodeName()) {
        _ConnectToNode(input, usdInput, parentPath, stage);
    } else if (input->hasInterfaceName()) {
        auto interfaceInput = input->getInterfaceInput();
        _ConnectToInterfaceInput(interfaceInput, usdInput, parentPath, stage);
    } else if (!input->hasOutputString()) {
        _SetInputValue(input, usdInput);
    }
}

// Adds a shader input to a USD shader based on a MaterialX input.
void _AddShaderInput(
    const MaterialX::InputPtr& input,
    UsdShadeShader&            usdShader,
    const SdfPath&             parentPath,
    const UsdStagePtr&         stage)
{
    auto typeStr = input->getType();
    auto valType = UsdMtlxGetUsdType(typeStr);
    auto usdInput = usdShader.CreateInput(TfToken(input->getName()), valType.valueTypeName);
    if (usdInput.IsDefined()) {
        _AddInput(input, usdInput, parentPath, stage);
    }
}

// Adds a node graph input to a USD node graph based on a MaterialX input.
void _AddNodeGraphInput(
    const MaterialX::InputPtr& input,
    UsdShadeNodeGraph&         usdNodeGraph,
    const SdfPath&             parentPath,
    const UsdStagePtr&         stage)
{
    auto typeStr = input->getType();
    auto valType = UsdMtlxGetUsdType(typeStr);
    auto usdInput = usdNodeGraph.CreateInput(TfToken(input->getName()), valType.valueTypeName);
    if (usdInput.IsDefined()) {
        _AddInput(input, usdInput, parentPath, stage);
        _SetInputUIAttributes(input, usdInput);
    }
}

// Sets UI attributes for a prim based on a MaterialX node.
void _SetShaderUIAttribute(const MaterialX::InterfaceElementPtr& node, UsdPrim& prim)
{
    if (!prim.HasAPI<UsdUINodeGraphNodeAPI>()) {
        UsdUINodeGraphNodeAPI::Apply(prim);
    }

    if (auto nodeGraphApi = UsdUINodeGraphNodeAPI(prim)) {
        if (node->hasAttribute("ypos") && node->hasAttribute("xpos")) {
            nodeGraphApi.CreatePosAttr(VtValue(GfVec2f(
                std::stof(node->getAttribute("xpos")), std::stof(node->getAttribute("ypos")))));
        }
    }
}

// Retrieves the output of a USD prim.
UsdShadeOutput _GetPrimOutput(UsdPrim& prim, const TfToken& outputName)
{
    auto           shader = UsdShadeShader(prim);
    UsdShadeOutput output;
    if (auto shader = UsdShadeShader(prim)) {
        output = shader.GetOutput(outputName);
    } else if (auto nodeGraph = UsdShadeNodeGraph(prim)) {
        output = nodeGraph.GetOutput(outputName);
    }
    return output;
}

// Adds a Shader prim to the USD stage based on a MaterialX node.
void _AddNode(const MaterialX::NodePtr& node, const UsdStagePtr& stage, const SdfPath& parentPath)
{
    // Don't do anything for NodeGraphs, they are handled separately.
    if (node->getCategory() == "nodegraph") {
        return;
    }

    auto primPath = parentPath.AppendPath(SdfPath(node->getName()));
    auto shader = UsdShadeShader::Define(stage, primPath);

    if (!TF_VERIFY(shader, "Could not define UsdShadeShader at path '%s'\n", primPath.GetText())) {
        return;
    }

    auto shaderPrim = shader.GetPrim();
    _SetShaderUIAttribute(node, shaderPrim);
    _SetShaderInfoAttributes(node, shader);

    if (auto nodeDef = _GetNodeDef(node)) {
        for (auto output : nodeDef->getOutputs()) {
            auto outName
                = output->getName().empty() ? UsdMtlxTokens->DefaultOutputName : output->getName();
            shader.CreateOutput(
                TfToken(outName), UsdMtlxGetUsdType(output->getType()).valueTypeName);
        }
    }

    for (auto input : node->getInputs()) {
        _AddShaderInput(input, shader, parentPath, stage);
    }
}

// Adds a node and all its dependent nodes to the USD stage.
void _AddDependentNodes(
    const MaterialX::InterfaceElementPtr&     node,
    std::set<MaterialX::InterfaceElementPtr>& collectedNodes,
    const UsdStagePtr&                        stage,
    const SdfPath&                            parentPath)
{
    if (!node || collectedNodes.find(node) != collectedNodes.end()) {
        return;
    }
    collectedNodes.insert(node);

    SdfPath           targetPath = parentPath;
    bool              isNodeGraph = node->getCategory() == "nodegraph";
    UsdShadeNodeGraph usdNodeGraph;
    if (isNodeGraph) {
        // Define the NodeGraph
        targetPath = parentPath.AppendPath(SdfPath(node->getName()));
        auto nodeGraphPrim = stage->DefinePrim(targetPath, TfToken("NodeGraph"));
        if (!TF_VERIFY(
                nodeGraphPrim, "Could not define NodeGraph at path '%s'\n", targetPath.GetText())) {
            return;
        }
        usdNodeGraph = UsdShadeNodeGraph(nodeGraphPrim);
        _SetShaderUIAttribute(node, nodeGraphPrim);

        auto nodeGraph = node->asA<MaterialX::NodeGraph>();
        for (auto graphNode : nodeGraph->getNodes()) {
            _AddDependentNodes(graphNode, collectedNodes, stage, targetPath);
            _AddNode(graphNode, stage, targetPath);
        }
        for (auto output : nodeGraph->getOutputs()) {
            auto usdOutput = usdNodeGraph.CreateOutput(
                TfToken(output->getName()), UsdMtlxGetUsdType(output->getType()).valueTypeName);
            if (auto targetOutput = output->getConnectedOutput()) {
                auto targetPrim = stage->GetPrimAtPath(
                    targetPath.AppendPath(SdfPath(targetOutput->getParent()->getName())));
                usdOutput.ConnectToSource(
                    _GetPrimOutput(targetPrim, TfToken(targetOutput->getName())));
            } else if (auto targetNode = output->getConnectedNode()) {
                if (targetNode->getParent() != node) {
                    TF_WARN(
                        "NodeGraph output '%s' is connected to a node outside the NodeGraph",
                        output->getName().c_str());
                    continue;
                }
                auto targetOutputName = _GetOutputName(output, targetNode);
                auto targetPrim
                    = stage->GetPrimAtPath(targetPath.AppendPath(SdfPath(targetNode->getName())));
                auto primOut = _GetPrimOutput(targetPrim, TfToken(targetOutputName));
                if (!TF_VERIFY(
                        primOut,
                        "Could not find output '%s' for Prim at path '%s\n'",
                        targetOutputName.c_str(),
                        targetPrim.GetPrimPath().GetText())) {
                    continue;
                }
                usdOutput.ConnectToSource(primOut);
            }
        }
    }

    for (auto input : node->getInputs()) {
        // if it's connected to a NodeGraph, collect all the nodes in the NodeGraph
        if (!input->getNodeGraphString().empty()) {
            _AddDependentNodes(
                node->getDocument()->getNodeGraph(input->getNodeGraphString()),
                collectedNodes,
                stage,
                parentPath);
        }
        // If it's connected to an "independent" node, add that node and its dependencies
        else if (MaterialX::NodePtr connectedNode = input->getConnectedNode()) {
            if (collectedNodes.find(connectedNode) == collectedNodes.end()) {
                _AddDependentNodes(connectedNode, collectedNodes, stage, targetPath);
                _AddNode(connectedNode, stage, parentPath);
            }
        }
        if (isNodeGraph) {
            _AddNodeGraphInput(input, usdNodeGraph, parentPath, stage);
        }
    }
}

class MtlxShaderWriter : public MaxUsdShaderWriter
{
public:
    MtlxShaderWriter(Mtl* material, const SdfPath& usdPath, MaxUsdWriteJobContext& jobCtx);

    static ContextSupport CanExport(const MaxUsd::USDSceneBuilderOptions&);
    void                  Write() override;
};

// OPENPBR_CLASS_ID, can't use the macro here, because it's only 2025.3+
PXR_MAXUSD_REGISTER_SHADER_WRITER(Class_ID(0xf1551e33, 0x37fb1337), MtlxShaderWriter);
PXR_MAXUSD_REGISTER_SHADER_WRITER(PHYSICALMATERIAL_CLASS_ID, MtlxShaderWriter);

MaxUsdShaderWriter::ContextSupport
MtlxShaderWriter::CanExport(const MaxUsd::USDSceneBuilderOptions& exportArgs)
{
    if (!exportArgs.GetTranslateMaterials()) {
        return ContextSupport::Unsupported;
    }

    return exportArgs.GetConvertMaterialsTo() == TfToken("MaterialX") ? ContextSupport::Fallback
                                                                      : ContextSupport::Unsupported;
}

MtlxShaderWriter::MtlxShaderWriter(
    Mtl*                   material,
    const SdfPath&         usdPath,
    MaxUsdWriteJobContext& jobCtx)
    : MaxUsdShaderWriter(material, usdPath, jobCtx)
{
}

void MtlxShaderWriter::Write()
{
    UsdShadeShader shaderSchema = UsdShadeShader::Define(GetUsdStage(), GetUsdPath());
    if (!TF_VERIFY(
            shaderSchema,
            "Could not define UsdShadeShader at path '%s'\n",
            GetUsdPath().GetText())) {
        return;
    }

    usdPrim = shaderSchema.GetPrim();
    if (!TF_VERIFY(
            usdPrim,
            "Could not get UsdPrim for UsdShadeShader at path '%s'\n",
            shaderSchema.GetPath().GetText())) {
        return;
    }
    auto parentPath = GetUsdPrim().GetParent().GetPath();

    // Export the materialX document via Maxscript
    auto     animHandle = Animatable::GetHandleByAnim(GetMaterial());
    auto     tarLayer = GetUsdStage()->GetEditTarget().GetLayer();
    fs::path p(tarLayer->GetIdentifier());
    if (!_IsWellFormedPath(p)) {
        p = fs::path("");
    } else if (p.has_filename() && !tarLayer->IsAnonymous()) {
        p = p.parent_path();
    } else {
        p = writeJobCtx.GetFilename();
        p = p.remove_filename();
    }

    static const TSTR exportMtlToMtlx = LR"(
		fn exportMtlToMtlx materialAnimHandle layerPath = (

			local m = getAnimByHandle materialAnimHandle
			local mArr = #(m)
			local mtlxStr = MtlxIOUtil.ExportMtlxString layerPath mArr
			return mtlxStr
		)
		exportMtlToMtlx )";

    FPValue rvalue;
    rvalue.Init();
    std::wstringstream ss;
    ss << exportMtlToMtlx << animHandle << L" " << "@"
       << "\"" << MaxUsd::UsdStringToMaxString(p.string()) << "\"" << L'\0';
    ExecuteMAXScriptScript(ss.str().c_str(), MAXScript::ScriptSource::Dynamic, false, &rvalue);
    auto mtlxString = MaxUsd::MaxStringToUsdString(rvalue.s);
    auto mtlxDoc = MaterialX::createDocument();
    try {
        readFromXmlString(mtlxDoc, mtlxString);
    } catch (MaterialX::ExceptionParseError& e) {
        TF_WARN("Error reading MaterialX document: %s", e.what());
        return;
    }

    _NormalizeStandardSurfaceSpecularRotation(mtlxDoc);
    _NormalizeStandardSurfaceEmissionDefault(mtlxDoc);

    // Sanitize the material name using the same logic as with the MaterialX component
    // to match the node name produced by the MaterialX exporter. createValidName
    // does not guard against leading digits.
    auto mtlxMatName
        = MaterialX::createValidName(MaxUsd::MaxStringToUsdString(GetMaterial()->GetName()));
    if (!mtlxMatName.empty()
        && std::isdigit(static_cast<unsigned char>(mtlxMatName[0]))) {
        mtlxMatName = "_" + mtlxMatName;
    }
    // surfaceMaterialNode
    auto MaterialNode = mtlxDoc->getNode(mtlxMatName);
    if (MaterialNode == nullptr) {
        TF_WARN(
            "Material Node '%s' not found in the MaterialX Document",
            mtlxMatName.c_str());
        return;
    }
    // Collection of the MaterialX nodes already processed, to avoid processing them again.
    std::set<MaterialX::InterfaceElementPtr> collectedNodes;

    // Handle the displacement shader output connection.
    if (auto displacementNode = MaterialNode->getConnectedNode("displacementshader")) {
        UsdShadeOutput _mtlDisplacementOutput;
        if (writeJobCtx.GetArgs().GetAllMaterialConversions().size() > 1) {
            _mtlDisplacementOutput
                = UsdShadeMaterial(GetUsdPrim().GetParent()).CreateDisplacementOutput();
            auto output = UsdShadeMaterial(GetUsdPrim().GetParent().GetParent())
                              .CreateDisplacementOutput(TfToken("mtlx"));
            output.ConnectToSource(_mtlDisplacementOutput);
        } else {
            _mtlDisplacementOutput = UsdShadeMaterial(GetUsdPrim().GetParent())
                                         .CreateDisplacementOutput(TfToken("mtlx"));
        }
        UsdShadeShader displacementShader;
        displacementShader = UsdShadeShader::Define(
            GetUsdStage(), parentPath.AppendPath(SdfPath(displacementNode->getName())));
        auto _ShaderDisplacementOutput = displacementShader.CreateOutput(
            UsdMtlxTokens->DefaultOutputName, _mtlDisplacementOutput.GetTypeName());
        _mtlDisplacementOutput.ConnectToSource(_ShaderDisplacementOutput);
        auto& dispNodeName = displacementNode->getName();
        _SetShaderInfoAttributes(displacementNode, displacementShader);
        _AddDependentNodes(displacementNode, collectedNodes, GetUsdStage(), parentPath);
        for (auto input : displacementNode->getInputs()) {
            _AddShaderInput(input, displacementShader, parentPath, GetUsdStage());
        }
    }

    auto shaderNode = MaterialNode->getConnectedNode("surfaceshader");
    if (shaderNode == nullptr) {
        TF_WARN(
            "Surface Shader Node not found in the MaterialX Document, for Shader at path '%s'",
            usdPrim.GetPrimPath().GetText());
        return;
    }
    _SetShaderInfoAttributes(shaderNode, shaderSchema);
    _AddDependentNodes(shaderNode, collectedNodes, GetUsdStage(), parentPath);

    for (auto input : shaderNode->getInputs()) {
        _AddShaderInput(input, shaderSchema, parentPath, GetUsdStage());
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
#endif // IS_MAX2025_OR_GREATER