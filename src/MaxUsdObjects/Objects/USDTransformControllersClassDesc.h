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

#include <iparamb2.h>

class USDXformableControllerClassDesc : public USDBaseControllerClassDesc
{
public:
    void*                         Create(BOOL /*loading = FALSE*/) override;
    const TCHAR*                  ClassName() override;
    const TCHAR*                  NonLocalizedClassName() override;
    SClass_ID                     SuperClassID() override;
    Class_ID                      ClassID() override;
    const TCHAR*                  InternalName() override;
    MaxSDK::QMaxParamBlockWidget* CreateQtWidget(
        ReferenceMaker& owner,
        IParamBlock2&   paramBlock,
        const MapID     BparamMapID,
        MSTR&           rollupTitle,
        int&            rollupFlags,
        int&            rollupCategory) override;
};

ClassDesc2* GetUSDXformableControllerClassDesc();

class USDPRSControllerClassDesc : public USDBaseControllerClassDesc
{
    MaxSDK::QMaxParamBlockWidget* CreateQtWidget(
        ReferenceMaker& owner,
        IParamBlock2&   paramBlock,
        const MapID     BparamMapID,
        MSTR&           rollupTitle,
        int&            rollupFlags,
        int&            rollupCategory) override;
};

class USDPositionControllerClassDesc : public USDPRSControllerClassDesc
{
public:
    void*        Create(BOOL /*loading = FALSE*/) override;
    const TCHAR* ClassName() override;
    const TCHAR* NonLocalizedClassName() override;
    SClass_ID    SuperClassID() override;
    Class_ID     ClassID() override;
    const TCHAR* InternalName() override;
};

ClassDesc2* GetUSDPositionControllerClassDesc();

class USDScaleControllerClassDesc : public USDPRSControllerClassDesc
{
public:
    void*        Create(BOOL /*loading = FALSE*/) override;
    const TCHAR* ClassName() override;
    const TCHAR* NonLocalizedClassName() override;
    SClass_ID    SuperClassID() override;
    Class_ID     ClassID() override;
    const TCHAR* InternalName() override;
};

ClassDesc2* GetUSDScaleControllerClassDesc();

class USDRotationControllerClassDesc : public USDPRSControllerClassDesc
{
public:
    void*        Create(BOOL /*loading = FALSE*/) override;
    const TCHAR* ClassName() override;
    const TCHAR* NonLocalizedClassName() override;
    SClass_ID    SuperClassID() override;
    Class_ID     ClassID() override;
    const TCHAR* InternalName() override;
};

ClassDesc2* GetUSDRotationControllerClassDesc();