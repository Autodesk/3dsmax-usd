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

struct usd_explorer_test
(  
    private 
    originalFileFormatOption,
    exportMaterialTestOptions = USDExporter.CreateOptions(),
    
    public 
    function setup =
    (
        resetmaxfile #noprompt
    ),

	function findUsdExplorer = (
		local dialogs = UIAccessor.GetPopupDialogs()
		for d in dialogs do (
			if (UIAccessor.GetWindowText d) == "USD Explorer" then return true
		)
		return false
	),
	
    function test_explorer_stage_open_close = (
        
        local stageObject = USDStageObject()
                
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\box_sample.usda")
        stageObject.SetRootLayer sample
			
		-- Open via the object function.
		stageObject.OpenInUsdExplorer()
		assert_true (findUsdExplorer())
		
		-- Close the tab for the explorer. We only test that the explorer itself is still opened.
		stageObject.CloseInUsdExplorer()
		assert_true (findUsdExplorer())
			
		-- Actually close the explorer.
		USDStageObject.CloseUsdExplorer()
		assert_false (findUsdExplorer())
		
		-- Open the explorer via static funciton.
		USDStageObject.OpenUsdExplorer()
		assert_true (findUsdExplorer())
		
		-- Close it again (from state where it has no stage active)
		USDStageObject.CloseUsdExplorer()
		assert_false (findUsdExplorer())

		-- Close stage that isnt in explorer - no-op.
		stageObject.CloseInUsdExplorer()
		assert_false (findUsdExplorer())
	),
	
    function teardown =
    (
    ),	

    Tests = #(
        test_explorer_stage_open_close
    )
)

runUsdTestFixture usd_explorer_test (getThisScriptFilename())