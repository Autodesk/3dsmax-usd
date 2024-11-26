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

#include <MaxUsd/Utilities/MaxSupportUtils.h>

#include <iparamb2.h>

class USDStageObjectclassDesc : public ClassDesc2
{
public:
    int          IsPublic() override;
    void*        Create(BOOL loading = FALSE) override;
    const MCHAR* ClassName() override;
    const MCHAR* NonLocalizedClassName() override;
    Class_ID     ClassID() override;

    const MCHAR* InternalName() override;

    HINSTANCE HInstance() override;

    MaxSDK::QMaxParamBlockWidget* CreateQtWidget(
        ReferenceMaker& owner,
        IParamBlock2&   paramBlock,
        const MapID     BparamMapID,
        MSTR&           rollupTitle,
        int&            rollupFlags,
        int&            rollupCategory) override;

    SClass_ID    SuperClassID() override;
    const MCHAR* Category() override;
};

ClassDesc2* GetUSDStageObjectClassDesc();