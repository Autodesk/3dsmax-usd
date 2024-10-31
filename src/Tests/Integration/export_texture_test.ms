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

FileIn "helpers/usd_test_utils.mxs"

-- Make sure the USD plugin is properly loaded.
setupUsdPlugins()
    
output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_TEXTURE_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_texture_test
(  
    private 
    test_python_path,
   
    public 
    function setup =
    (
        resetmaxfile #noprompt

        thisFileName = getSourceFileName()
        test_python_path = pathConfig.removePathLeaf thisFileName
        pysys = python.import("sys")
        if pysys.path.__contains__(test_python_path) then (
            return false
        ) else (
            pysys.path.insert 0 test_python_path
        )
    ),
    
    function test_usd_texture_export = 
    (
        python.Execute "from importlib import reload"
        python.Execute "import export_texture_test"
        python.Execute "reload(export_texture_test)"
        python.Execute "import usd_material_reader"
        python.Execute "reload(usd_material_reader)"
        python.Execute "import usd_material_writer"
        python.Execute "reload(usd_material_writer)"	
		
        mat_exp_test = python.import("export_texture_test")
        results = mat_exp_test.run_tests()

        processPythonUnittestResults results
    ), 
    function teardown = (
        pysys = python.import("sys")
        if pysys.path.__contains__(test_python_path) then (
            pysys.path.remove(test_python_path)
        )
    ),
    
    Tests = #(
        test_usd_texture_export
    )
)

runUsdTestFixture export_texture_test (getThisScriptFilename())