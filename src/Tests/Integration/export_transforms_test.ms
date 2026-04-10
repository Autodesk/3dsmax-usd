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
pyGf = python.import("pxr.Gf")
pyVt = python.import("pxr.Vt")

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_TRANSFORMS_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_transforms_test
(
    private 
    exportOptions = USDExporter.CreateOptions(),

    public 
    function setup =
    (
        quickReset()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
    ),

    function test_object_offset_transforms = (
        exportFormats = #(#singleMatrix, #splitComponents)

        for exportFormat in exportFormats do (
            quickReset()
            -- Each pair : (v1: expected usd suffix for the object on the USD side, v2: Max node).
            local light = DataPair "Light" (free_light());
            local camera = DataPair "Camera" (Freecamera())
            local cube = DataPair "Shape" (box())
            local helper = DataPair "Dummy" (dummy())
            local usdStage = DataPair "Layer" (UsdStageObject())
            local anonUsdStage = DataPair "Layer" (UsdStageObject())
            sphere()    
            sphere()
            local sphereGroup = DataPair "Dummy" (group $sphere*) -- Groups are dummy nodes also.

            -- Set empty root layer (but valid one) from file
            local testDataDir = getFilenamePath (getThisScriptFilename())
            local sample = (testDataDir + @"data\empty.usda")
            usdStage.v2.SetRootLayer sample

            local nodes = #(light, camera, cube, helper, usdStage, sphereGroup);

            -- This setting only applies to meshes and is tested in export_options_test. Here we are only 
            -- testing the general case, i.e. when Max node maps to two USD prims, an Xform for the Node 
            -- transform, and xformable prim for the max node.
            exportOptions.BakeObjectOffsetTransform = false
            
            -- Apply some translation/ rotation / scale to both the nodes. Leave offset transform as identity
            for n in nodes do (
                n.v2.pos = [1,2,3]
                n.v2.scale = [1,2,3]
                n.v2.rotation = quat 1 2 3 1
            )

            local exportPath = output_prefix + (exportFormat as string) + "_test_object_offset_transforms_identity.usd"
            exportOptions.TransformFormat = exportFormat

            USDExporter.ExportFile exportPath exportOptions:exportOptions
            local stage = pyUsd.Stage.Open(exportPath)

            -- First validate that the anonUsdStage was not exported
            local anonStageNodePath = "/" + anonUsdStage.v2.name;
            local anonStageNodePrim = stage.GetPrimAtPath(anonStageNodePath)
            assert_false (pyUsd.Prim.IsValid(anonStageNodePrim)) message:("There should be no prim/node for because it has an anonymous root " + anonUsdStage.v2.name)

            -- Validate the USD export... Max nodes should map to single USD prims.
            for n in nodes do (
                local nodePath = "/" + n.v2.name;
                local objectPath = nodePath + "/" + n.v2.name + "_" + n.v1
                local nodePrim = stage.GetPrimAtPath(nodePath)
                local objectPrim = stage.GetPrimAtPath(objectPath)
                assert_true (pyUsd.Prim.IsValid(nodePrim)) message:("Missing node prim for " + n.v2.name)
                assert_false (pyUsd.Prim.IsValid(objectPrim)) message:"Should not need a sub-object prim, offset transform was the identity"
                -- Make sure the node's transform is valid : 
                xformable = pyUsdGeom.Xformable(nodePrim)
                assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) n.v2.transform)  message:("The transform is wrong in the USD scene for " + nodePath)
            )
            
            -- Now add some translation/ rotation / scale to both the object offsets.
            for n in nodes do (
                n.v2.objectOffsetPos = [4,5,6]
                n.v2.objectOffsetScale = [4,5,6]
                n.v2.objectOffsetRot = quat 4 5 6 1
            )
            -- Re-export...
            exportPath = output_prefix + (exportFormat as string) + "_test_object_offset_transforms.usd"
            USDExporter.ExportFile exportPath exportOptions:exportOptions
            stage = pyUsd.Stage.Open(exportPath)
            
            -- Validate the USD export... Max node should map to 2 USD prims, to properly represent the object offset with an Xform.
            for n in nodes do (
                local nodePath = "/" + n.v2.name;
                local objectPath = nodePath + "/" + n.v2.name + "_" + n.v1            
                local xformPrim = stage.GetPrimAtPath(nodePath)
                local objectPrim = stage.GetPrimAtPath(objectPath)
                assert_true (pyUsd.Prim.IsValid(xformPrim)) message:("Missing xform object for " + n.v2.name)
                assert_true (xformPrim.IsA(pyUsdGeom.Xform)) message:("Wrong prim type for " + nodePath)
                assert_true (pyUsd.Prim.IsValid(objectPrim)) message:("Missing object prim for " + objectPath)
                
                -- Make sure the node transform and the offset transform are both valid : 
                xformable = pyUsdGeom.Xformable(xformPrim)
                assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) n.v2.transform)  message:("The transform is wrong in the USD scene for " + nodePath)
                objectXForm = pyUsdGeom.Xformable(objectPrim)
                local scaleOffset = scaleMatrix n.v2.objectOffsetScale
                local rotationOffset =n.v2.objectOffsetRot as matrix3
                local translationOffset = transMatrix n.v2.objectOffsetPos
                local offsetTransform = scaleOffset * rotationOffset * translationOffset

                if (isGroupHead n.v2) then (
                    -- for max groups, object offset transforms should not have been exported
                    local expectedIdentityTransform = (pyGf.Matrix4d 1.0)
                    assert_true (compare2dMatrix expectedIdentityTransform (objectXForm.GetLocalTransformation()) 4 4) message:("Expected identity on XForm for exported max group for " + objectPath)
                ) else (
                    assert_true (compareUsdAndMaxTransforms (objectXForm.GetLocalTransformation()) offsetTransform) message:("The transform is wrong in the USD scene for " + objectPath)
                )
            )
        )
    ),
    function test_object_without_prim_creator = (
        -- The tape object should not have any prim creator
        local tapeNode = Tape Name:"tape" pos:[100,100,100]
        local boxNode = Box Name:"box" pos:[10,10,10]
        append tapeNode.children boxNode
        
        local exportPath = output_prefix + "test_object_without_prim_creator.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local tapePrim = stage.GetPrimAtPath("/tape")
        local boxPrim = stage.GetPrimAtPath("/tape/box")
        
        assert_true (pyUsd.Prim.IsValid(tapePrim)) message:("Missing Tape prim")
        assert_true (pyUsd.Prim.IsValid(boxPrim)) message:("Missing Box prim")
            
        xformable = pyUsdGeom.Xformable(tapePrim)
        assert_true (compareUsdAndMaxTransforms (xformable.GetLocalTransformation()) tapeNode.transform)
    ),
    
    function export_object_with_wsm_test = (
        -- Create a hierarchy with transforms and object offsets.
        local parent = PointHelperObj name:"parent"
        parent.position = [-10,-10,-10]
        parent.rotation = (quat  0.3 0 0 0.3)
        
        local boxNode = box name:"box"
        boxNode.parent = parent
        boxNode.position = [1,2,3]
        boxNode.objectOffsetPos = [10,15,20]
        boxNode.objectOffsetRot = (quat  0.3025 0 0 0.707)
        
        -- Create a Noise space warp and bind it to the box.
        local sw = SpaceNoise()        
        sw.position = [20,30,40] 
        sw.rotation = (quat  0 0.707 0 0.707)
        sw.strength = [1000,0,0]
        bindSpaceWarp boxNode sw
        
        -- 1) Make sure the exported box is at the correct position considering the space warp.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.BakeObjectOffsetTransform = true
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        -- Disallow nesting, so that we isolate the effect of the offset when creating or not a shape prim VS the WSM.
        exportOptions.AllowNestedGprims = true
        local exportPath = output_prefix + "export_wsm_bake_offset_on.usda"

        assert_true (USDExporter.ExportFile exportPath exportOptions:exportOptions)
        local stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/parent/box")
        xformable = pyUsdGeom.Xformable(boxPrim)
        local exportedTransform = xformable.GetLocalTransformation()
        local expectedTransform = (pyGf.Matrix4d 1 0 0 0 0 0 -1 0 0 1 0 0 11 13 8 1)
        assert_true(compareUsdAndMaxTransforms exportedTransform expectedTransform) message:"Incorrect box transform following space warp."
        local boxShapePrim = stage.GetPrimAtPath("/parent/box/box_Shape")
        assert_false (boxShapePrim.IsValid()) message:"A shape prim should not have been exported."
        
        -- Make sure the points are being transformed by the SW and moved back to local space.
        local pointsWithWSM = (pyVt.Vec3fArray #((pyGf.Vec3f -2.5 6.368503 -18.467451), (pyGf.Vec3f 22.5 6.3685 7.430602), (pyGf.Vec3f -2.5 23.631504 -12.123078), (pyGf.Vec3f 22.500002 23.6315 7.705943), (pyGf.Vec3f -2.5 24.451326 24.987469), (pyGf.Vec3f 22.500002 24.451324 44.305202), (pyGf.Vec3f -2.4999962 41.71432 46.4238), (pyGf.Vec3f 22.500002 41.71432 52.274662)))
        local geom = pyUsdGeom.Mesh(boxPrim)
        local boxPrimPoints = (geom.GetPointsAttr()).Get()    
        assert_true (compareVec3fArrays pointsWithWSM boxPrimPoints) message:"The exported vertices with the noise WSM applied are not correct."
        
        -- 2) Rerun the same test, but now with BakeObjectOffsetTransform disabled. Still not expecting 
        -- a Shape prim, even though an offset exists, indeed, the offset will be part of the transform
        -- applied to the mesh's point in world space.
        exportOptions.BakeObjectOffsetTransform = false
        exportPath = output_prefix + "export_wsm_bake_offset_off.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/parent/box")
        xformable = pyUsdGeom.Xformable(boxPrim)
        local exportedTransform = xformable.GetLocalTransformation()
        assert_true(compareUsdAndMaxTransforms exportedTransform expectedTransform) message:"Incorrect box transform following space warp."
        local boxShapePrim = stage.GetPrimAtPath("/parent/box/box_Shape")
        assert_false (boxShapePrim.IsValid()) message:"A shape prim should not have been exported."
        
        -- Make sure the points are being transformed by the SW and moved back to local space.
        local geom = pyUsdGeom.Mesh(boxPrim)
        local boxPrimPoints = (geom.GetPointsAttr()).Get()            
        assert_true (compareVec3fArrays pointsWithWSM boxPrimPoints) message:"The exported vertices with the noise WSM applied are not correct."
            
        -- 3) Make sure that adding a child node makes no difference.
        childBox = box name:"child"
        childBox.parent = boxNode
        exportPath = output_prefix + "export_wsm_bake_offset_with_child.usda"
        exportOptions.BakeObjectOffsetTransform = true
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        -- Make sure the box prim is at the correct transform.
        stage = pyUsd.Stage.Open(exportPath)
        boxPrim = stage.GetPrimAtPath("/parent/box")
        xformable = pyUsdGeom.Xformable(boxPrim)
        exportedTransform = xformable.GetLocalTransformation()
        assert_true(compare2dMatrix exportedTransform expectedTransform 4 4) message:"Incorrect box transform following space warp."
        boxShapePrim = stage.GetPrimAtPath("/parent/box/box_Shape")
        assert_false (boxShapePrim.IsValid()) message:"A shape prim should not have been exported."
        childPrim = stage.GetPrimAtPath("/parent/box/child")
        assert_true (childPrim.IsValid()) message:"The child prim should have been exported."    
            
        -- Make sure the points are being transformed by the SW and moved back to local space.
        local geom = pyUsdGeom.Mesh(boxPrim)
        local boxPrimPoints = (geom.GetPointsAttr()).Get()            
        assert_true (compareVec3fArrays pointsWithWSM boxPrimPoints) message:"The exported vertices with the noise WSM applied are not correct."    
        
        -- 4) Now disable the WSM. Points should no longer be affected byt the WSM.    
        boxNode.modifiers[1].enabled = false
        exportOptions.BakeObjectOffsetTransform = true
        exportPath = output_prefix + "export_wsm_disabled_mod.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions    
        stage = pyUsd.Stage.Open(exportPath)
        boxPrim = stage.GetPrimAtPath("/parent/box")
        xformable = pyUsdGeom.Xformable(boxPrim)        
        exportedTransform = xformable.GetLocalTransformation()
        assert_true(compare2dMatrix exportedTransform expectedTransform 4 4) message:"Incorrect box transform following space warp."
        boxShapePrim = stage.GetPrimAtPath("/parent/box/box_Shape")
        assert_false (boxShapePrim.IsValid())

        -- Make sure the points are no longer affected by the WSM.
        local geom = pyUsdGeom.Mesh(boxPrim)
        local boxPrimPoints = (geom.GetPointsAttr()).Get()
        local pointsWithoutWSM = (pyVt.Vec3fArray #((pyGf.Vec3f -2.5 6.368498 29.041412), (pyGf.Vec3f 22.5 6.368498 29.041412), (pyGf.Vec3f -2.5 23.631502 10.958587), (pyGf.Vec3f 22.5 23.631502 10.958587), (pyGf.Vec3f -2.5 24.451324 46.304417), (pyGf.Vec3f 22.5 24.451324 46.304417), (pyGf.Vec3f -2.5 41.71433 28.221592), (pyGf.Vec3f 22.5 41.71433 28.221592)))
        assert_true (compareVec3fArrays pointsWithoutWSM boxPrimPoints) message:"The exported vertices with the noise WSM disabled are not correct."
    ),

    function setup_worldspaceroot_tests = (
        -- Create a hierarchy with transforms and object offsets.
        local parent = PointHelperObj name:"parent"
        parent.position = [10, 10, 25]
        
        local boxNode = box name:"box"
        boxNode.parent = parent
        boxNode.position = [40, 40, 25]

        local boxNode2 = box name:"box2"
        boxNode2.parent = boxNode
        boxNode2.position = [40, 40, 50]

        select #(boxNode, boxNode2)  
    ),

    function test_worldspaceroot_on = (

        setup_worldspaceroot_tests()

        local exportOptions = USDExporter.CreateOptions()
        exportOptions.UseWorldspaceRoot = true
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
    
        local exportPath = output_prefix + "test_worldspaceRoot_ON.usda"

        exportFormats = #(#singleMatrix, #splitComponents)
        for exportFormat in exportFormats do (
            exportOptions.TransformFormat = exportFormat
            assert_true (USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected)

            local stage = pyUsd.Stage.Open(exportPath)
            -- Validate "box" transform
            local boxPrim = stage.GetPrimAtPath("/box")
            assert_true (boxPrim.IsValid()) message:"Missing 'box' prim"
            local boxXformable = pyUsdGeom.Xformable(boxPrim)
            local boxTransform = boxXformable.GetLocalTransformation()
            local expectedBoxTransform = (pyGf.Matrix4d 1 0 0 0 0 1 0 0 0 0 1 0 40 40 25 1)
            assert_true (compare2dMatrix boxTransform expectedBoxTransform 4 4) message:"Incorrect transform for 'box'"

            -- Validate "box2" transform
            local box2Prim = stage.GetPrimAtPath("/box/box2")
            assert_true (box2Prim.IsValid()) message:"Missing 'box2' prim"
            local box2Xformable = pyUsdGeom.Xformable(box2Prim)
            local box2Transform = box2Xformable.GetLocalTransformation()
            local expectedBox2Transform = (pyGf.Matrix4d 1 0 0 0 0 1 0 0 0 0 1 0 0 0 25 1)
            assert_true (compare2dMatrix box2Transform expectedBox2Transform 4 4) message:"Incorrect transform for 'box2'"
        )
    ),

    function test_worldspaceroot_off = (

        setup_worldspaceroot_tests()

        local exportOptions = USDExporter.CreateOptions()
        exportOptions.UseWorldspaceRoot = false
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"

        local exportPath = output_prefix + "test_worldspaceRoot_OFF.usda"

        exportFormats = #(#singleMatrix, #splitComponents)
        for exportFormat in exportFormats do (
            exportOptions.TransformFormat = exportFormat
            assert_true (USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected)

            local stage = pyUsd.Stage.Open(exportPath)
            stage.Reload()
            -- Validate "box" transform
            local boxPrim = stage.GetPrimAtPath("/box")
            assert_true (boxPrim.IsValid()) message:"Missing 'box' prim"
            local boxXformable = pyUsdGeom.Xformable(boxPrim)
            local boxTransform = boxXformable.GetLocalTransformation()
            local expectedBoxTransform = (pyGf.Matrix4d 1 0 0 0 0 1 0 0 0 0 1 0 30 30 0 1)
            assert_true (compare2dMatrix boxTransform expectedBoxTransform 4 4) message:"Incorrect transform for 'box'"

            -- Validate "box2" transform
            local box2Prim = stage.GetPrimAtPath("/box/box2")
            assert_true (box2Prim.IsValid()) message:"Missing 'box2' prim"
            local box2Xformable = pyUsdGeom.Xformable(box2Prim)
            local box2Transform = box2Xformable.GetLocalTransformation()
            local expectedBox2Transform = (pyGf.Matrix4d 1 0 0 0 0 1 0 0 0 0 1 0 0 0 25 1)
            assert_true (compare2dMatrix box2Transform expectedBox2Transform 4 4) message:"Incorrect transform for 'box2'"
        )
    ),

    function test_export_transform_with_spline_animation = (
        -- Check if USD version supports splines (24.11+)
        usdVersion = pyUsd.GetVersion()
        local supportCurves = (usdVersion[2] > 24)
        if not supportCurves then (
            format "Skipping test_export_transform_with_spline_animation: USD version % does not support curves\n" usdVersion
            return undefined
        )

        -- Import the Ts module for spline validation
        pyTs = python.import("pxr.Ts")

        -- Create a box with animated transforms
        local boxNode = box name:"animated_box"
        
        -- Set up animation range
        animationRange = interval 0 100
        
        -- Animate position (translation) using keyframes
        with animate on (
            at time 0 (boxNode.pos = [0, 0, 0])
            at time 50 (boxNode.pos = [100, 50, 25])
            at time 100 (boxNode.pos = [200, 100, 50])
        )

        local posCtrl = boxNode.pos.controller
        local xCtrl = posCtrl[1]  -- X position controller

        for k in xCtrl.keys do (
            k.inTangentType = #custom
            k.outTangentType = #custom
            k.inTangent = 2.0
            k.outTangent = 2.0
            k.inTangentLength = 0.333
            k.outTangentLength = 0.333
        )

        -- Animate rotation using keyframes
        with animate on (
            at time 0 (boxNode.rotation = quat 0 0 0 1)
            at time 50 (boxNode.rotation = eulerAngles 45 30 15)
            at time 100 (boxNode.rotation = eulerAngles 90 60 30)
        )

        local rotationXControl = boxNode.rotation.controller[1]
        for k in rotationXControl.keys do (
            k.inTangentType = #step
            k.outTangentType = #step
        )

        local rotationYControl = boxNode.rotation.controller[2]
        for k in rotationYControl.keys do (
            k.inTangentType = #smooth
            k.outTangentType = #smooth
        )
        
        -- Animate scale using keyframes
        with animate on (
            at time 0 (boxNode.scale = [1, 1, 1])
            at time 50 (boxNode.scale = [2, 1.5, 1.25])
            at time 100 (boxNode.scale = [3, 2, 1.5])
        )
        
        -- Configure export options for spline animation
        local curveExportOptions = USDExporter.CreateOptions()
        curveExportOptions.FileFormat = #ascii
        curveExportOptions.RootPrimPath = "/"
        curveExportOptions.UpAxis = #z -- Keep Max's Z-up coordinate system to avoid axis conversion
        curveExportOptions.TimeMode = #frameRange
        curveExportOptions.StartFrame = 0
        curveExportOptions.EndFrame = 100
        curveExportOptions.AnimationType = #curves
        curveExportOptions.TransformFormat = #splitComponents -- Required for spline export
        
        local exportPath = output_prefix + "test_export_transform_spline_animation.usda"
        
        -- Export with spline animation
        assert_true (USDExporter.ExportFile exportPath exportOptions:curveExportOptions) message:"Export with spline animation failed"
        
        -- Open the exported stage and validate splines
        local stage = pyUsd.Stage.Open(exportPath)
        assert_true (stage != undefined) message:"Failed to open exported USD stage"
        
        local boxPrim = stage.GetPrimAtPath("/animated_box")
        assert_true (pyUsd.Prim.IsValid(boxPrim)) message:"Missing animated_box prim"
        
        -- Validate that transform attributes have splines
        local xformable = pyUsdGeom.Xformable(boxPrim)
        local xformOps = xformable.GetOrderedXformOps()
        
        -- We expect split components: translateX/Y/Z, rotateX/Y/Z, scaleX/Y/Z
        assert_true (xformOps.count == 9) message:"Missing split components in transform"
        
        -- Helper function to validate a spline attribute exists and has knots
        fn validateSplineAttribute prim attrName expectedKnotCount = (
            local attr = prim.GetAttribute(attrName)
            if attr == undefined or not attr.IsValid() then (
                return false
            )
            local spline = attr.GetSpline()
            local knots = spline.GetKnots()
            return knots.count == expectedKnotCount
        )
        
        -- Validate translation splines (3 keyframes = 3 knots each)
        assert_true (validateSplineAttribute boxPrim "xformOp:translateX" 3) message:"translateX spline should have 3 knots"
        assert_true (validateSplineAttribute boxPrim "xformOp:translateY" 3) message:"translateY spline should have 3 knots"
        assert_true (validateSplineAttribute boxPrim "xformOp:translateZ" 3) message:"translateZ spline should have 3 knots"
        
        -- Validate rotation splines (3 keyframes = 3 knots each)
        assert_true (validateSplineAttribute boxPrim "xformOp:rotateX" 3) message:"rotateX spline should have 3 knots"
        assert_true (validateSplineAttribute boxPrim "xformOp:rotateY" 3) message:"rotateY spline should have 3 knots"
        assert_true (validateSplineAttribute boxPrim "xformOp:rotateZ" 3) message:"rotateZ spline should have 3 knots"
        
        -- Validate scale splines (3 keyframes = 3 knots each)
        assert_true (validateSplineAttribute boxPrim "xformOp:scaleX" 3) message:"scaleX spline should have 3 knots"
        assert_true (validateSplineAttribute boxPrim "xformOp:scaleY" 3) message:"scaleY spline should have 3 knots"
        assert_true (validateSplineAttribute boxPrim "xformOp:scaleZ" 3) message:"scaleZ spline should have 3 knots"
        
        -- Validate spline values at keyframe times
        -- Check translateX values at key times
        translateXAttr = boxPrim.GetAttribute("xformOp:translateX")
        translateXSpline = translateXAttr.GetSpline()
        local translateXKnots = translateXSpline.GetKnots()
        
        -- Knots should be at times 0, 50, 100 with values 0, 100, 200
        local knot0 = translateXKnots[1]
        local knot50 = translateXKnots[51]
        local knot100 = translateXKnots[101]
        local knot0Value = knot0.GetValue()
        local knot50Value = knot50.GetValue()
        local knot100Value = knot100.GetValue()
        assert_true (compareFloats knot0Value 0.0) message:("translateX knot at time 0: expected 0.0, got " + (knot0Value as string))
        assert_true (compareFloats knot50Value 82.358825) message:("translateX knot at time 50: expected 82.3588, got " + (knot50Value as string))
        assert_true (compareFloats knot100Value 120.892959) message:("translateX knot at time 100: expected 120.8929, got " + (knot100Value as string))
        
        -- Validate tangent width and slope values on knots
        -- First knot (time 1) - should have post tangent data
        local knot0PreWidth = knot0.GetPreTanWidth()
        local knot0PreSlope = knot0.GetPreTanSlope()
        local knot0PostWidth = knot0.GetPostTanWidth()
        local knot0PostSlope = knot0.GetPostTanSlope()
        assert_true (compareFloats knot0PreWidth 0.0) message:("First knot pre tangent width should be 0.0, got " + (knot0PreWidth as string))
        assert_true (compareFloats knot0PreSlope 0.0) message:("First knot pre tangent slope should be 0.0, got " + (knot0PreSlope as string))

        -- First knot should use curve interpolation (not held)
        local knot0Interp = knot0.GetNextInterpolation()
        assert_true (knot0Interp == pyTs.InterpCurve) message:("First knot should have InterpCurve interpolation, got " + (knot0Interp as string))
        
        -- Middle knot (time 51) - should have both pre and post tangent data
        local knot50 = translateXKnots[51]
        local knot50PreWidth = knot50.GetPreTanWidth()
        local knot50PreSlope = knot50.GetPreTanSlope()
        local knot50PostWidth = knot50.GetPostTanWidth()
        local knot50PostSlope = knot50.GetPostTanSlope()
        assert_true (compareFloats knot50PreWidth 16.649999) message:("Middle knot pre tangent width should be 16.649999, got " + (knot50PreWidth as string))
        assert_true (compareFloats knot50PostWidth 16.649999) message:("Middle knot post tangent width should be 16.649999, got " + (knot50PostWidth as string))
        assert_true (compareFloats knot50PreSlope -10.666667) message:("Middle knot pre tangent slope should be 10.666667, got " + (knot50PreSlope as string))
        assert_true (compareFloats knot50PostSlope 10.666667) message:("Middle knot post tangent slope should be 10.666667, got " + (knot50PostSlope as string))
        -- Middle knot should use curve interpolation (not held)
        local knot50Interp = knot50.GetNextInterpolation()
        assert_true (knot50Interp == pyTs.InterpCurve) message:("Middle knot should have InterpCurve interpolation, got " + (knot50Interp as string))
        
        -- Last knot (time 101) - should have pre tangent data
        local knot100PreWidth = knot100.GetPreTanWidth()
        local knot100PreSlope = knot100.GetPreTanSlope()
        local knot100PostWidth = knot100.GetPostTanWidth()
        local knot100PostSlope = knot100.GetPostTanSlope()
        assert_true (compareFloats knot100PreWidth 16.649999) message:("Last knot pre tangent width should be 16.649999, got " + (knot100PreWidth as string))
        assert_true (compareFloats knot100PreSlope -10.666667) message:("Last knot pre tangent slope should be 10.666667, got " + (knot100PreSlope as string))
        assert_true (compareFloats knot100PostWidth 0.0) message:("Last knot post tangent width should be 0.0, got " + (knot100PostWidth as string))
        assert_true (compareFloats knot100PostSlope 0.0) message:("Last knot post tangent slope should be 0.0, got " + (knot100PostSlope as string))
        -- Last knot should use held interpolation (no more keyframes after)
        local knot100Interp = knot100.GetNextInterpolation()
        assert_true (knot100Interp == pyTs.InterpHeld) message:("Last knot should have InterpHeld interpolation, got " + (knot100Interp as string))

        -- Validate rotation spline interpolation types
        -- rotateX was set with #step tangent type - should map to InterpHeld
        local rotateXAttr = boxPrim.GetAttribute("xformOp:rotateX")
        local rotateXSpline = rotateXAttr.GetSpline()
        local rotateXKnots = rotateXSpline.GetKnots()
        local rotateXKnot0 = rotateXKnots[1]
        local rotateXKnot0Interp = rotateXKnot0.GetNextInterpolation()
        assert_true (rotateXKnot0Interp == pyTs.InterpHeld) message:("rotateX first knot should have InterpHeld (step) interpolation, got " + (rotateXKnot0Interp as string))
        local rotateXKnot50 = rotateXKnots[51]
        local rotateXKnot50Interp = rotateXKnot50.GetNextInterpolation()
        assert_true (rotateXKnot50Interp == pyTs.InterpHeld) message:("rotateX middle knot should have InterpHeld (step) interpolation, got " + (rotateXKnot50Interp as string))

        -- rotateY was set with #smooth tangent type - should map to InterpCurve
        local rotateYAttr = boxPrim.GetAttribute("xformOp:rotateY")
        local rotateYSpline = rotateYAttr.GetSpline()
        local rotateYKnots = rotateYSpline.GetKnots()
        local rotateYKnot0 = rotateYKnots[1]
        local rotateYKnot0Interp = rotateYKnot0.GetNextInterpolation()
        assert_true (rotateYKnot0Interp == pyTs.InterpCurve) message:("rotateY first knot should have InterpCurve (smooth) interpolation, got " + (rotateYKnot0Interp as string))
        local rotateYKnot50 = rotateYKnots[51]
        local rotateYKnot50Interp = rotateYKnot50.GetNextInterpolation()
        assert_true (rotateYKnot50Interp == pyTs.InterpCurve) message:("rotateY middle knot should have InterpCurve (smooth) interpolation, got " + (rotateYKnot50Interp as string))

        -- Test export with #both option (curves + time samples)
        curveExportOptions.AnimationType = #both
        local exportPathBoth = output_prefix + "test_export_transform_spline_animation_both.usda"
        assert_true (USDExporter.ExportFile exportPathBoth exportOptions:curveExportOptions) message:"Export with both animation types failed"
        
        -- Validate that both splines and time samples exist
        local stageBoth = pyUsd.Stage.Open(exportPathBoth)
        local boxPrimBoth = stageBoth.GetPrimAtPath("/animated_box")
        assert_true (pyUsd.Prim.IsValid(boxPrimBoth)) message:"Missing animated_box prim when AnimationType is #both"

        local xformableBoth = pyUsdGeom.Xformable(boxPrimBoth)
        local xformOpsBoth = xformableBoth.GetOrderedXformOps()
        assert_true (xformOpsBoth.Count == 9) message:"Wrong number of transforms when AnimationType is #both"

        expectedOps = #("xformOp:translateX", "xformOp:translateY", "xformOp:translateZ", "xformOp:rotateZ", "xformOp:rotateY", "xformOp:rotateX", "xformOp:scaleX", "xformOp:scaleY", "xformOp:scaleZ")
        for i in 1 to expectedOps.Count do (
            assert_true (((xformOpsBoth[i]).GetName()) == (expectedOps[i]))
        )

        -- Validate that the time samples are exported correctly
        local translateXAttrBoth = boxPrimBoth.GetAttribute("xformOp:translateX")
        local timeSamplesBoth = translateXAttrBoth.GetTimeSamples()
        assert_true (timeSamplesBoth.Count == 101)
    ),

    function test_target_camera_exports_time_samples = (
        usdVersion = pyUsd.GetVersion()
        local supportCurves = (usdVersion[2] > 24)
        if not supportCurves then (
            format "Skipping test_target_camera_exports_time_samples: USD version % does not support curves\n" usdVersion
            return undefined
        )

        -- Test that a camera with a target exports as time samples even when AnimationType is curves
        b = box height:20 width:20 lenght:20 pos:[25, 25, 0]
        local cam = (TargetCamera height:10 length:10 width:10 pos:[100, 100, 0] target:b)

        with animate on (
            at time 0 (b.pos = [0, 100, 0])
            at time 5 (b.pos = [100, 100, 0])
            at time 10 (b.pos = [200, 100, 0])
        )

        local curveExportOptions = USDExporter.CreateOptions()
        curveExportOptions.FileFormat = #ascii
        curveExportOptions.RootPrimPath = "/"
        curveExportOptions.TimeMode = #frameRange
        curveExportOptions.StartFrame = 0
        curveExportOptions.EndFrame = 10
        curveExportOptions.AnimationType = #curves
        curveExportOptions.TransformFormat = #splitComponents

        local exportPath = output_prefix + "test_target_camera_time_samples.usda"

        assert_true (USDExporter.ExportFile exportPath exportOptions:curveExportOptions) message:"Export with target camera failed"

        local stage = pyUsd.Stage.Open(exportPath)
        local cameraPrim = stage.GetPrimAtPath("/Camera001")

        -- Validate that the camera has time samples (not splines) even though AnimationType is curves
        -- A camera with a target should always export as time samples because its rotation depends on the target
        local xformable = pyUsdGeom.Xformable(cameraPrim)
        local xformOps = xformable.GetOrderedXformOps()

        for i = 1 to xformOps.Count do (
            local attr = (xformOps[i]).GetAttr()
            local timeSamples = attr.GetTimeSamples()
            assert_true (timeSamples.Count > 0) message:("Camera with target should have time samples when AnimationType is curves, found " + (timeSamples.Count as string) + " time samples on " + attr.GetName())
            assert_false (attr.HasSpline()) message:("Camera with target should not have splines when AnimationType is curves, found spline on " + attr.GetName())
        )
    ),

    function test_biped_curves_export_test = (
        usdVersion = pyUsd.GetVersion()
        local supportCurves = (usdVersion[2] > 24)
        if not supportCurves then (
            format "Skipping test_biped_curves_export_test: USD version % does not support curves\n" usdVersion
            return undefined
        )

        biped.createNew 100 0 [0,0,0]
        
        local curveExportOptions = USDExporter.CreateOptions()
        curveExportOptions.FileFormat = #ascii
        curveExportOptions.RootPrimPath = "/"
        curveExportOptions.TimeMode = #frameRange
        curveExportOptions.StartFrame = 0
        curveExportOptions.EndFrame = 10
        curveExportOptions.AnimationType = #curves
        curveExportOptions.TransformFormat = #splitComponents
        
        local exportPath = output_prefix + "test_biped_curves.usda"
        
        assert_true (USDExporter.ExportFile exportPath exportOptions:curveExportOptions) message:"Biped export with curves failed"
        
        local stage = pyUsd.Stage.Open(exportPath)
        assert_true (stage != undefined) message:"Failed to open exported USD stage"
    ),

    function teardown = (
    ),

    Tests = #(test_object_offset_transforms,
        test_object_without_prim_creator,
        export_object_with_wsm_test,
        test_worldspaceroot_on,
        test_worldspaceroot_off,
        test_export_transform_with_spline_animation,
        test_target_camera_exports_time_samples,
        test_biped_curves_export_test
    )
)

runUsdTestFixture export_transforms_test (getThisScriptFilename())