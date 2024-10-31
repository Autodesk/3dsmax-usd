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
/* ********************************************************************************************************
    @param Desc             : Set offline, we do not run the viewport performance tests from the pipeline.
    @param Status           : Offline
******************************************************************************************************** */

TEST_DATA_DIR = @"{folder with usd files to profile}"

FileIn "helpers/usd_test_utils.mxs"
FileIn "helpers/fps_profiler.mxs"

-- Make sure the USD plugin is properly loaded.
setupUsdPlugins()

-- Profiles the viewport performance of USD animations defined in the layers found in TEST_DATA_DIR.
struct viewport_performance_test
(  
    private 
    testDataDir = TEST_DATA_DIR,
    
    public 
    function setup =
    (
        resetmaxfile #noprompt
    ),
        
    function test_viewport_performance = (
        local files = #()

        join files (sort (getFiles (testDataDir + "*.usd*")))
            
        if (files.count == 0) then (
            print "Found no USD layers to profile."
        )    
        else (
            for usdLayer in files do 
            (
                profile_viewport_performance usdLayer
            )
        )
    ),
    
    function teardown = (
    ),

    Tests = #(test_viewport_performance)
)

runUsdTestFixture viewport_performance_test (getThisScriptFilename())
