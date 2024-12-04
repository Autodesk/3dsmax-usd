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

#include "USDTransformControllersClassDesc.h"

#include "USDTransformControllers.h"

#include <MaxUsdObjects/DLLEntry.h>
#include <MaxUsdObjects/Views/UsdControllerWidget.h>
#include <MaxUsdObjects/resource.h>

void* USDXformableControllerClassDesc::Create(BOOL) { return new USDXformableController(); }

const TCHAR* USDXformableControllerClassDesc::ClassName()
{
    return GetString(IDS_USDXFORMCONTROLLER_CLASS_NAME);
}

const TCHAR* USDXformableControllerClassDesc::NonLocalizedClassName()
{
    return _T("USDXformableController");
}

SClass_ID USDXformableControllerClassDesc::SuperClassID() { return CTRL_MATRIX3_CLASS_ID; }

Class_ID USDXformableControllerClassDesc::ClassID() { return USDXFORMABLECONTROLLER_CLASS_ID; }

const TCHAR* USDXformableControllerClassDesc::InternalName()
{
    return _T("USDXformableController");
}

MaxSDK::QMaxParamBlockWidget* USDXformableControllerClassDesc::CreateQtWidget(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock,
    const MapID     paramMapID,
    MSTR&           rollupTitle,
    int&            rollupFlags,
    int&            rollupCategory)
{
    switch (paramMapID) {
    case USDTransformControllerMapID_General: {
        const auto controllerUI = new UsdControllerWidget(owner, paramBlock);
        controllerUI->SetPathErrorMessage(QObject::tr("Invalid Xformable Path: "));
        controllerUI->SetLabel(QObject::tr("Xformable Prim:"));
        controllerUI->SetLabelTooltip(QObject::tr("The path of the xformable used as source."));
        controllerUI->SetPickButtonTooltip(
            QObject::tr("Select the USD stage node that contains the source xformable."));
        rollupTitle = ClassName();
        return controllerUI;
    }
    }
    return nullptr;
}

ClassDesc2* GetUSDXformableControllerClassDesc()
{
    static USDXformableControllerClassDesc classDesc;
    return &classDesc;
}

MaxSDK::QMaxParamBlockWidget* USDPRSControllerClassDesc::CreateQtWidget(
    ReferenceMaker& owner,
    IParamBlock2&   paramBlock,
    const MapID     paramMapID,
    MSTR&           rollupTitle,
    int&            rollupFlags,
    int&            rollupCategory)
{
    switch (paramMapID) {
    case USDTransformControllerMapID_General: {
        const auto controllerUI = new UsdControllerWidget(owner, paramBlock);
        controllerUI->SetPathErrorMessage(QObject::tr("Invalid Path: "));
        controllerUI->SetLabel(QObject::tr("Xformable Prim or Attribute:"));
        controllerUI->SetLabelTooltip(
            QObject::tr("The path of the xformable or attribute used as source."));
        controllerUI->SetPickButtonTooltip(QObject::tr(
            "Select the USD stage node that contains the source attribute or xformable."));
        rollupTitle = ClassName();
        return controllerUI;
    }
    }
    return nullptr;
}

void* USDPositionControllerClassDesc::Create(BOOL) { return new USDPositionController(); }

const TCHAR* USDPositionControllerClassDesc::ClassName()
{
    return GetString(IDS_USDPOSITIONCONTROLLER_CLASS_NAME);
}

const TCHAR* USDPositionControllerClassDesc::NonLocalizedClassName()
{
    return _T("USDPositionController");
}

SClass_ID USDPositionControllerClassDesc::SuperClassID() { return CTRL_POSITION_CLASS_ID; }

Class_ID USDPositionControllerClassDesc::ClassID() { return USDPOSITIONCONTROLLER_CLASS_ID; }

const TCHAR* USDPositionControllerClassDesc::InternalName() { return _T("USDPositionController"); }

ClassDesc2* GetUSDPositionControllerClassDesc()
{
    static USDPositionControllerClassDesc classDesc;
    return &classDesc;
}

void* USDScaleControllerClassDesc::Create(BOOL) { return new USDScaleController(); }

const TCHAR* USDScaleControllerClassDesc::ClassName()
{
    return GetString(IDS_USDSCALECONTROLLER_CLASS_NAME);
}

const TCHAR* USDScaleControllerClassDesc::NonLocalizedClassName()
{
    return _T("USDScaleController");
}

SClass_ID USDScaleControllerClassDesc::SuperClassID() { return CTRL_SCALE_CLASS_ID; }

Class_ID USDScaleControllerClassDesc::ClassID() { return USDSCALECONTROLLER_CLASS_ID; }

const TCHAR* USDScaleControllerClassDesc::InternalName() { return _T("USDScaleController"); }

ClassDesc2* GetUSDScaleControllerClassDesc()
{
    static USDScaleControllerClassDesc classDesc;
    return &classDesc;
}

void* USDRotationControllerClassDesc::Create(BOOL) { return new USDRotationController(); }

const TCHAR* USDRotationControllerClassDesc::ClassName()
{
    return GetString(IDS_USDROTATIONCONTROLLER_CLASS_NAME);
}

const TCHAR* USDRotationControllerClassDesc::NonLocalizedClassName()
{
    return _T("USDRotationController");
}

SClass_ID USDRotationControllerClassDesc::SuperClassID() { return CTRL_ROTATION_CLASS_ID; }

Class_ID USDRotationControllerClassDesc::ClassID() { return USDROTATIONCONTROLLER_CLASS_ID; }

const TCHAR* USDRotationControllerClassDesc::InternalName() { return _T("USDRotationController"); }

ClassDesc2* GetUSDRotationControllerClassDesc()
{
    static USDRotationControllerClassDesc classDesc;
    return &classDesc;
}
