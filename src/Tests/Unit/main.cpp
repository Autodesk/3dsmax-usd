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
#include <gtest/gtest.h>

#include "Mocks/MockCoreInterface.h"

#if defined(MAX_2022) || defined(MAX_2023) || defined(MAX_2024) || defined(MAX_2025) || defined(MAX_2026)
extern void SetCOREInterface(Interface17* ip);
#endif

int main(int argc, char **argv) {

	// Use a simple diagnostics delegate to fail tests if USD issues any errors or warnings.
	TestUtils::DiagnosticsDelegate del;
	pxr::TfDiagnosticMgr::GetInstance().AddDelegate(&del);
	
	SetCOREInterface(new MockCoreInterface());
	::testing::InitGoogleTest(&argc, argv);
	const auto returnCode =  RUN_ALL_TESTS();

	pxr::TfDiagnosticMgr::GetInstance().RemoveDelegate(&del);
	return returnCode;
}
