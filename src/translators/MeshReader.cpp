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
#include <MaxUsd/MeshConversion/MeshConverter.h>
#include <MaxUsd/Translators/PrimReader.h>
#include <MaxUsd/Translators/PrimReaderRegistry.h>
#include <MaxUsd/Translators/ReadJobContext.h>
#include <MaxUsd/Translators/TranslatorMaterial.h>
#include <MaxUsd/Translators/TranslatorMorpher.h>
#include <MaxUsd/Translators/TranslatorPrim.h>
#include <MaxUsd/Translators/TranslatorXformable.h>
#include <MaxUsd/Utilities/TypeUtils.h>

#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdSkel/bindingAPI.h>

#include <stdmat.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace {
bool assignMaterial(const UsdGeomMesh& mesh, INode* meshObj, MaxUsdReadJobContext& context)
{
    auto args = context.GetArgs();
    if (args.GetTranslateMaterials()) {
        // If a material is bound, create (or reuse if already present) and assign it
        // If no binding is present, assign the mesh to the default shader
        return MaxUsdTranslatorMaterial::AssignMaterial(args, mesh, meshObj, context);
    }
    return false;
}
} // namespace

// prim reader for mesh
class MaxUsdPrimReaderMesh final : public MaxUsdPrimReader
{
public:
    MaxUsdPrimReaderMesh(const UsdPrim& prim, MaxUsdReadJobContext& jobCtx)
        : MaxUsdPrimReader(prim, jobCtx)
    {
    }

    ~MaxUsdPrimReaderMesh() override { }

    bool Read() override;

    void InstanceCreated(const UsdPrim& prim, INode* instance) override;
};

TF_REGISTRY_FUNCTION_WITH_TAG(MaxUsdPrimReaderRegistry, UsdGeomMesh)
{
    MaxUsdPrimReaderRegistry::Register<UsdGeomMesh>(
        [](const UsdPrim& prim, MaxUsdReadJobContext& jobCtx) {
            return std::make_shared<MaxUsdPrimReaderMesh>(prim, jobCtx);
        });
}

bool MaxUsdPrimReaderMesh::Read()
{
    auto prim = GetUsdPrim();
    auto mesh = UsdGeomMesh(prim);
    if (!mesh) {
        return false;
    }

    // skinned meshes have the joints weights set to a specific number of vertices
    // check if the mesh is skinned to prevent the possibility of having some vertices cleaned when
    // converted to max
    SdfPathVector     skelTargets;
    UsdSkelBindingAPI binding = UsdSkelBindingAPI::Get(GetJobContext().GetStage(), prim.GetPath());
    binding.GetSkeletonRel().GetTargets(&skelTargets);

    // Then, proceed to conversion
    std::map<int, std::string> channelNames;
    MultiMtl*                  materialBind = nullptr;
    MaxUsd::MeshConverter      meshConverter;
    const auto                 timeConfig = GetArgs().GetResolvedTimeConfig(prim.GetStage());
    const auto                 startTimeCode = timeConfig.GetStartTimeCode();
    PolyObject*                meshObject = meshConverter.ConvertToPolyObject(
        mesh,
        GetArgs().GetPrimvarMappingOptions(),
        channelNames,
        &materialBind,
        startTimeCode,
        skelTargets.empty());
    if (meshObject == nullptr) {
        return false;
    }

    auto createdNode = MaxUsdTranslatorPrim::CreateAndRegisterNode(
        prim, meshObject, prim.GetName(), GetJobContext());

    // Configure the morpher modifier, if the mesh prim have blendshapes defined.
    MaxUsdTranslatorMorpher::Read(prim, createdNode, GetJobContext());

    // position the node
    MaxUsdTranslatorXformable::Read(prim, createdNode, GetJobContext());

    // Mesh Node configuration
    pxr::VtArray<pxr::GfVec3f> colorArray;
    pxr::UsdAttribute          displayColorAttribute = mesh.GetDisplayColorAttr();
    displayColorAttribute.Get(&colorArray, startTimeCode);
    Color diffuseDisplayColor {};

    bool hasAuthoredColor = false;
    // If a display color is specified, use it as wire color.
    if (!colorArray.empty()) {
        auto color = colorArray[0];
        diffuseDisplayColor = { color[0], color[1], color[2] };
        createdNode->SetWireColor(diffuseDisplayColor.toRGB());
        hasAuthoredColor = true;
    }

    // If the original prim had material subsets, we generated a multi-material along with the
    // object. Set it to the node now, and connect a simple physical material to all its slots to
    // represent the displayColor.
    const auto subsetsMaterial = materialBind;
    if (subsetsMaterial != nullptr) {
        createdNode->SetMtl(subsetsMaterial);
        auto colorMtl = NewPhysicalMaterial(_T("USDImporter"));
        if (hasAuthoredColor) {
            colorMtl->SetDiffuse(
                diffuseDisplayColor,
                MaxUsd::GetMaxTimeValueFromUsdTimeCode(
                    prim.GetStage(), timeConfig.GetStartTimeCode()));
        }
        colorMtl->SetName(L"displayColor");

        IParamBlock2* mtlParamBlock2 = subsetsMaterial->GetParamBlockByID(0);
        short         paramId = MaxUsd::FindParamId(mtlParamBlock2, L"materialIDList");
        if (paramId < 0) {
            MaxUsd::Log::Error(
                "Unable to find materialIDList param id on multiMaterial param block.");
        } else {
            Interval valid = FOREVER;
            for (int i = 0; i < subsetsMaterial->NumSubs(); ++i) {
                int matId;
                mtlParamBlock2->GetValue(paramId, 0, matId, valid, i);
                subsetsMaterial->SetSubMtl(matId, colorMtl);
            }
        }
    }

    // Set channel names, via the channel info interface exposed in a utility plugin.
    // Dirty trick to get the ChannelInfo interface...
    const auto CHANNEL_INFO_INTERFACE_ID = Interface_ID(0x438a1122, 0xef966644);
    const auto NAME_CHANNEL_FUNC_ID = 9;
    auto       channelInfoInterface
        = static_cast<FPInterface*>(GetCOREInterface(CHANNEL_INFO_INTERFACE_ID));
    if (!channelInfoInterface) {
        MaxUsd::Log::Error(
            "Unable to retrieve the ChannelInfo interface. Mapped channels will not be named.");
    } else {
        for (const auto& channelEntry : channelNames) {
            const auto channelName = MaxUsd::UsdStringToMaxString(channelEntry.second);
            // Signature : NameChannel(TYPE_INODE, TYPE_INT, TYPE_INT, TYPE_STRING)
            FPParams params(
                4,
                TYPE_INODE,
                createdNode,
                TYPE_INT,
                3,
                TYPE_INT,
                channelEntry.first,
                TYPE_STRING,
                channelName.data());
            const auto status = channelInfoInterface->Invoke(NAME_CHANNEL_FUNC_ID, &params);
            if (status != FPS_OK) {
                MaxUsd::Log::Error(
                    "Error while attempting to name channel {} to {}.",
                    channelEntry.first,
                    channelEntry.second);
            }
        }
    }

    // assign material
    assignMaterial(mesh, createdNode, GetJobContext());

    return true;
}

void MaxUsdPrimReaderMesh::InstanceCreated(const UsdPrim& prim, INode* instance)
{
    auto mesh = UsdGeomMesh(prim);
    if (mesh) {
        assignMaterial(mesh, instance, GetJobContext());
    }
}

PXR_NAMESPACE_CLOSE_SCOPE