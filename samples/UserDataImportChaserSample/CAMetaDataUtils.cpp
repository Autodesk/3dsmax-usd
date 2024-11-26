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
#include "CAMetaDataUtils.h"

#include <MaxUsd/Utilities/TranslationUtils.h>

PXR_NAMESPACE_USING_DIRECTIVE

enum CAMetaDataCodes
{
    UnableToCreateMetadataObject
};
TF_REGISTRY_FUNCTION(TfEnum)
{
    TF_ADD_ENUM_NAME(
        UnableToCreateMetadataObject, "Unable to define built-in USD Metadata object.");
};

namespace caMetaData {

IMetaDataManager::MetaDataID getOrDefineCABuiltInMetaData(
    const std::vector<std::pair<caMetaData::UsdMetaDataTypeCA, caMetaData::ParameterValue>>& cas)
{
    if (cas.empty()) {
        return EmptyMetaDataID;
    }

    IMetaDataManager*                      maxMetaDataManager = IMetaDataManager::GetInstance();
    Tab<IMetaDataManager::ParamDescriptor> tabParams;

    for (const auto& ca : cas) {
        auto def = getCAMetaDataDef(ca.first, ca.second.key);
        tabParams.Append(1, &def.usdMetaDataParamDef);
    }

    TSTR                         errMsg = nullptr;
    IMetaDataManager::MetaDataID usdBuiltInMetaData
        = maxMetaDataManager->CreateMetaDataDefinition(_T("USD"), _T("USD"), tabParams, &errMsg);
    if (usdBuiltInMetaData == EmptyMetaDataID) {
        TF_ERROR(UnableToCreateMetadataObject, MaxUsd::MaxStringToUsdString(errMsg).c_str());
    }

    return usdBuiltInMetaData;
}

} // namespace caMetaData