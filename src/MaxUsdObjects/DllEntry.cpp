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

#include "ClassDescs.h"
#include "MaxUsdUfe/UfeUtils.h"
#include "resource.h"

#include <pxr/base/plug/registry.h>

#include <iparamb2.h>
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
    }
    return (TRUE);
}

// This function returns a string that describes the DLL
__declspec(dllexport) const MCHAR* LibDescription() { return GetString(IDS_LIBDESCRIPTION); }

// This function returns the number of plug-in classes this DLL
__declspec(dllexport) int LibNumberClasses() { return GetNumClassDesc(); }

// This function returns the number of plug-in classes this DLL
__declspec(dllexport) ClassDesc* LibClassDesc(int i)
{
    if (i < GetNumClassDesc()) {
        return GetClassDesc(i);
    }

    return nullptr;
}

// This function returns a pre-defined constant indicating the version of
// the system under which it was compiled.  It is used to allow the system
// to catch obsolete DLLs.
__declspec(dllexport) ULONG LibVersion() { return VERSION_3DSMAX; }

// This function is called once, right after your plugin has been loaded by 3ds Max.
// Perform one-time plugin initialization in this method.
// Return TRUE if you deem your plugin successfully loaded, or FALSE otherwise. If
// the function returns FALSE, the system will NOT load the plugin, it will then call FreeLibrary
// on your DLL, and send you a message.
__declspec(dllexport) int LibInitialize(void)
{
    MaxUsd::ufe::initialize();
    return TRUE;
}

// This function is called once, just before the plugin is unloaded.
// Perform one-time plugin un-initialization in this method."
// The system doesn't pay attention to a return value.
__declspec(dllexport) int LibShutdown(void)
{
    MaxUsd::ufe::finalize();
    return TRUE;
}

MCHAR* GetString(int id)
{
    static MCHAR buf[256];

    if (hInstance)
        return LoadStringW(hInstance, id, buf, _countof(buf)) ? buf : NULL;
    return NULL;
}
