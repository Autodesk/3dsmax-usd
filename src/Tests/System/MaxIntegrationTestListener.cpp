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
#include "MaxIntegrationTestListener.h"
#include <sstream>
#include <algorithm>
#include <maxscript/maxscript.h>
#include <maxscript/util/listener.h>

#include <MaxUsd/Utilities/TranslationUtils.h>
#include <MaxUsd/Utilities/MaxSupportUtils.h>
#include <MaxUsd/Utilities/Logging.h>

#define ScriptPrint (the_listener->edit_stream->printf)

MaxIntegrationTestListener::MaxIntegrationTestListener()
{
}

MaxIntegrationTestListener::~MaxIntegrationTestListener()
{
}

void MaxIntegrationTestListener::OnTestStart(const testing::TestInfo& test_info)
{
	std::stringstream ss;
	ss << "Running test ";
	ss << test_info.test_case_name();
	ss << ".";
	ss << test_info.name();
	ss << "...\n";
	ScriptPrint(MaxUsd::UsdStringToMaxString(ss.str()).data());
}

void MaxIntegrationTestListener::OnTestEnd(const testing::TestInfo& test_info)
{
	std::wstring resultMessage;
	bool failed = test_info.result()->Failed();
	std::wstring msg{};
	if (failed)
	{
		std::stringstream ss;
		for (int i = 0; i < test_info.result()->total_part_count(); ++i)
		{
			ss << "\n";
			const auto& part = test_info.result()->GetTestPartResult(i);
			std::string summary = part.summary();
			std::string fileName = (part.file_name() ? std::string(part.file_name()) : "");
			std::replace(fileName.begin(), fileName.end(), '\\', '/');
			ss << fileName << "(" << part.line_number() << "): error: " << summary << "\n";
		}
		msg.append(std::wstring(L"\"") + std::wstring(MaxUsd::UsdStringToMaxString(ss.str()).data()) + std::wstring(L"\""));
		ScriptPrint(L"FAILED.\n");
	}
	MaxAssertTrue(!failed, msg.c_str());
}

// Abstract methods from test::TestEventListener.
void MaxIntegrationTestListener::OnTestProgramStart(const testing::UnitTest& unit_test){}
void MaxIntegrationTestListener::OnTestIterationStart(const testing::UnitTest& unit_test, int iteration){}
void MaxIntegrationTestListener::OnEnvironmentsSetUpStart(const testing::UnitTest& unit_test){}
void MaxIntegrationTestListener::OnEnvironmentsSetUpEnd(const testing::UnitTest& unit_test){}
void MaxIntegrationTestListener::OnTestCaseStart(const testing::TestCase& test_case){}
void MaxIntegrationTestListener::OnTestPartResult(const testing::TestPartResult& result){}
void MaxIntegrationTestListener::OnTestCaseEnd(const testing::TestCase& test_case){}
void MaxIntegrationTestListener::OnEnvironmentsTearDownStart(const testing::UnitTest& unit_test){}
void MaxIntegrationTestListener::OnEnvironmentsTearDownEnd(const testing::UnitTest& unit_test){}
void MaxIntegrationTestListener::OnTestIterationEnd(const testing::UnitTest& unit_test, int iteration){}
void MaxIntegrationTestListener::OnTestProgramEnd(const testing::UnitTest& unit_test){}

// Calls the assert_true function from the AssertReporter module.
void MaxIntegrationTestListener::MaxAssertTrue(bool val, const TCHAR* msg)
{
	static value_cf assert_true_fn = nullptr;  // Acquire pointer to function once.
	if (assert_true_fn == nullptr)
	{
		assert_true_fn = ((Primitive*)globals->get(Name::intern(_T("assert_true")))->eval())->fn_ptr;
	}
	ScopedMaxScriptEvaluationContext scopedMaxScriptEvaluationContext;  // Set up maxscript thread locals.
	MAXScript_TLS* _tls = scopedMaxScriptEvaluationContext.Get_TLS(); // Get thread local storage.
	Value** arg_list;
	value_local_array_tls(arg_list, 4); // Create a local array of list of args, protects them from gc.
	int argCount = 1;
	arg_list[0] = val ? &true_value : &false_value; // The value to test.
	if (msg)
	{ 
		argCount = 4;
		arg_list[1] = &keyarg_marker; // Marks beginning of keyword args.
		arg_list[2] = n_message; // Keyword name.
		arg_list[3] = new String(msg); // Keyword value.
	}
	(*assert_true_fn)(arg_list, argCount); // Make the call!
}