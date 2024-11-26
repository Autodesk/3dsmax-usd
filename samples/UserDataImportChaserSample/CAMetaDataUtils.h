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

#include <IMetaData.h>

namespace caMetaData {

enum UsdMetaDataTypeCA
{
	CA_BOOL,
	CA_INT,
	CA_STR
};

struct ParameterValue
{
	std::wstring key = L"";
	int intValue = 0;
	bool boolValue = false;
	std::wstring strValue = L"";
};

struct UsdMetaDataDefCA
{
	UsdMetaDataTypeCA usdMetaData;
	const wchar_t* usdMetaDataKey;
	IMetaDataManager::ParamDescriptor usdMetaDataParamDef;
};

static UsdMetaDataDefCA createCABoolMetaDataDef(const std::wstring& key)
{
	IMetaDataManager::ParamDescriptor metaDataParamDef;
	metaDataParamDef.m_name = key.c_str();
	metaDataParamDef.m_dataType = static_cast<ParamType2>(TYPE_BOOL);
	metaDataParamDef.m_ctrlType = static_cast<ControlType2>(TYPE_SINGLECHECKBOX);
	metaDataParamDef.m_ctrlAlign = IMetaDataManager::ControlAlign::eAlignLeft;

	return UsdMetaDataDefCA{ UsdMetaDataTypeCA::CA_BOOL, key.c_str(), metaDataParamDef };
}

static UsdMetaDataDefCA createCAIntMetaDataDef(const std::wstring& key)
{
	IMetaDataManager::ParamDescriptor metaDataParamDef;
	metaDataParamDef.m_name = key.c_str();
	metaDataParamDef.m_dataType = static_cast<ParamType2>(TYPE_INT);
	metaDataParamDef.m_ctrlType = TYPE_SLIDER;
	metaDataParamDef.m_ctrlAlign = IMetaDataManager::ControlAlign::eAlignLeft;

	return UsdMetaDataDefCA{ UsdMetaDataTypeCA::CA_INT, key.c_str(), metaDataParamDef };
}


static UsdMetaDataDefCA createCAStrMetaDataDef(const std::wstring& key)
{
	IMetaDataManager::ParamDescriptor metaDataParamDef;
	metaDataParamDef.m_name = key.c_str();
	metaDataParamDef.m_dataType = TYPE_STRING;
	metaDataParamDef.m_ctrlType = TYPE_EDITBOX;
	metaDataParamDef.m_ctrlAlign = IMetaDataManager::ControlAlign::eAlignLeft;

	return UsdMetaDataDefCA{ UsdMetaDataTypeCA::CA_STR, key.c_str(), metaDataParamDef };
}


IMetaDataManager::MetaDataID getOrDefineCABuiltInMetaData(
		const std::vector<std::pair<UsdMetaDataTypeCA, ParameterValue>> &cas);

static const UsdMetaDataDefCA getCAMetaDataDef(UsdMetaDataTypeCA type, const std::wstring& key)
{
	switch (type)
	{
		case CA_BOOL:
			return createCABoolMetaDataDef(key);
		case CA_INT:
			return createCAIntMetaDataDef(key);
		case CA_STR:
			return createCAStrMetaDataDef(key);
		default:
			return createCABoolMetaDataDef(L"not_found");
	}
}

} // namespace caMetaData
