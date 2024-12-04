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

#include "Logging.h"

#include <MaxUsd/MaxUSDAPI.h>

#include <maxscript/mxsplugin/mxsCustomAttributes.h>

#include <IMetaData.h>
#include <iparamb2.h>

namespace MAXUSD_NS_DEF {
namespace MetaData {

static const wchar_t* USD_KIND = L"usd_kind";
static const wchar_t* USD_PURPOSE = L"usd_purpose";
static const wchar_t* USD_HIDDEN = L"usd_hidden";

enum UsdMetaDataType
{
    // NOTE: usdMetaDataDefMap needs to be updated for each new entry
    Kind,
    Purpose,
    Hidden
};

struct UsdMetaDataDef
{
    UsdMetaDataType                   usdMetaData;
    const wchar_t*                    usdMetaDataKey;
    IMetaDataManager::ParamDescriptor usdMetaDataParamDef;
};

// can be used to hold multiple types of data to be stored
// in paramblocks
struct ParameterValue
{
    int          intValue = 0;
    bool         boolValue = false;
    std::wstring strValue;
};

static UsdMetaDataDef CreateUsdKindMetaDataDef()
{
    IMetaDataManager::ParamDescriptor metaDataParamDef;
    metaDataParamDef.m_name = USD_KIND;
    metaDataParamDef.m_dataType = TYPE_STRING;
    metaDataParamDef.m_ctrlType = TYPE_EDITBOX;
    metaDataParamDef.m_ctrlAlign = IMetaDataManager::ControlAlign::eAlignLeft;

    return UsdMetaDataDef { UsdMetaDataType::Kind, USD_KIND, metaDataParamDef };
}

static UsdMetaDataDef CreateUsdPurposeMetaDataDef()
{
    IMetaDataManager::ParamDescriptor metaDataParamDef;
    metaDataParamDef.m_name = USD_PURPOSE;
    metaDataParamDef.m_dataType = TYPE_STRING;
    metaDataParamDef.m_ctrlType = TYPE_EDITBOX;
    metaDataParamDef.m_ctrlAlign = IMetaDataManager::ControlAlign::eAlignLeft;

    return UsdMetaDataDef { UsdMetaDataType::Purpose, USD_PURPOSE, metaDataParamDef };
}

static UsdMetaDataDef CreateUsdHiddenMetaDataDef()
{
    IMetaDataManager::ParamDescriptor metaDataParamDef;
    metaDataParamDef.m_name = USD_HIDDEN;
    metaDataParamDef.m_dataType = static_cast<ParamType2>(TYPE_BOOL);
    metaDataParamDef.m_ctrlType = static_cast<ControlType2>(TYPE_SINGLECHECKBOX);
    metaDataParamDef.m_ctrlAlign = IMetaDataManager::ControlAlign::eAlignLeft;
    return UsdMetaDataDef { UsdMetaDataType::Hidden, USD_HIDDEN, metaDataParamDef };
}

// this mapping needs to be updated every time we add a new property to be round-tripped
static std::map<UsdMetaDataType, UsdMetaDataDef> usdMetaDataDefMap {
    { UsdMetaDataType::Kind, CreateUsdKindMetaDataDef() },
    { UsdMetaDataType::Purpose, CreateUsdPurposeMetaDataDef() },
    { UsdMetaDataType::Hidden, CreateUsdHiddenMetaDataDef() }
};

static const UsdMetaDataDef& GetUsdMetaDataDef(UsdMetaDataType id) { return usdMetaDataDefMap[id]; }

/**
 * \brief retrieves usd metadata stored as custom attribute in max
 * \param valueHolder output value will be written in this object
 * \return bool false if parameter does not exist or paramType is different from what was expected
 */
MaxUSDAPI bool GetUsdMetaDataValue(
    IParamBlock2*          pb2,
    const UsdMetaDataType& metaDataType,
    const TimeValue&       t,
    ParameterValue&        valueHolder);

/**
 * \brief we use IMetaDataManager to attach metadata/customAttributes to BaseObject.
 * Since our properties are dynamic i.e some BaseObject might have none, others might
 * only have one or two or all, we don't base ourselves on MetaDataID. But we simply
 * find the first ParamBlock that contains a Parameter with 'usd_' as prefix.
 * \param baseObject should be the BaseObject of the node in max
 * \return IParamBlock* pb2 containing usd custom attributes or nullptr if could not find.
 */
MaxUSDAPI IParamBlock2* FindUsdCustomAttributeParamBlock(Animatable* baseObject);

/**
 * \brief This function retrieves or define the MetaDataID that represents the structure
 * we use to store the USD Built-in metadata such as Hidden, Purpose, Kind etc...
 * MetaDataID will be different based on the properties we want to support
 * \param metaDataIds list of all UsdMetaDataType properties that should be supported for this definition
 * \return IMetaDataManager::MetaDataID The MetaDataID representing the ParamBlock data for USD built-in metadata
 */
MaxUSDAPI IMetaDataManager::MetaDataID
          GetOrDefineUsdBuiltInMetaData(const std::vector<UsdMetaDataType>& metaDataIds);

/**
 * \brief This function return true if a conflict exist between 2 object USD metadata.
 * \param object1 first to compare for conflict
 * \param object2 second to compare for conflict
 * \param timeValue time at which to compare values
 * \return True if both object contains different metadata value for the same metadata key.
 */
MaxUSDAPI bool CheckForConflict(Object* object1, Object* object2, const TimeValue& t);

} // namespace MetaData
} // namespace MAXUSD_NS_DEF
