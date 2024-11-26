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
#include "TranslatorPrim.h"

#include <MaxUsd/MaxTokens.h>
#include <MaxUsd/Utilities/MetaDataUtils.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usdGeom/imageable.h>

#include <IMetaData.h>
#include <inode.h>

PXR_NAMESPACE_OPEN_SCOPE

INode* MaxUsdTranslatorPrim::CreateAndRegisterNode(
    const UsdPrim&        prim,
    Object*               object,
    const TfToken&        name,
    MaxUsdReadJobContext& context,
    bool                  attachToParent)
{
    WStr   nodeName = MaxUsd::UsdStringToMaxString(name.GetString());
    INode* createdNode = GetCOREInterface17()->CreateObjectNode(object, nodeName);

    // add node to read job context for later reference
    context.RegisterNewMaxRefTargetHandle(prim.GetPath(), createdNode);
    MaxUsd::Log::Info(
        "Importing prim {0} to node {1}.", prim.GetPath().GetString(), name.GetString());

    if (attachToParent) {
        auto parentNode = context.GetMaxNode(prim.GetPath().GetParentPath(), false);
        if (parentNode) {
            parentNode->AttachChild(createdNode);
        }
    }
    return createdNode;
}

void MaxUsdTranslatorPrim::Read(const UsdPrim& prim, INode* maxNode, MaxUsdReadJobContext& context)
{
    const auto timeConfig = context.GetArgs().GetResolvedTimeConfig(prim.GetStage());
    const auto timeCode = timeConfig.GetStartTimeCode();

    // If the mesh is set to Guide in USD, the mesh imported will be set to non-renderable.
    UsdGeomImageable imageablePrim(prim);
    TfToken          purpose;
    imageablePrim.GetPurposeAttr().Get(&purpose, timeCode);
    if (purpose == UsdGeomTokens->guide) {
        maxNode->SetRenderable(false);
    }

    TfToken     visibility { UsdGeomTokens->invisible };
    std::string primPath = prim.GetPath().GetString();
    auto        parentPrim = prim.GetParent();
    if (parentPrim && !parentPrim.IsPseudoRoot()) {
        std::string parentPrimPath = parentPrim.GetPath().GetString();
        auto        parentNode = context.GetMaxNode(parentPrim.GetPath(), false);
        if (parentNode && parentNode->IsHidden()) {
            visibility = UsdGeomTokens->invisible;
        } else {
            visibility = imageablePrim.ComputeVisibility(timeCode);
        }
    } else {
        visibility = imageablePrim.ComputeVisibility(timeCode);
    }
    // Set hide on node according to computed visibility
    maxNode->Hide(visibility == UsdGeomTokens->invisible);

    // meta data
    ReadMaxCustomAttributes(prim, maxNode, context);
}

