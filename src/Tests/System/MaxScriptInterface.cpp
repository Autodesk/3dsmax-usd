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
#include <Max.h>
#include <maxscript/maxscript.h>
#include <maxscript/util/listener.h>

#include "TestGUP.h"
#include <gtest/gtest.h>

#include "MaxIntegrationTestListener.h"

// Simple Maxscript interface to allow running the tests from Maxscript.
class MaxScriptInterface : public FPStaticInterface
{
public:
	enum
	{
		fnIdRunTests
	};

	void RunTests();

	DECLARE_DESCRIPTOR_INIT(MaxScriptInterface)
	BEGIN_FUNCTION_MAP
		VFN_0(fnIdRunTests, RunTests)
	END_FUNCTION_MAP
};

const Interface_ID FP_MAXSCRIPT_INTERFACE(0x67001746, 0x4ff4055f);

static MaxScriptInterface fpMaxScriptInterface(
	FP_MAXSCRIPT_INTERFACE, GetTestGUPDesc()->InternalName(), 0, GetTestGUPDesc(), 0,
	MaxScriptInterface::fnIdRunTests, _T("RunTests"), 0, TYPE_VOID, 0, 0,
	p_end
);

void MaxScriptInterface::init() {
	// Register a listener, which will collect test errors and report them
	// via the MxsUnitReporter.
	auto& listeners = testing::UnitTest::GetInstance()->listeners();
	listeners.Append(new MaxIntegrationTestListener());
}

void MaxScriptInterface::RunTests() {
	// Setup GTEST arguments so that all found tests are executed.
	std::vector<char*> argv;
    argv.push_back(const_cast<char*>("--gtest_filter=*"));
	int argc = static_cast<int>(argv.size());

	::testing::InitGoogleTest(&argc, argv.data());
	auto result = RUN_ALL_TESTS();
}
