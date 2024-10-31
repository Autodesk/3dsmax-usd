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

#include <ifnpub.h>
#include <memory>

/**
* \brief Mock object for 3ds Max's FPInterface interface.
* \remarks Mock behaviors should be supported by Google Mock (in conjunction
* with Google Test), but as of this writing the version of the library
* delivered through Artifactory contains an defect which prevents compilation
* of Mock objects. Given that no other 3ds Max project use the Google Mocks as
* part of their test setup, the Baking to Texture tool is currently the only
* project affected by this. Given the limited resource available, updating the
* Google Mock library (in parallel with the Google Test library) constitutes
* an overhead that the Baking to Texture team cannot afford at this time.
* \remarks To reproduce this defect:
*  * Create a traditional Google Mock class in the test project.
*  * Link against "gmock_main" or "gmock_maind".
*  * Add the "TEST_LINKED_AS_SHARED_LIBRARY" compilation flag to the project.
*  * Build the test project.
*  * Check compilation warnings.
*/
class MockFPInterface : public FPInterface
{
public:
	std::unique_ptr<FPInterfaceDesc> mockDesc = std::make_unique<FPInterfaceDesc>();
	MockFPInterface() { mockDesc->ID = defaultInterfaceID; }
	FPInterfaceDesc* GetDesc() { return mockDesc.get(); }
public:
	/**
	* The following members inherited from the FPInterface interface are not implemented.
	* Their return values should not be considered, and can cause undefined
	* side-effects.
	*/
	FPStatus Invoke(FunctionID /*fid*/, TimeValue /*t*/ = 0, FPParams* /*params*/ = NULL) { return 0; }
	FPStatus Invoke(FunctionID /*fid*/, FPParams* /*params*/ = NULL) { return 0; }
	FPStatus Invoke(FunctionID /*fid*/, TimeValue /*t*/, FPValue& /*result*/, FPParams* /*params*/ = NULL) { return 0; }
	FPStatus Invoke(FunctionID /*fid*/, FPValue& /*result*/, FPParams* /*params*/ = NULL) { return 0; }
	FunctionID FindFn(const MCHAR* /*name*/) { return 0; }
	BOOL IsEnabled(FunctionID /*actionID*/) { return FALSE; }
	BOOL IsChecked(FunctionID /*actionID*/) { return FALSE; }
	BOOL IsVisible(FunctionID /*actionID*/) { return FALSE; }
	FunctionID	GetIsEnabled(FunctionID /*actionID*/) { return 0; }
	FunctionID	GetIsChecked(FunctionID /*actionID*/) { return 0; }
	FunctionID	GetIsVisible(FunctionID /*actionID*/) { return 0; }
	ActionTable* GetActionTable() { return NULL; }
	void EnableActions(BOOL /*onOff*/) { }

protected:
	Interface_ID defaultInterfaceID = Interface_ID(0x1337, 0x0000);
};