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

#include "UsdCameraObjectclassDesc.h"

#include "USDStageObject.h"
#include "UsdCameraObject.h"

#include <MaxUsdObjects/DLLEntry.h>
#include <MaxUsdObjects/Views/UsdCameraObjectRollup.h>
#include <MaxUsdObjects/resource.h>

#include <MaxUsd/Utilities/TranslationUtils.h>

#include <Qt/QMaxParamBlockWidget.h>
#include <maxscript/foundation/arrays.h>
#include <maxscript/foundation/strings.h>

#include <QVBoxLayout>

namespace {
const std::wstring usdCameraClassName = L"USDCameraObject";
}

int USDCameraObjectclassDesc::IsPublic()
{
    // Don't want users to be able to directly instanciate the USD cameras.
    return false;
}

void* USDCameraObjectclassDesc::Create(BOOL loading) { return new USDCameraObject(); }

const MCHAR* USDCameraObjectclassDesc::ClassName() { return usdCameraClassName.c_str(); }

Class_ID USDCameraObjectclassDesc::ClassID() { return USDCAMERAOBJECT_CLASS_ID; }

const MCHAR* USDCameraObjectclassDesc::InternalName() { return usdCameraClassName.c_str(); }

const MCHAR* USDCameraObjectclassDesc::NonLocalizedClassName()
{
    return usdCameraClassName.c_str();
}

SClass_ID USDCameraObjectclassDesc::SuperClassID() { return CAMERA_CLASS_ID; }

const MCHAR* USDCameraObjectclassDesc::Category() { return GetString(IDS_USD_CATEGORY); }

HINSTANCE USDCameraObjectclassDesc::HInstance() { return hInstance; }

MaxSDK::QMaxParamBlockWidget* USDCameraObjectclassDesc::CreateQtWidget(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock,
    const MapID     paramMapID,
    MSTR&           rollupTitle,
    int&            rollupFlags,
    int&            rollupCategory)
{
    switch (paramMapID) {
    case USDCameraMapID_General: {
        const auto cameraUI = new UsdCameraObjectRollup(owner, paramBlock);
        rollupTitle = GetString(IDS_USDCAMERA_ROLL_OUT);
        return cameraUI;
    }
    }
    return nullptr;
}

ClassDesc2* GetUSDCameraObjectClassDesc()
{
    static USDCameraObjectclassDesc classDesc;
    return &classDesc;
}