void MaxUsdTranslatorPrim::ReadMaxCustomAttributes(
    const UsdPrim&        prim,
    INode*                maxNode,
    MaxUsdReadJobContext& context)
{
    IMetaDataManager* maxMetaDataManager = IMetaDataManager::GetInstance();

    // contains the usd metadata names for defining our CustomAttributes and also the value to set
    // for each parameter
    std::vector<std::pair<MaxUsd::MetaData::UsdMetaDataType, MaxUsd::MetaData::ParameterValue>>
        customAttributesToAddAndUpdate;

    const auto& metaDataIncludes = context.GetArgs().GetMetaData();
    auto        isMetaDataTypeIncluded
        = [&metaDataIncludes](MaxUsd::MetaData::UsdMetaDataType mdType) -> bool {
        return std::find(
                   metaDataIncludes.cbegin(), metaDataIncludes.cend(), static_cast<int>(mdType))
            != metaDataIncludes.end();
    };

    const auto timeConfig = context.GetArgs().GetResolvedTimeConfig(prim.GetStage());

    // usd purpose
    UsdAttribute purposeAttr = UsdGeomImageable(prim).GetPurposeAttr();
    if (isMetaDataTypeIncluded(MaxUsd::MetaData::UsdMetaDataType::Purpose)
        && purposeAttr.IsDefined() && purposeAttr.HasAuthoredValue()) {
        TfToken purpose;
        purposeAttr.Get(&purpose, timeConfig.GetStartTimeCode());
        std::string purposeStr = purpose.GetString();

        if (purpose != UsdGeomTokens->default_ && !purpose.IsEmpty()) {
            std::wstring purposeWStr = MaxUsd::UsdStringToMaxString(purposeStr).data();
            MaxUsd::MetaData::ParameterValue purposeVal;
            purposeVal.strValue = purposeWStr;
            customAttributesToAddAndUpdate.push_back(
                std::make_pair(MaxUsd::MetaData::Purpose, purposeVal));
        }
    }

    // usd kind
    if (isMetaDataTypeIncluded(MaxUsd::MetaData::UsdMetaDataType::Kind)
        && prim.HasMetadata(MaxUsdMetadataTokens->kind)) {
        TfToken     kind;
        bool        canReadKind = UsdModelAPI(prim).GetKind(&kind);
        std::string kindStr = kind.GetString();
        if (canReadKind && !kind.IsEmpty()) {
            std::wstring kindWStr = MaxUsd::UsdStringToMaxString(kindStr).data();
            MaxUsd::MetaData::ParameterValue kindVal;
            kindVal.strValue = kindWStr;
            customAttributesToAddAndUpdate.push_back(
                std::make_pair(MaxUsd::MetaData::Kind, kindVal));
        }
    }

    // usd hidden
    if (isMetaDataTypeIncluded(MaxUsd::MetaData::UsdMetaDataType::Hidden)
        && prim.HasMetadata(MaxUsdMetadataTokens->hidden)) {
        bool usdHidden = prim.IsHidden();
        if (usdHidden) {
            MaxUsd::MetaData::ParameterValue hiddenVal;
            hiddenVal.boolValue = usdHidden;
            customAttributesToAddAndUpdate.push_back(
                std::make_pair(MaxUsd::MetaData::Hidden, hiddenVal));
        }
    }

    if (customAttributesToAddAndUpdate.empty()) {
        // no metadata to add so exit early
        return;
    }

    // create definition for CustAttrib to be assigned to this BaseObject
    std::vector<MaxUsd::MetaData::UsdMetaDataType> customAttributesToCreate;
    std::for_each(
        customAttributesToAddAndUpdate.begin(),
        customAttributesToAddAndUpdate.end(),
        [&customAttributesToCreate](auto& entry) {
            customAttributesToCreate.push_back(entry.first);
        });
    IMetaDataManager::MetaDataID usdBuiltInMetaData
        = MaxUsd::MetaData::GetOrDefineUsdBuiltInMetaData(customAttributesToCreate);
    if (usdBuiltInMetaData == EmptyMetaDataID) {
        // creating scripted custom attributes failed
        return;
    }

    CustAttrib* usdCustomAttribute = maxMetaDataManager->AddMetaDataToAnimatable(
        usdBuiltInMetaData, *(maxNode->GetObjectRef()));
    if (usdCustomAttribute == nullptr) {
        MaxUsd::Log::Error(
            L"Could not add USD Metadata to object: {0} ",
            std::wstring(maxNode->GetObjectRef()->GetObjectName(false)));
        return;
    }

    IParamBlock2* usdCustomAttributePb = usdCustomAttribute->GetParamBlock(0);
    if (usdCustomAttributePb == nullptr) {
        MaxUsd::Log::Error(
            L"Could not get ParamBlock2 for IMetaDataManager/CustAttrib for object: {0} ",
            std::wstring(maxNode->GetObjectRef()->GetObjectName(false)));
        return;
    }

    const auto startTimeValue
        = MaxUsd::GetMaxTimeValueFromUsdTimeCode(prim.GetStage(), timeConfig.GetStartTimeCode());
    for (const auto& prop : customAttributesToAddAndUpdate) {
        auto def = MaxUsd::MetaData::GetUsdMetaDataDef(prop.first);
        auto val = prop.second;
        switch (def.usdMetaDataParamDef.m_dataType) {
        case TYPE_STRING:
            usdCustomAttributePb->SetValueByName(
                def.usdMetaDataKey, val.strValue.c_str(), startTimeValue);
            break;
        case TYPE_BOOL:
            usdCustomAttributePb->SetValueByName(def.usdMetaDataKey, val.boolValue, startTimeValue);
            break;
        case TYPE_INT:
            usdCustomAttributePb->SetValueByName(def.usdMetaDataKey, val.intValue, startTimeValue);
            break;
        default:
            // unhandled type that was not implemented
            DbgAssert(_T("Unhandled customattribute data type"));
            break;
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE