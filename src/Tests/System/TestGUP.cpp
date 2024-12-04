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
#include "TestGUP.h"

#include <Max.h>

#define TESTGUP_CLASS_ID Class_ID(0x17f35bb1, 0x3e874149)

extern TCHAR* GetString(int id);

class TestGUPClassDesc : public ClassDesc2
{
public:
    int          IsPublic() override { return TRUE; }
    void*        Create(BOOL /*loading = FALSE*/) override { return new TestGUP(); }
    const TCHAR* ClassName() override { return L"USDSystemTests"; }

    SClass_ID SuperClassID() override { return GUP_CLASS_ID; }
    Class_ID  ClassID() override { return TESTGUP_CLASS_ID; }

    const TCHAR* Category() override { return _T(""); }

    const TCHAR* InternalName() override { return _T("USDSystemTests"); }

    HINSTANCE HInstance() override { return hInstance; }

    int NumActionTables() override { return 0; }

    const MCHAR* NonLocalizedClassName() override { return L"TestGUP"; }
};

ClassDesc2* GetTestGUPDesc()
{
    static TestGUPClassDesc desc;
    return &desc;
}

TestGUP::TestGUP() { }

void TestGUP::NotifyProc(void* /*param*/, NotifyInfo* info) { }

DWORD TestGUP::Start() { return GUPRESULT_KEEP; }

void TestGUP::Stop() { }

void TestGUP::DeleteThis() { delete this; }
