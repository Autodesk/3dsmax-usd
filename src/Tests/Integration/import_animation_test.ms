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
pyUsdLux = python.import("pxr.UsdLux")
pyGf = python.import("pxr.Gf")

output_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_ANIMATION_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct import_animation_test
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
    ),

    function setupStage path addPivot:false = (
        local stage = pyUsd.Stage.CreateNew(path)
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z

        stage.SetStartTimeCode(0)
        stage.SetEndTimeCode(10)

        local fooPrim = pyUsdGeom.Sphere.Define stage "/foo"
        local translateOp = fooPrim.AddTranslateOp()
        local rotateOp = fooPrim.AddRotateXYZOp pyUsdGeom.XformOp.PrecisionDouble "foo" false
        translateOp.Set (pyGf.Vec3f 0 0 1) -- Default timecode
        translateOp.Set (pyGf.Vec3f 0 0 2) 0 -- time 0
        translateOp.Set (pyGf.Vec3f 0 0 10) 10 -- time 10
        rotateOp.Set(pyGf.Vec3f 0 0 0)
        rotateOp.Set(pyGf.Vec3f 0 0 60) 10

        local barPrim = pyUsdGeom.Sphere.Define stage "/foo/bar"
        if addPivot then (
            local barPivotOp = barPrim.AddTranslateOp pyUsdGeom.XformOp.PrecisionDouble "pivot" false
            barPivotOp.Set(pyGf.Vec3f 10 0 0)
            local invertBarPivotOp = barPrim.AddTranslateOp pyUsdGeom.XformOp.PrecisionDouble "pivot" true
        )
        local barTranslateOp = barPrim.AddTranslateOp()
        barTranslateOp.Set (pyGf.Vec3f 0 0 0) -- Default timecode
        barTranslateOp.Set (pyGf.Vec3f 20 0 0) 5 -- time 5
        barTranslateOp.Set (pyGf.Vec3f 30 0 0) 10 -- time 10
        local barRotateOp = barPrim.AddRotateXYZOp pyUsdGeom.XformOp.PrecisionDouble "bar" false
        barRotateOp.Set(pyGf.Vec3f -60 0 0) 10

        local lux = pyUsdLux.CylinderLight.Define stage "/foo/lux"

        (stage.GetRootLayer()).Save()

        return stage
    ),

    public
    function setup =
    (
        resetMaxFileAndSetupUnits()
    ),

    function test_import_transforms = (
        resetMaxFileAndSetupUnits()

        local stagePath = (output_prefix + "import_transforms.usda")
        local stage = setupStage stagePath

        local timeCodeErrorMsg = "Incorrect node transform on SliderTime = "
        USDImporter.ImportFile stagePath importOptions:importOptions
        foo = getNodeByName "foo"
        bar = getNodeByName "bar"
        lux = getNodeByName "lux"
        slidertime = 0
        assert_equal 2 foo.position.z message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal 10.0 bar.position.x message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal -60.0 (in coordsys world quatToEuler2 foo.rotation).z  message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal 40.8934 (in coordsys world quatToEuler2 bar.rotation).x  message:(timeCodeErrorMsg + (slidertime as string))
        slidertime = 7 -- upper half of the translated time (12.5)
        assert_equal 6 foo.position.z message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal 10.504 bar.position.x message:(timeCodeErrorMsg + (slidertime as string))
        slidertime = 13 -- (10 timeCode / 24 stage timeCodePerSecond) * 30 max FPS = 12.5
        assert_equal 10 foo.position.z message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal 15 bar.position.x message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal -60.0 (in coordsys world quatToEuler2 foo.rotation).z  message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal 40.8934 (in coordsys world quatToEuler2 bar.rotation).x  message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal -150 (in coordsys world quatToEuler2 lux.rotation).z  message:(timeCodeErrorMsg + (slidertime as string))
    ),

    function test_import_up_axis = (
        resetMaxFileAndSetupUnits()

        local stagePath = (output_prefix + "up_axis.usda")
        local stage = setupStage stagePath

        local timeCodeErrorMsg = "Incorrect node transform on SliderTime = "

        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.y
        USDImporter.ImportFile stagePath importOptions:importOptions
        foo = getNodeByName "foo"
        bar = getNodeByName "bar"
        lux = getNodeByName "lux"
        slidertime = 0
        assert_equal -2 foo.position.y message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal 10.0 bar.position.x message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal -60.0 (in coordsys world quatToEuler2 foo.rotation).z  message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal -49.1066 (in coordsys world quatToEuler2 bar.rotation).x  message:(timeCodeErrorMsg + (slidertime as string))
        slidertime = 7 -- upper half of the translated time (12.5)
        assert_float_equal -6.48 foo.position.y message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal 10.504 bar.position.x message:(timeCodeErrorMsg + (slidertime as string))
        slidertime = 13 -- (10 timeCode / 24 stage timeCodePerSecond) * 30 max FPS = 12.5
        assert_equal -10 foo.position.y message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal 15 bar.position.x message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal -60.0 (in coordsys world quatToEuler2 foo.rotation).z  message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal -49.1066 (in coordsys world quatToEuler2 bar.rotation).x  message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal -90 (in coordsys world quatToEuler2 lux.rotation).x  message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal -150 (in coordsys world quatToEuler2 lux.rotation).z  message:(timeCodeErrorMsg + (slidertime as string))
    ),

    function test_import_pivot_change = (
        resetMaxFileAndSetupUnits()

        local stagePath = (output_prefix + "pivot_change.usda")
        local stage = setupStage stagePath addPivot:true

        local timeCodeErrorMsg = "Incorrect node transform on SliderTime = "

        USDImporter.ImportFile stagePath importOptions:importOptions
        foo = getNodeByName "foo"
        bar = getNodeByName "bar"
        slidertime = 0
        assert_float_equal 15.0 bar.position.x message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal 40.8934 (in coordsys world quatToEuler2 bar.rotation).x  message:(timeCodeErrorMsg + (slidertime as string))
        slidertime = 7 -- upper half of the translated time (12.5)
        assert_float_equal 15.504 bar.position.x message:(timeCodeErrorMsg + (slidertime as string))
        slidertime = 13 -- (10 timeCode / 24 stage timeCodePerSecond) * 30 max FPS = 12.5
        assert_float_equal 20 bar.position.x message:(timeCodeErrorMsg + (slidertime as string))
        assert_float_equal 40.8934 (in coordsys world quatToEuler2 bar.rotation).x  message:(timeCodeErrorMsg + (slidertime as string))
    ), 

    function test_import_fps_difference = (
        resetMaxFileAndSetupUnits()

        local stagePath = (output_prefix + "fps_difference.usda")
        local stage = setupStage stagePath

        frameRate = 24
        USDImporter.ImportFile stagePath importOptions:importOptions
        local timeCodeErrorMsg = "(FPS 24) Incorrect node position for time code "

        -- slider time and location should match stage's timecode (same fps)
        foo = getNodeByName "foo"
        slidertime = 0
        assert_equal 2 foo.position.z message:(timeCodeErrorMsg)
        assert_float_equal -60.0 (in coordsys world quatToEuler2 foo.rotation).z  message:(timeCodeErrorMsg)
        slidertime = 5 -- upper half of the translated time (12.5)
        assert_equal 6 foo.position.z message:(timeCodeErrorMsg)
        slidertime = 10 -- (10 timeCode / 24 stage timeCodePerSecond) * 30 max FPS = 12.5
        assert_equal 10 foo.position.z message:(timeCodeErrorMsg)
        assert_float_equal -60.0 (in coordsys world quatToEuler2 foo.rotation).z  message:(timeCodeErrorMsg)

        resetMaxFileAndSetupUnits()
        -- max's slider should be half of the stage's data
        frameRate = 12
        USDImporter.ImportFile stagePath importOptions:importOptions
        timeCodeErrorMsg = "(FPS 12) Incorrect node position for time code "

        foo = getNodeByName "foo"
        slidertime = 0
        assert_equal 2 foo.position.z message:(timeCodeErrorMsg)
        assert_float_equal -60.0 (in coordsys world quatToEuler2 foo.rotation).z  message:(timeCodeErrorMsg)
        slidertime = 3
        assert_equal 6 foo.position.z message:(timeCodeErrorMsg)
        slidertime = 5
        assert_equal 10 foo.position.z message:(timeCodeErrorMsg)
        assert_float_equal -60.0 (in coordsys world quatToEuler2 foo.rotation).z  message:(timeCodeErrorMsg)

        resetMaxFileAndSetupUnits()
        -- max's slider should be twice of the stage's data
        frameRate = 48
        USDImporter.ImportFile stagePath importOptions:importOptions
        timeCodeErrorMsg = "(FPS 48) Incorrect node position for time code "

        foo = getNodeByName "foo"
        slidertime = 0
        assert_equal 2 foo.position.z message:(timeCodeErrorMsg)
        assert_float_equal -60.0 (in coordsys world quatToEuler2 foo.rotation).z  message:(timeCodeErrorMsg)
        slidertime = 10
        assert_equal 6 foo.position.z message:(timeCodeErrorMsg)
        slidertime = 20
        assert_equal 10 foo.position.z message:(timeCodeErrorMsg)
        assert_float_equal -60.0 (in coordsys world quatToEuler2 foo.rotation).z  message:(timeCodeErrorMsg)
    ),

    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    tests = #(test_import_transforms,
        test_import_up_axis,
        test_import_pivot_change,
        test_import_fps_difference
    )
)

runUsdTestFixture import_animation_test (getThisScriptFilename())