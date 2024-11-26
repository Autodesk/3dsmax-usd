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

output_prefix = pathConfig.appendPath (getDir #temp) "/USD_CAMERA_OBJECT_TEST_"
deleteTestFiles(output_prefix + "*.*")
    
struct usd_camera_object_test
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
    ),
    
    function generatecameras_test = (
    
        animated_camera_file = (testDataDir + @"animated_camera.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer animated_camera_file
        
        -- Test toggling the option.
        stageObject.GenerateCameras = true
        assert_defined $PhysCamera001
        stageObject.GenerateCameras = false
        assert_undefined $PhysCamera001
        stageObject.GenerateCameras = true
        assert_defined $PhysCamera001
        
        local cameraPrimPath = "/root/PhysCamera001"
        
        -- Test the camera's setup.
        local cam = $PhysCamera001
        assert_equal cam.USDStage stageObject
        assert_equal cam.CameraPrimPath cameraPrimPath 
        
        -- Camera should have a transform controller following the camera prim.
        assert_equal (classOf cam.transform.controller) USDXformableController
        assert_equal cam.transform.controller.USDStage stageObject
        assert_equal cam.transform.controller.XformablePrimPath cameraPrimPath
        
        -- Cameras are not (easily) deletable - it is blocked from the transform controller.
        assert_true cam.transform.controller.PreventsNodeDeletion
        delete cam
        assert_true (IsValidNode cam)
    ),

    function rootfile_change_test = (
        
        -- File with a single usd camera (PhysCamera001)
        animated_camera_file = (testDataDir + @"animated_camera.usda")
        -- File with two usd cameras (Cam1 and Cam2)
        animated_camera_file_2 = (testDataDir + @"animated_camera_2.usda")
        
        no_camera_file = (testDataDir + @"cube_native.usda")
        
        local stageObject = USDStageObject()
        
        local sceneCams = cameras as Array
        assert_equal sceneCams.Count 0
        
        stageObject.GenerateCameras = true
        stageObject.SetRootLayer animated_camera_file
        
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 1
        assert_defined $PhysCamera001
        
        stageObject.SetRootLayer animated_camera_file_2
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        assert_defined $Cam2
        
        stageObject.SetRootLayer no_camera_file
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 0
        
        stageObject.SetRootLayer animated_camera_file
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 1
        assert_defined $PhysCamera001
    ),
    
    function deactivated_cameras = (
        
        -- File with two usd cameras (Cam1 and Cam2)
        animated_camera_file_2 = (testDataDir + @"animated_camera_2.usda")
                
        local stageObject = USDStageObject()
        stageObject.SetRootLayer animated_camera_file_2
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        assert_defined $Cam2
        
        local cam2 = $Cam2
        
        -- Toggle the activate state of Cam1 (in this case, camera is still part of traversal, but inactive)
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        local cam1Prim = stage.GetPrimAtPath("/root/Cam1")
        
        cam1Prim.SetActive false
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 1
        assert_defined $Cam2
        -- Also make sure that we didnt rebuild Cam2, it is the same as before
        assert_equal cam2 $Cam2
            
        cam1Prim.SetActive true
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        assert_defined $Cam2
        
        -- Toggle the active state of /root, in this case, the cameras are not even part of the traversal.
        local rootPrim = stage.GetPrimAtPath("/root")
        
        rootPrim.SetActive false
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 0
        
        rootPrim.SetActive true
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        assert_defined $Cam2
    ),
    
    -- Test that the camera node(s) are cleaned up if the stage object is removed from the scene.
    function delete_and_undo_test = (

        animated_camera_file = (testDataDir + @"animated_camera_2.usda")
        
        undo on (
            local stageObject = USDStageObject()
            stageObject.SetRootLayer animated_camera_file        
        )
            
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        assert_defined $Cam2
        
        -- Remove the object via undo, cameras removed.
        max undo
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 0
        
        -- Readd via redo, cameras back.
        max redo
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        assert_defined $Cam2
        
        -- Delete the stage, removes cameras.
        undo on (
            delete stageObject
        )
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 0
        
        -- Undo delete, readds cameras.
        max undo
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        assert_defined $Cam2
        
        -- Redo delete, removes cameras again.
        max redo
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 0            
    ),
       
    -- Seperate test for a specific undo pattern which previously caused a crash.
    function undo_ref_file_change = (

        animated_camera_file = (testDataDir + @"animated_camera_2.usda")
        
        -- Mimic the USD Reference macro
        undo on (
            local stageObject = USDStageObject()
            stageObject.SetRootLayer animated_camera_file        
        )
        
        local sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        
        no_cam_file = (testDataDir + @"cube_native.usda")
        undo on (
            stageObject.SetRootLayer no_cam_file
        )
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 0
        
        max undo 
        
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        
        max undo 
        
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 0
        
        max undo 
        
        local allObjects = objects as Array
        assert_equal allObjects.Count 0
    ),
    
    -- Another undo scenario that has previously caused crashes.
    function camera_node_select_undo_redo = (
        
        -- Here, the camera is selected in an undoable op. 
        -- As everything is undone and redone, it is crucial that the same exact cameras
        -- are re-added, and not newly rebuilt from scratch-- Otherwise when attempting to redo 
        -- the selection, that would crash.
        
        animated_camera_file = (testDataDir + @"animated_camera_2.usda")
        undo on (
            local stageObject = USDStageObject()
            stageObject.SetRootLayer animated_camera_file        
        )
            
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        
        undo on (
            select $Cam1
        )
        
        max undo -- undo selection 
        max undo -- undo stage setup
        max redo -- redo stage setup
        max redo -- redo selection of Cam1
        
        assert_equal selection[1] $Cam1 
    ),
    
    function stage_instancing_test = (
    
        animated_camera_file = (testDataDir + @"animated_camera_2.usda")
        
        undo on (
            local stageObject = USDStageObject()
            stageObject.SetRootLayer animated_camera_file
        )
        
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        assert_defined $Cam2
                
        with undo on (
            instancedStage = create_clone stageObject #instance "stageInstance"
        )
        
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 4
        assert_equal sceneCams[1].Name "Cam1"
        assert_equal sceneCams[2].Name "Cam2"
        assert_equal sceneCams[3].Name "Cam1"
        assert_equal sceneCams[4].Name "Cam2"
        
        max undo
        
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        assert_defined $Cam2
    ),
    
    function stage_copy_test = (
    
        animated_camera_file = (testDataDir + @"animated_camera_2.usda")
        
        undo on (
            local stageObject = USDStageObject()
            stageObject.SetRootLayer animated_camera_file
        )
        
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        assert_defined $Cam2
                
        with undo on (
            instancedStage = create_clone stageObject #copy "stageCopy"
        )
        
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 4
        assert_equal sceneCams[1].Name "Cam1"
        assert_equal sceneCams[2].Name "Cam2"
        assert_equal sceneCams[3].Name "Cam1"
        assert_equal sceneCams[4].Name "Cam2"
        
        max undo
        
        sceneCams = cameras as Array
        assert_equal sceneCams.Count 2
        assert_defined $Cam1
        assert_defined $Cam2
    ),    

    -- Tests that adding a modifier onto cameras doesnt interfere with the 
    -- management of cameras (top of the mod stack will not be a USDCameraObject,
    -- but a derived object).
    function modifier_on_camera_test = (
    
        animated_camera_file = (testDataDir + @"animated_camera.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer animated_camera_file
      
        local cam = $PhysCamera001
        assert_defined cam
        
        mirrorMod = Mirror()
        addModifier cam mirrorMod
        
        delete stageObject
        assert_false (IsValidNode cam)
    ),        
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(
        generatecameras_test,
        rootfile_change_test,
        deactivated_cameras,
        delete_and_undo_test,
        stage_instancing_test,
        stage_copy_test,
        undo_ref_file_change,
        camera_node_select_undo_redo,
        modifier_on_camera_test
    )
)

runUsdTestFixture usd_camera_object_test(getThisScriptFilename())
