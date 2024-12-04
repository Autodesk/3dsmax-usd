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

fileIn "helpers/usd_test_utils.mxs"

-- Make sure the USD plugin is properly loaded.
setupUsdPlugins()
clearlistener()

pyUsd = python.import("pxr.Usd")
pyUsdGeom = python.import("pxr.UsdGeom")
pyGf = python.import("pxr.Gf")

output_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_OPTIONS_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct import_options_test
(
    private 
    importOptions = USDImporter.CreateOptions(),
    initialUnits,
    initialScale,
    
    function resetMaxFileAndSetupUnits = 
    (
        resetmaxfile #noprompt
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        units.SystemType = #Centimeters
        units.SystemScale = 1
        importOptions.TimeMode = #AllRange
    ),
    
    public
    function setup =
    (
        resetMaxFileAndSetupUnits()
    ),

    function test_ui_options_accessors = 
    (
         -- Make sure options created are properly set by default
        local importOptions = USDImporter.CreateOptions()

        assert_equal importOptions.StageMask.count 1 message:"The StageMask property was not properly initialized."
        assert_equal importOptions.StageMask[1] "/" message:"The StageMask property was not property initialized."
        assert_float_equal 0.0 importOptions.StartTimeCode message:"The TimeCode property was not properly initialized."
        assert_float_equal 0.0 importOptions.EndTimeCode message:"The TimeCode property was not properly initialized."
        assert_equal #allRange importOptions.TimeMode message:"The TimeMode property was not properly initialized"
        assert_equal #LoadAll importOptions.InitialLoadSet message:"The InitialLoadSet property was not properly initialized."
        assert_equal ((GetDir #temp) + "\\MaxUsdImport.log") importOptions.LogPath message:"The LogPath property was not properly initialized."
        assert_equal importOptions.LogLevel #off message:"The LogLevel property was not properly initialized."
        assert_equal importOptions.ImportUnmappedPrimvars true message:"The ImportUnmappedPrimvars property was not properly initialized."
        assert_true (importOptions.Materials()) message:"The Materials property was not properly initialized to true (default)"
        assert_equal "none" importOptions.PreferredMaterial message:"The PreferredMaterial property was not properly initialized."
        assert_equal 1 importOptions.ShadingModes.count message:"The ShadingModes property was not properly initialized."
        assert_equal "useRegistry" importOptions.ShadingModes[1][1] message:"The ShadingModes property was not properly initialized with expected 'mode'."
        assert_equal "UsdPreviewSurface" importOptions.ShadingModes[1][2] message:"The ShadingModes property was not properly initialized with expected 'materialConversion'."

        -- Set them all to non-default values
        importOptions.StageMask = #("/foo")
        importOptions.StartTimeCode = 10.0
        importOptions.EndTimeCode = 12.0
        importOptions.TimeMode = #customRange
        importOptions.InitialLoadSet = #loadNone
        importOptions.LogLevel = #info
        importOptions.LogPath = (output_prefix + "log_path.txt")
        importOptions.ImportUnmappedPrimvars = false
        importOptions.PreferredMaterial = "MaxUsdPreviewSurface"

        -- Setting a ShadingMode to 'none' inside an array should set only 'none' as the ShadingModes
        importOptions.ShadingModes = #(#("useRegistry", "MaterialX"), #("none", "VRayMaterial"))
        assert_equal 1 importOptions.ShadingModes.count message:"The ShadingModes property was not properly set back."
        assert_equal "none" importOptions.ShadingModes[1][1] message:"The ShadingModes property was not properly back to 'none'."
        assert_false (importOptions.Materials()) message:"The Materials property was not properly initialized to false"

        -- Setting multiple ShadingModes with 'none' mode will not pose any issues
        importOptions.ShadingModes = #(#("useRegistry", "MaterialX"), #("useRegistry", "VRayMaterial"))
        assert_equal 2 importOptions.ShadingModes.count message:"The ShadingModes property was not properly set to multiple modes."
        assert_true (importOptions.Materials()) message:"The Materials property was not properly initialized to true"

        -- Test the UI options setter
        USDImporter.UIOptions = importOptions

        -- The new global options should have been updated
        uiImportOptions = USDImporter.UIOptions

        assert_equal uiImportOptions.StageMask[1] "/foo" message:"The StageMask property was not properly set to \"/foo\"."
        assert_float_equal 10.0 uiImportOptions.StartTimeCode message:"The StartTimeCode property was not properly set to 10.0."
        assert_float_equal 12.0 uiImportOptions.EndTimeCode message:"The EndTimeCode property was not properly set to 12.0."
        assert_equal #customRange uiImportOptions.TimeMode message:"The TimeMode property was not properly set to #customRange"
        assert_equal #loadNone uiImportOptions.InitialLoadSet message:"The InitialLoadSet property was not properly set to #LoadNone."
        assert_equal (output_prefix + "log_path.txt") uiImportOptions.LogPath message:"The LogPath property was not properly set."
        assert_equal uiImportOptions.LogLevel #info message:"The LogLevel property was not property set to #info."
        assert_false uiImportOptions.ImportUnmappedPrimvars message:"The ImportUnmappedPrimvars property was not properly set to false."
        assert_true (uiImportOptions.Materials()) message:"The Materials property was not properly set to true"
        assert_equal "MaxUsdPreviewSurface" uiImportOptions.PreferredMaterial message:"The PreferredMaterial property was not properly set."
        assert_equal 2 uiImportOptions.ShadingModes.count message:"The ShadingModes property was not properly set."
        assert_equal "useRegistry" uiImportOptions.ShadingModes[1][1] message:"The ShadingModes property was not properly set."
        assert_equal "MaterialX" uiImportOptions.ShadingModes[1][2] message:"The ShadingModes property was not properly set."
        assert_equal "useRegistry" uiImportOptions.ShadingModes[2][1] message:"The ShadingModes property was not properly set."
        assert_equal "VRayMaterial" uiImportOptions.ShadingModes[2][2] message:"The ShadingModes property was not properly set."

        -- Reset & test
        uiImportOptions.Reset()

        assert_equal uiImportOptions.StageMask.count 1 message:"The StageMask property was not properly reset."
        assert_equal uiImportOptions.StageMask[1] "/" message:"The StageMask property was not properly reset."
        assert_equal 0.0 uiImportOptions.StartTimeCode message:"The StartTimeCode property was not properly reset."
        assert_equal 0.0 uiImportOptions.EndTimeCode message:"The EndTimeCode property was not properly reset."
        assert_equal #allRange uiImportOptions.TimeMode message:"The TimeMode property was not properly initialized"
        assert_equal #LoadAll uiImportOptions.InitialLoadSet message:"The InitialLoadSet property was not properly reset."
        assert_equal ((GetDir #temp) + "\\MaxUsdImport.log") uiImportOptions.LogPath message:"The LogPath property was not properly reset."
        assert_equal uiImportOptions.LogLevel #off message:"The LogLevel property was not properly reset."
        assert_true uiImportOptions.ImportUnmappedPrimvars message:"The ImportUnmappedPrimvars property was not properly reset."
        assert_true (uiImportOptions.Materials()) message:"The Materials property was not properly reset"
        assert_equal "none" uiImportOptions.PreferredMaterial message:"The PreferredMaterial property was not properly reset."
        assert_equal 1 uiImportOptions.ShadingModes.count message:"The ShadingModes property was not properly reset."
        assert_equal "useRegistry" uiImportOptions.ShadingModes[1][1] message:"The ShadingModes property was not properly reseted with expected 'mode'."
        assert_equal "UsdPreviewSurface" uiImportOptions.ShadingModes[1][2] message:"The ShadingModes property was not properly reseted with expected 'materialConversion'."
    ),

    function test_stage_mask_option = (
        local stagePath = (output_prefix + "masked.usda")
        local stage = pyUsd.Stage.CreateNew(stagePath)
        local fooPrim = pyUsdGeom.Xform.Define stage "/foo"
        local barPrim = pyUsdGeom.Xform.Define stage "/foo/bar"
        local bazPrim = pyUsdGeom.Xform.Define stage "/baz"
        local buzzPrim = pyUsdGeom.Xform.Define stage "/foo/buzz"
        (stage.GetRootLayer()).Save()
        
        -- Test default, no mask.
        USDImporter.ImportFile stagePath importOptions:importOptions
        local missingNodeMessage = "Missing an node with the given stage mask."
        local unfilteredNode = "The node not filtered out as expected by the stage mask."
        assert_defined (getNodeByName "foo") message:missingNodeMessage
        assert_defined (getNodeByName "bar") message:missingNodeMessage
        assert_defined (getNodeByName "baz") message:missingNodeMessage
        assert_defined (getNodeByName "buzz") message:missingNodeMessage
        
        -- Test Applying a single mask path.
        resetMaxFileAndSetupUnits()
        importOptions.StageMask = #("/foo/bar")
        USDImporter.ImportFile stagePath importOptions:importOptions
        assert_defined (getNodeByName "foo") message:missingNodeMessage
        assert_defined (getNodeByName "bar") message:missingNodeMessage
        assert_undefined (getNodeByName "baz") message:unfilteredNode
        assert_undefined (getNodeByName "buzz") message:unfilteredNode
            
        -- Test applying a multiple mask path.
        resetMaxFileAndSetupUnits()
        importOptions.StageMask = #("/foo/bar", "/baz")
        USDImporter.ImportFile stagePath importOptions:importOptions
        assert_defined (getNodeByName "foo") message:missingNodeMessage
        assert_defined (getNodeByName "bar") message:missingNodeMessage
        assert_defined (getNodeByName "baz") message:missingNodeMessage
        assert_undefined (getNodeByName "buzz") message:unfilteredNode
        
        -- Test attempting to set an invalid path throws.
        local errorMessage = "No exception was thrown when setting invalid USD path as stage mask."
        
        -- Completely wrong path.
        local exceptionThrown = false
        try (
            importOptions.StageMask = #("#&$")
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown message:errorMessage

        -- Valid prim path, but not absolute
        local exceptionThrown = false
        try (
            importOptions.StageMask = #("Foo/bar")
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown message:errorMessage
        
        -- Absolute path, but to a property, not a prim.
        local exceptionThrown = false
        try (
            importOptions.StageMask = #("/Foo.prop")
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown message:errorMessage

    ),

    function test_time_code_options = (
        local stagePath = (output_prefix + "timeCode.usda");
        local stage = pyUsd.Stage.CreateNew(stagePath)
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z
        stage.SetStartTimeCode(0)
        stage.SetEndTimeCode(10)
        -- Create a sphere with an animated transform.
        -- Node moves from (0,0,2) to (0,0,10).
        local fooPrim = pyUsdGeom.Sphere.Define stage "/foo"
        local translateOp = fooPrim.AddTranslateOp()
        translateOp.Set (pyGf.Vec3f 0 0 1) -- Default timecode
        translateOp.Set (pyGf.Vec3f 0 0 2) 0 -- time 0
        translateOp.Set (pyGf.Vec3f 0 0 10) 10 -- time 10

        (stage.GetRootLayer()).Save()

        local timeCodeErrorMsg = "Incorrect node position for time mode "

        -- Now with the start timecode : 0
        resetMaxFileAndSetupUnits()
        importOptions.TimeMode = #startTime
        USDImporter.ImportFile stagePath importOptions:importOptions

        foo = getNodeByName "foo"
        assert_equal 2 foo.position.z message:(timeCodeErrorMsg + importOptions.StartTimeCode as string)

        -- Time code 5 is half-way. (10-2)/2 + 2 = 6
        resetMaxFileAndSetupUnits()
        importOptions.TimeMode = #customRange
        importOptions.StartTimeCode = 5
        USDImporter.ImportFile stagePath importOptions:importOptions
        foo = getNodeByName "foo"
        assert_equal 6 foo.position.z message:(timeCodeErrorMsg + importOptions.StartTimeCode as string)

        -- Now with the end timecode : 10
        resetMaxFileAndSetupUnits()
        importOptions.TimeMode = #customRange
        importOptions.StartTimeCode = 10
        USDImporter.ImportFile stagePath importOptions:importOptions
        foo = getNodeByName "foo" 
        assert_equal 10 foo.position.z message:(timeCodeErrorMsg + importOptions.StartTimeCode as string)
        
        -- Now with a timecode < start 
        resetMaxFileAndSetupUnits()
        importOptions.TimeMode = #customRange
        importOptions.StartTimeCode = -10
        USDImporter.ImportFile stagePath importOptions:importOptions
        foo = getNodeByName "foo" 
        assert_equal 2 foo.position.z message:(timeCodeErrorMsg + importOptions.StartTimeCode as string)
        
        -- Now with a timecode > end
        resetMaxFileAndSetupUnits()
        importOptions.TimeMode = #customRange
        importOptions.StartTimeCode = 99
        USDImporter.ImportFile stagePath importOptions:importOptions
        foo = getNodeByName "foo" 
        assert_equal 10 foo.position.z message:(timeCodeErrorMsg + importOptions.StartTimeCode as string)

         -- Now with #starttime
         resetMaxFileAndSetupUnits()
         importOptions.TimeMode = #startTime
         USDImporter.ImportFile stagePath importOptions:importOptions
         foo = getNodeByName "foo" 
         assert_equal 2 foo.position.z message:(timeCodeErrorMsg + "#startTime")

        -- Now with #endtime
        resetMaxFileAndSetupUnits()
        importOptions.TimeMode = #endTime
        USDImporter.ImportFile stagePath importOptions:importOptions
        foo = getNodeByName "foo" 
        assert_equal 10 foo.position.z message:(timeCodeErrorMsg + "#endTime")

        -- Now with #allRange, testing from start frame (0) to last frame (13)
        resetMaxFileAndSetupUnits()
        USDImporter.ImportFile stagePath importOptions:importOptions
        foo = getNodeByName "foo" 
        sliderTime = 0
        assert_equal 2 foo.position.z message:(timeCodeErrorMsg + "#allRange")
        sliderTime = 13 -- last frame is actually 12.5 due to the fps difference
        assert_equal 10 foo.position.z message:(timeCodeErrorMsg + "#allRange")
    ),
    
    function test_initial_loading_set_option = (
        -- Setup payload.
        local payloadPath = (output_prefix + "payload.usda")
        local stage = pyUsd.Stage.CreateNew(payloadPath)
        local fooPrim = pyUsdGeom.Xform.Define stage "/foo"
        local barPrim = pyUsdGeom.Xform.Define stage "/foo/bar"
        pyUsd.Stage.SetDefaultPrim stage (fooPrim.GetPrim())
        (stage.GetRootLayer()).Save()
        -- Setup host for the payload.
        local hostPath = (output_prefix + "host.usda")
        stage = pyUsd.Stage.CreateNew(hostPath)
        local bazPrim = pyUsdGeom.Xform.Define stage "/baz"
        ((bazPrim.GetPrim()).GetPayloads()).AddPayload(payloadPath)
        (stage.GetRootLayer()).Save()
        
        -- #loadNone -> The payload should not be imported.
        importOptions.InitialLoadSet = #loadNone
        USDImporter.ImportFile hostPath importOptions:importOptions
        local barNode = getNodeByName "bar"
        assert_undefined  barNode message:"The payload was imported even though #loadNone was chosen as loading set."
        
        -- #loadAll -> The payload should be imported.
        resetMaxFileAndSetupUnits()
        importOptions.InitialLoadSet = #loadAll
        USDImporter.ImportFile hostPath importOptions:importOptions
        local barNode = getNodeByName "bar"
        assert_defined  barNode message:"The payload was not imported even though #loadAll was chosen as loading set."
        
        -- Test that invalid InitialLoadingSet values are rejected.
        try (
            local importOptions = USDImporter.CreateOptions()
            importOptions.InitialLoadSet = 10
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to set an invalid InitialLoadingSet should have thrown an exception."            
    ),
    
    function test_log_options = (
        -- Test no log.
        local logPath = (output_prefix + "log_path_test.log")
        -- Make sure the log doesnt currently exist.
        deleteFile logPath
        
        importOptions.LogPath = logPath
        assert_equal logPath importOptions.LogPath
        importOptions.LogLevel = #off
        assert_equal #off importOptions.LogLevel
        
        -- Create a usd file to import...
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        local exportPath = (output_prefix + "teapot.usda")
        teapot()
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        -- Import it, make sure no log was created.
        USDImporter.ImportFile exportPath importOptions:importOptions
        assert_equal 0 (getfiles logPath).count
        
        local levels = #(#info, #warn, #error)
        -- For other levels, a log should be created.
        for level in levels do (
            -- Log should now be created.
            importOptions.LogLevel = level
            assert_equal importOptions.LogLevel level
            USDImporter.ImportFile exportPath importOptions:importOptions
            assert_not_equal 0 (getfiles logPath).count
            local logSize = getfileSize logPath            
            if level == #info then (
                assert_not_equal 0 logSize message:"#info log should not be empty."
            )
            else (
                assert_equal 0 logSize message:((level as string) + " log should be empty.")
            )
            deleteFile logPath
        )
        
        try (
            -- Test that a bad path throws an error.
            importOptions.LogPath = "<not valid!>"
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown

        -- Test that invalid log levels are rejected.
        try (
            local importOptions = USDImporter.CreateOptions()
            importOptions.LogLevel = 10
        )
        catch 
        (
            format "Caught expected exception : %\n" (getCurrentException())
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to set an invalid LogLevel should have thrown an exception."
    ),

    -- Small test to validate the maxscript interface, more testing done in python.
    -- See options_serialization_test.py
    function test_Serialize = (
        local importOptions = USDImporter.CreateOptions()
        importOptions.EndTimeCode = 12.0
        
        jsonStr = importOptions.Serialize()
        
        loadedOptions = USDImporter.CreateOptionsFromJson(jsonStr)
        assert_float_equal 12.0 loadedOptions.EndTimeCode message:"The EndTimeCode property was not properly set to 12.0."
    ),
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    tests = #(test_ui_options_accessors,
            test_stage_mask_option,
            test_time_code_options,
            test_initial_loading_set_option,
            test_log_options,
            test_Serialize
        )
)

runUsdTestFixture import_options_test (getThisScriptFilename())