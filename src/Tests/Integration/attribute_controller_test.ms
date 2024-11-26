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

output_prefix = pathConfig.appendPath (getDir #temp) "/USD_ATTR_CONTROLLER_TEST_"
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
    
    function attr_controller_types_test = (
                
        animated_values_file = (testDataDir + @"attribute_contol.usda")
        local stageObject = USDStageObject()
        stageObject.SetRootLayer animated_values_file

        local floatCtrl = USDFloatController()
        local intToFloatCtrl = USDFloatController() -- through cast

        local point3Ctrl = USDPoint3Controller()
        local color3ToPoint3Ctrl = USDPoint3Controller() -- through cast
        local point2toPoint3Ctrl = USDPoint3Controller() -- test Point3 support of 2 dimension values.

        local point4Ctrl = USDPoint4Controller()
        local color4ToPoint4Ctrl = USDPoint4Controller() -- through cast
        local color3toPoint4Ctrl = USDPoint4Controller() -- test Point4 support of 3 dimension values.
        
        -- Test defaults.
        assert_float_equal 0 floatCtrl.value
        assert_point3_equal (Point3 0 0 0) point3Ctrl.value
        assert_point4_equal (Point4 0 0 0 0) point4Ctrl.value
                
        local cubePrimPath = "/root/Cube"
        
        floatCtrl.USDStage = stageObject
        floatCtrl.AttrPath = "/root/Cube.floatVal"
        
        intToFloatCtrl.USDStage = stageObject
        intToFloatCtrl.AttrPath = "/root/Cube.intVal"
        
        point3Ctrl.USDStage = stageObject
        point3Ctrl.AttrPath = "/root/Cube.float3Val"
        
        color3ToPoint3Ctrl.USDStage = stageObject
        color3ToPoint3Ctrl.AttrPath = "/root/Cube.color3Val"
        
        point2toPoint3Ctrl.USDStage = stageObject
        point2toPoint3Ctrl.AttrPath = "/root/Cube.float2Val"
        
        point4Ctrl.USDStage = stageObject
        point4Ctrl.AttrPath = "/root/Cube.float4Val"
        
        color4ToPoint4Ctrl.USDStage = stageObject
        color4ToPoint4Ctrl.AttrPath = "/root/Cube.color4Val"
        
        color3toPoint4Ctrl.USDStage = stageObject
        color3toPoint4Ctrl.AttrPath = "/root/Cube.color3Val"
        
        -- Test values at the first 3 frames.
        
        sliderTime = 0
        assert_float_equal 10 floatCtrl.value
        assert_float_equal 77 intToFloatCtrl.value
        assert_point3_equal (Point3 11 10 10) point3Ctrl.value
        assert_point3_equal (Point3 0.5 0 0) color3ToPoint3Ctrl.value
        assert_point3_equal (Point3 1 2 0) point2toPoint3Ctrl.value
        assert_point4_equal (Point4 11 10 10 5) point4Ctrl.value
        assert_point4_equal (Point4 0 1 0 0.25) color4ToPoint4Ctrl.value
        assert_point4_equal (Point4 0.5 0 0 0) color3toPoint4Ctrl.value
        
        sliderTime = 1
        assert_float_equal 20 floatCtrl.value
        assert_float_equal 88 intToFloatCtrl.value
        assert_point3_equal (Point3 22 20 10) point3Ctrl.value
        assert_point3_equal (Point3 0 0.5 0) color3ToPoint3Ctrl.value
        assert_point3_equal (Point3 10 11 0) point2toPoint3Ctrl.value
        assert_point4_equal (Point4 22 20 10 10) point4Ctrl.value
        assert_point4_equal (Point4 1 0 0 0.50) color4ToPoint4Ctrl.value
        assert_point4_equal (Point4 0 0.5 0 0) color3toPoint4Ctrl.value
        
        sliderTime = 2
        assert_float_equal 30 floatCtrl.value
        assert_float_equal 99 intToFloatCtrl.value
        assert_point3_equal (Point3 33 30 10) point3Ctrl.value
        assert_point3_equal (Point3 0 0 0.5) color3ToPoint3Ctrl.value
        assert_point3_equal (Point3 20 22 0) point2toPoint3Ctrl.value
        assert_point4_equal (Point4 33 30 10 15) point4Ctrl.value
        assert_point4_equal (Point4 0 0 1 0.75) color4ToPoint4Ctrl.value
        assert_point4_equal (Point4 0 0 0.5 0) color3toPoint4Ctrl.value

    ),
    
    function attr_controller_notif_changed_test = (
                
        animated_values_file = (testDataDir + @"attribute_contol.usda")
        local stageObject = USDStageObject()
        stageObject.SetRootLayer animated_values_file

        local floatCtrl = USDFloatController()
    
        assert_float_equal 0 floatCtrl.value
        floatPath = "/root/Cube.floatVal"
        floatCtrl.USDStage = stageObject
        floatCtrl.AttrPath = floatPath
        assert_float_equal 10 floatCtrl.value
            
        -- Test unwiring the params individually.
        floatCtrl.USDStage = undefined
        assert_float_equal 0 floatCtrl.value
        floatCtrl.USDStage = stageObject
        assert_float_equal 10 floatCtrl.value
                
        floatCtrl.AttrPath = ""
        assert_float_equal 0 floatCtrl.value
        floatCtrl.AttrPath = floatPath
        assert_float_equal 10 floatCtrl.value
        
        -- Test invalid values (invidually)
        floatCtrl.AttrPath = "foo"
        assert_float_equal 0 floatCtrl.value
        floatCtrl.AttrPath = floatPath
        floatCtrl.USDStage = Box()
        assert_float_equal 0 floatCtrl.value
    ),    
    
    function attr_controller_save_load = (
        
        local testSphere = sphere pos:[0,0,10]
        animated_values_file = (testDataDir + @"attribute_contol.usda")
    
        local stageObject = USDStageObject()
        stageObject.SetRootLayer animated_values_file

        local floatCtrl = USDFloatController()
        testSphere.radius.controller = floatCtrl
        floatCtrl.USDStage = stageObject
        floatCtrl.AttrPath = "/root/Cube.floatVal"
        
        local maxSceneSavePath = output_prefix + "sphere_float_radius_controller.max"
        saveMaxFile maxSceneSavePath quiet:true
        
        resetMaxFile #noprompt
        
        -- Load the save scene, and make sure the controller still behaves..
        loadMaxFile maxSceneSavePath useFileUnits:true quiet:true
        
        local savedSphere = $Sphere001
        sliderTime = 0
        assert_equal 10 savedSphere.radius
        sliderTime = 1
        assert_equal 20 savedSphere.radius
        sliderTime = 2
        assert_equal 30 savedSphere.radius
        
    ),
    
    function attr_controller_anim_offset = (
        
        local testSphere = sphere pos:[0,0,10]
        animated_values_file = (testDataDir + @"attribute_contol.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer animated_values_file

        -- Offset the animation by 10 frames.
        stageObject.AnimationMode = 1
        stageObject.CustomAnimationStartFrame = 10
        
        local floatCtrl = USDFloatController()
        testSphere.radius.controller = floatCtrl
        floatCtrl.USDStage = stageObject
        floatCtrl.AttrPath = "/root/Cube.floatVal"
        
        -- Validate that the offset is taken into account...
        sliderTime = 0
        assert_equal 10 testSphere.radius
        sliderTime = 10
        assert_equal 10 testSphere.radius
        sliderTime = 11
        assert_equal 20 testSphere.radius
        sliderTime = 12
        assert_equal 30 testSphere.radius
    ),
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(
        attr_controller_types_test,
        attr_controller_notif_changed_test,
        attr_controller_save_load,
        attr_controller_anim_offset
    )
)

runUsdTestFixture usd_xformable_controller_test(getThisScriptFilename())