--
-- Copyright 2026 Autodesk
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

FileIn "helpers/usd_test_utils.mxs"

-- Make sure the USD plugin is properly loaded.
setupUsdPlugins()

struct render_setup_test
(
    private
    test_python_path,

    public
    function setup =
    (
        quickReset()

        thisFileName = getSourceFileName()
        test_python_path = pathConfig.removePathLeaf thisFileName
        pysys = python.import("sys")
        if pysys.path.__contains__(test_python_path) then (
            return false
        ) else (
            pysys.path.insert 0 test_python_path
        )
    ),

    function test_render_setup =
    (
        -- Run only on 3ds Max 2027 and up (adsk_abi >= 2027 = MAX_VERSION_MAJOR >= 29)
        maxver = maxversion()
        if maxver[1] >= 29000 then (
            python.Execute "from importlib import reload"
            python.Execute "import render_setup_test"
            python.Execute "reload(render_setup_test)"
            rs_test = python.import("render_setup_test")
            results = rs_test.run_tests()

            processPythonUnittestResults results
        )
    ),

    function teardown = (
        pysys = python.import("sys")
        if pysys.path.__contains__(test_python_path) then (
            pysys.path.remove(test_python_path)
        )
    ),

    Tests = #(
        test_render_setup
    )
)

-- Disabled: USD Render Setup is switched off (see 'AdskRenderSetupEnabled').
-- runUsdTestFixture render_setup_test (getThisScriptFilename())
