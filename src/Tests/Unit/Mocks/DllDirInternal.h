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

#include <plugin.h>

// Extends class DllDir. The sole instance of DllDirInternal is owned by class App.
class DllDirInternal : public DllDir
{
public:
	// Instantiated as a member of class App
	CoreExport DllDirInternal();
	CoreExport ~DllDirInternal();

	/** Unloads every plug-in DLL 3ds Max has loaded; for internal use only
		Note that plug-ins should not be calling this method as unloading plug-in dlls
		is only supported after the scene has been destroyed. */
	CoreExport void UnloadAllDlls();

	/** Unloads the DlLDesc at the specified index */
	void UnloadADll(int dllIndex);

	// The following methods are used only in core
	/** Registers a DllDesc with the DllDir. 
	\param loadedDllDesc - The DllDesc to register. It's supposed to be loaded.
	\return The index into DllDir where the DllDesc got registered. */
	int RegisterLoadedDllDesc(const DllDesc* loadedDllDesc);

	/** Replaces a deferred DllDesc with its loaded counterpart. 
	\param loadedDllDesc - The DllDesc used to resolve the deferred one. It's supposed to be loaded.
	\param deferredDllIndex - The DllDir index of the DllDesc to be resolved
	\return true if the DllDesc was resolved, false otherwise. */
	bool ResolveDeferredDllDesc(const DllDesc* loadedDllDesc, int deferredDllIndex);

	/** Registers a DllDesc that represents a plug-in Dll proxy.
	\param filePath The full path of the plug-in Dll
	\param description The description string of the plug-in Dll
	\param lastWriteTime The time and date of last modification of the plug-in Dll
	\return The DllDir index where the DllDesc was registered. */
	int RegisterDeferredDllDesc(
		const MaxSDK::Util::Path& filePath, 
		const MCHAR* description, 
		const FILETIME& lastWriteTime);

	/** Finds a DllDesc based on the plug-in DLL's file name and description string.
	A plug-in DLL is considered to be identified uniquely by its file name and description.
	For example, if a plug-in dll has two copies 	in two different folders, they 
	are considered the same from the plug-in DLL registry's 	point of view and only 
	one of them is loaded. If the description of the two plug-in dlls differs, then
	both will be loaded, but only the classes from one of them will get registered
	if they both expose classes that have the same class ids.
	\param fileName - the file name of the plug-in whose DllDesc to find
	\param description - the description of the plug-in whose DllDesc to find
	\return -1 if no DllDesc is found, otherwise the index in the DllDir of the found DllDesc.	 */
	int FindDllDesc(const MCHAR* fileName, const MCHAR* description) const;

	/** Singleton access */
	static DllDirInternal& GetInstance();
};
