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

pyUsd = python.import("pxr.Usd")
pyUsdUtils = python.import("pxr.UsdUtils")
pyGf = python.import("pxr.Gf")

testDataDir = (getFilenamePath (getThisScriptFilename())) + @"data\"

output_prefix = pathConfig.appendPath (getDir #temp) "/USD_PTRS_CONTROLLERS_TEST_"
deleteTestFiles(output_prefix + "*.*")
    
struct usd_prs_controllers_test
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
    
    -- Test assigning controllers to drive the position, rotation, and scale individually.
    -- This confirms that the full transform gets built correctly, meaning the controllers
    -- update the matrix they are given correctly (pretranslate, prescale, prerotate).
    function prs_controllers_from_xformable_test = (
        
        local testBox = Box pos:[0,0,10]
        
        prs_test_file = (testDataDir + @"prs_controller_data.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer prs_test_file
        
        local posControl = USDPositionController()        
        testBox.position.controller = posControl
        
        local rotControl = USDRotationController()        
        testBox.rotation.controller = rotControl
        
        local scaleControl = USDScaleController()        
        testBox.scale.controller = scaleControl
        
        
        -- No prim bound yet, defaults to origin
        assert_equal testBox.transform (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
                
        local boxPrimPath = "/root/Box001"
        
        posControl.USDStage = stageObject
        posControl.Path = boxPrimPath
        
        rotControl.USDStage = stageObject
        rotControl.Path = boxPrimPath
        
        scaleControl.USDStage = stageObject
        scaleControl.Path = boxPrimPath
        
        sliderTime = 0
        assert_matrix_equal (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0]) testBox.transform -- CTRL_RELATIVE
        assert_point3_equal (Point3 0 0 0) testBox.position -- CTRL_ABSOLUTE
        assert_quat_equal (Quat 0 0 0 0) testBox.rotation
        assert_point3_equal (Point3 1 1 1) testBox.scale
        sliderTime = 1
        assert_matrix_equal (matrix3 [0.799188,0.344736,0] [-0.344736,0.799188,0] [0,0,0.87037] [12.963,0,0]) testBox.transform
        assert_point3_equal (Point3 12.963 0 0) testBox.position
        assert_quat_equal (Quat 0 0 0.202218 0.979341) testBox.rotation
        assert_point3_equal (Point3 0.87037 0.87037 0.87037) testBox.scale        
        sliderTime = 2
        assert_matrix_equal (matrix3 [0.249384,0.578136,0] [-0.578136,0.249384,0] [0,0,0.62963] [37.037,0,0]) testBox.transform 
        assert_point3_equal (Point3 37.037 0 0) testBox.position
        assert_quat_equal (Quat 0 0 0.549509 0.835488) testBox.rotation
        assert_point3_equal (Point3 0.62963 0.62963 0.62963) testBox.scale        
        sliderTime = 3
        assert_matrix_equal (matrix3 [0,0.5,0] [-0.5,0,0] [0,0,0.5] [50,0,0]) testBox.transform 
        assert_point3_equal (Point3 50 0 0) testBox.position
        assert_quat_equal (Quat 0 0 0.707107 0.707107) testBox.rotation
        assert_point3_equal (Point3 0.5 0.5 0.5) testBox.scale
    ),
    
    function prs_controllers_from_attr_test = (
            
        local testBox = Box pos:[0,0,10]
        
        prs_test_file = (testDataDir + @"prs_controller_data.usda")
        
        local stageObject = USDStageObject()
        stageObject.Reload()
        stageObject.SetRootLayer prs_test_file
        
        local posControl = USDPositionController()        
        testBox.position.controller = posControl
        
        local rotControl = USDRotationController()        
        testBox.rotation.controller = rotControl
        
        local scaleControl = USDScaleController()        
        testBox.scale.controller = scaleControl
        
        posControl.USDStage = stageObject
        posControl.Path = "/root/Box001.posVal"
        
        rotControl.USDStage = stageObject
        rotControl.Path = "/root/Box001.rotVal"
        
        scaleControl.USDStage = stageObject
        scaleControl.Path = "/root/Box001.scaleVal"
        
        -- We already tested the CTRL_RELATIVE code path in prs_controllers_from_xformable_test()
        -- Here, just test using the position/scale/rotation getters.
        sliderTime = 0
        assert_point3_equal (Point3 10 10 10) testBox.position
        assert_quat_equal (Quat 0.707 0 0 0.707) testBox.rotation
        assert_point3_equal (Point3 1 1 1) testBox.scale
        sliderTime = 1
        assert_point3_equal (Point3 20 20 20) testBox.position
        assert_quat_equal (Quat 0.5 0.5 0.5 0.5) testBox.rotation
        assert_point3_equal (Point3 2 2 2) testBox.scale        
        sliderTime = 2
        assert_point3_equal (Point3 30 30 30) testBox.position
        assert_quat_equal (Quat 0.707 0.0 0.707 0.0) testBox.rotation
        assert_point3_equal (Point3 3 3 3) testBox.scale        
        sliderTime = 3
        assert_point3_equal (Point3 40 40 40) testBox.position
        assert_quat_equal (Quat 0.707 0 0 0.707) testBox.rotation
        assert_point3_equal (Point3 4 4 4) testBox.scale
    ),
    
    function rotation_controller_types_test = (
            
        local testBox = Box pos:[0,0,10]
        
        prs_test_file = (testDataDir + @"prs_controller_data.usda")
        
        local stageObject = USDStageObject()
        stageObject.Reload()
        stageObject.SetRootLayer prs_test_file
        
        local rotControl = USDRotationController()        
        testBox.rotation.controller = rotControl
          
        rotControl.USDStage = stageObject
        
        rotControl.Path = "/root/Box001.rotGfQuatf"
        assert_quat_equal (quat 0.1 0.1 0.1 0.5)  testBox.rotation
        rotControl.Path = "/root/Box001.rotGfQuatd"
        assert_quat_equal (quat 0.2 0.2 0.2 0.5)  testBox.rotation
        rotControl.Path = "/root/Box001.rotGfQuath"
        assert_quat_equal (quat 0.3 0.3 0.3 0.5)  testBox.rotation        
    ),
    
    function prs_controllers_save_load = (
        
        local testBox = Box pos:[0,0,10]
        
        prs_test_file = (testDataDir + @"prs_controller_data.usda")
        
        local stageObject = USDStageObject()
        stageObject.Reload()
        stageObject.SetRootLayer prs_test_file
        
        local posControl = USDPositionController()        
        testBox.position.controller = posControl
        local rotControl = USDRotationController()        
        testBox.rotation.controller = rotControl
        local scaleControl = USDScaleController()        
        testBox.scale.controller = scaleControl
        
        -- Control PRS with the xformable box.
        posControl.USDStage = stageObject
        posControl.Path = "/root/Box001"
        rotControl.USDStage = stageObject
        rotControl.Path = "/root/Box001"
        scaleControl.USDStage = stageObject
        scaleControl.Path = "/root/Box001"
        
        local maxSceneSavePath = output_prefix + "prs_controllers_save.max"
        saveMaxFile maxSceneSavePath quiet:true
        
        resetMaxFile #noprompt
        
        -- Load the save scene, and make sure the controllers still behaves..
        loadMaxFile maxSceneSavePath useFileUnits:true
        
        local savedBox = $Box001
        
        sliderTime = 0
        assert_matrix_equal (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0]) savedBox.transform
        sliderTime = 1
        assert_matrix_equal (matrix3 [0.799188,0.344736,0] [-0.344736,0.799188,0] [0,0,0.87037] [12.963,0,0]) savedBox.transform
        sliderTime = 2
        assert_matrix_equal (matrix3 [0.249384,0.578136,0] [-0.578136,0.249384,0] [0,0,0.62963] [37.037,0,0]) savedBox.transform 
        sliderTime = 3
        assert_matrix_equal (matrix3 [0,0.5,0] [-0.5,0,0] [0,0,0.5] [50,0,0]) savedBox.transform 
    ),

    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(
        rotation_controller_types_test,
        prs_controllers_from_xformable_test,
        prs_controllers_from_attr_test,
        prs_controllers_save_load
    )
)

runUsdTestFixture usd_prs_controllers_test(getThisScriptFilename())