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
        resetmaxfile #noprompt
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
    ),

    function test_object_offset_transforms = (
        -- Each pair : (v1: expected usd suffix for the object on the USD side, v2: Max node).
        local light = DataPair "Light" (free_light());
        local camera = DataPair "Camera" (Freecamera())
        local cube = DataPair "Shape" (box())
        local helper = DataPair "Dummy" (dummy())
        local usdStage = DataPair "Layer" (UsdStageObject())
        sphere()    
        sphere()
        local sphereGroup = DataPair "Dummy" (group $sphere*) -- Groups are dummy nodes also.
        
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
        
        local exportPath = output_prefix + "test_object_offset_transforms_identity.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
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
        exportPath = output_prefix + "test_object_offset_transforms.usd"
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
    ),
    function test_object_without_prim_creator = (
        -- The tape object should not have any prim creator
        local tapeNode = Tape Name:"tape" pos:[100,100,100]
        local boxNode = Box Name:"box" pos:[10,10,10]
        append tapeNode.children boxNode
        
        local exportPath = output_prefix + "test_object_offset_transforms_identity.usd"
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
        
    function teardown = (
    ),

    Tests = #(test_object_offset_transforms,
        test_object_without_prim_creator,
        export_object_with_wsm_test)
)

runUsdTestFixture export_transforms_test (getThisScriptFilename())