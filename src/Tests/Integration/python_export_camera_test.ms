--
-- Copyright 2025 Autodesk
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

struct python_export_camera_test
(
    private
    test_python_path,

    public
    function setup =
    (
        thisFileName = getSourceFileName()
        test_python_path = pathConfig.removePathLeaf thisFileName
        pysys = python.import("sys")
        if pysys.path.__contains__(test_python_path) then (
            return false
        ) else (
            pysys.path.insert 0 test_python_path
        )
    ),

    function export_camera_properties_test =
    (
        python.Execute "from importlib import reload"
        python.Execute "import python_export_camera_test"
        python.Execute "reload(python_export_camera_test)"
        -- Run tests.
        camera_test = python.import("python_export_camera_test")
        results = camera_test.run_tests()
        processPythonUnittestResults results
    ),

    function teardown = (
        pysys = python.import("sys")
        if pysys.path.__contains__(test_python_path) then (
            pysys.path.remove(test_python_path)
        )
    ),

    Tests = #(
        export_camera_properties_test
    )
)

runUsdTestFixture python_export_camera_test (getThisScriptFilename())
