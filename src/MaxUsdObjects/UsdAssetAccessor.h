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
#include "Objects/USDStageObject.h"

#include <assetmanagement/IAssetAccessor.h>

class USDAssetAccessor : public IAssetAccessor
{
public:
    USDAssetAccessor(USDStageObject* refHolder);
    ~USDAssetAccessor();

    virtual MaxSDK::AssetManagement::AssetUser GetAsset() const;
    virtual bool SetAsset(const MaxSDK::AssetManagement::AssetUser& aNewAssetUser);
    virtual MaxSDK::AssetManagement::AssetType GetAssetType() const;
    virtual const MCHAR*                       GetAssetDesc() const;
    virtual const MCHAR*                       GetAssetClientDesc() const;

private:
    USDStageObject* usdObject;
};

class USDItemPostLoadCB : public PostLoadCallback
{
public:
    USDItemPostLoadCB(USDStageObject* usdStageObject) { usdObject = usdStageObject; }
    void proc(ILoad* iload)
    {
        UNREFERENCED_PARAMETER(iload);
        using namespace MaxSDK::AssetManagement;
        if (nullptr == usdObject) {
            return;
        }

        IParamBlock2* pblock = usdObject->GetParamBlock(0);
        AssetUser     assetFile(
            pblock ? pblock->GetAssetUser(PBParameterIds::StageFile)
                   : MaxSDK::AssetManagement::AssetUser());
        MSTR resolvedPath;
        if (pblock && assetFile.GetFullFilePath(resolvedPath)) {
            pblock->SetValue(PBParameterIds::StageFile, 0, resolvedPath.ToMCHAR());
        }
        usdObject->UpdateViewportStageIcon();
        usdObject->LoadUSDStage();
        delete this;
    }

private:
    USDStageObject* usdObject;
};
