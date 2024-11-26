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
#pragma once

#include "MaxUsdObjects/DLLEntry.h"
#include "MaxUsdObjects/Objects/USDStageObject.h"
#include "MaxUsdObjects/resource.h"

#include <osnap.h>
#include <osnaphit.h>

extern Class_ID USDSNAPS_CLASS_ID;

class USDSnaps : public Osnap
{
public:
    // Osnap overrides
    Class_ID     ClassID() override { return USDSNAPS_CLASS_ID; }
    int          numsubs() override;
    MSTR*        snapname(int index) override;
    MSTR         iconName(int index) const override;
    boolean      ValidInput(SClass_ID scid, Class_ID cid) override;
    OsnapMarker* GetMarker(int index) override;
    HBITMAP      getTools() override;
    HBITMAP      getMasks() override;
    WORD         AccelKey(int index) override;
    void         Snap(Object* pobj, IPoint2* p, TimeValue t) override;
    const MCHAR* Category() override;
};

class USDSnapsClassDesc : public ClassDesc2
{
public:
    int IsPublic() override { return TRUE; }

    void* Create(BOOL /*loading = FALSE*/) override { return new USDSnaps(); }

    const TCHAR* ClassName() override { return GetString(IDS_USDSNAPS_CLASS_NAME); }

    const TCHAR* NonLocalizedClassName() override { return _T("USDSnaps"); }

    SClass_ID SuperClassID() override { return OSNAP_CLASS_ID; }

    Class_ID ClassID() override { return USDSNAPS_CLASS_ID; }

    const TCHAR* Category() override { return GetString(IDS_CATEGORY); }

    const TCHAR* InternalName() override { return _T("USDSnaps"); }

    HINSTANCE HInstance() override { return hInstance; }
};

ClassDesc2* GetUSDSnapsClassDesc();