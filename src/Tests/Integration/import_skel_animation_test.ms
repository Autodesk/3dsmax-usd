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
pyUsdSkel = python.import("pxr.UsdSkel")
pyVt = python.import("pxr.Vt")

testDataDir = getFilenamePath (getThisScriptFilename())
importArmUsdPath = (testDataDir + @"data\arm.usda")
importBoneAxisUsdPath = (testDataDir + @"data\usdskel_sample.usdc")

output_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_SKEL_ANIMATION_TEST_"
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

    function setupStage path startTime endTime = (
        local stage = pyUsd.Stage.CreateNew(path)
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z

        -- stating later to consider the case of non 0 start time code
        stage.SetStartTimeCode(startTime)
        stage.SetEndTimeCode(endTime)

        local skelRootPrim = pyUsdSkel.Root.Define stage "/root"
        local translateOp = skelRootPrim.AddTranslateOp()
        local rotateOp = skelRootPrim.AddRotateXYZOp pyUsdGeom.XformOp.PrecisionDouble
        translateOp.Set (pyGf.Vec3f 0 0 1) -- Default timecode
        translateOp.Set (pyGf.Vec3f 0 0 2) startTime
        translateOp.Set (pyGf.Vec3f 0 0 10) endTime
        rotateOp.Set(pyGf.Vec3f 0 0 0) startTime
        rotateOp.Set(pyGf.Vec3f 0 0 60) endTime

        (stage.GetRootLayer()).Save()

        return stage
    ),

    public
    function setup =
    (
        resetMaxFileAndSetupUnits()
    ),

    function import_skelroot_transforms_test = (
        local stagePath = (output_prefix + "import_skelroot_anim_test.usda")
        local stage = setupStage stagePath 10 20

        local timeCodeErrorMsg = "Incorrect node position for slider time "

        -- there shouldn't exist any movement until 12.5 when it starts (timecode 10)
        slidertime = 12
        USDImporter.ImportFile stagePath importOptions:importOptions

        local root = getNodeByName "root"
        assert_equal 2 root.position.z message:(timeCodeErrorMsg + importOptions.StartTimeCode as string)
        assert_float_equal 0.0 (in coordsys world quatToEuler2 root.rotation).z  message:(timeCodeErrorMsg + slidertime as string)
        slidertime = 13 -- upper half of the translated time (12.5)
        assert_equal 2.2048 root.position.z message:(timeCodeErrorMsg + slidertime as string)
        slidertime = 25 -- (10 timeCode / 24 stage timeCodePerSecond) * 30 max FPS = 12.5
        assert_equal 10 root.position.z message:(timeCodeErrorMsg + slidertime as string)
        assert_float_equal -60.0 (in coordsys world quatToEuler2 root.rotation).z  message:(timeCodeErrorMsg + slidertime as string)
    ),

    function import_skelroot_transforms_up_axis_test = (
        resetMaxFileAndSetupUnits()

        local stagePath = (output_prefix + "up_axis.usda")
        local stage = setupStage stagePath 10 20

        local timeCodeErrorMsg = "Incorrect node position for time "

        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.y
        USDImporter.ImportFile stagePath importOptions:importOptions

        local root = getNodeByName "root"

        slidertime = 10
        assert_equal -2 root.position.y message:(timeCodeErrorMsg + slidertime as string)
        assert_float_equal 0.0 (in coordsys world quatToEuler2 root.rotation).z  message:(timeCodeErrorMsg + slidertime as string)
        slidertime = 20 -- upper half of the translated time
        assert_float_equal -6.8 root.position.y message:(timeCodeErrorMsg + slidertime as string)
        slidertime = 25 -- (20 timeCode / 24 stage timeCodePerSecond) * 30 max FPS = 12.5
        assert_equal -10 root.position.y message:(timeCodeErrorMsg + slidertime as string)
        assert_float_equal -60.0 (in coordsys world quatToEuler2 root.rotation).z  message:(timeCodeErrorMsg + slidertime as string)
    ),

    function import_skelroot_fps_difference_test = (
        resetMaxFileAndSetupUnits()

        local stagePath = (output_prefix + "skelroot_fps_difference.usda")
        local stage = setupStage stagePath 10 20

        -- max's slider should be half of the stage's data
        frameRate = 12
        USDImporter.ImportFile stagePath importOptions:importOptions
        timeCodeErrorMsg = "(FPS 12) Incorrect node transform for time "

        rn = getNodeByName "root"
        slidertime = 0
        assert_equal 2 rn.position.z message:(timeCodeErrorMsg + slidertime as string)
        assert_float_equal 0.0 (in coordsys world quatToEuler2 rn.rotation).z  message:(timeCodeErrorMsg + slidertime as string)
        slidertime = 6
        assert_equal 3.6 rn.position.z message:(timeCodeErrorMsg + slidertime as string)
        slidertime = 12
        assert_equal 10 rn.position.z message:(timeCodeErrorMsg + slidertime as string)
        assert_float_equal -60.0 (in coordsys world quatToEuler2 rn.rotation).z  message:(timeCodeErrorMsg + slidertime as string)

        resetMaxFileAndSetupUnits()
        -- max's slider should be twice of the stage's data
        frameRate = 48
        USDImporter.ImportFile stagePath importOptions:importOptions
        timeCodeErrorMsg = "(FPS 48) Incorrect node transform for time "

        rn = getNodeByName "root"
        slidertime = 0
        assert_equal 2 rn.position.z message:(timeCodeErrorMsg + slidertime as string)
        assert_float_equal 0.0 (in coordsys world quatToEuler2 rn.rotation).z  message:(timeCodeErrorMsg + slidertime as string)
        slidertime = 35
        assert_equal 8 rn.position.z message:(timeCodeErrorMsg + slidertime as string)
        slidertime = 50
        assert_equal 10 rn.position.z message:(timeCodeErrorMsg + slidertime as string)
        assert_float_equal -60.0 (in coordsys world quatToEuler2 rn.rotation).z  message:(timeCodeErrorMsg + slidertime as string)
    ),

    function import_skinned_mesh_test = (

        -- changing the slider shouldn't affect the initial animation positon
        -- doing this change here intentionally to test for that case
        slidertime = 11
        USDImporter.ImportFile importArmUsdPath importOptions:importOptions

        local armNode = getNodeByName "Arm"

        local shoulderNode = getNodeByName "Shoulder"
        local elbowNode = getNodeByName "Elbow"
        local handNode = getNodeByName "Hand"

        local skinMod = armNode.modifiers[1]

        assert_true ((ClassOf skinMod) == Skin) message: "Arm node should have a skin modifier"
        assert_equal 3 (skinOps.GetNumberBones skinMod) message: "Wrong number of bones for skin modifier in Arm node"
        assert_equal 12 (skinOps.GetNumberVertices skinMod) message: "Wrong number of vertices for skin modifier in Arm node"

        -- GetVertexWeight <skin> <vert integer > <bone integer>
        -- the <bone integer> argument is the bone # influencing that vertex
        assert_equal 1 (skinOps.GetVertexWeight skinMod 1 1) message: "Wrong vertex weight for vertex 1 bone 1 in Arm node"
        assert_equal 1 (skinOps.GetVertexWeight skinMod 5 1) message: "Wrong vertex weight for vertex 1 bone 3 in Arm node"
        assert_equal 1 (skinOps.GetVertexWeight skinMod 8 1) message: "Wrong vertex weight for vertex 5 bone 1 in Arm node"
        assert_equal 1 (skinOps.GetVertexWeight skinMod 12 1) message: "Wrong vertex weight for vertex 5 bone 3 in Arm node"

        slidertime = 0
        assert_float_equal 0.0 (in coordsys world quatToEuler2 elbowNode.rotation).x  message:("(Time: 0) Incorrect elbow x rotation" + importOptions.StartTimeCode as string)
        slidertime = 10
        assert_float_equal -69.9991 (in coordsys world quatToEuler2 elbowNode.rotation).x  message:("(Time: 10) Incorrect elbow x rotation" + importOptions.EndTimeCode as string)
    ),


    function import_bone_transforms_test = (
        USDImporter.ImportFile importBoneAxisUsdPath importOptions:importOptions
        local headBone = getNodeByName "Head"
        local rTipBone = getNodeByName "RTip"

        animationrange = (interval 0 200f)
        slidertime = 126

        assert_point3_equal [-0.501194,6.56602,109.513] (headBone.transform.pos)
        assert_point3_equal [-0.501194,6.56602,109.513] (headBone.objecttransform.pos)
        local expectedRotation = (eulerAngles 9.50466 0.451236 0.210077)
        local expectedObjectRotation = (eulerAngles 87.2694 -80.4847 -87.0968)
        assert_float_equal ((quatToEuler2 headBone.transform.rotation).x) (expectedRotation.x) message:"Unexpected x component in rotation"
        assert_float_equal ((quatToEuler2 headBone.transform.rotation).y) (expectedRotation.y) message:"Unexpected y component in rotation"
        assert_float_equal ((quatToEuler2 headBone.transform.rotation).z) (expectedRotation.z)message:"Unexpected z component in rotation"
        assert_float_equal ((quatToEuler2 headBone.objecttransform.rotation).x) (expectedObjectRotation.x) message:"Unexpected x component in object rotation"
        assert_float_equal ((quatToEuler2 headBone.objecttransform.rotation).y) (expectedObjectRotation.y) message:"Unexpected y component in object rotation"
        assert_float_equal ((quatToEuler2 headBone.objecttransform.rotation).z) (expectedObjectRotation.z) message:"Unexpected z component in object rotation"

        assert_point3_equal [-2.90865,-24.0769,7.57919] (rTipBone.transform.pos)
        assert_point3_equal [-2.90865,-24.0769,7.57919] (rTipBone.objecttransform.pos)
        expectedRotation = quatToEuler2(quat 0 0.568203 -0.822889 0)
        expectedObjectRotation = quatToEuler2(quat -0.58187 0.40178 -0.58187 -0.40178)
        assert_float_equal ((quatToEuler2 rTipBone.transform.rotation).x) (expectedRotation.x) message:"Unexpected x component in rotation"
        assert_float_equal ((quatToEuler2 rTipBone.transform.rotation).y) (expectedRotation.y) message:"Unexpected y component in rotation"
        assert_float_equal (-(quatToEuler2 rTipBone.transform.rotation).z) (expectedRotation.z) message:"Unexpected z component in rotation"
        assert_float_equal ((quatToEuler2 rTipBone.objecttransform.rotation).x) (expectedObjectRotation.x) message:"Unexpected x component in object rotation"
        assert_float_equal ((quatToEuler2 rTipBone.objecttransform.rotation).y) (expectedObjectRotation.y) message:"Unexpected y component in object rotation"
        assert_float_equal ((quatToEuler2 rTipBone.objecttransform.rotation).z) (expectedObjectRotation.z) message:"Unexpected z component in object rotation"

        slidertime = 162
        assert_point3_equal [-0.501196,-77.7574,109.513] headBone.transform.pos
        assert_point3_equal [-0.501196,-77.7574,109.513] headBone.objecttransform.pos
        expectedRotation = (eulerAngles 9.50466 0.451236 0.210077)
        expectedObjectRotation = (eulerAngles 87.2694 -80.4847 -87.0968)
        assert_float_equal ((quatToEuler2 headBone.transform.rotation).x) (expectedRotation.x) message:"Unexpected x component in rotation"
        assert_float_equal ((quatToEuler2 headBone.transform.rotation).y) (expectedRotation.y) message:"Unexpected y component in rotation"
        assert_float_equal ((quatToEuler2 headBone.transform.rotation).z) (expectedRotation.z)message:"Unexpected z component in rotation"
        assert_float_equal ((quatToEuler2 headBone.objecttransform.rotation).x) (expectedObjectRotation.x) message:"Unexpected x component in object rotation"
        assert_float_equal ((quatToEuler2 headBone.objecttransform.rotation).y) (expectedObjectRotation.y) message:"Unexpected y component in object rotation"
        assert_float_equal ((quatToEuler2 headBone.objecttransform.rotation).z) (expectedObjectRotation.z) message:"Unexpected z component in object rotation"

        assert_point3_equal [-2.90866,-108.442,7.57919] (rTipBone.transform.pos)
        assert_point3_equal [-2.90866,-108.442,7.57919] (rTipBone.objecttransform.pos)
        expectedRotation = (quat 0 0.568203 -0.822889 0)
        expectedObjectRotation = quatToEuler2(quat -0.58187 0.40178 -0.58187 -0.40178)
        assert_float_equal ((rTipBone.transform.rotation).x) (expectedRotation.x) message:"Unexpected x component in rotation"
        assert_float_equal ((rTipBone.transform.rotation).y) (expectedRotation.y) message:"Unexpected y component in rotation"
        assert_float_equal ((rTipBone.transform.rotation).z) (expectedRotation.z) message:"Unexpected z component in rotation"
        assert_float_equal ((quatToEuler2 rTipBone.objecttransform.rotation).x) (expectedObjectRotation.x) message:"Unexpected x component in object rotation"
        assert_float_equal ((quatToEuler2 rTipBone.objecttransform.rotation).y) (expectedObjectRotation.y) message:"Unexpected y component in object rotation"
        assert_float_equal ((quatToEuler2 rTipBone.objecttransform.rotation).z) (expectedObjectRotation.z) message:"Unexpected z component in object rotation"

        assert_equal 2 (rTipBone.width)
        assert_equal 4 (headBone.width)
        assert_equal 4(headBone.height)
        assert_equal 2 (rTipBone.height)
        assert_equal 2 (rTipBone.length)
        assert_float_equal 8.1526 (headBone.length)

        assert_equal #z (headBone.boneAxis)
        assert_equal #z (rTipBone.boneAxis)
        assert_false (headBone.boneAxisFlip)
        assert_false (rTipBone.boneAxisFlip)
        assert_true (headBone.boneEnable)
        assert_true (rTipBone.boneEnable)
        assert_true (headBone.boneAutoAlign)
        assert_true (rTipBone.boneAutoAlign)
        assert_true (headBone.boneFreezeLength)
        assert_true (rTipBone.boneFreezeLength)
        assert_equal #none (headBone.boneScaleType)
        assert_equal #none (rTipBone.boneScaleType)
    ),

    function import_keyframes_test = (
        resetMaxFileAndSetupUnits()

        local stagePath = (output_prefix + "import_keyframes_test.usda")
        local stage = setupStage stagePath 0 10

        -- during import the default auto key option is disabled. test if it remains the same as before
        maxops.autoKeyDefaultKeyOn = true

        USDImporter.ImportFile stagePath importOptions:importOptions

        assert_true (autoKeyDefaultKeyOn) message: "autoKeyDefaultKeyOn was modified during import"

        local rn = getNodeByName "root"

        -- test for the bug where the key at the position 0 is not created when the stage starts at time 0
        assert_equal 11 (numKeys rn.position.controller) message:"Failed to create some assign some of the required animation keys"
    ),

    function import_keyframes_test = (
        resetMaxFileAndSetupUnits()

        local stagePath = (output_prefix + "import_keyframes_test.usda")
        local stage = setupStage stagePath 0 10

        -- during import the default auto key option is disabled. test if it remains the same as before
        autoKeyDefaultKeyOn = true
        USDImporter.ImportFile stagePath importOptions:importOptions

        assert_true (autoKeyDefaultKeyOn) message: "autoKeyDefaultKeyOn was modified during import"

        local rn = getNodeByName "root"

        -- test for the bug where the key at the position 0 is not created when the stage starts at time 0
        assert_equal 11 (numKeys rn.position.controller) message:"Failed to create some assign some of the required animation keys"
    ),

    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    tests = #(import_skelroot_transforms_test,
        import_skelroot_transforms_up_axis_test,
        import_skelroot_fps_difference_test,
        import_skinned_mesh_test,
        import_bone_transforms_test,
        import_keyframes_test
    )
)

runUsdTestFixture import_animation_test (getThisScriptFilename())