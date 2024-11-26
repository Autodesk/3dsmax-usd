--
-- Copyright 2023 Autodesk
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--

fileIn "helpers/usd_test_utils.mxs"

-- Make sure the USD plugin is properly loaded.
setupUsdPlugins()
clearlistener()

-- Load up the system test GUP.
scriptPath = getThisScriptFilename()
testPath = (pathConfig.removePathLeaf scriptPath) + @"\SystemTestGup\"
loadDllsFromDir testPath "*.gup"

struct system_tests
(
    public
    function setup =
    (
        resetmaxfile #noprompt
    ),

    function test_system = (
        USDSystemTests.RunTests()
    ),
    
    function teardown = (
        -- Nothing to do.
    ),

    tests = #(test_system)
)

runUsdTestFixture system_tests (getThisScriptFilename())
