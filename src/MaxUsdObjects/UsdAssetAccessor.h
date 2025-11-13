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

#include <MaxUsdObjects/LayerEditor/USDLayerManager.h>

#include <IParamm2.h>
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

        AssetUser assetFile(
            pblock ? pblock->GetAssetUser(PBParameterIds::StageFile)
                   : MaxSDK::AssetManagement::AssetUser());
        MSTR resolvedPath;

        // Based on the AnonRootId param value saved to the disk, we get
        // the anon layer that maps to it and can recreate the UsdStageObject
        // that was previously saved, in the case of serialized anon layers
        // to disk/max scene.
        const MCHAR* anonRootIdVal = L"";
        pblock->GetValue(PBParameterIds::AnonRootId, 0, anonRootIdVal);
        const auto& oldIDToNewLayerMap = USDLayerManager::Instance()->GetLoadedLayerMap();
        const auto  anonRootId = MaxUsd::MaxStringToUsdString(anonRootIdVal);

        // NOTE: maybe re-use this for when we save dirty non-anon root layers?
        if (oldIDToNewLayerMap.find(anonRootId) != oldIDToNewLayerMap.end()) {
            auto loadedLayer = oldIDToNewLayerMap.at(anonRootId);
            auto mappedLayerAnonId = MaxUsd::UsdStringToMaxString(loadedLayer->GetIdentifier());
            pblock->SetValue(PBParameterIds::AnonRootId, 0, mappedLayerAnonId);

            auto stageFromAnonRoot = pxr::UsdStage::Open(loadedLayer);
            usdObject->SetUSDStage(stageFromAnonRoot);
            usdObject->ApplyLoadedStateFromMax();
        } else if (pblock && assetFile.GetFullFilePath(resolvedPath)) {
            pblock->SetValue(PBParameterIds::StageFile, 0, resolvedPath.ToMCHAR());
            usdObject->LoadUSDStage(MaxUsd::MaxStringToUsdString(resolvedPath), "/");
            usdObject->ApplyLoadedStateFromMax();
        } else {
            // This case happens when you have a UsdStageObject that
            // was saved with the "Save3dsMaxOnly" option and was
            // anonymous at the time of saving. When you reload
            // the associated .max file, this code will ensure that
            // the data that was saved to the .max scene which is
            // associated with the UsdStageObject is reapplied
            // (i.e. session layer, mute/lock state and edit target)
            usdObject->ApplyLoadedStateFromMax();

            // HACK/PROBLEM: For some reason, in the situation where
            // a user saves a .max file with Save3dsMaxOnly option, 
            // then reopens that file, even though the "AnonRootId"
            // gets set via the Constructor -> CreateInMemoryStage()
            // call, when we get the "AnonRootId" above in this
            // function, it's still holding the old value from the
            // previous save. As such, we force an update here, so
            // that, when/if the user resaves the scene with the other
            // options, the correct value will be in "AnonRootId"
            // for subsequent re-loading of the generated .max
            // file.
            auto rootLayer = usdObject->GetUSDStage()->GetRootLayer();
            if (rootLayer->IsAnonymous()) {
                auto rootLayerId = MaxUsd::UsdStringToMaxString(rootLayer->GetIdentifier());
                pblock->SetValue(AnonRootId, GetCOREInterface()->GetTime(), rootLayerId);
            }
        }
        usdObject->UpdateViewportStageIcon();
        delete this;
    }

private:
    USDStageObject* usdObject;
};
