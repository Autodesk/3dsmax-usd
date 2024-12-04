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
#include <max.h>

HINSTANCE hInstance;

BOOL WINAPI DllMain(HINSTANCE hinstDLL,ULONG fdwReason,LPVOID /*lpvReserved*/)
{
	if( fdwReason == DLL_PROCESS_ATTACH )
	{
		// Hang on to this DLL's instance handle.
		hInstance = hinstDLL;
		DisableThreadLibraryCalls(hInstance);
	}
	return(TRUE);
}

TCHAR* GetString(int id)
{
	static TCHAR buf[256];

	if (hInstance)
		return LoadString(hInstance, id, buf, sizeof(buf)) ? buf : NULL;
	return nullptr;
}

__declspec( dllexport ) const TCHAR* LibDescription()
{
	return L"System test utility for the USD plugin.";
}

__declspec(dllexport) int LibNumberClasses()
{
	return 1;
}

__declspec(dllexport) ClassDesc* LibClassDesc(int i)
{
	switch (i) {
	case 0: return GetTestGUPDesc();
	default: return nullptr;
	}
}

__declspec( dllexport ) ULONG LibVersion()
{
	return VERSION_3DSMAX;
}

__declspec( dllexport ) int LibInitialize(void)
{
	return TRUE;
}

__declspec( dllexport ) int LibShutdown(void)
{
	return TRUE;
}