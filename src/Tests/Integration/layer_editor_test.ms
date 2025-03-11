--
-- Copyright 2024 Autodesk
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

struct layer_editor_test
(  
    private 
    test_setup_path,
    layer_editor_test_path,
   
    public 
    function setup =
    (
        thisFileName = getSourceFileName()
        test_setup_path = pathConfig.removePathLeaf thisFileName
        
        layer_editor_test_path = test_setup_path + "/../../../usd-layer-editor/test"
        
        pysys = python.import("sys")
        if not (pysys.path.__contains__(test_setup_path)) then (
            pysys.path.insert 0 test_setup_path
        )
        if not (pysys.path.__contains__(layer_editor_test_path)) then (
            pysys.path.insert 0 layer_editor_test_path
        )
    ),
    
    function layer_editor_test = 
    (
        local test_found = doesFileExist layer_editor_test_path allowDirectory:true
        if not test_found then (
            print("UsdLayerEditor tests skipped - test file not available.")
            return false
        )
        
        python.Execute "from importlib import reload"
        python.Execute "import layer_editor_test"
        python.Execute "reload(layer_editor_test)"
        python.Execute "import layer_editor_test_setup"
        python.Execute "reload(layer_editor_test_setup)"        
        
        python_test = python.import("layer_editor_test")
        
        -- Configure the test : setting up some DCC hooks to create stage object, undo, redo, etc.
        le_test = python.import("layer_editor_test_setup")
        le_test.setup()
        
        results = python_test.run_tests()
        processPythonUnittestResults results
    ), 
    function teardown = (
        pysys = python.import("sys")
        if pysys.path.__contains__(test_setup_path) then (
            pysys.path.remove(test_setup_path)
        )
        if pysys.path.__contains__(layer_editor_test_path) then (
            pysys.path.remove(layer_editor_test_path)
        )
    ),
    
    Tests = #(
        layer_editor_test
    )
)

runUsdTestFixture layer_editor_test (getThisScriptFilename())