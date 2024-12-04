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
#include "MetaDataUtils.h"

#include "TranslationUtils.h"

namespace MAXUSD_NS_DEF {
namespace MetaData {

IMetaDataManager::MetaDataID
GetOrDefineUsdBuiltInMetaData(const std::vector<UsdMetaDataType>& metaDataIds)
{
    if (metaDataIds.empty()) {
        return EmptyMetaDataID;
    }

    static std::map<std::string, IMetaDataManager::MetaDataID> cache;
    std::vector<UsdMetaDataType>                               sortedMetaDataIds = metaDataIds;
    std::sort(sortedMetaDataIds.begin(), sortedMetaDataIds.end());
    std::string hashKey = std::accumulate(
        sortedMetaDataIds.begin(),
        sortedMetaDataIds.end(),
        std::string(""),
        [](const std::string& s, UsdMetaDataType type) {
            return s + "|" + std::to_string(static_cast<int>(type));
        });
    const auto it = cache.find(hashKey);
    if (it != cache.end()) {
        return it->second;
    }

    IMetaDataManager*                      maxMetaDataManager = IMetaDataManager::GetInstance();
    Tab<IMetaDataManager::ParamDescriptor> tabParams;

    for (const auto& id : metaDataIds) {
        auto def = GetUsdMetaDataDef(id);
        tabParams.Append(1, &def.usdMetaDataParamDef);
    }

    TSTR                         errMsg = nullptr;
    IMetaDataManager::MetaDataID usdBuiltInMetaData
        = maxMetaDataManager->CreateMetaDataDefinition(_T("USD"), _T("USD"), tabParams, &errMsg);
    if (usdBuiltInMetaData == EmptyMetaDataID) {
        MaxUsd::Log::Error(
            L"Could not define built-in USD Metadata object, errorMsg: {0}", std::wstring(errMsg));
    }
    cache.insert({ hashKey, usdBuiltInMetaData });
    return usdBuiltInMetaData;
}

IParamBlock2* FindUsdCustomAttributeParamBlock(Animatable* baseObject)
{
    // loop through all CA to find one that contains a parameter name with usd prefix
    // we don't use `IMetaDataManager::GetInstance()->GetAllMetaData` so that we can
    // also detect manually added Custom Attributes via Parameter Editor or maxscript
    ICustAttribContainer* caContainer = baseObject->GetCustAttribContainer();
    if (!caContainer) {
        // no CA was attached to this object
        return nullptr;
    }

    for (int i = 0; i < caContainer->GetNumCustAttribs(); i++) {
        CustAttrib* ca = caContainer->GetCustAttrib(i);
        if (!ca) {
            continue;
        }
        MSCustAttrib* msCA = (MSCustAttrib*)ca->GetInterface(I_SCRIPTEDCUSTATTRIB);
        if (!msCA) {
            continue;
        }

        if (ca->NumParamBlocks() < 1) {
            continue;
        }

        IParamBlock2* pb2 = ca->GetParamBlock(0);
        if (!pb2) {
            continue;
        }

        for (int paramIndex = 0; paramIndex < pb2->NumParams(); paramIndex++) {
            const ParamDef* paramDef = pb2->GetParamDefByIndex(paramIndex);
            TSTR            name = pb2->GetLocalName(paramDef->ID);

            if (name.StartsWith(_T("usd_"), false)) {
                return pb2;
            }
        }
    }

    return nullptr;
}

bool CheckForConflict(Object* obj1, Object* obj2, const TimeValue& t)
{
    IParamBlock2* pb1 = MaxUsd::MetaData::FindUsdCustomAttributeParamBlock(obj1);
    IParamBlock2* pb2 = MaxUsd::MetaData::FindUsdCustomAttributeParamBlock(obj2);

    if (pb1 == pb2) {
        return false;
    }

    if (pb1 == nullptr || pb2 == nullptr) {
        return false;
    }

    int nbParam1 = pb1->NumParams();
    int nbParam2 = pb2->NumParams();

    std::unordered_set<ParamID> listOfParamIdToCompare;
    for (int i = 0; i < nbParam1; ++i) {
        listOfParamIdToCompare.emplace(pb1->IndextoID(i));
    }

    for (int i = 0; i < nbParam2; ++i) {
        listOfParamIdToCompare.emplace(pb2->IndextoID(i));
    }

    for (auto it = listOfParamIdToCompare.begin(); it != listOfParamIdToCompare.end(); ++it) {
        const ParamID   paramId = *it;
        const ParamDef& paramDef = pb1->GetParamDef(paramId);
        Interval        valid = FOREVER;
        switch (paramDef.type) {
        case TYPE_INT:
            int intValue1;
            int intValue2;
            if (pb1->GetValue(paramId, t, intValue1, valid)
                && pb2->GetValue(paramId, t, intValue2, valid)) {
                if (intValue1 != intValue2) {
                    return true;
                }
            }
            break;
        case TYPE_FLOAT:
            float floatValue1;
            float floatValue2;
            if (pb1->GetValue(paramId, t, floatValue1, valid)
                && pb2->GetValue(paramId, t, floatValue2, valid)) {
                if (floatValue1 != floatValue2) {
                    return true;
                }
            }
            break;
        case TYPE_BOOL:
            BOOL boolValue1;
            BOOL boolValue2;
            if (pb1->GetValue(paramId, t, boolValue1, valid)
                && pb2->GetValue(paramId, t, boolValue2, valid)) {
                if (boolValue1 != boolValue2) {
                    return true;
                }
            }
            break;
        case TYPE_STRING:
            const wchar_t* value1;
            const wchar_t* value2;
            if (pb1->GetValue(paramId, t, value1, valid)
                && pb2->GetValue(paramId, t, value2, valid)) {
                if (wcscmp(value1, value2) != 0) {
                    return true;
                }
            }
            break;
        default:
            // unhandled type that was not implemented
            DbgAssert(_T("Unhandled customattribute data type"));
            return true;
        }
    }

    return false;
}

bool GetUsdMetaDataValue(
    IParamBlock2*          pb2,
    const UsdMetaDataType& metaDataType,
    const TimeValue&       t,
    ParameterValue&        valueHolder)
{
    UsdMetaDataDef metaDataDef = GetUsdMetaDataDef(metaDataType);
    ParamID        paramId = MaxUsd::FindParamId(pb2, metaDataDef.usdMetaDataKey);
    // If this specific USD metadata doesn't exist in the PB we are done!
    if (paramId == -1) {
        return false;
    }
    const ParamDef& paramDef = pb2->GetParamDef(paramId);
    if (paramDef.type != metaDataDef.usdMetaDataParamDef.m_dataType) {
        // parameter found but type is different than expected
        MaxUsd::Log::Warn(
            L"USD Metadata '{0}' found but got different type actual:{1} expected:{2}",
            metaDataDef.usdMetaDataKey,
            static_cast<int>(paramDef.type),
            static_cast<int>(metaDataDef.usdMetaDataParamDef.m_dataType));
        return false;
    }

    Interval       iv = FOREVER;
    bool           hasFoundParam = false;
    int            boolVal = 0;
    const wchar_t* strVal = L"";
    switch (metaDataDef.usdMetaDataParamDef.m_dataType) {
    case TYPE_INT: hasFoundParam = pb2->GetValue(paramId, t, valueHolder.intValue, iv); break;
    case TYPE_BOOL:
        hasFoundParam = pb2->GetValue(paramId, t, boolVal, iv);
        valueHolder.boolValue = (boolVal == 0) ? false : true;
        break;
    case TYPE_STRING:
        hasFoundParam = pb2->GetValue(paramId, t, strVal, iv);
        if (strVal) {
            valueHolder.strValue = std::wstring(strVal);
        }
        break;
    default:
        // unhandled type that was not implemented
        DbgAssert(_T("Unhandled customattribute data type"));
        break;
    }
    return hasFoundParam;
}

} // namespace MetaData
} // namespace MAXUSD_NS_DEF
