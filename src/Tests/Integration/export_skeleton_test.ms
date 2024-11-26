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

struct export_skin_test
(
    private 
    output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_SKIN_TESTS_",
    exportOptions = USDExporter.CreateOptions(),
    initialUnits = units.SystemType,
    initialScale = units.SystemScale,
    wasAutoKeyOn = false,

    function resetMaxFileAndSetupUnits = 
    (
        resetmaxfile #noprompt
        units.SystemType = #Centimeters
        units.SystemScale = 1

        local rangeStart = 0
        local rangeEnd = 30

        animationRange = interval rangeStart rangeEnd
        exportOptions.TimeMode = #animationRange
    ),

    private
    function createBone trans = (
        local b = (BoneSys.createBone trans.row4 (trans.row4 + [0, 0, 4]) [0, 0, 1])
        b.showLinks = true
        b.showLinksOnly = off
        bone_size = 1
        b.width = bone_size
        b.height = bone_size
        b.transform = trans
        b.length = 3
        b.boneEnable = true        
        return b
    ),

    private
    function createBonesArray numOfBones = (
        newBones = #()
        pos = [0,0,0]

        for i = 1 to numOfBones do (
            local trans = matrix3 [1,0,0] [0,1,0] [0,0,1] pos
            local b = createBone trans

            if newBones.count > 0 then (
                b.parent = newBones[newBones.count]
            )
            append newBones b
            pos += [0,0,24]
        )

        return newBones
    ),

    private
    function skinBonesToCharacter bonesArray maxNode = (
        sk = skin()
        addModifier maxNode sk

        u = 0
        for i = 1 to bonesArray.count do (
            local b = bonesArray[i]
            if i == bonesArray.count then u = -1 -- '-1' here forces an update and redraw
            skinOps.addbone sk b u node:maxNode
        )

        select maxNode
        max modify Mode

        skinOps.voxelWeighting sk 0 0.2 4 256 false true node:maxNode
    ),
    
    private
    function createCharMesh = 
    (
        local char = Cylinder radius:6 height:36 heightsegs:30 pos:[0,0,0] isSelected:on

        volSel1 = VolumeSelect level:2 volume:5
        addModifier char volSel1

        volSel2 = VolumeSelect level:2 volume:5 method:1 matID:2
        addModifier char volSel2
        addModifier char (DeleteMesh())    

        volSel3 = VolumeSelect level:0 method:0
        addModifier char volSel3

        t = Taper amount:1.72 curve:-4.75 effectaxis:0 primaryaxis:2 
        addModifier char t

        h = Sphere radius:4 segments:32 pos:[0,0,48]

        body = Cylinder radius:4.5 height:15 heightsegs:24 pos:[0,0,28]
        addModifier body volSel1
        addModifier body volSel2
        addModifier body (DeleteMesh())        

        arm1 = Cylinder radius:4.5 height:15 heightsegs:24  pos:[0,0,28] height:36 dir:[0.751061,0,0.660233]
        addModifier arm1 volSel1
        addModifier arm1 volSel2
        addModifier arm1 (DeleteMesh())        

        arm2 = Cylinder radius:4.5 height:15 heightsegs:24  pos:[0,0,28] height:36 dir:[-0.719323,0,0.694675]
        addModifier arm2 volSel1
        addModifier arm2 volSel2
        addModifier arm2 (DeleteMesh())        

        convertToPoly char
        polyop.attach char body
        polyop.attach char arm1
        polyop.attach char h
        polyop.attach char arm2
        
        return char
    ),

    public
    function setup =
    (
        resetMaxFileAndSetupUnits()
        wasAutoKeyOn = maxops.autoKeyDefaultKeyOn
        maxops.autoKeyDefaultKeyOn = true

        exportOptions.FileFormat = #ascii
        exportOptions.Skin = true
        exportOptions.Meshes = true
        exportOptions.RootPrimPath = "/root"
    ),

    function export_skin_animation_test = 
    (
        newBones = createBonesArray 3

        b = createBone (matrix3 [0.707107,0,-0.707107] [0,1,0] [0.707107,0,0.707107] [24,0,50]) 
        b.parent = newBones[newBones.count-1]
        append newBones b

        b = createBone(matrix3 [0.707107,0,0.707107] [0,1,0] [-0.707107,0,0.707107] [-24,0,50])
        b.parent = newBones[newBones.count - 2]
        append newBones b

        local ub = createBone (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,72])
        ub.name = "unused_bone"
        
        animate on (
            in coordsys world (
                at time 30 newBones[2].rotation = (quat 0.275154 0 0 0.9614)
                at time 30 newBones[3].rotation = (quat 0.562967 0 0 0.826479)
                at time 30 newBones[4].rotation =  (quat 0.229887 0.528306 0.151202 0.803233)
                at time 30 newBones[5].rotation =  (quat 0.223103 -0.562692 -0.161043 0.779531)
            )
        )

        char = createCharMesh()
        skinBonesToCharacter newBones char

        local exportPath = output_prefix + "export_simple_skin_animation_test.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local meshPrimPath = (skelRootPath + "/Cylinder001")
        local skelRootPrim = pyUsdSkel.Root(stage.GetPrimAtPath(skelRootPath))
        local meshPrim = stage.GetPrimAtPath meshPrimPath

        assert_true (meshPrim.IsA(pyUsdGeom.Mesh)) message: "The animated mesh Prim should have been exported as a mesh prim"
        assert_false ((meshPrim.GetAttribute("extent")).HasAuthoredValue()) message: "Skinned meshes shouldn't have the extents attribute"
        skelRootExtentsAttr = ((stage.GetPrimAtPath(skelRootPath)).GetAttribute("extent"))

        -- testing half of the time samples in the file
        expectedRootExtents =  #(#(#(-29.02168, -6.0000005, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-29.02168, -6.0000005, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-29.02168, -6.0000005, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-29.02168, -6.0000005, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-29.02168, -6.0000005, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-29.02168, -6.0000005, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-29.02168, -7.428551, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-29.02168, -9.402174, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-29.02168, -11.514503, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-29.02168, -13.734598, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-29.022, -16.031898, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-29.8008, -18.376095, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-30.6016, -20.737707, 0), #(30.009243, 6.0000005, 56.24527)), #(#(-31.417, -23.088703, 0), #(30.2875, 6.0000005, 56.24527)), #(#(-32.2398, -25.402304, 0), #(30.985, 6.0000005, 56.24527)))

        for timeCode = 1 to 15 do
        (
            assert_true (compare2dMatrix ((skelRootExtentsAttr.Get(timeCode))) expectedRootExtents[timeCode] 2 3)
        )

        local skelPrim = stage.GetPrimAtPath(skelRootPath + "/Bones")
        local animPrim = stage.GetPrimAtPath(skelRootPath + "/Bones/Animations")

        local usdSkelPurposeAttribute = (skelPrim.GetAttribute("purpose")).Get()
        assert_equal usdSkelPurposeAttribute "guide" message:"Skeleton prim purpose should have been set to guide"

        local skel = pyUsdSkel.Skeleton(skelPrim)
        local bindTransform = (skel.GetBindTransformsAttr()).Get()
        local restTransform = (skel.GetRestTransformsAttr()).Get()
        local expectedBindTransform = #(#( #(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(0, 0, 0, 1) ), #( #(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(0, 0, 24, 1) ), #( #(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(0, 0, 48, 1) ), #( #(0.707107, 0, -0.707107, 0), #(0, 1, 0, 0), #(0.707107, 0, 0.707107, 0), #(24, 0, 50, 1) ), #( #(0.707107, 0, 0.707107, 0), #(0, 1, 0, 0), #(-0.707107, 0, 0.707107, 0), #(-24, 0, 50, 1) ))
        local expectedRestTransform = #(#( #(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(0, 0, 0, 1) ), #( #(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(0, 0, 24, 1) ), #( #(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(0, 0, 24, 1) ), #( #(0.707107, 0, -0.707107, 0), #(0, 1, 0, 0), #(0.707107, 0, 0.707107, 0), #(24, 0, 26, 1) ), #( #(0.707107, 0, 0.707107, 0), #(0, 1, 0, 0), #(-0.707107, 0, 0.707107, 0), #(-24, 0, 26, 1) ))

        for i = 1 to expectedBindTransform.count do 
        (
            assert_true (compare2dMatrix expectedBindTransform[i] bindTransform[i] 4 4) message: ("Unexpected bind transform at indice: " + (i as string))
            assert_true (compare2dMatrix expectedRestTransform[i] restTransform[i] 4 4) message: ("Unexpected rest transform at indice: " + (i as string))
        )

        local skelPrimJointsAttribute = (skelPrim.GetAttribute("joints")).Get()
        local animPrimJointsAttribute = (animPrim.GetAttribute("joints")).Get()
        assert_equal 5 skelPrimJointsAttribute.count message:"Didn't export the correct number of joints"    
        assert_equal skelPrimJointsAttribute.count animPrimJointsAttribute.count message:"Animation and Skeleton don't have the same amount of joints exported"
        for i = 1 to skelPrimJointsAttribute.count do (
            assert_equal skelPrimJointsAttribute[i] animPrimJointsAttribute[i] message:("Animation and Skeleton don't match " + skelPrimJointsAttribute[i] + ";" + animPrimJointsAttribute[i])
            assert_true (skelPrimJointsAttribute[i] != "root/unused_bone") message: "'unused_bone' shouldn't have been exported as a bone"
        )

        local meshBindingApi = pyUsdSkel.BindingAPI.Get stage meshPrimPath
        local meshSkelRel = meshBindingApi.GetSkeletonRel()
        local meshSkelTargets = meshSkelRel.GetTargets()
        assert_equal 1 meshSkelTargets.count Message: "There should only exist 1 skel relationship"
        assert_true (meshSkelTargets[1] as string == skelPrim.GetPath() as string) Message: "SkelTarget doesn't match skel path"

        local skelBindingApi = pyUsdSkel.BindingAPI.Get stage (skelPrim.GetPath())
        assert_true (animPrim.GetPath() as string == (skelBindingApi.GetAnimationSource()).GetPath() as string) Message:"Animation binding doesn't match the animation prim"

        local geomBindTransform = (meshPrim.GetAttribute("primvars:skel:geomBindTransform")).Get()
        local expectedGeomBindTransform = #(#(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(0, 0, 0, 1))
        assert_true (compare2dMatrix geomBindTransform expectedGeomBindTransform 4 4) message: "Unexpected geomBindTransform prim var for the mesh!"

        local jointsIndicesAttribute = (meshPrim.GetAttribute("primvars:skel:jointIndices")).Get()
        local jointsWeightsAttribute = (meshPrim.GetAttribute("primvars:skel:jointWeights")).Get()

        assert_equal jointsIndicesAttribute.count jointsWeightsAttribute.count Message: "Skel joints weights and indices must be of equal size"
    
        -- export the scene again, as a single frame that is not the first one

        sliderTime  = 30
        exportOptions.TimeMode = #current
        exportPath = output_prefix + "export_simple_skin_animation_single_frame_test.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        stage = pyUsd.Stage.Open(exportPath)
        skelRootPath = "/root"
        meshPrimPath = (skelRootPath + "/Cylinder001")
        skelRootPrim = pyUsdSkel.Root(stage.GetPrimAtPath(skelRootPath))
        meshPrim = stage.GetPrimAtPath meshPrimPath
        skelPrim = stage.GetPrimAtPath(skelRootPath + "/Bones")
        animPrim = stage.GetPrimAtPath(skelRootPath + "/Bones/Animations")

        skel = pyUsdSkel.Skeleton(skelPrim)
        bindTransform = (skel.GetBindTransformsAttr()).Get()
        restTransform = (skel.GetRestTransformsAttr()).Get()

        expectedRestTransform = #( #( #(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(0, 0, 0, 1) ), #( #(1, 0, 0, 0), #(0, 0.848581, 0.529066, 0), #(0, -0.529066, 0.848581, 0), #(0, -12.6976, 20.3659, 1) ), #( #(1, 0, 0, 0), #(0, 0.8030238006576558, 0.595945949627975, 0), #(0, -0.595945949627975, 0.8030238006576558, 0), #(0, -28.6053597795478, 14.545132729964774, 1) ), #( #(0.396062, 9.976194554339735e-7, -0.9182241033048522, 0), #(5.0568e-7, 0.9999996804851743, 0.0000013776462479131446, 0), #(0.918224, -0.0000012651233093630143, 0.3960619779770332, 0), #(37.97, -6.2744763502566965, 15.936383332557416, 1) ), #( #(0.314886, -1.2053993414529174e-8, 0.949129200839234, 0), #(-5.06011e-7, 1, 5.551115123125783e-17, 0), #(-0.94913, -5.906796775312362e-7, 0.31488604212035654, 0), #(-40.4795, -8.078664287019828, 13.042416221523265, 1) ))

        -- the bind transform is expected to be the same, when exporting any frame, the rest transform will change depending on the mesh position for the frame
        for i = 1 to expectedBindTransform.count do 
        (
            assert_true (compare2dMatrix expectedBindTransform[i] bindTransform[i] 4 4) message: "Unexpected bind transform when exporting a single frame"
            assert_true (compare2dMatrix expectedRestTransform[i] restTransform[i] 4 4) message: "Unexpected rest transform when exporting a single frame"
        )

        anim = pyUsdSkel.Animation(animPrim)
        animScales = (anim.GetScalesAttr()).Get()
        animTranslations = (anim.GetTranslationsAttr()).Get()
        animRotations = (anim.GetRotationsAttr()).Get()

        assert_equal 5 animRotations.count message: "Didn't export all expected rotations when exporting a single frame!"

        expectedScales = #(#(1, 1, 1), #(1, 1, 1), #(1, 1, 1), #(1, 1, 1), #(1, 1, 1))
        expectedTranslations = #(#(0, 0, 0), #(0, -12.6976, 20.3659), #(0, -28.60536, 14.545133), #(37.97, -6.2744765, 15.936383), #(-40.4795, -8.078664, 13.042417))

        for i = 1 to expectedScales.count do (
            for x = 1 to 3 do (
                assert_float_equal (animScales[i])[x] (expectedScales[i])[x] tolerance:1e-5 message: "Animation Scale attribute didn't match when exporting a single frame!"
                assert_float_equal (animTranslations[i])[x] (expectedTranslations[i])[x] tolerance:1e-5 message: "Animation Translation attribute didn't match when exporting a single frame!"
            )
        )
    ),

    function export_skin_invalid_root_test = 
    (
        resetMaxFileAndSetupUnits()

        newBones = createBonesArray 1

        char = createCharMesh()
        skinBonesToCharacter newBones char

        exportOptions.RootPrimPath = "/"
        local exportPath = output_prefix + "export_skin_invalid_root_test.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
    ),

    function export_multiple_bind_transform_test = 
    (
        resetMaxFileAndSetupUnits()

        slidertime = 0

        newBones = createBonesArray 3
        -- bind bones to the mesh before its been transform
        char = createCharMesh()
        skinBonesToCharacter newBones char

        newBones[2].pos = [2, 3, 4]
        newBones[3].pos = [5, 6, 7]

        exportOptions.TimeMode = #current
        exportPath = output_prefix + "export_bind_transformed_test.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        stage = pyUsd.Stage.Open(exportPath)
        skelRootPath = "/root"
        skelRootPrim = pyUsdSkel.Root(stage.GetPrimAtPath(skelRootPath))
        skelPrim = stage.GetPrimAtPath(skelRootPath + "/Bones")

        skel = pyUsdSkel.Skeleton(skelPrim)
        -- cache bind transform for bones before they've been transformed
        local beforeTransformBindTransform = (skel.GetBindTransformsAttr()).Get()

        resetMaxFileAndSetupUnits()

        newBones = createBonesArray 3

        -- bind bones to the mesh before its been transform
        char = createCharMesh()
        skinBonesToCharacter newBones char

        newBones[2].pos = [2, 3, 4]
        newBones[3].pos = [5, 6, 7]

        -- now bind the same bones (after being transformed to a different mesh)
        -- USD can't have multiple bind transforms, so one bind transform will be prevalent, breaking the other
        char2 = createCharMesh()
        char2.name = "char2"
        skinBonesToCharacter newBones char2

        exportOptions.TimeMode = #current
        exportPath = output_prefix + "export_multiple_bind_transform_test.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        stage = pyUsd.Stage.Open(exportPath)
        skelRootPath = "/root"
        skelRootPrim = pyUsdSkel.Root(stage.GetPrimAtPath(skelRootPath))
        skelPrim = stage.GetPrimAtPath(skelRootPath + "/Bones")

        skel = pyUsdSkel.Skeleton(skelPrim)
        local afterTransformBindTransform = (skel.GetBindTransformsAttr()).Get()

        -- for this test, position 1 remained the same, then we'll test i = 2 and i = 3
        -- in this case, the bind transform after transform was kept thus being different from the one before
        for i = 2 to beforeTransformBindTransform.count do 
        (
            assert_false (compare2dMatrix afterTransformBindTransform[i] beforeTransformBindTransform[i] 4 4) message: ("Unexpected bind transform when exporting a single frame at i = " + (i as string))
        )
    ),

    function export_skeleton_bone_weights_test = 
    (
        newBones = createBonesArray 1

        -- create a local box to be used as a bone
        local bb = box()
        bb.pos = [0,0,24]
        bb.parent = newBones[1]
        append newBones bb

        animate on (
            in coordsys world (
                at time 30 newBones[2].rotation = (quat 0.275154 0 0 0.9614)
            )
        )

        -- create a simple cylinder character
        local char = Cylinder radius:6 height:36 heightsegs:30 pos:[0,0,0] isSelected:on
        convertToPoly char

        skinBonesToCharacter newBones char

        local exportPath = output_prefix + "export_skeleton_bone_weights_test.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        local expectedWeights = #(0.9999967, 0.0000032524124, 0.9999967, 0.0000032825183, 0.9999968, 0.0000032389332, 0.99999666, 0.0000033105077, 0.99999684, 0.0000031422517, 0.9999968, 0.0000032153623, 0.99999684, 0.0000031887573, 0.9999966, 0.0000033778902, 0.99999654, 0.0000034555042, 0.99999607, 0.000003915311, 0.99999624, 0.0000037605273, 0.9999962, 0.000003803032, 0.9999962, 0.0000038293583, 0.99999577, 0.0000042537345, 0.9999959, 0.000004113259, 0.9999958, 0.0000041949083, 0.9999962, 0.000003795831, 0.999996, 0.0000039797583, 0.9999961, 0.000003854701, 0.999996, 0.0000040114605, 0.9999963, 0.0000036713882, 0.9999964, 0.000003591174, 0.9999966, 0.0000033790561, 0.9999966, 0.000003384658, 0.99999595, 0.0000040825516, 0.99999595, 0.000004042248, 0.9999959, 0.0000041202456, 0.9999958, 0.000004145737, 0.99999595, 0.000004035201, 0.999996, 0.000003970759, 0.99999607, 0.000003963262, 0.9999959, 0.0000041121725, 0.99999547, 0.000004516164, 0.99999505, 0.00000493347, 0.99999505, 0.0000049649607, 0.99999523, 0.0000047397284, 0.99999523, 0.000004789697, 0.99999493, 0.000005072504, 0.9999948, 0.0000051683814, 0.9999948, 0.000005167787, 0.9999949, 0.0000051285897, 0.99999505, 0.000004968006, 0.9999951, 0.000004869929, 0.999995, 0.000004981978, 0.99999505, 0.0000049192518, 0.9999955, 0.000004488759, 0.9999959, 0.0000041359094, 0.9999959, 0.0000041049634, 0.999994, 0.000006047738, 0.9999939, 0.0000060564157, 0.99999386, 0.000006142562, 0.9999938, 0.000006196997, 0.9999939, 0.0000060877596, 0.99999404, 0.000005964183, 0.9999941, 0.0000058752944, 0.9999939, 0.0000060620882, 0.9999932, 0.0000067839705, 0.99999243, 0.0000075875396, 0.99999225, 0.000007771255, 0.99999255, 0.0000074373493, 0.9999927, 0.0000072941707, 0.99999255, 0.000007444806, 0.9999925, 0.00000750143, 0.99999243, 0.000007576102, 0.9999923, 0.000007708176, 0.9999924, 0.0000076036986, 0.99999255, 0.000007473218, 0.99999243, 0.000007578149, 0.99999255, 0.0000074784366, 0.9999932, 0.0000067850515, 0.9999939, 0.000006074035, 0.99999404, 0.0000059471904, 0.9999904, 0.000009609695, 0.9999903, 0.000009724985, 0.99999017, 0.000009849422, 0.9999901, 0.0000099062545, 0.9999902, 0.000009783932, 0.9999904, 0.000009624896, 0.9999905, 0.000009449576, 0.99999034, 0.000009684574, 0.99998915, 0.00001085843, 0.99998766, 0.000012317398, 0.9999871, 0.000012881385, 0.9999874, 0.000012559339, 0.9999877, 0.000012273235, 0.9999878, 0.000012209755, 0.99998796, 0.000012048813, 0.99998784, 0.000012137201, 0.99998754, 0.000012472246, 0.9999875, 0.00001250755, 0.99998754, 0.000012465268, 0.9999874, 0.000012574503, 0.9999878, 0.000012217942, 0.999989, 0.00001101921, 0.9999902, 0.000009795896, 0.9999906, 0.000009444207, 0.9999842, 0.000015792872, 0.999984, 0.000015981299, 0.99998385, 0.000016177779, 0.9999837, 0.000016242917, 0.9999839, 0.000016081414, 0.99998415, 0.000015875044, 0.9999844, 0.000015624562, 0.99998397, 0.000016007561, 0.9999821, 0.000017884726, 0.9999797, 0.000020312924, 0.99997854, 0.000021464772, 0.9999787, 0.00002125178, 0.9999791, 0.000020901916, 0.9999794, 0.000020618894, 0.99997985, 0.000020118157, 0.9999798, 0.000020178495, 0.9999792, 0.000020787304, 0.999979, 0.000020995589, 0.9999789, 0.000021071313, 0.9999788, 0.000021193466, 0.9999797, 0.000020344116, 0.99998176, 0.000018229961, 0.9999837, 0.000016253127, 0.9999844, 0.000015621752, 0.999973, 0.000027000913, 0.99997276, 0.00002723289, 0.99997246, 0.00002751673, 0.99997234, 0.000027627271, 0.9999726, 0.000027418042, 0.99997294, 0.000027081556, 0.9999733, 0.000026698934, 0.9999725, 0.000027480262, 0.9999693, 0.000030668853, 0.9999653, 0.000034691, 0.9999633, 0.000036715628, 0.9999634, 0.00003661312, 0.9999638, 0.00003616648, 0.9999645, 0.00003553582, 0.9999655, 0.00003451951, 0.99996537, 0.00003464871, 0.99996424, 0.00003576841, 0.99996376, 0.000036239417, 0.9999635, 0.000036473473, 0.99996346, 0.000036550013, 0.99996525, 0.0000347521, 0.999969, 0.000030967865, 0.9999723, 0.000027741191, 0.99997324, 0.000026784186, 0.9999512, 0.00004882425, 0.9999511, 0.00004888775, 0.9999507, 0.000049290393, 0.9999503, 0.00004969543, 0.99995023, 0.000049781458, 0.9999507, 0.00004930577, 0.99995154, 0.000048428796, 0.99995, 0.0000500048, 0.99994415, 0.000055876513, 0.99993724, 0.00006275385, 0.99993384, 0.00006615027, 0.9999339, 0.00006610925, 0.9999346, 0.00006540249, 0.999936, 0.00006400968, 0.9999381, 0.00006191593, 0.9999377, 0.000062281986, 0.99993545, 0.00006456509, 0.9999344, 0.00006561132, 0.9999338, 0.00006624309, 0.9999336, 0.00006640674, 0.9999372, 0.00006281259, 0.99994427, 0.000055736582, 0.9999498, 0.000050215276, 0.9999513, 0.00004872373, 0.9999066, 0.00009339614, 0.99990803, 0.00009199515, 0.99990803, 0.000091958624, 0.99990684, 0.00009314453, 0.9999052, 0.00009474334, 0.9999049, 0.000095105475, 0.9999067, 0.00009330138, 0.9999042, 0.000095800926, 0.999893, 0.00010697265, 0.99988055, 0.000119436154, 0.99987507, 0.00012495903, 0.99987537, 0.00012461057, 0.99987686, 0.000123164, 0.99987984, 0.00012015607, 0.9998841, 0.00011585586, 0.9998834, 0.0001165909, 0.9998787, 0.000121313096, 0.99987626, 0.0001237235, 0.99987483, 0.00012519877, 0.9998741, 0.00012589454, 0.9998805, 0.00011952943, 0.9998937, 0.000106264946, 0.99990374, 0.00009623997, 0.99990606, 0.000093946495, 0.999814, 0.00018604373, 0.9998194, 0.00018057678, 0.999822, 0.00017795952, 0.99981993, 0.00018006697, 0.9998149, 0.00018507353, 0.9998117, 0.00018831878, 0.99981403, 0.00018594199, 0.99981, 0.00019001696, 0.9997888, 0.00021118713, 0.9997648, 0.00023521189, 0.999755, 0.00024499957, 0.9997566, 0.00024341136, 0.9997604, 0.00023962786, 0.99976736, 0.0002326578, 0.99977595, 0.00022406635, 0.99977404, 0.00022594759, 0.9997642, 0.00023578454, 0.9997587, 0.00024126074, 0.9997559, 0.00024406618, 0.9997552, 0.00024478568, 0.9997669, 0.00023309868, 0.999791, 0.00020897937, 0.99980915, 0.00019085742, 0.9998124, 0.0001875471, 0.9996104, 0.00038958172, 0.9996237, 0.00037630953, 0.99963254, 0.0003674648, 0.9996295, 0.00037050823, 0.99961895, 0.00038107854, 0.9996116, 0.00038839912, 0.9996157, 0.00038424614, 0.99960774, 0.00039227912, 0.99956405, 0.000435926, 0.9995131, 0.00048693825, 0.9994923, 0.000507706, 0.99949604, 0.0005039366, 0.9995058, 0.0004941721, 0.9995237, 0.00047630456, 0.99954224, 0.0004577637, 0.9995362, 0.0004637817, 0.99951404, 0.0004859364, 0.99950105, 0.0004989561, 0.9994963, 0.0005037254, 0.99950063, 0.0004993585, 0.9995281, 0.00047186133, 0.9995743, 0.00042566785, 0.99960643, 0.0003935532, 0.9996093, 0.00039072096, 0.999129, 0.0008710176, 0.99915534, 0.0008446354, 0.9991765, 0.0008234786, 0.99916947, 0.0008305059, 0.9991477, 0.0008522774, 0.9991376, 0.00086242886, 0.99915236, 0.00084766495, 0.9991407, 0.0008593391, 0.99904585, 0.00095417857, 0.99892277, 0.0010772167, 0.9988661, 0.0011339458, 0.9988719, 0.0011280895, 0.998896, 0.0011039841, 0.9989412, 0.0010588203, 0.9989852, 0.0010148175, 0.99896866, 0.0010313668, 0.9989155, 0.0010844914, 0.9988835, 0.001116514, 0.99887496, 0.0011250183, 0.99890107, 0.0010989481, 0.9989789, 0.0010210642, 0.9990772, 0.0009227945, 0.99913514, 0.000864842, 0.9991336, 0.0008664068, 0.99794, 0.0020600192, 0.9979966, 0.0020033538, 0.9980436, 0.0019563846, 0.9980174, 0.001982639, 0.997961, 0.0020390265, 0.99794877, 0.002051209, 0.99799836, 0.0020016555, 0.99799156, 0.0020084546, 0.9977906, 0.002209425, 0.99748623, 0.0025137495, 0.9973121, 0.002687894, 0.99730825, 0.0026917362, 0.9973735, 0.0026264568, 0.99749035, 0.0025096405, 0.9975946, 0.0024054043, 0.9975481, 0.0024518983, 0.9974142, 0.0025858383, 0.9973351, 0.0026649276, 0.99732536, 0.0026746525, 0.99741876, 0.0025812422, 0.9976374, 0.0023626375, 0.9978659, 0.0021340996, 0.9979716, 0.0020284287, 0.9979545, 0.002045523, 0.9949063, 0.005093691, 0.9950492, 0.0049508335, 0.99518484, 0.004815143, 0.9951161, 0.0048838565, 0.99494505, 0.005054978, 0.99490005, 0.0050999476, 0.9950073, 0.004992751, 0.994989, 0.005011048, 0.9945585, 0.0054415143, 0.99385226, 0.0061477595, 0.99336123, 0.0066387854, 0.9932788, 0.0067211934, 0.99343586, 0.0065641277, 0.99375, 0.0062500425, 0.9940212, 0.005978836, 0.99390703, 0.006092948, 0.9935586, 0.0064414325, 0.9933341, 0.0066658896, 0.9933098, 0.00669021, 0.9936014, 0.0063986476, 0.994227, 0.00577303, 0.9948119, 0.0051881005, 0.99502265, 0.004977351, 0.99494684, 0.005053169, 0.98728263, 0.012717368, 0.98755157, 0.012448413, 0.9879055, 0.01209451, 0.98785156, 0.012148415, 0.9874554, 0.012544547, 0.9872639, 0.012736104, 0.9873487, 0.01265131, 0.98704386, 0.012956168, 0.9859355, 0.014064481, 0.98443294, 0.015567039, 0.98334545, 0.01665455, 0.9830054, 0.016994573, 0.9832022, 0.016797809, 0.9838719, 0.01612815, 0.98460037, 0.015399609, 0.9844978, 0.015502234, 0.98374915, 0.016250823, 0.9831374, 0.016862595, 0.98288035, 0.017119633, 0.9834571, 0.016542898, 0.9851685, 0.014831496, 0.9869464, 0.013053605, 0.9876204, 0.012379594, 0.98743045, 0.012569528, 0.9696091, 0.030390892, 0.97007823, 0.029921787, 0.97067803, 0.029321963, 0.9706478, 0.02935218, 0.9699589, 0.030041112, 0.96950066, 0.030499328, 0.9694754, 0.030524615, 0.9682795, 0.031720534, 0.9651912, 0.034808822, 0.96190256, 0.03809745, 0.9600542, 0.039945755, 0.9594763, 0.040523723, 0.9594231, 0.04057685, 0.960149, 0.03985101, 0.9615776, 0.038422406, 0.96196985, 0.03803015, 0.9609771, 0.039022915, 0.9598514, 0.04014859, 0.9588539, 0.041146085, 0.95907515, 0.040924836, 0.9623573, 0.037642725, 0.9671771, 0.03282289, 0.969849, 0.030151037, 0.96988857, 0.030111454, 0.932973, 0.067026965, 0.934072, 0.06592798, 0.93534404, 0.064655945, 0.9351994, 0.064800635, 0.9339798, 0.066020206, 0.933063, 0.06693701, 0.9329467, 0.06705329, 0.93090785, 0.06909218, 0.9247384, 0.07526161, 0.91784173, 0.08215826, 0.9147004, 0.08529964, 0.91452295, 0.085477054, 0.9143632, 0.08563678, 0.91423094, 0.085769065, 0.9155537, 0.0844463, 0.91683316, 0.083166845, 0.91649824, 0.08350177, 0.9155228, 0.08447718, 0.9138202, 0.08617978, 0.9123636, 0.087636426, 0.91570777, 0.08429224, 0.92450315, 0.07549686, 0.9318093, 0.06819069, 0.9333568, 0.06664319, 0.8687464, 0.1312536, 0.8695978, 0.1304022, 0.8724475, 0.1275525, 0.873167, 0.12683302, 0.87175816, 0.1282418, 0.8705791, 0.12942089, 0.8702803, 0.12971964, 0.8679553, 0.13204469, 0.8601044, 0.13989562, 0.8495322, 0.15046781, 0.84393847, 0.1560615, 0.8443365, 0.1556635, 0.84512883, 0.15487118, 0.84422356, 0.15577647, 0.8440675, 0.15593246, 0.84509706, 0.15490296, 0.8457842, 0.15421578, 0.846157, 0.15384299, 0.84512657, 0.15487342, 0.84213996, 0.15786001, 0.84277886, 0.15722114, 0.8525733, 0.1474267, 0.8652089, 0.13479108, 0.86999017, 0.13000986, 0.77894115, 0.22105882, 0.7761308, 0.22386917, 0.7784147, 0.22158526, 0.7809628, 0.21903718, 0.7810877, 0.21891232, 0.78127223, 0.21872777, 0.78204507, 0.21795493, 0.77985567, 0.2201443, 0.7723337, 0.2276663, 0.7614026, 0.2385974, 0.7539118, 0.24608818, 0.7536724, 0.24632758, 0.75565046, 0.24434954, 0.7553894, 0.24461064, 0.75444436, 0.24555561, 0.7541504, 0.2458496, 0.7542693, 0.24573068, 0.75550526, 0.24449477, 0.75627464, 0.24372536, 0.75400066, 0.24599932, 0.751942, 0.24805802, 0.7580291, 0.24197091, 0.77197075, 0.22802925, 0.78087765, 0.21912237, 0.67834336, 0.32165664, 0.67227745, 0.32772258, 0.67012423, 0.3298758, 0.67197806, 0.3280219, 0.67367214, 0.32632786, 0.67585707, 0.32414296, 0.6792502, 0.32074982, 0.67918605, 0.32081395, 0.6731963, 0.3268037, 0.6646182, 0.33538178, 0.65796363, 0.34203637, 0.6565178, 0.3434822, 0.6582425, 0.34175748, 0.6588991, 0.34110087, 0.6586802, 0.3413198, 0.6584276, 0.3415724, 0.6574489, 0.3425511, 0.65752065, 0.34247935, 0.6588783, 0.3411217, 0.6585201, 0.3414799, 0.65648335, 0.34351662, 0.6580447, 0.34195527, 0.6674085, 0.33259147, 0.67784435, 0.32215565, 0.5881829, 0.41181707, 0.58429474, 0.4157053, 0.57970035, 0.42029965, 0.5788094, 0.42119062, 0.5798206, 0.42017943, 0.58186233, 0.41813767, 0.58571917, 0.4142808, 0.58822304, 0.411777, 0.58551484, 0.4144852, 0.5800838, 0.41991624, 0.5758826, 0.42411736, 0.574337, 0.42566302, 0.57493544, 0.4250646, 0.5755457, 0.42445424, 0.57576764, 0.4242324, 0.576291, 0.423709, 0.5758322, 0.4241678, 0.57486564, 0.42513436, 0.5751884, 0.4248116, 0.57562125, 0.42437875, 0.57491, 0.42509004, 0.5746729, 0.4253271, 0.57809657, 0.42190343, 0.5849465, 0.41505346, 0.52960753, 0.47039247, 0.5291107, 0.47088927, 0.5265207, 0.47347927, 0.5249138, 0.47508618, 0.52478206, 0.47521794, 0.525455, 0.474545, 0.5272525, 0.47274753, 0.52942085, 0.47057918, 0.5294043, 0.4705957, 0.52719784, 0.47280213, 0.5252251, 0.47477493, 0.52440274, 0.4755973, 0.5244124, 0.4755876, 0.52461845, 0.47538155, 0.52469563, 0.4753044, 0.5250993, 0.47490075, 0.52533877, 0.4746612, 0.5248158, 0.4751842, 0.5244874, 0.4755126, 0.5246559, 0.47534415, 0.5246128, 0.47538722, 0.52441424, 0.47558576, 0.52496296, 0.47503704, 0.5271981, 0.47280195, 0.505717, 0.49428302, 0.50598824, 0.49401173, 0.5054766, 0.49452344, 0.5048413, 0.49515867, 0.5045965, 0.49540353, 0.50463873, 0.49536127, 0.50497276, 0.49502724, 0.5056278, 0.49437222, 0.50599116, 0.49400887, 0.50561, 0.49439, 0.5050421, 0.49495792, 0.50478214, 0.49521786, 0.5047442, 0.49525583, 0.50477207, 0.49522793, 0.50476885, 0.49523112, 0.5048413, 0.49515867, 0.50500184, 0.49499816, 0.50496125, 0.49503878, 0.5047921, 0.4952079, 0.5047637, 0.49523625, 0.5047849, 0.4952151, 0.50475985, 0.49524018, 0.5047779, 0.4952221, 0.5050794, 0.4949206, 0.5004689, 0.49953112, 0.500528, 0.49947205, 0.50051117, 0.49948886, 0.50043654, 0.49956346, 0.50038636, 0.49961364, 0.50037515, 0.49962482, 0.5003918, 0.49960822, 0.500453, 0.49954703, 0.5005234, 0.4994766, 0.500518, 0.49948198, 0.50045466, 0.49954534, 0.5004152, 0.49958482, 0.5004081, 0.49959192, 0.5004094, 0.49959058, 0.5004077, 0.49959227, 0.5004083, 0.4995917, 0.5004272, 0.4995728, 0.50043875, 0.49956125, 0.50042146, 0.49957857, 0.5004076, 0.4995924, 0.50040853, 0.49959147, 0.50040895, 0.49959105, 0.5004081, 0.4995919, 0.5004185, 0.49958152, 0.5, 0.49999997, 0.5, 0.49999997, 0.5, 0.49999997, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.49999997, 0.5, 0.49999997, 0.5, 0.49999997, 0.5, 0.49999997, 0.5, 0.49999997, 0.5, 0.49999997, 0.50000006, 0.49999997, 0.50000006, 0.49999997, 0.50000006, 0.49999997, 0.50000006, 0.49999997, 0.50000006, 0.49999997, 0.50000006, 0.49999997, 0.50000006, 0.49999997, 0.50000006, 0.49999997, 0.50000006, 0.49999997, 0.5, 0.49999997, 0.5, 0.49999997, 0.5, 0.49999997, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5)
        local expectedIndices = #(0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0)

        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local meshPrimPath = (skelRootPath + "/Cylinder001")
        local meshPrim = stage.GetPrimAtPath meshPrimPath
        local mesh = pyUsdGeom.Mesh(meshPrim)

        local primvarApi = pyUsdGeom.PrimvarsAPI(mesh)
        local jointsIndicesPrimVar = (primvarApi.GetPrimvar("primvars:skel:jointIndices")).Get()
        local jointsWeightsPrimVar = (primvarApi.GetPrimvar("primvars:skel:jointWeights")).Get()
        local meshBindingApi = pyUsdSkel.BindingAPI.Get stage meshPrimPath

        assert_equal jointsIndicesPrimVar.count jointsWeightsPrimVar.count message: "Joint weights and indices array must have the same amount of elements"

        local areEqual = true
        for i = 1 to jointsIndicesPrimVar.count do
        (
            assert_equal jointsIndicesPrimVar[i] expectedIndices[i] message:"Export generated wrong joints indices for the skel binding"
            areEqual = (compareFloats jointsWeightsPrimVar[i] expectedWeights[i])
            assert_true areEqual message:"Export generated wrong joints weights for the skel binding"
            if not areEqual then exit -- This is a work around to prevent error spam when there's an error
        )

        assert_true areEqual message: "Exited checking for joints Weight early with an error!"
    ),

    function export_skeleton_deleted_modifier_test = 
    (
        resetMaxFileAndSetupUnits()

        newBones = createBonesArray 1

        -- create a simple cylinder character
        local char = Cylinder radius:6 height:36 heightsegs:30 pos:[0,0,0] isSelected:on
        convertToPoly char

        skinBonesToCharacter newBones char

        -- after a modifier was delete, it stays in memory and it would still create (in the Skeleton writer)
        -- the skeleton prim to export bones that were attached to it
        undo on
        (
            deleteModifier char 1
        )

        local exportPath = output_prefix + "export_skeleton_deleted_modifier_test.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local skelPrim = stage.GetPrimAtPath(skelRootPath + "/Bones")
        assert_false (pyUsd.Prim.IsValid skelPrim) Message: "Skeleton prim shouldn't have been created; modifier was deleted!"

        -- check if the skeleton can still be created after the undo
        max undo
        local exportPathUndo = output_prefix + "export_skeleton_deleted_modifier_undo_test.usda"
        USDExporter.ExportFile exportPathUndo exportOptions:exportOptions
        local undoStage = pyUsd.Stage.Open(exportPathUndo)
        local undoSkelPrim = undoStage.GetPrimAtPath(skelRootPath + "/Bones")
        assert_true (pyUsd.Prim.IsValid undoSkelPrim) Message: "Skeleton prim should have been created after undo!"
    ),

    function export_skeleton_can_export_conditions = 
    (
        resetMaxFileAndSetupUnits()

        newBones = createBonesArray 1

        animate on (
            in coordsys world (
                at time 30 newBones[1].rotation = (quat 0.275154 0 0 0.9614)
            )
        )

        -- create a simple cylinder character
        local char = Cylinder radius:6 height:36 heightsegs:30 pos:[0,0,0] isSelected:on
        convertToPoly char

        skinBonesToCharacter newBones char

        exportOptions.Meshes = false
        local exportPath = output_prefix + "export_skeleton_can_export_conditions_without_meshes.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local skelRootPrim = pyUsdSkel.Root(stage.GetPrimAtPath(skelRootPath))
        local skelPrim = stage.GetPrimAtPath(skelRootPath + "/Bones")
        local animPrim = stage.GetPrimAtPath(skelRootPath + "/Bones/Animations")

        assert_true (pyUsd.Prim.IsValid skelPrim) Message: "Skeleton prim should have been created even though Meshes is disabled"
        assert_true (pyUsd.Prim.IsValid animPrim) Message: "SkelAnimation prim should have been created even though Meshes is disabled"

        local meshPrimPath = (skelRootPath + "/Cylinder001")
        local meshPrim = stage.GetPrimAtPath meshPrimPath

        assert_false (pyUsd.Prim.IsValid meshPrim) Message: "Mesh prim shouldn't have been created with Meshes disabled"

        exportOptions.Meshes = true
        exportPath = output_prefix + "export_skeleton_can_export_conditions.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        meshPrim = stage.GetPrimAtPath meshPrimPath

        assert_true (pyUsd.Prim.IsValid meshPrim) Message: "Mesh prim should have been created with Meshes enabled"
    ),

    function export_skeleton_without_skin_test = 
    (
        resetMaxFileAndSetupUnits()

        newBones = createBonesArray 3

        b = createBone (matrix3 [0.707107,0,-0.707107] [0,1,0] [0.707107,0,0.707107] [24,0,50]) 
        b.parent = newBones[newBones.count-1]
        append newBones b

        b = createBone(matrix3 [0.707107,0,0.707107] [0,1,0] [-0.707107,0,0.707107] [-24,0,50])
        b.parent = newBones[newBones.count - 2]
        append newBones b
        
        animate on (
            in coordsys world (
                at time 30 newBones[2].rotation = (quat 0.275154 0 0 0.9614)
                at time 30 newBones[3].rotation = (quat 0.562967 0 0 0.826479)
                at time 30 newBones[4].rotation = (quat 0.229887 0.528306 0.151202 0.803233)
                at time 30 newBones[5].rotation = (quat 0.223103 -0.562692 -0.161043 0.779531)
            )
        )

        char = createCharMesh()

        -- the bones aren't being added to the skin modifier, thus shouldn't be exported
        sk = skin()
        addModifier char sk

        -- exporting bones without adding them to the skin modifier
        local exportPath = output_prefix + "export_skeleton_without_skin_test.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local meshPrimPath = (skelRootPath + "/Cylinder001")
        local skelRootPrim = pyUsdSkel.Root(stage.GetPrimAtPath(skelRootPath))
        local meshPrim = stage.GetPrimAtPath meshPrimPath
        local skelPrim = stage.GetPrimAtPath(skelRootPath + "/Bones")
        local animPrim = stage.GetPrimAtPath(skelRootPath + "/Bones/Animations")

        assert_false (pyUsd.Prim.IsValid skelPrim) Message: "Skeleton prim was created without bones being added to any skin modifier"
        assert_false (pyUsd.Prim.IsValid animPrim) Message: "SkelAnimation prim was created without bones being added to any skin modifier"
        
        local meshBindingApi = pyUsdSkel.BindingAPI.Get stage meshPrimPath
        local meshSkelRel = meshBindingApi.GetSkeletonRel()
        local meshSkelTargets = meshSkelRel.GetTargets()
        assert_equal 0 meshSkelTargets.count Message: "There shouldn't exist a SkelBinding without bones being exported"
    ),

    -- Test that instanced bones are not instanced in the output USD. 
    -- There is a limitation of PrimWriters, where only the first instance hits the write() 
    -- method, so we can't setup the UsdSkel prims properly, causing issues. Whenever this 
    -- is adressed, this test should start failing and can be removed.
    function bone_instancing_ignored_test = 
    (
        resetMaxFileAndSetupUnits()

        newBones = #()
        local firstBone = createBone (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0])
        firstBone.Name = "Bone1"
        local instancedBone = create_clone firstBone #instance "Bone2"
        local exportPath = output_prefix + "bone_instancing_ignored_test.usda"

        USDExporter.ExportFile exportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(exportPath)
        local bonePrim1 = stage.GetPrimAtPath("/root/Bone1")
        local bonePrim2 = stage.GetPrimAtPath("/root/Bone2")

        assert_false (bonePrim1.IsInstance())
        assert_false (bonePrim2.IsInstance())
    ),

    function export_skin_with_turbosmooth = 
    (
        resetMaxFileAndSetupUnits()

        newBones = createBonesArray 1

        local char = Box()
        convertToPoly char

        skinBonesToCharacter newBones char

        local ts = TurboSmooth iterations:1 renderIterations:2 useRenderIterations:on
        addModifier char ts

        local exportPath = output_prefix + "export_skin_with_turbosmooth.usda"
        exportOptions.BakeObjectOffsetTransform = true
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        -- convert to editable poly to check number of vertices
        convertToPoly char

        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local meshPrimPath = (skelRootPath + "/Box001")
        local meshPrim = stage.GetPrimAtPath meshPrimPath

        local polyNumVerts = polyop.getNumVerts char
        local exportedMeshNumVerts = ((meshPrim.GetAttribute("points")).Get()).count

        assert_equal exportedMeshNumVerts 8 message: "TurboSmooth should have been disabled for the export!"
        assert_equal polyNumVerts 26 message:"TurboSmooth should still be enabled after the export"
    ),

    function export_skin_with_wsm = 
    (
        resetMaxFileAndSetupUnits()

        newBones = createBonesArray 1

        local char = Box()
        convertToPoly char

        skinBonesToCharacter newBones char

        local sw = SpaceNoise()        
        sw.position = [20,30,40] 
        sw.rotation = (quat  0 0.707 0 0.707)
        sw.strength = [1000,0,0]
        bindSpaceWarp char sw

        local exportPath = output_prefix + "export_skin_with_wsm.usda"
        exportOptions.BakeObjectOffsetTransform = true
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        -- convert to editable poly to check number of vertices after export
        convertToPoly char

        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local meshPrimPath = (skelRootPath + "/Box001")
        local meshPrim = stage.GetPrimAtPath meshPrimPath

        local polyNumVerts = polyop.getNumVerts char
        local exportedMeshNumVerts = (meshPrim.GetAttribute("points")).Get()
        local expectedExportedNumVerts = #(#(-12.5, -12.5, 0), #(12.5, -12.5, 0), #(-12.5, 12.5, 0), #(12.5, 12.5, 0), #(-12.5, -12.5, 25), #(12.5, -12.5, 25), #(-12.5, 12.5, 25), #(12.5, 12.5, 25))
        local expectedVertsWithWSM = #(#(-12.5,-12.5,-37.5044), #(12.5,-12.5,-4.06682), #(-12.5,12.5,-61.7349), #(12.5,12.5,-27.9767), #(-12.5,-12.5,-12.5044), #(12.5,-12.5,20.9332), #(-12.5,12.5,-36.7349), #(12.5,12.5,-2.97669))

        -- SpaceNoise shouldn't affect points position after export
        assert_true (compare2dMatrix exportedMeshNumVerts exportedMeshNumVerts 8 3) message: "SpaceNoise should have been disabled for the export!"

        local theMesh = snapshotAsMesh char
        for i = 1 to polyNumVerts do (
            for x = 1 to 3 do (
                assert_float_equal (expectedVertsWithWSM[i])[x] (getVert theMesh i)[x] tolerance:1e-4 message: "The mesh should have maintained the SpaceNoise vertices trasnform after export!"
            )
        )
    ),

    function export_skin_with_offset_object = 
    (
        resetMaxFileAndSetupUnits()

        local char = Box()
        -- applying transform to the object (Box) only, not the node
        char.objectOffsetPos = [0,0,10]
        char.objectOffsetRot = (quat 0 0.5 0 1)

        newBones = createBonesArray 1
        skinBonesToCharacter newBones char

        local exportPath = output_prefix + "export_skin_with_offset_object.usda"
        exportOptions.BakeObjectOffsetTransform = false
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        local stage = pyUsd.Stage.Open(exportPath)
        local skelRootPath = "/root"
        local meshPrimPath = (skelRootPath + "/Box001/Box001_Shape")
        local meshPrim = stage.GetPrimAtPath meshPrimPath
        local geomBindTransform = (meshPrim.GetAttribute("primvars:skel:geomBindTransform")).Get()
        local expectedBindTransform = #( #(0.6, 0, 0.8, 0), #(0, 1, 0, 0), #(-0.8, 0, 0.6, 0), #(0, 0, 10, 1) )

        -- When BakeOffset transform is off, the mesh and the xform prim should be split
        -- The bindTransform should account for this difference by using the object transform
        assert_true (compare2dMatrix geomBindTransform expectedBindTransform 4 4) message: "BindTransform should account for the mesh object offset"
    ),

    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
        maxops.autoKeyDefaultKeyOn = wasAutoKeyOn
    ),

    Tests = #(
        export_skin_animation_test,
        export_skin_invalid_root_test,
        export_multiple_bind_transform_test,
        export_skeleton_can_export_conditions,
        export_skeleton_without_skin_test,
        export_skeleton_bone_weights_test,
        bone_instancing_ignored_test,
        export_skin_with_turbosmooth,
        export_skin_with_wsm,
        export_skin_with_offset_object,
        export_skeleton_deleted_modifier_test
    )
)

runUsdTestFixture export_skin_test(getThisScriptFilename())