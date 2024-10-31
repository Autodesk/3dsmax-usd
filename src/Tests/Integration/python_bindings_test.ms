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

struct python_bindings_test
(  
    private 
    test_python_path,
   
    public 
    function setup =
    (
        resetmaxfile #noprompt

    ),
    
    function test_import_all_python_bindings = 
    (
        -- Import all usd python modules.
        python.Execute "from pxr import *"
    ), 
    function teardown = (
    ),
    
    Tests = #(
        test_import_all_python_bindings
    )
)

runUsdTestFixture python_bindings_test (getThisScriptFilename())