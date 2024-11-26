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

struct prim_reader_test
(  
    private 
    test_python_path,
    initialUnits,
    initialScale,

    public 
    function setup =
    (
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        thisFileName = getSourceFileName()
        test_python_path = pathConfig.removePathLeaf thisFileName
        pysys = python.import("sys")
        if pysys.path.__contains__(test_python_path) then (
            return false
        ) else (
            pysys.path.insert 0 test_python_path
        )
    ),
    
    function prim_reader_test = 
    (
        -- Reload the test module, useful in development. 
        python.Execute "from importlib import reload"
        python.Execute "import prim_reader_test"
        python.Execute "reload(prim_reader_test)"
        -- Run tests.
        test = python.import("prim_reader_test")
        results = test.run_tests()
        
        processPythonUnittestResults results
    ), 
    function teardown = (
        pysys = python.import("sys")
        if pysys.path.__contains__(test_python_path) then (
            pysys.path.remove(test_python_path)
        )
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),
    
    Tests = #(
        prim_reader_test
    )
)

runUsdTestFixture prim_reader_test (getThisScriptFilename())