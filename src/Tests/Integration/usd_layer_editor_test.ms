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

struct usd_layer_editor_test
(  
    function setup =
    (
        resetmaxfile #noprompt
    ),

    function findUsdLayerEditor = (
        local dialogs = UIAccessor.GetPopupDialogs()
        for d in dialogs do (
            if (UIAccessor.GetWindowText d) == "USD Layer Editor" then return true
        )
        return false
    ),
    
    function test_usd_layer_editor_open_close = (
        
        local stageObject = USDStageObject()
                
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\box_sample.usda")
        stageObject.SetRootLayer sample
            
        -- Open via the object function.
        stageObject.OpenInUsdLayerEditor()
        assert_true (findUsdLayerEditor())
            
        USDStageObject.CloseUsdLayerEditor()
        assert_false (findUsdLayerEditor())
        
        -- Open the via static function.
        USDStageObject.OpenUsdLayerEditor()
        assert_true (findUsdLayerEditor())
        
        USDStageObject.CloseUsdLayerEditor()
        assert_false (findUsdLayerEditor())
    ),
    
    function teardown =
    (
    ),    

    Tests = #(
        test_usd_layer_editor_open_close
    )
)

runUsdTestFixture usd_layer_editor_test (getThisScriptFilename())