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

pyUsd = python.import("pxr.Usd")
pyUsdUtils = python.import("pxr.UsdUtils")
pyGf = python.import("pxr.Gf")

testDataDir = (getFilenamePath (getThisScriptFilename())) + @"data\"

output_prefix = pathConfig.appendPath (getDir #temp) "/USD_XFORM_CONTROLLER_TEST_"
deleteTestFiles(output_prefix + "*.*")
    
struct usd_xformable_controller_test
(  
    private 
    initialUnits,
    initialScale,
    
    public
    function setup =
    (
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        resetMaxFile #noprompt
        units.SystemType = #inches
        units.SystemScale = 1
    ),
    
    function transform_controller_base_test = (
        
        local testSphere = sphere pos:[0,0,10]
        
        animated_box_test_file = (testDataDir + @"usd_xformable_controller.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer animated_box_test_file

        local xformControl = USDXformableController()
                
        testSphere.transform.controller = xformControl
        
        -- No prim bound yet, defaults to origin
        assert_equal testSphere.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
                
        local boxPrimPath = "/root/Box001"
        
        xformControl.USDStage = stageObject
        xformControl.XformablePrimPath = boxPrimPath
        
        -- Tied to the moving box, but still at origin at frame 0.
        assert_equal testSphere.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
        
        -- Make sure the sphere follows along.
        sliderTime = 5
        assert_matrix_equal testSphere.transform (matrix3 [0.793025, 0, -0.609189] [0, 1, 0] [0.609189, 0, 0.793025] [0, -20.3743, 0])
        sliderTime = 0
        assert_matrix_equal testSphere.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
        sliderTime = 10
        assert_matrix_equal testSphere.transform (matrix3 [-4.37114e-8, 0, -1] [0, 1, 0] [1, 0, -4.37114e-8] [0, -48.8582, 0])

        -- Test unwiring the params individually.
        xformControl.USDStage = undefined
        assert_equal testSphere.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
        xformControl.USDStage = stageObject
        assert_matrix_equal testSphere.transform (matrix3 [-4.37114e-8, 0, -1] [0, 1, 0] [1, 0, -4.37114e-8] [0, -48.8582, 0])
        
        xformControl.XformablePrimPath = ""
        assert_equal testSphere.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
        xformControl.XformablePrimPath = boxPrimPath
        assert_matrix_equal testSphere.transform (matrix3 [-4.37114e-8, 0, -1] [0, 1, 0] [1, 0, -4.37114e-8] [0, -48.8582, 0])
        
        -- Test invalid values (invidually)
        xformControl.XformablePrimPath = "foo"
        assert_equal testSphere.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
        xformControl.XformablePrimPath = boxPrimPath
        xformControl.USDStage = Box()
        assert_equal testSphere.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
        xformControl.USDStage = stageObject
    ),
    
    function transform_controller_save_load = (
        
        local testSphere = sphere pos:[0,0,10]
        
        animated_box_test_file = (testDataDir + @"usd_xformable_controller.usda")
        
        -- Bind to the xform of box001, and to the max scene.
        local stageObject = USDStageObject()
        stageObject.SetRootLayer animated_box_test_file

        local xformControl = USDXformableController()
        testSphere.transform.controller = xformControl
        
        local boxPrimPath = "/root/Box001"
        xformControl.USDStage = stageObject
        xformControl.XformablePrimPath = boxPrimPath
        
        local maxSceneSavePath = output_prefix + "sphere_xform_controller.max"
        saveMaxFile maxSceneSavePath quiet:true
        
        resetMaxFile #noprompt
        
        -- Load the save scene, and make sure the controller still behaves..
        loadMaxFile maxSceneSavePath useFileUnits:true
        
        local savedSphere = $Sphere001
        
        sliderTime = 5
        assert_matrix_equal savedSphere.transform (matrix3 [0.793025, 0, -0.609189] [0, 1, 0] [0.609189, 0, 0.793025] [0, -20.3743, 0])
        sliderTime = 0
        assert_equal savedSphere.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
        sliderTime = 10
        assert_matrix_equal savedSphere.transform (matrix3 [-4.37114e-8, 0, -1] [0, 1, 0] [1, 0, -4.37114e-8] [0, -48.8582, 0])
    ),
    
    function transform_controller_anim_offset = (
        
        local testSphere = sphere pos:[0,0,10]
        animated_box_test_file = (testDataDir + @"usd_xformable_controller.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer animated_box_test_file

        -- Offset the animation by 10 frames.
        stageObject.AnimationMode = 1
        stageObject.CustomAnimationStartFrame = 10
        
        local xformControl = USDXformableController()
                
        testSphere.transform.controller = xformControl
                
        local boxPrimPath = "/root/Box001"
        
        xformControl.USDStage = stageObject
        xformControl.XformablePrimPath = boxPrimPath
        
        -- Validate that the offset is taken into account...
        sliderTime = 0
        assert_equal testSphere.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
        sliderTime = 5
        assert_equal testSphere.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
        sliderTime = 10
        assert_equal testSphere.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])    -- new start of animation...
        sliderTime = 15
        assert_matrix_equal testSphere.transform (matrix3 [0.793025, 0, -0.609189] [0, 1, 0] [0.609189, 0, 0.793025] [0, -20.3743, 0])
        sliderTime = 20
        assert_matrix_equal testSphere.transform (matrix3 [-4.37114e-8, 0, -1] [0, 1, 0] [1, 0, -4.37114e-8] [0, -48.8582, 0])
    ),
    
    function transform_controller_prevent_node_delete = (
        local testSphere = sphere pos:[0,0,10]
        animated_box_test_file = (testDataDir + @"usd_xformable_controller.usda")
        
        local aSphere = Sphere()
        local xformControl = USDXformableController()
        assert_false xformControl.PreventsNodeDeletion
        
        xformControl.PreventsNodeDeletion = true
        aSphere.transform.controller = xformControl
        delete aSphere
        assert_true (IsValidNode aSphere)
        
        xformControl.PreventsNodeDeletion = false
        delete aSphere
        assert_false (IsValidNode aSphere)
    ),    
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(
        transform_controller_base_test,
        transform_controller_save_load,
        transform_controller_anim_offset,
        transform_controller_prevent_node_delete
    )
)

runUsdTestFixture usd_xformable_controller_test(getThisScriptFilename())