//
// Copyright 2024 Autodesk
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

#include "USDBaseController.h"

#include <MaxUsdObjects/DLLEntry.h>
#include <MaxUsdObjects/resource.h>

#include <MaxUsd/Utilities/TranslationUtils.h>

#include <inode.h>
#include <iparamm2.h>

#define PBLOCK_REF 0

void USDBaseController::BeginEditParams(IObjParam* ip, ULONG flags, Animatable* prev)
{
    Control::BeginEditParams(ip, flags, prev);
    GetControllerClassDesc()->BeginEditParams(ip, this, flags, prev);
}

void USDBaseController::EndEditParams(IObjParam* ip, ULONG flags, Animatable* next)
{
    Control::EndEditParams(ip, flags, next);
    GetControllerClassDesc()->EndEditParams(ip, this, flags, next);
}

int USDBaseController::NumRefs() { return 1; }

RefTargetHandle USDBaseController::GetReference(int i)
{
    if (i == PBLOCK_REF) {
        return paramBlock;
    }
    return nullptr;
}

void USDBaseController::SetReference(int i, RefTargetHandle rtarg)
{
    if (i == PBLOCK_REF) {
        paramBlock = static_cast<IParamBlock2*>(rtarg);
    }
}

int USDBaseController::NumParamBlocks() { return 1; }

IParamBlock2* USDBaseController::GetParamBlock(int i) { return paramBlock; }

IParamBlock2* USDBaseController::GetParamBlockByID(BlockID id)
{
    return paramBlock->ID() == id ? paramBlock : nullptr;
}

RefResult USDBaseController::NotifyRefChanged(
    const Interval& changeInt,
    RefTargetHandle hTarget,
    PartID&         partID,
    RefMessage      message,
    BOOL            propagate)
{
    switch (message) {
    case REFMSG_CHANGE: {
        const auto pb = dynamic_cast<IParamBlock2*>(hTarget);
        if (!pb) {
            return REF_DONTCARE;
        }
        int     tabIndex = -1;
        ParamID pID = pb->LastNotifyParamID(tabIndex);
        switch (pID) {
        case USDControllerParams_USDStage:
        case USDControllerParams_Path:
            if (!UpdateSource(pb)) {
                return REF_SUCCEED;
            }
            if (const auto map = pb->GetMap()) {
                map->UpdateUI(GetCOREInterface()->GetTime());
            }
            NotifyDependents(FOREVER, 0, REFMSG_CHANGE);
            GetCOREInterface()->RedrawViews(GetCOREInterface()->GetTime());
            break;
        }
        break;
    }
    }
    return REF_SUCCEED;
};

int USDBaseControllerClassDesc::IsPublic() { return TRUE; }

const TCHAR* USDBaseControllerClassDesc::Category() { return GetString(IDS_USD_CATEGORY); }

HINSTANCE USDBaseControllerClassDesc::HInstance() { return hInstance; }

bool USDBaseControllerClassDesc::UseOnlyInternalNameForMAXScriptExposure() { return true; }