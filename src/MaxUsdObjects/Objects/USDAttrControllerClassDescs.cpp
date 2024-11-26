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

#include "USDAttrControllerClassDescs.h"

#include "USDAttrControllers.h"

#include <MaxUsdObjects/DLLEntry.h>
#include <MaxUsdObjects/Views/UsdControllerWidget.h>
#include <MaxUsdObjects/resource.h>

void* USDFloatControllerClassDesc::Create(BOOL) { return new USDFloatController(); }

const TCHAR* USDFloatControllerClassDesc::ClassName()
{
    return GetString(IDS_USDFLOATCONTROLLER_CLASS_NAME);
}

const TCHAR* USDFloatControllerClassDesc::NonLocalizedClassName()
{
    return _T("USDFloatController");
}

SClass_ID USDFloatControllerClassDesc::SuperClassID() { return CTRL_FLOAT_CLASS_ID; }

Class_ID USDFloatControllerClassDesc::ClassID() { return USDFLOATCONTROLLER_CLASS_ID; }

const TCHAR* USDFloatControllerClassDesc::InternalName() { return _T("USDFloatController"); }

ClassDesc2* GetUSDFloatControllerClassDesc()
{
    static USDFloatControllerClassDesc classDesc;
    return &classDesc;
}

void* USDPoint3ControllerClassDesc::Create(BOOL) { return new USDPoint3Controller(); }

const TCHAR* USDPoint3ControllerClassDesc::ClassName()
{
    return GetString(IDS_USDPOINT3CONTROLLER_CLASS_NAME);
}

const TCHAR* USDPoint3ControllerClassDesc::NonLocalizedClassName()
{
    return _T("USDPoint3Controller");
}

SClass_ID USDPoint3ControllerClassDesc::SuperClassID() { return CTRL_POINT3_CLASS_ID; }

Class_ID USDPoint3ControllerClassDesc::ClassID() { return USDPOINT3CONTROLLER_CLASS_ID; }

const TCHAR* USDPoint3ControllerClassDesc::InternalName() { return _T("USDPoint3Controller"); }

ClassDesc2* GetUSDPoint3ControllerClassDesc()
{
    static USDPoint3ControllerClassDesc classDesc;
    return &classDesc;
}

void* USDPoint4ControllerClassDesc::Create(BOOL) { return new USDPoint4Controller(); }

const TCHAR* USDPoint4ControllerClassDesc::ClassName()
{
    return GetString(IDS_USDPOINT4CONTROLLER_CLASS_NAME);
}

const TCHAR* USDPoint4ControllerClassDesc::NonLocalizedClassName()
{
    return _T("USDPoint4Controller");
}

SClass_ID USDPoint4ControllerClassDesc::SuperClassID() { return CTRL_POINT4_CLASS_ID; }

Class_ID USDPoint4ControllerClassDesc::ClassID() { return USDPOINT4CONTROLLER_CLASS_ID; }

const TCHAR* USDPoint4ControllerClassDesc::InternalName() { return _T("USDPoint4Controller"); }

ClassDesc2* GetUSDPoint4ControllerClassDesc()
{
    static USDPoint4ControllerClassDesc classDesc;
    return &classDesc;
}