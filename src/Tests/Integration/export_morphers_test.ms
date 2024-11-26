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

setupUsdPlugins()

pyUsd = python.import("pxr.Usd")
pyUsdSkel = python.import("pxr.UsdSkel")
pyUsdGeom = python.import("pxr.UsdGeom")

struct export_morphers_test
(
    private 
    output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_MORPHER_TESTS_",
    exportOptions = USDExporter.CreateOptions(),
    initialUnits,
    initialScale,

    function resetMaxFileAndSetupUnits = 
    (
        resetmaxfile #noprompt
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        units.SystemType = #Centimeters
        units.SystemScale = 1

        exportOptions.FileFormat = #ascii
        exportOptions.Morpher = true
        exportOptions.Meshes = true
        exportOptions.RootPrimPath = "/root"

        local rangeStart = 0
        local rangeEnd = 30
        
        animationRange = interval rangeStart rangeEnd
        exportOptions.TimeMode = #animationRange
    ),

    function createTargetFromNode node vertOffset = 
    (
        mytarget = snapshot node
        for i = 1 to mytarget.numVerts do
        (
            local vertex = mytarget.verts[i]
            move vertex vertOffset
        )
        return mytarget
    ),

    -- scramble the vertices so it generates different normals
    function randomizeVertices node = 
    (
        seed 1234 -- always generate the same number for testing purposes

        numVertices = meshOp.getNumVerts node.mesh
        verticesArray = for vert = 1 to numVertices collect (meshOp.getVert node.mesh vert)
        for i = 1 to verticesArray.count do
        (
            verticesArray[i] = verticesArray[i] * (random 1.0 10.0) / 10.0
        )

        numFaces = meshOp.getNumFaces node.mesh
        facesArray = for face = 1 to numFaces collect (getFace node.mesh face)

        m = triMesh()
        setMesh m verts:verticesArray faces:facesArray
        node.mesh = m
        update node.mesh
    ),

    function createMorpherTestScene =
    (
        createdNodes = #()

        mybox = Box isSelected:on width: 20.0 length: 20.0 height: 20.0
        morpherMod = Morpher()
        addModifier mybox morpherMod
        morpherMod.Spinner_Minimum = -100.0

        append createdNodes mybox

        mytarget1 = createTargetFromNode mybox [0,0,10]
        randomizeVertices mytarget1 -- this will generate delta normals
        mytarget1.name = "Target 1"
        WM3_MC_BuildFromNode morpherMod 1 mytarget1
        append createdNodes mytarget1

        mytarget2 = createTargetFromNode mybox [0,20,0]
        mytarget2.name = "Target 2"
        WM3_MC_BuildFromNode morpherMod 2 mytarget2
        append createdNodes mytarget2

        mytarget3 = createTargetFromNode mybox [30,0,0]
        mytarget3.name = "Target 3"
        WM3_MC_BuildFromNode morpherMod 3 mytarget3
        append createdNodes mytarget3

        floatController = bezier_float()
        morpherMod[2].controller = floatController

        with animate on 
        (
            at time 10 WM3_MC_SetValue morpherMod 1 100.0
            -- adding the keyframe here due to the bug where we can't auto add frame 0
            at time 0 WM3_MC_SetValue morpherMod 1 0.0
            at time 10 WM3_MC_SetValue morpherMod 2 0.0
            at time 10 WM3_MC_SetValue morpherMod 3 0.0
            at time 20 WM3_MC_SetValue morpherMod 1 0.0
            at time 20 floatController.value = 100.0
            at time 20 WM3_MC_SetValue morpherMod 3 0.0
            at time 30 WM3_MC_SetValue morpherMod 1 0.0
            at time 30 floatController.value = 0.0
            at time 30 WM3_MC_SetValue morpherMod 3 100.0
        )

        return createdNodes
    ),

    function createProgressiveMorpherTestScene =
    (
        createdNodes = #()

        mybox = Box isSelected:on width: 20.0 length: 20.0 height: 20.0
        morpherMod = Morpher()
        addModifier mybox morpherMod

        append createdNodes mybox

        mytarget1 = createTargetFromNode mybox [0,0,10]
        mytarget1.name = "Target 1"
        WM3_MC_BuildFromNode morpherMod 1 mytarget1
        append createdNodes mytarget1

        mytarget2 = createTargetFromNode mybox [0,20,0]
        -- setting the name here to start with a number to cover the case that usd was replacing the first number
        -- to go around changing the name of the node completely, we intially pre-pend _ before the number
        mytarget2.name = "2"
        WM3_AddProgressiveMorphNode morpherMod 1 mytarget2
        append createdNodes mytarget2

        mytarget3 = createTargetFromNode mybox [30,0,0]
        mytarget3.name = "Target 3"
        WM3_AddProgressiveMorphNode morpherMod 1 mytarget3
        append createdNodes mytarget3

        WM3_SetProgressiveMorphWeight morpherMod 1 mytarget1 25.0
        WM3_SetProgressiveMorphWeight morpherMod 1 mytarget2 50.0
        WM3_SetProgressiveMorphWeight morpherMod 1 mytarget3 100.0

        with animate on 
        (
            at time 10 WM3_MC_SetValue morpherMod 1 0.0
            at time 20 WM3_MC_SetValue morpherMod 1 25.0
            at time 30 WM3_MC_SetValue morpherMod 1 50.0
            at time 50 WM3_MC_SetValue morpherMod 1 100.0
        )

        return createdNodes
    ),

    function execute_morpher_properties_tests exportPath deletedExtraMeshes = 
    (
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        -- general variables
        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local skelRootPrim = pyUsdSkel.Root(stage.GetPrimAtPath(skelRootPath))
        local meshPrimPath = (skelRootPath + "/Box001")
        local boxPrim = stage.GetPrimAtPath(meshPrimPath)
        local skelPrim = stage.GetPrimAtPath(skelRootPath + "/Bones")
        local animPrim = stage.GetPrimAtPath(skelRootPath + "/Bones/Animations")

        -- check if skel has the animation target
        local skelBindingApi = pyUsdSkel.BindingAPI.Get stage (skelPrim.GetPath())
        assert_true (animPrim.GetPath() as string == (skelBindingApi.GetAnimationSource()).GetPath() as string) Message:"Animation binding doesn't match the animation prim"

        -- check if mesh has the skel binding
        local meshBindingApi = pyUsdSkel.BindingAPI.Get stage meshPrimPath
        local meshSkelRel = meshBindingApi.GetSkeletonRel()
        local meshSkelTargets = meshSkelRel.GetTargets()
        assert_equal 1 meshSkelTargets.count Message: "There should only exist 1 skel relationship"
        assert_true (meshSkelTargets[1] as string == skelPrim.GetPath() as string) Message: "SkelTarget doesn't match skel path"

        -- we create an extra mesh when exporting blendshape. Make sure it has been deleted
        foundExportNodes = #()
        rootChildren = (stage.GetPrimAtPath(skelRootPath)).GetChildren()
        for i = 1 to rootChildren.count do
        (
            if (rootChildren[i].IsA(pyUsdGeom.Xformable)) then
            (
                append foundExportNodes rootChildren[i]
            )
        )

        if deletedExtraMeshes then
        (
            assert_equal 2 foundExportNodes.count Message: "The root node should only have 2 children: Skeleton prim + the mesh prim"
        )
        else 
        (
            assert_equal 5 foundExportNodes.count Message: "The root node should only have 5 children: Skeleton prim + the mesh/xform with meshes"
        )

        -- check if there are any unexpected blendshapes
        foundBlendShapes = #()
        meshChildren = (boxPrim).GetChildren()
        for i = 1 to meshChildren.count do
        (
            if (meshChildren[i].IsA(pyUsdSkel.BlendShape)) then
            (
                append foundBlendShapes meshChildren[i]
            )
        )
        assert_equal 3 foundBlendShapes.count Message: "There should exist 3 BlendShapes"

        -- check BlendShapes names
        for i = 1 to foundBlendShapes.count do
        (
            assert_equal (meshPrimPath + "/Target_" + (i as string)) (foundBlendShapes[i].GetPath() as string)
        )

        local expectedOffset1 = #(#(4.439627, 4.439627, 5.560373), #(-0.4664812, 0.4664812, 9.533519), #(3.6491175, -3.6491175, 6.3508825), #(-6.426284, -6.426284, 3.5737162), #(4.2561393, 4.2561393, -2.7684174), #(-7.4031057, 7.4031057, -12.209317), #(0.40749073, -0.40749073, 8.777527), #(-5.428038, -5.428038, -6.284114))
        local offset1 = (foundBlendShapes[1].GetAttribute("offsets")).Get()
        for i = 1 to offset1.count do 
        (
            for x = 1 to 3 do 
            (
                assert_float_equal offset1[i][x] expectedOffset1[i][x] tolerance:0.001 message: "BlendShape offset1 didn't match"
            )
        )

        local offset2 = (foundBlendShapes[2].GetAttribute("offsets")).Get()
        for i = 1 to offset2.count do 
        (
            for x = 1 to 3 do 
            (
                assert_float_equal offset2[i][x] #(0,20.0,0)[x] tolerance:0.001 message: "BlendShape offset2 didn't match"
            )
        )

        local offset3 = (foundBlendShapes[3].GetAttribute("offsets")).Get()
        for i = 1 to offset3.count do 
        (
            for x = 1 to 3 do 
            (
                assert_float_equal (offset3[i][x]) (#(30.0,0,0)[x]) tolerance:0.001 message: "BlendShape offset3 didn't match"
            )
        )

        local expectedDeltaNormals1 = #(#(-0.030527309, -0.18318819, 0.01739633), #(-0.25695634, 0.04700898, 0.034720957), #(-0.041669764, -0.17238171, 0.015851557), #(0.15065439, -0.35161147, 0.076055944), #(0.65152735, -0.44120553, -0.38286966), #(0.70589006, -0.55658096, -0.56189394), #(0.6662265, -0.46848, -0.41976827), #(0.6107727, -0.37267947, -0.30138236), #(0.2179837, 0.10576475, 0.39093027), #(-0.2583895, 0.98429745, 0.9659131), #(-0.07189513, 0.53415203, 0.88193923), #(0.3437027, 0.06096816, -0.009359415), #(-0.42478037, -0.03992137, 0.81702423), #(-0.07658225, 0.3623322, -0.12655029), #(-0.09399164, 0.20966579, 0.36768055), #(-1.5877967, -0.41301677, 0.69563794), #(0.23156409, -0.033403575, -0.10986113), #(0.24072152, -0.03506726, -0.104680054), #(0.23116723, -0.033334076, -0.11008506), #(0.21904086, -0.031312764, -0.11690244), #(0.009492993, -0.11602661, -0.07371334), #(0.0022616386, -0.06510757, -0.016709436), #(0.010827899, -0.12243896, -0.08091565), #(0.019511461, -0.15637314, -0.119120575))
        local normals1 = (foundBlendShapes[1].GetAttribute("normalOffsets")).Get()
        for i = 1 to normals1.count do 
        (
            for x = 1 to 3 do 
            (
                assert_float_equal normals1[i][x] expectedDeltaNormals1[i][x] tolerance:0.001 message: "BlendShape normals1 didn't match"
            )
        )

        local normals2 = (foundBlendShapes[2].GetAttribute("normalOffsets")).Get()
        for i = 1 to normals2.count do 
        (
            for x = 1 to 3 do 
            (
                assert_float_equal normals2[i][x] #(0.0,0.0,0.0)[x] tolerance:0.001 message: "BlendShape normals2 didn't match"
            )
        )

        local normals3 = (foundBlendShapes[3].GetAttribute("normalOffsets")).Get()
        for i = 1 to normals3.count do 
        (
            for x = 1 to 3 do 
            (
                assert_float_equal normals3[i][x] #(0.0,0.0,0.0)[x] tolerance:0.001 message: "BlendShape normals3 didn't match"
            )
        )

        local blendShapeWeightsAttribute = animPrim.GetAttribute("blendShapeWeights")
        local blendShapeWeightsTimeSamples = blendShapeWeightsAttribute.GetTimeSamples()
        local expectedAnimatedWeights = #(#(0, 0, 0), #(0.028000003, 0, 0), #(0.104, 0, 0), #(0.216, 0, 0), #(0.35199997, 0, 0), #(0.5, 0, 0), #(0.648, 0, 0), #(0.7839999, 0, 0), #(0.89599997, 0, 0), #(0.97199994, 0, 0), #(1, 0, 0), #(0.97199994, 0.028000005, 0), #(0.89599997, 0.10400002, 0), #(0.78400004, 0.216, 0), #(0.64799994, 0.35200006, 0), #(0.5, 0.5, 0), #(0.35200006, 0.64799994, 0), #(0.21600004, 0.7839999, 0), #(0.10399991, 0.8960001, 0), #(0.027999997, 0.97199994, 0), #(0, 1, 0), #(0, 0.97199994, 0.028000008), #(0, 0.89599997, 0.10400003), #(0, 0.7839999, 0.21600007), #(0, 0.6479999, 0.3520001), #(0, 0.4999999, 0.50000006), #(0, 0.35199988, 0.6480001), #(0, 0.21599996, 0.78400004), #(0, 0.10399998, 0.8960001), #(0, 0.027999997, 0.97199994), #(0, 0, 1))

        assert_equal blendShapeWeightsTimeSamples.count expectedAnimatedWeights.count message:"Didn't export the expected number of animation frames"

        local counter = 1
        for timeCode = 0 to 30 do (
            local w = (blendShapeWeightsAttribute).Get(timeCode)
            for x = 1 to 3 do 
            (
                assert_float_equal expectedAnimatedWeights[counter][x] w[x] tolerance:0.00001
            )
            counter = counter + 1
        )
    ),

    function execute_progressive_morpher_properties_tests exportPath = 
    (
        local rangeStart = 0
        local rangeEnd = 50
        
        animationRange = interval rangeStart rangeEnd
        exportOptions.TimeMode = #animationRange
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        -- general variables
        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local meshPrimPath = (skelRootPath + "/Box001")
        local boxPrim = stage.GetPrimAtPath(meshPrimPath)
        local skelPrim = stage.GetPrimAtPath(skelRootPath + "/Bones")
        local animPrim = stage.GetPrimAtPath(skelRootPath + "/Bones/Animations")

        -- check if mesh has the skel binding
        local meshBindingApi = pyUsdSkel.BindingAPI.Get stage meshPrimPath
        local meshSkelRel = meshBindingApi.GetSkeletonRel()
        local meshSkelTargets = meshSkelRel.GetTargets()

        -- check if there are any unexpected blendshapes
        foundBlendShapes = #()
        meshChildren = (boxPrim).GetChildren()
        for i = 1 to meshChildren.count do
        (
            if (meshChildren[i].IsA(pyUsdSkel.BlendShape)) then
            (
                append foundBlendShapes meshChildren[i]
            )
        )
        assert_equal 1 foundBlendShapes.count Message: "There should exist 1 BlendShapes"

        -- check BlendShapes names
        for i = 1 to foundBlendShapes.count do
        (
            assert_equal (meshPrimPath + "/Target_" + (i as string)) (foundBlendShapes[i].GetPath() as string)
        )

        local expectedOffset1 = #(#(30.0,0.0,0.0), #(30.0,0.0,0.0), #(30.0,0.0,0.0), #(30.0,0.0,0.0), #(30.0,0.0,0.0), #(30.0,0.0,0.0), #(30.0,0.0,0.0), #(30.0,0.0,0.0))
        local offset1 = (foundBlendShapes[1].GetAttribute("offsets")).Get()
        for i = 1 to offset1.count do 
        (
            for x = 1 to 3 do 
            (
                assert_float_equal offset1[i][x] expectedOffset1[i][x] tolerance:0.001 message: "BlendShape offset1 didn't match"
            )
        )

        local expectedInbetween1 = #(#(0, 0, 10), #(0, 0, 10), #(0, 0, 10), #(0, 0, 10), #(0, 0, 10), #(0, 0, 10), #(0, 0, 10), #(0, 0, 10))
        local expectedInbetween2 = #(#(0, 20, 0), #(0, 20, 0), #(0, 20, 0), #(0, 20, 0), #(0, 20, 0), #(0, 20, 0), #(0, 20, 0), #(0, 20, 0))
        local inbetweens1 = (foundBlendShapes[1].GetAttribute("inbetweens:Target_1")).Get()
        local inbetweens2 = (foundBlendShapes[1].GetAttribute("inbetweens:_2")).Get()

        for i = 1 to expectedInbetween1.count do 
        (
            for x = 1 to 3 do 
            (
                assert_float_equal expectedInbetween1[i][x] inbetweens1[i][x] tolerance:0.001 message: "BlendShape inbetween1 didn't match"
                assert_float_equal expectedInbetween2[i][x] inbetweens2[i][x] tolerance:0.001 message: "BlendShape inbetween2 didn't match"
            )
        )
    ),

    public
    function setup =
    (
        resetMaxFileAndSetupUnits()
    ),

    function export_morpher_can_export_conditions = 
    (
        resetMaxFileAndSetupUnits()
        myCreatedNodes = createMorpherTestScene()

        exportOptions.Meshes = false
        local exportPath = output_prefix + "export_morpher_can_export_conditions_without_meshes.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local skelRootPrim = pyUsdSkel.Root(stage.GetPrimAtPath(skelRootPath))
        local skelPrim = stage.GetPrimAtPath(skelRootPath + "/Bones")
        local animPrim = stage.GetPrimAtPath(skelRootPath + "/Bones/Animations")

        assert_false (pyUsd.Prim.IsValid skelPrim) Message: "Skeleton prim shouldn't have been created with Meshes disabled"
        assert_false (pyUsd.Prim.IsValid animPrim) Message: "SkelAnimation prim shouldn't have been created with Meshes disabled"

        exportOptions.Meshes = true
        exportPath = output_prefix + "export_morpher_can_export_conditions_with_meshes.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        stage = pyUsd.Stage.Open(exportPath)
        skelRootPath = "/root"
        skelRootPrim = pyUsdSkel.Root(stage.GetPrimAtPath(skelRootPath))
        skelPrim = stage.GetPrimAtPath(skelRootPath + "/Bones")
        animPrim = stage.GetPrimAtPath(skelRootPath + "/Bones/Animations")

        assert_true (pyUsd.Prim.IsValid skelPrim) Message: "Skeleton prim should have been created with Meshes enabled"
        assert_true (pyUsd.Prim.IsValid animPrim) Message: "SkelAnimation prim should have been created with Meshes enabled"
    ),

    function export_morpher_properties_with_nodes = 
    (
        resetMaxFileAndSetupUnits()
        local exportPath = output_prefix + "export_morpher_properties_with_nodes.usda"
        myCreatedNodes = createMorpherTestScene()

        execute_morpher_properties_tests exportPath false
    ),

    function export_morpher_current_frame = 
    (
        resetMaxFileAndSetupUnits()

        exportOptions.TimeMode = #current
        local exportPath = output_prefix + "export_morpher_current_frame.usda"
        myCreatedNodes = createMorpherTestScene()
        slidertime = 2
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local animPrim = stage.GetPrimAtPath(skelRootPath + "/Bones/Animations")

        local blendShapeWeightsAttribute = animPrim.GetAttribute("blendShapeWeights")
        local expectedAnimatedWeights = #(0.104, 0, 0)

        -- we exported the scene at frame 2. It's expected that weight to be the morpher default weight
        local timeCode = 0
        local w = (blendShapeWeightsAttribute).Get(timeCode)
        for x = 1 to 3 do 
        (
            assert_float_equal expectedAnimatedWeights[x] w[x] tolerance:0.00001
        )
    ),

    function export_morpher_properties_without_nodes = 
    (
        resetMaxFileAndSetupUnits()
        local exportPath = output_prefix + "export_morpher_properties_without_nodes.usda"
        myCreatedNodes = createMorpherTestScene()

        -- delete the morph targets from the scene
        for i = 2 to myCreatedNodes.count do
        (
            delete myCreatedNodes[i]
        )

        execute_morpher_properties_tests exportPath true
    ),

    function export_progressive_morpher = 
    (
        resetMaxFileAndSetupUnits()
        local exportPath = output_prefix + "export_progressive_morpher.usda"
        myCreatedNodes = createProgressiveMorpherTestScene()
        assert_equal 4 (objects.count)
        execute_progressive_morpher_properties_tests exportPath
        -- make sure that we didn't leave any extra nodes in the scene after exporting morphers
        assert_equal 4 (objects.count)
    ),

    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(
        export_morpher_can_export_conditions,
        export_morpher_properties_with_nodes,
        export_morpher_current_frame,
        export_morpher_properties_without_nodes,
        export_progressive_morpher
    )
)

runUsdTestFixture export_morphers_test(getThisScriptFilename())