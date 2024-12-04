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

-- Some useful python modules for USD validation.
pyUsd = python.import("pxr.Usd")
pyUsdGeom = python.import("pxr.UsdGeom")
pyUsdTf = python.import("pxr.Tf")
pyUsdGf = python.import("pxr.Gf")

output_prefix = pathConfig.appendPath (getDir #temp) "/IO_CAMERA_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct io_camera_test
(  
    private 
    exportOptions = USDExporter.CreateOptions(),
    initialUnits,initialScale,
    public 
    function resetMaxAndUnitSetup = (
        resetmaxfile #noprompt
        units.SystemType = #Inches
        units.SystemScale = 1
    ),

    function setup =
    (
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        resetMaxAndUnitSetup()
    ),

    function usdTimeToFrame usdTime stage = (
        return usdTime * ((ticksPerFrame as float)/ 4800.f) / stage.GetTimeCodesPerSecond()
    ),
    
    -- Export all instanciable camera types.
    function export_all_cameras = (
        local allCameras = #()
        allCameras = for o in camera.classes where o.creatable collect o

        local expectedCameras = #()
        for i = 1 to allCameras.count do
        (
            -- Align the cameras in a grid for easy visual inspection...
            local x = (mod i 10) * 100
            local y = ((i / 10) as integer) * 100
            local cam = (allCameras[i] height:10 length:10 width:10 pos:[x, y, 0])
            append expectedCameras cam
        )
        
        local exportPath = output_prefix + "export_all_cameras.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        root = stage.GetPrimAtPath("/")
        assert_equal expectedCameras.count (root.GetAllChildren()).count message:"Unexpected prim count when exporting all camera types."
    
        for i = 1 to expectedCameras.count do (
            local name = expectedCameras[i].name
            local objPrim = stage.GetPrimAtPath("/" + pyUsdTf.MakeValidIdentifier(name))
            assert_true (pyUsd.Prim.IsValid(objPrim)) message:(name + " of class " + ((classOf expectedCameras[i]) as string) + " was not properly exported to a USD camera.")
            assert_true (objPrim.IsA(pyUsdGeom.Camera)) message:(name + " of class " + ((classOf expectedCameras[i]) as string) + " should have been exported to a Camera type prim.")
        )
    ),
    
    function test_free_camera_attributes =
    (
        b = box height:20 width:20 lenght:20 pos:[25, 25, 0]
        local cam = (FreeCamera height:10 length:10 width:10 pos:[100, 100, 0] target:b)
        
        -- enable clipping planes
        cam.clipManually = true
        cam.near_clip = 10.0f
        cam.far_clip = 1000.0f
        
        -- make sure all params are initialized
        redrawViews()

        local exportPath = output_prefix + "export_free_camera.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local objPrim =  stage.GetPrimAtPath("/" + cam.name)
        local usdCamera = pyUsdGeom.Camera(objPrim)
        local usdGfCamera = usdCamera.GetCamera()

        assert_equal pyUsdGf.Camera.Perspective usdGfCamera.projection message:"Expected 'projection' type to be 'Perspective'"
        assert_float 0.0f usdGfCamera.focusDistance message:"Expected 'focusDistance' to be equal to default USD value"
        
        -- the FOV value covers the horizontal aperture and focal lenght
        local usdGfCameraFOV = usdGfCamera.frustum.GetFOV()
        assert_float cam.fov usdGfCameraFOV message:"Expected FOV value is not equal to free camera setting"
        -- clipping plane range
        local usdGfCameraClipRange = usdGfCamera.clippingRange
        assert_float cam.near_clip (usdGfCameraClipRange.GetMin()) message:("Expected minimum value for 'clippingRange' to be equal to free camera setting")
        assert_float cam.far_clip (usdGfCameraClipRange.GetMax()) message:("Expected maximum value for 'clippingRange' to be equal to free camera setting")

        -- keep track of expected values on import 
        local camFOV = cam.fov
        local camClipNear = cam.near_clip
        local camClipFar = cam.far_clip
            
        -- now the import test
        -- Free Camera only define the following attributes
        --    clippingRange
        --    focalLength
        --    horizontalAperture
        --    projection
        --    verticalAperture
        -- all other unspecified attributes will report the Physical Camera defaults
        resetMaxAndUnitSetup()
        USDImporter.ImportFile exportPath
        
        local importedCamera = $Camera001
        assert_equal Physical (classof importedCamera) message:("Imported camera from USD is expected to be a 'Physical' camera type")
        assert_float 160.0f (importedCamera.target_distance) message:"Expected 'focusDistance' to be equal to default free camera setting"
        assert_float camFOV (importedCamera.fov) message:"Expected FOV value is not equal to free camera setting"
        assert_false (importedCamera.specifyfov) message:"Expected 'Specify FOV' option to be disabled"
        assert_true (importedCamera.clip_on) message:("The clip range property is defined in the imported USD file - The camera 'clip_on' property must be on")
        assert_float camClipNear (importedCamera.clip_near) message:("The near clip camera property value is expected to be equal to what it was originally set")
        assert_float camClipFar (importedCamera.clip_far) message:("The far clip camera property value is expected to be equal to what it was originally set")
        -- defaults from Physical Camera because USD attributes were not specified otherwise
        assert_float 1.f (importedCamera.zoom_factor) message:("No zoom factor should be applied by default on imported USD cameras")
        assert_float 0.f (importedCamera.lens_breathing_amount) message:("No lens breathing amount value should be applied by default on imported USD cameras")
        assert_equal 3 (importedCamera.shutter_unit_type) message:("Expected shutter unit type to be set to 'Frames'")
        assert_float 0.5f (importedCamera.shutter_length_frames) message:("The shutter duration property value is is to the default to physical camera setting")
        assert_false (importedCamera.shutter_offset_enabled) message:("The shutter offset shall be disabled when value is not defined in the USD file")
        assert_float 0.0f (importedCamera.shutter_offset_frames) message:("The default shutter offset property value is expected when not specified in the USD file")
        assert_float 8f (importedCamera.f_number) message:("Expected 'fStop' default value set to '8' when not specified in the USD file")
        assert_float 0f (importedCamera.horizontal_shift) message:("The default horizontal aperture offset is not equal to '0' when value is not used in the USD file")
        assert_float 0f (importedCamera.vertical_shift) message:("The default vertical aperture offset is not equal to '0' when value is not used in the USD file")
    ),

    function test_target_camera_attributes =
    (
        b = box height:20 width:20 lenght:20 pos:[25, 25, 0]
        local cam = (TargetCamera height:10 length:10 width:10 pos:[100, 100, 0] target:b)
        
        -- enable clipping planes
        cam.clipManually = true
        cam.near_clip = 10.0f
        cam.far_clip = 1000.0f
        
        -- make sure all params are initialized
        redrawViews()

        local exportPath = output_prefix + "export_target_camera.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local objPrim =  stage.GetPrimAtPath("/" + cam.name)
        local usdCamera = pyUsdGeom.Camera(objPrim)
        local usdGfCamera = usdCamera.GetCamera()

        assert_equal pyUsdGf.Camera.Perspective usdGfCamera.projection message:"Expected 'projection' type to be 'Perspective'"
        assert_float cam.targetdistance usdGfCamera.focusDistance message:"Expected 'focusDistance' is not equal to free camera target setting"
        
        -- the FOV value covers the horizontal aperture and focal lenght
        local usdGfCameraFOV = usdGfCamera.frustum.GetFOV()
        assert_float cam.fov usdGfCameraFOV message:"Expected FOV value is not equal to free camera setting"
        -- clipping plane range
        local usdGfCameraClipRange = usdGfCamera.clippingRange
        assert_float cam.near_clip (usdGfCameraClipRange.GetMin()) message:("Expected minimum value for 'clippingRange' to be equal to free camera setting")
        assert_float cam.far_clip (usdGfCameraClipRange.GetMax()) message:("Expected maximum value for 'clippingRange' to be equal to free camera setting")

        -- keep track of expected values on import
        local camFOV = cam.fov
        local camTargetDistance = usdGfCamera.focusDistance  -- parameter is coming from USD
        local camClipNear = cam.near_clip
        local camClipFar = cam.far_clip
            
        -- now the import test
        -- Target Camera only define the following attributes
        --    clippingRange
        --    focalLength
        --    focusDistance
        --    horizontalAperture
        --    projection
        --    verticalAperture
        -- all other unspecified attributes will report the Physical Camera defaults
        resetMaxAndUnitSetup()
            
        USDImporter.ImportFile exportPath
        
        local importedCamera = $Camera001
        assert_equal Physical (classof importedCamera) message:("Imported camera from USD is expected to be a 'Physical' camera type")
        assert_float camTargetDistance (importedCamera.target_distance) message:"Expected 'focusDistance' is not equal to target camera target setting"
        assert_float camFOV (importedCamera.fov) message:"Expected FOV value is not equal to target camera setting"
        assert_false (importedCamera.specifyfov) message:"Expected 'Specify FOV' option to be disabled"
        assert_true (importedCamera.clip_on) message:("The clip range property is defined in the imported USD file - The camera 'clip_on' property must be on")
        assert_float camClipNear (importedCamera.clip_near) message:("The near clip camera property value is expected to be equal to the value it was originally set to")
        assert_float camClipFar (importedCamera.clip_far) message:("The far clip camera property value is expected to be equal to the value it was originally set to")
        -- defaults from Physical Camera because USD attributes were not specified otherwise
        assert_float 1.f (importedCamera.zoom_factor) message:("No zoom factor should be applied by default on imported USD cameras")
        assert_float 0.f (importedCamera.lens_breathing_amount) message:("No lens breathing amount value should be applied by default on imported USD cameras")
        assert_equal 3 (importedCamera.shutter_unit_type) message:("Expected shutter unit type to be set to 'Frames'")
        assert_float 0.5f (importedCamera.shutter_length_frames) message:("The shutter duration property value is is to the default to physical camera setting")
        assert_false (importedCamera.shutter_offset_enabled) message:("The shutter offset shall be disabled when value is not defined in the USD file")
        assert_float 0.0f (importedCamera.shutter_offset_frames) message:("The default shutter offset property value is expected when not specified in the USD file")
        assert_float 8f (importedCamera.f_number) message:("Expected 'fStop' default value set to '8' when not specified in the USD file")
        assert_float 0f (importedCamera.horizontal_shift) message:("The default horizontal aperture offset is not equal to '0' when value is not used in the USD file")
        assert_float 0f (importedCamera.vertical_shift) message:("The default vertical aperture offset is not equal to '0' when value is not used in the USD file")
    ),
    
    function test_default_physical_camera_attributes =
    (
        phys_camera = Physical height:10 length:10 width:10
        -- prevent lens breathing from having an influence on the focal length
        -- this is the ONLY non-default attribute for the physical camera
        phys_camera.lens_breathing_amount = 0.0f
        -- shutter offset is disabled by default
        -- setting a value different from 0f to this parameter shall not have any influence on USD export default for shutter:open attribute
        assert_false phys_camera.shutter_offset_enabled message:("Expected not to enable the shutter offset attribute")
        phys_camera.shutter_offset_frames = 0.2f

        local exportPath = output_prefix + "export_default_physical_camera.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local objPrim =  stage.GetPrimAtPath("/" + phys_camera.name)
        local usdCamera = pyUsdGeom.Camera(objPrim)
        local usdGfCamera = usdCamera.GetCamera()

        assert_equal pyUsdGf.Camera.Perspective usdGfCamera.projection message:"Expected 'projection' type to be 'Perspective'"
        assert_float phys_camera.target_distance usdGfCamera.focusDistance message:"Expected 'focusDistance' is not equal to physical camera target setting"
        assert_float phys_camera.focal_length_mm usdGfCamera.focalLength message:"Expected 'focalLength' is not equal to physical camera setting"
        
        -- the FOV value covers the horizontal aperture and zoom values
        local usdGfCameraFOV = usdGfCamera.frustum.GetFOV()
        assert_float phys_camera.fov usdGfCameraFOV message:"Expected FOV value is not equal to physical camera setting"

        -- the clipping plane range was not specified nor enabled at the
        -- physical camera creation, therefor, the default USD camera values
        -- should be in effect
        assert_false phys_camera.clip_on message:("Expected physical camera not to enable the clipping planes by default")
        local usdGfCameraClipRange = usdGfCamera.clippingRange
        assert_float 1.0f (usdGfCameraClipRange.GetMin()) message:("Expected default minimum value for 'clippingRange' when setting is not explicitly exported")
        assert_float 1000000f (usdGfCameraClipRange.GetMax()) message:("Expected default maximum value for 'clippingRange' when setting is not explicitly exported")

        -- other verified values from the physical camera
        assert_float phys_camera.f_number usdGfCamera.fStop message:("'fStop' value is not equal to physical camera setting")
        local shutterOpenAttr = usdCamera.GetShutterOpenAttr()
        assert_float 0.0f (shutterOpenAttr.Get()) message:("Default 'shutter:open' value is not 0.0f when physical camera shutter offset if not used")
        local shutterCloseAttr = usdCamera.GetShutterCloseAttr()
        assert_float phys_camera.shutter_length_frames (usdTimeToFrame (shutterCloseAttr.Get()) stage) message:("'shutter:close' value is not equal to physical camera setting")
        local exposureAttr = usdCamera.GetExposureAttr()
        assert_float phys_camera.exposure_value (exposureAttr.Get()) message:("'exposure' value is not equal to physical camera setting")
        local horizontalApertureOffsetAttr = usdCamera.GetHorizontalApertureOffsetAttr()
        assert_float 0f (horizontalApertureOffsetAttr.Get()) message:("Expected default horizontal aperture offset to be at 0.0f when setting is not explicitly exported")
        local verticalApertureOffsetAttr = usdCamera.GetVerticalApertureOffsetAttr()
        assert_float 0f (verticalApertureOffsetAttr.Get()) message:("Expected default vertical aperture offset to be at 0.0f when setting is not explicitly exported")

        -- keep track of expected values on import
        local camFOV = phys_camera.fov
        local camTargetDistance = usdGfCamera.focusDistance  -- parameter is coming from USD
            
        -- now the import test
        resetMaxAndUnitSetup()
            
        USDImporter.ImportFile exportPath
        
        local importedCamera = $PhysCamera001
        assert_equal Physical (classof importedCamera) message:("Imported camera from USD is expected to be a 'Physical' camera type")
        assert_float camTargetDistance (importedCamera.target_distance) message:"Expected 'focusDistance' is not equal to physical camera setting"
        assert_float camFOV (importedCamera.fov) message:"Expected FOV value is not equal to physical camera setting"
        assert_false (importedCamera.specifyfov) message:"Expected 'Specify FOV' option to be disabled"
        assert_false (importedCamera.clip_on) message:("The clip range property is not defined in the imported USD file - The camera 'clip_on' property must be off")
        assert_float 1.f (importedCamera.zoom_factor) message:("No zoom factor should be applied by default on imported USD cameras")
        assert_float 0.f (importedCamera.lens_breathing_amount) message:("No lens breathing amount value should be applied by default on imported USD cameras")
        assert_equal 3 (importedCamera.shutter_unit_type) message:("Expected shutter unit type to be set to 'Frames'")
        assert_float 0.5f (importedCamera.shutter_length_frames) message:("The shutter duration property value is not equal to physical camera setting")
        assert_false (importedCamera.shutter_offset_enabled) message:("The shutter offset shall be disabled when value is not defined in the USD file")
        assert_float 0.0f (importedCamera.shutter_offset_frames) message:("The default shutter offset property value is expected when not specified in the USD file")
        assert_float 8f (importedCamera.f_number) message:("Expected 'fStop' default value set to '8' when not specified in the USD file")
        assert_float 0f (importedCamera.horizontal_shift) message:("The default horizontal aperture offset is not equal to '0' when value is not used in the USD file")
        assert_float 0f (importedCamera.vertical_shift) message:("The default vertical aperture offset is not equal to '0' when value is not used in the USD file")
    ),

    function test_physical_camera_attributes =
    (
        b = box height:20 width:20 lenght:20 pos:[25, 25, 0]
        phys_camera = Physical target:b pos:[0, -45, 25]
        
        -- Focus distance
        -- instead of using the default target distance setting, use the user specified focus
        phys_camera.specify_focus = 1
        phys_camera.focus_distance = phys_camera.target_distance + 50.f

        -- shutter offset
        phys_camera.shutter_length_frames = 0.6f
        phys_camera.shutter_offset_enabled = true
        phys_camera.shutter_offset_frames = 0.2f

        -- enable clipping planes
        phys_camera.clip_on = true
        phys_camera.clip_near = 10.0f
        phys_camera.clip_far = 300.f

        -- change zoom factor
        -- has an impact on FOV
        phys_camera.zoom_factor = 1.5

        -- fStop
        phys_camera.f_number = 2.8f

        -- Aperture Offset
        phys_camera.horizontal_shift = -25.f
        phys_camera.vertical_shift = -25.f
        
        -- Exposure
        phys_camera.exposure_value = 12.f
        
        -- export physical camera
        local exportPath = output_prefix + "export_physical_camera.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        -- load the USD stage and control camera's parameters
        local stage = pyUsd.Stage.Open(exportPath)
        local objPrim =  stage.GetPrimAtPath("/" + phys_camera.name)
        local usdCamera = pyUsdGeom.Camera(objPrim)
        local usdGfCamera = usdCamera.GetCamera()

        -- Focus distance
        assert_float phys_camera.focus_distance usdGfCamera.focusDistance message:"'focusDistance' is not equal to physical camera focus setting"
        -- Focal length; takes into consideration the lens breathing
        assert_float 40.2568f usdGfCamera.focalLength message:"'focalLength' is not equal to the effective lens focal length"

        -- Field of view
        local usdGfCameraFOV = usdGfCamera.frustum.GetFOV()
        assert_float phys_camera.fov usdGfCameraFOV message:"FOV value is not equal to physical camera setting"
        -- clipping plane range
        local usdGfCameraClipRange = usdGfCamera.clippingRange
        assert_float phys_camera.clip_near (usdGfCameraClipRange.GetMin()) message:("Expected minimum value for 'clippingRange' to be equal to physical camera setting")
        assert_float phys_camera.clip_far (usdGfCameraClipRange.GetMax()) message:("Expected maximum value for 'clippingRange' to be equal to physical camera setting")
        -- fstop
        assert_float phys_camera.f_number usdGfCamera.fStop message:("'fStop' value is not equal to physical camera setting")
        -- shutter open/close
        assert_true phys_camera.shutter_offset_enabled message:("The shutter offset attribute to be enabled on the physical camera")
        local shutterOpenAttr = usdCamera.GetShutterOpenAttr()
        local shutterOpenValue = (usdTimeToFrame (shutterOpenAttr.Get()) stage) 
        assert_float phys_camera.shutter_offset_frames shutterOpenValue message:("'shutter:open' value is not equal to physical camera setting")
        local shutterCloseAttr = usdCamera.GetShutterCloseAttr()
        local shutterCloseValue = (usdTimeToFrame (shutterCloseAttr.Get()) stage)
        assert_float (phys_camera.shutter_length_frames+phys_camera.shutter_offset_frames) shutterCloseValue message:("'shutter:close' value is not equal to physical camera setting")
        -- exposure
        local exposureAttr = usdCamera.GetExposureAttr()
        assert_float phys_camera.exposure_value (exposureAttr.Get()) message:("'exposure' value is not equal to physical camera setting")
        -- Aperture Offset
        -- horizontal 25% offset from 24mm is 6mm
        -- vertical   25% offset from 13mm is 6mm (it considers aspect ratio)
        local horizontalApertureOffsetAttr = usdCamera.GetHorizontalApertureOffsetAttr()
        assert_float 6f (horizontalApertureOffsetAttr.Get()) message:("Horizontal aperture offset is not equal to physical camera setting")
        local verticalApertureOffsetAttr = usdCamera.GetVerticalApertureOffsetAttr()
        assert_float 6f (verticalApertureOffsetAttr.Get()) message:("Vertical aperture offset is not equal to physical camera setting")

        -- keep track of expected values on import
        local camFOV = phys_camera.fov
        local camTargetDistance = usdGfCamera.focusDistance  -- parameter is coming from USD
        local camClipNear = phys_camera.clip_near
        local camClipFar = phys_camera.clip_far
        local camHozizontalShift = phys_camera.horizontal_shift
        local camVerticalShift = phys_camera.vertical_shift
        local camShutterDuration = phys_camera.shutter_length_frames
        local camShutterOffset = phys_camera.shutter_offset_frames
        local camFStop = phys_camera.f_number
        local camExposure = phys_camera.exposure_value
            
        -- now the import test
        resetMaxAndUnitSetup()
            
        USDImporter.ImportFile exportPath
        
        local importedCamera = $PhysCamera001
        assert_equal Physical (classof importedCamera) message:("Imported camera from USD is expected to be a 'Physical' camera type")
        assert_float camTargetDistance (importedCamera.target_distance) message:"Expected 'focusDistance' is not equal to the value specified in the USD file"
        assert_float camFOV (importedCamera.fov) message:"Expected FOV value is not equal to physical camera setting"
        assert_false (importedCamera.specifyfov) message:"Expected 'Specify FOV' option to be disabled"
        assert_true (importedCamera.clip_on) message:("The clip range property is defined in the imported USD file - The camera 'clip_on' property must be on")
        assert_float camClipNear (importedCamera.clip_near) message:("The near clip camera property value is expected to be equal to the value it was originally set to")
        assert_float camClipFar (importedCamera.clip_far) message:("The far clip camera property value is expected to be equal to the value it was originally set to")
        assert_float 1.f (importedCamera.zoom_factor) message:("No zoom factor should be applied by default on imported USD cameras")
        assert_float 0.f (importedCamera.lens_breathing_amount) message:("No lens breathing amount value should be applied by default on imported USD cameras")
        assert_equal 3 (importedCamera.shutter_unit_type) message:("Expected shutter unit type to be set to 'Frames'")
        assert_float camShutterDuration (importedCamera.shutter_length_frames) message:("The shutter duration property value is not equal to the value specified in the USD file")
        assert_true (importedCamera.shutter_offset_enabled) message:("The shutter offset shall be enabled when value is specified in the USD file")
        assert_float camShutterOffset (importedCamera.shutter_offset_frames) message:("The shutter offset property value is not equal to the value specified in the USD file")
        assert_float camFStop (importedCamera.f_number) message:("The 'fStop' value is not equal to the value specified in the USD file")
        assert_float camExposure (importedCamera.exposure_value) message:("Expected 'exposure' value is not equal to the value specified in the USD file")
        assert_float camHozizontalShift (importedCamera.horizontal_shift) message:("Horizontal aperture offset is not equal to the value specified in the USD file")
        assert_float camVerticalShift (importedCamera.vertical_shift) message:("Vertical aperture offset is not equal to the value specified in the USD file")
    ),

    function test_animated_phys_camera_export = (
        
        b = box height:20 width:20 lenght:20 pos:[25, 25, 0]
        phys_camera = Physical target:b pos:[0, -45, 25]
        
        -- Non-animatable properties...
        phys_camera.specify_focus = 1
        phys_camera.clip_on = true
        phys_camera.shutter_offset_enabled = true 
        
        -- Animate every other camera property over 10 frames.
        with animate on (
            
            at time 0 phys_camera.focus_distance = phys_camera.target_distance + 50.f 
            at time 10 phys_camera.focus_distance = phys_camera.target_distance + 100.f 
            
            -- shutter offset
            at time 0 phys_camera.shutter_length_frames = 0.3f
            at time 10 phys_camera.shutter_length_frames = 0.6f
            
            at time 0 phys_camera.shutter_offset_frames = 0.2f
            at time 10 phys_camera.shutter_offset_frames = 0.4f

            -- clipping planes
            at time 0 phys_camera.clip_near = 10.0f
            at time 10 phys_camera.clip_near = 500.0f
            at time 0 phys_camera.clip_far = 300.f
            at time 10 phys_camera.clip_far = 400.f

            -- change zoom factor
            -- has an impact on FOV
            at time 0 phys_camera.zoom_factor = 1.5
            at time 10 phys_camera.zoom_factor = 3.0
            
            -- fStop
            at time 0 phys_camera.f_number = 2.8f
            at time 10 phys_camera.f_number = 5.0f

            -- Aperture Offset
            at time 0 phys_camera.horizontal_shift = -25.f
            at time 10 phys_camera.horizontal_shift = -75.f
            at time 0 phys_camera.vertical_shift = -50.f
            at time 10 phys_camera.vertical_shift = -150.f
            
        )
        
        local animExportOptions = USDExporter.CreateOptions()
        animExportOptions.FileFormat = #ascii
        animExportOptions.RootPrimPath = "/"
        animExportOptions.TimeMode = #frameRange
        animExportOptions.StartFrame = 0
        animExportOptions.EndFrame = 10
        
        -- Export the animated camera.
        local exportPath = output_prefix + "export_animated_physical_camera.usda"
        
        USDExporter.ExportFile exportPath exportOptions:animExportOptions

        -- load the USD stage and control camera's parameters.
        local stage = pyUsd.Stage.Open(exportPath)
        local objPrim =  stage.GetPrimAtPath("/" + phys_camera.name)
        local usdCamera = pyUsdGeom.Camera(objPrim)
        
        -- Expected values for computed attributes.
        expectedHorizontalApertureOffsets = #(6.0, 
            6.0, 
            6.16342,
            6.56522,
            7.06579,
            7.56213,
            8.0,
            8.35922,
            8.63677,
            8.83544,
            8.95741,
            9.0,
            9.0)
        expectedVerticalApertureOffsets = #(12.0,
            12.0,
            12.3268,
            13.1304,
            14.1316,
            15.1243,
            16.0,
            16.7184,
            17.2735,
            17.6709,
            17.9148,
            18.0,
            18.0)
        expectedFocalLengths = #(40.2568,
            40.2568,
            40.2554,
            40.2515,
            40.246,
            40.2396,
            40.2331,
            40.2269,
            40.2214,
            40.2172,
            40.2143,
            40.2133,
            40.2133)

        dataIndex = 1
        -- Test exported values...
        -- We test frames outside of the frame range to make sure the interpolation
        -- works as expected.
        for frame in -1 to 11 do (
            local usdGfCamera = usdCamera.GetCamera(frame)

            -- Focus distance
            assert_float (at time frame phys_camera.focus_distance) usdGfCamera.focusDistance message:("'focusDistance' is not equal to physical camera focus setting at t = " + frame as string)
            -- Focal length; takes into consideration the lens breathing
            assert_float expectedFocalLengths[dataIndex] usdGfCamera.focalLength message:("'focalLength' is not equal to the effective lens focal length at t = " + frame as string)

            -- Field of view
            local usdGfCameraFOV = usdGfCamera.frustum.GetFOV()
            assert_float (at time frame phys_camera.fov) usdGfCameraFOV message:("FOV value is not equal to physical camera setting at t = " + frame as string)
            -- clipping plane range
            local usdGfCameraClipRange = usdGfCamera.clippingRange
            assert_float (at time frame phys_camera.clip_near) (usdGfCameraClipRange.GetMin()) message:("Expected minimum value for 'clippingRange' to be equal to physical camera setting at t = " + frame as string)
            assert_float (at time frame phys_camera.clip_far) (usdGfCameraClipRange.GetMax()) message:("Expected maximum value for 'clippingRange' to be equal to physical camera setting at t = " + frame as string)
            -- fstop
            assert_float (at time frame phys_camera.f_number) usdGfCamera.fStop message:("'fStop' value is not equal to physical camera setting at t = " + frame as string)
            -- shutter open/close
            assert_true phys_camera.shutter_offset_enabled message:("The shutter offset attribute to be enabled on the physical camera at t = " + frame as string)
            local shutterOpenAttr = usdCamera.GetShutterOpenAttr()
            local shutterOpenValue = (usdTimeToFrame (shutterOpenAttr.Get(frame)) stage) 
            
            local shutterOffsetAtFrame = at time frame phys_camera.shutter_offset_frames
            assert_float shutterOffsetAtFrame shutterOpenValue message:("'shutter:open' value is not equal to physical camera setting at t = " + frame as string)
            
            local shutterCloseAttr = usdCamera.GetShutterCloseAttr()
            local shutterCloseValue = (usdTimeToFrame (shutterCloseAttr.Get(frame)) stage)
            assert_float ((at time frame phys_camera.shutter_length_frames)+shutterOffsetAtFrame) shutterCloseValue message:("'shutter:close' value is not equal to physical camera setting at t = " + frame as string)
            -- Aperture Offset
            local horizontalApertureOffsetAttr = usdCamera.GetHorizontalApertureOffsetAttr()
            assert_float expectedHorizontalApertureOffsets[dataIndex] (horizontalApertureOffsetAttr.Get(frame)) message:("Horizontal aperture offset is not equal to physical camera setting at t = " + frame as string)
            local verticalApertureOffsetAttr = usdCamera.GetVerticalApertureOffsetAttr()
            assert_float expectedVerticalApertureOffsets[dataIndex] (verticalApertureOffsetAttr.Get(frame)) message:("Vertical aperture offset is not equal to physical camera setting at t = " + frame as string) 
            
            dataIndex = dataIndex + 1
        )
    ),
    
    -- Camera types which are not physical cameras, are handled slightly differently. 
    -- The free camera is tested above, for animation we only test the target camera
    -- as it covers all the attribute that are set from non-physical cameras.
    function test_animated_target_camera_export =
    (
        b = box height:20 width:20 lenght:20 pos:[25, 25, 0]
        local cam = (TargetCamera height:10 length:10 width:10 pos:[100, 100, 0] target:b)
        
        -- Enable clipping planes
        cam.clipManually = true
        
        -- Animate the clipping, targetDistance and FOV, the supported attributes.
        with animate on (
            at time 0 cam.near_clip = 10.0f
            at time 10 cam.near_clip = 100.0f
            at time 0 cam.far_clip = 1000.0f
            at time 10 cam.far_clip = 2000.0f
            at time 0 cam.fov = 10.0f
            at time 10 cam.fov = 100.0f
            at time 0 cam.targetDistance = 20.0f
            at time 10 cam.targetDistance = 100.0f
        )
        -- make sure all params are initialized
        redrawViews()
        
        local animExportOptions = USDExporter.CreateOptions()
        animExportOptions.FileFormat = #ascii
        animExportOptions.RootPrimPath = "/"
        animExportOptions.TimeMode = #frameRange
        animExportOptions.StartFrame = 0
        animExportOptions.EndFrame = 10 
        
        local exportPath = output_prefix + "export_target_camera_animation.usda"
        USDExporter.ExportFile exportPath exportOptions:animExportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local objPrim =  stage.GetPrimAtPath("/" + cam.name)
        local usdCamera = pyUsdGeom.Camera(objPrim)
        
        -- Test exported values at each frames.
        -- Also tests values outside the range to make sure the interpolation is doing 
        -- its job properly.
        for frame = -1 to 11 do (
            local usdGfCamera = usdCamera.GetCamera(frame)

            assert_equal pyUsdGf.Camera.Perspective usdGfCamera.projection message:("Expected 'projection' type to be 'Perspective' at t = " + frame as string)
            assert_float (at time frame cam.targetdistance) usdGfCamera.focusDistance message:("Expected 'focusDistance' is not equal to free camera target setting at t = " + frame as string)
        
            -- the FOV value covers the horizontal aperture and focal lenght
            local usdGfCameraFOV = usdGfCamera.frustum.GetFOV()
            assert_float (at time frame cam.fov) usdGfCameraFOV message:("Expected FOV value is not equal to free camera setting at t = " + frame as string)
            -- clipping plane range
            local usdGfCameraClipRange = usdGfCamera.clippingRange
            assert_float (at time frame cam.near_clip) (usdGfCameraClipRange.GetMin()) message:("Expected minimum value for 'clippingRange' to be equal to free camera setting at t = " + frame as string)
            assert_float (at time frame cam.far_clip) (usdGfCameraClipRange.GetMax()) message:("Expected maximum value for 'clippingRange' to be equal to free camera setting at t = " + frame as string)
        )
    ),
    
    function test_camera_animation_validity_intervals = (
        b = box height:20 width:20 lenght:20 pos:[25, 25, 0]
        phys_camera = Physical target:b pos:[0, -45, 25]

        -- Animate the zoom in such a way to generate validity intervals.
        with animate on (
            at time 0 phys_camera.zoom_factor = 1.0
            at time 2 phys_camera.zoom_factor = 2.0 -- valid till 4
            at time 4 phys_camera.zoom_factor = 2.0
            at time 6 phys_camera.zoom_factor = 3.0
            at time 8 phys_camera.zoom_factor = 3.0 -- valid till 10 
            at time 10 phys_camera.zoom_factor = 4.0    
        )
        
        -- Export the animiated camera.
        local exportPath = output_prefix + "test_camera_animation_validity_intervals.usda"
        
        local animExportOptions = USDExporter.CreateOptions()
        animExportOptions.FileFormat = #ascii
        animExportOptions.RootPrimPath = "/"
        animExportOptions.TimeMode = #frameRange
        animExportOptions.StartFrame = -1
        animExportOptions.EndFrame = 11
        USDExporter.ExportFile exportPath exportOptions:animExportOptions

        local stage = pyUsd.Stage.Open(exportPath)
        local objPrim =  stage.GetPrimAtPath("/" + phys_camera.name)
        local usdCamera = pyUsdGeom.Camera(objPrim)
        
        local horizontalApertureAttr = usdCamera.GetHorizontalApertureAttr()
        
        local timeSamples = horizontalApertureAttr.GetTimeSamples()
        
        -- From validity intervals, should need no samples for -1,3,7,11. 
        local expectedTimeSamples = #(0,1,2,4,5,6,8,9,10) 
        
        assert_equal expectedTimeSamples.Count timeSamples.Count message:"An unexpected number of time samples were exported given the camera's validity intervals."
        
        for i in 1 to expectedTimeSamples.Count do (
            assert_equal expectedTimeSamples[i] timeSamples[i] message:("Unexpected time sample exported " + timeSamples[i] as string)
        )
    ),

    function test_camera_animation_import_attributes = (
        
        units.SystemType = #Millimeters
        units.SystemScale = 1

        -- Import the animated camera
        local scriptDir = getFilenamePath (getThisScriptFilename())
        local importPath = (scriptDir + @"data\camera_animated.usda")
        
        importOptions = USDImporter.CreateOptions()
        importOptions.StartTimeCode = 0
        importOptions.EndTimeCode = 10
        importOptions.TimeMode = #CustomRange
        USDImporter.ImportFile importPath importOptions:importOptions
        
        local importedCamera = $Camera001
        sliderTime = 0
        assert_equal Physical (classof importedCamera) message:("Imported camera from USD is expected to be a 'Physical' camera type")
        assert_float 5000.0f (importedCamera.target_distance) message:"Expected 'focusDistance' to be equal to the value specified in the USD file"
        assert_float 40.3226 (importedCamera.focal_length_mm) message:"Expected 'focalLength' to be equal to the value specified in the USD file"
        assert_false (importedCamera.specifyfov) message:"Expected 'Specify FOV' option to be disabled"
        assert_float 6.0f (importedCamera.exposure_value) message:"Expected 'exposure' to be equal to the value specified in the USD file"
        assert_true (importedCamera.clip_on) message:("The clip range property is defined in the imported USD file - The camera 'clip_on' property must be on")
        assert_float 200.0 (importedCamera.clip_near) message:("The near clip camera property value is expected to be equal to what it was originally set")
        assert_float 1000.0 (importedCamera.clip_far) message:("The far clip camera property value is expected to be equal to what it was originally set")
        assert_float 1.f (importedCamera.zoom_factor) message:("No zoom factor should be applied by default on imported USD cameras")
        assert_float 0.f (importedCamera.lens_breathing_amount) message:("No lens breathing amount value should be applied by default on imported USD cameras")
        assert_equal 3 (importedCamera.shutter_unit_type) message:("Expected shutter unit type to be set to 'Frames'")
        assert_float 0.5f (importedCamera.shutter_length_frames) message:("The shutter duration property value is expected to be equal to what it was originally set")
        assert_true (importedCamera.shutter_offset_enabled) message:("The shutter offset shall be enabled when value is defined in the USD file")
        assert_float 0.0f (importedCamera.shutter_offset_frames) message:("The shutter offset property value is expected to be equal to what it was originally set")
        assert_float 8f (importedCamera.f_number) message:("Expected 'fStop' value is expected to be equal to what it was originally set")
        assert_float 0f (importedCamera.horizontal_shift) message:("The default horizontal aperture offset is not equal to '0' when value is not used in the USD file")
        assert_float 0f (importedCamera.vertical_shift) message:("The default vertical aperture offset is not equal to '0' when value is not used in the USD file")

        sliderTime = 10
        assert_equal Physical (classof importedCamera) message:("Imported camera from USD is expected to be a 'Physical' camera type")
        assert_float 5000.0f (importedCamera.target_distance) message:"Expected 'focusDistance' to be equal to the value specified in the USD file"
        assert_float 45.8249 (importedCamera.focal_length_mm) message:"Expected 'focalLength' to be equal to the value specified in the USD file"
        assert_float 12.0f (importedCamera.exposure_value) message:"Expected 'exposure' to be equal to the value specified in the USD file"
        assert_false (importedCamera.specifyfov) message:"Expected 'Specify FOV' option to be disabled"
        assert_true (importedCamera.clip_on) message:("The clip range property is defined in the imported USD file - The camera 'clip_on' property must be on")
        assert_float 400.0 (importedCamera.clip_near) message:("The near clip camera property value is expected to be equal to what it was originally set")
        assert_float 800.0 (importedCamera.clip_far) message:("The far clip camera property value is expected to be equal to what it was originally set")
        assert_float 1.f (importedCamera.zoom_factor) message:("No zoom factor should be applied by default on imported USD cameras")
        assert_float 0.f (importedCamera.lens_breathing_amount) message:("No lens breathing amount value should be applied by default on imported USD cameras")
        assert_equal 3 (importedCamera.shutter_unit_type) message:("Expected shutter unit type to be set to 'Frames'")
        assert_float 0.4f (importedCamera.shutter_length_frames) message:("The shutter duration property value is expected to be equal to what it was originally set")
        assert_true (importedCamera.shutter_offset_enabled) message:("The shutter offset shall be enabled when value is defined in the USD file")
        assert_float 0.1f (importedCamera.shutter_offset_frames) message:("The shutter offset property value is expected to be equal to what it was originally set")
        assert_float 16f (importedCamera.f_number) message:("Expected 'fStop' value is expected to be equal to what it was originally set")
        assert_float 0f (importedCamera.horizontal_shift) message:("The default horizontal aperture offset is not equal to '0' when value is not used in the USD file")
        assert_float 0f (importedCamera.vertical_shift) message:("The default vertical aperture offset is not equal to '0' when value is not used in the USD file")
    ),

    function test_invalid_import_attributes =
    (
        -- float focalLength = 57.42173
        -- float horizontalAperture = 36
        -- token projection = "perspective"
        -- float verticalAperture = 20.25
        -- generates a FOV of 34.8093
        local importPath = output_prefix + "import_invalid_import_attributes_test.usda"
        local stage = pyUsd.Stage.CreateNew(importPath)
        pyUsdGeom.SetStageMetersPerUnit stage 0.0254 -- default 3ds Max settings
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z

        local camPrim = pyUsdGeom.Camera.Define stage "/Camera001"
        camPrim.CreateFocalLengthAttr(57.42173)
        camPrim.CreateHorizontalApertureAttr(36)
        camPrim.CreateProjectionAttr("perspective")
        camPrim.CreateVerticalApertureAttr(20.25)
        
        -- set attributes to 0 but in import those should be set to default Physical camera values
        camPrim.CreateFocusDistanceAttr(0)
        camPrim.CreateShutterCloseAttr(0)
        camPrim.CreateFStopAttr(0)
        camPrim.CreateExposureAttr(0)
        
        (stage.GetRootLayer()).Save()
    
        -- now the import test
        resetMaxAndUnitSetup()
        importFile importPath #noPrompt
        local importedCamera = $Camera001
        assert_equal Physical (classof importedCamera) message:("Imported camera from USD is expected to be a 'Physical' camera type")
        assert_float 34.8093 (importedCamera.fov) message:"Expected FOV value is not equal to calculated value from input"
        assert_false (importedCamera.specifyfov) message:"Expected 'Specify FOV' option to be disabled"
        assert_false (importedCamera.clip_on) message:("The clip range property is not defined in the imported USD file - The camera 'clip_on' property must be off")
        -- overruled values from USD
        assert_float 160.0f (importedCamera.target_distance) message:"Expected 'focusDistance' to be equal to default '160.0' setting when USD attribute is set to '0'"
        assert_float 0.5f (importedCamera.shutter_length_frames) message:("The shutter duration property value shall be set to default '0.5' when USD attribute is set to '0'")
        assert_float 8f (importedCamera.f_number) message:("'fStop' value shall be set to default '8' when USD attribute is set to '0'")
        assert_float 6f (importedCamera.exposure_value) message:("'exposure' value shall be set to default '6' when USD attribute is set to '0'")
        -- defaults from Physical Camera because USD attributes
        assert_float 1.f (importedCamera.zoom_factor) message:("No zoom factor should be applied by default on imported USD cameras")
        assert_float 0.f (importedCamera.lens_breathing_amount) message:("No lens breathing amount value should be applied by default on imported USD cameras")
        assert_equal 3 (importedCamera.shutter_unit_type) message:("Expected shutter unit type to be set to 'Frames'")
        assert_false (importedCamera.shutter_offset_enabled) message:("The shutter offset shall be disabled when value is not defined in the USD file")
        assert_float 0.0f (importedCamera.shutter_offset_frames) message:("The default shutter offset property value is expected when not specified in the USD file")
        assert_float 0f (importedCamera.horizontal_shift) message:("The default horizontal aperture offset is not equal to '0' when value is not used in the USD file")
        assert_float 0f (importedCamera.vertical_shift) message:("The default vertical aperture offset is not equal to '0' when value is not used in the USD file")
    ),
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

   Tests = #(
        export_all_cameras,
        test_free_camera_attributes,
        test_target_camera_attributes,
        test_default_physical_camera_attributes,
        test_physical_camera_attributes,
        test_animated_phys_camera_export,
        test_animated_target_camera_export,
        test_camera_animation_validity_intervals,
        test_camera_animation_import_attributes,
        test_invalid_import_attributes
     )


)
runUsdTestFixture io_camera_test (getThisScriptFilename())