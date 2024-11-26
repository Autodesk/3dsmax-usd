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
#include "USDAssetAccessor.h"

#include "dllentry.h"
#include "resource.h"

USDAssetAccessor::USDAssetAccessor(USDStageObject* refHolder)
    : usdObject(refHolder)
{
}

USDAssetAccessor::~USDAssetAccessor() { }

MaxSDK::AssetManagement::AssetUser USDAssetAccessor::GetAsset() const
{
    IParamBlock2* pb = usdObject->GetParamBlock(0);
    return pb ? pb->GetAssetUser(PBParameterIds::StageFile) : MaxSDK::AssetManagement::AssetUser();
}

bool USDAssetAccessor::SetAsset(const MaxSDK::AssetManagement::AssetUser& aNewAssetUser)
{
    IParamBlock2*                    pb = usdObject->GetParamBlock(0);
    MaxSDK::AssetManagement::AssetId assetId = aNewAssetUser.GetId();
    if (!pb) {
        return false;
    }
    pb->SetValue(PBParameterIds::StageFile, 0, assetId);
    return true;
}

MaxSDK::AssetManagement::AssetType USDAssetAccessor::GetAssetType() const
{
    return MaxSDK::AssetManagement::kOtherAsset;
}

const MCHAR* USDAssetAccessor::GetAssetDesc() const { return GetString(IDS_USDSTAGE_ASSET_DESC); }

const MCHAR* USDAssetAccessor::GetAssetClientDesc() const { return GetString(IDS_LIBDESCRIPTION); }
