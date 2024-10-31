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
#include "DLLEntry.h"

#include "USDCore.h"

#include <iparamm2.h>

HINSTANCE hInstance;
int       controlsInit = FALSE;

// This function is called by Windows when the DLL is loaded.  This
// function may also be called many times during time critical operations
// like rendering.  Therefore developers need to be careful what they
// do inside this function.  In the code below, note how after the DLL is
// loaded the first time only a few statements are executed.

BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID /*lpvReserved*/)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        MaxSDK::Util::UseLanguagePackLocale();
        // Hang on to this DLL's instance handle.
        hInstance = hinstDLL;
        DisableThreadLibraryCalls(hInstance);
        // DO NOT do any initialization here. Use LibInitialize() instead.

        // FIXME: See above comment.
        USDCore::initialize();
    }
    return (TRUE);
}

std::wstring GetStdWString(const int id)
{
    LPWSTR    pString;
    const int res = LoadString(hInstance, id, reinterpret_cast<LPWSTR>(&pString), 0);
    if (res == 0) {
        return std::wstring();
    }
    return std::wstring(pString, res);
}

TCHAR* GetString(int id)
{
    static std::wstring s;
    s = GetStdWString(id);
    return const_cast<TCHAR*>(s.data());
}