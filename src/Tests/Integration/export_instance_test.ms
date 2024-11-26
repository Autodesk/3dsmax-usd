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
/* ********************************************************************************************************
    @param Desc             : Set offline due to Python issue in 2022 - MAXX-62109, avoid pipeline noise
    @param Status           : Offline
******************************************************************************************************** */

FileIn "helpers/usd_test_utils.mxs"

-- Make sure the USD plugin is properly loaded.
setupUsdPlugins()

-- Some useful python modules for USD validation.
pyUsd = python.import("pxr.Usd")
pyUsdGeom = python.import("pxr.UsdGeom")
pyGf = python.import("pxr.Gf")
pySdf = python.import("pxr.Sdf")

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_INSTANCE_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_instance_test
(    
    public 
    function setup =
    (
        resetmaxfile #noprompt
    ),
    
    function test_copy_types = (
        originalBox = Box name:"originalBox" pos:[0,0,0] width:10 length:10 height:10
        instanceBox = create_clone originalBox #instance "instanceBox"
        referenceBox = create_clone originalBox #reference "referenceBox"
        create_clone originalBox #copy "copyBox"
        
        exportPath = output_prefix + "test_copy_types.usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        originalBoxPrim = stage.GetPrimAtPath("/originalBox")
        referenceBoxPrim = stage.GetPrimAtPath("/referenceBox")
        copyBoxPrim = stage.GetPrimAtPath("/copyBox")
        instanceBoxPrim = stage.GetPrimAtPath("/instanceBox")
        
        assert_true (originalBoxPrim.IsInstance()) message:"originalBox prim should be instanceable"
        assert_true (referenceBoxPrim.IsInstance()) message:"referenceBoxPrim prim should be instanceable"
        assert_false (copyBoxPrim.IsInstance()) message:"copyBoxPrim prim should be instanceable"
        assert_true (instanceBoxPrim.IsInstance())message:"instanceBoxPrim prim should be instanceable"
    ),

    function test_reference_with_modifier = (
        originalBox = Box name:"originalBox" pos:[0,0,0] width:10 length:10 height:10
        instanceBox = create_clone originalBox #instance "instanceBox"
        referenceBox = create_clone originalBox #reference "referenceBox"
        referenceBoxWithModifier = create_clone originalBox #reference "referenceBoxWithModifier"
        
        bendModifier = Bend()
        twistModifier = Twist()
        addModifier referenceBoxWithModifier bendModifier
        addModifier instanceBox twistModifier
        
        exportPath = output_prefix + "test_reference_with_modifier.usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        originalBoxPrim = stage.GetPrimAtPath("/originalBox")
        instanceBoxPrim = stage.GetPrimAtPath("/instanceBox")
        referenceBoxPrim = stage.GetPrimAtPath("/referenceBox")
        referenceWithModifierPrim = stage.GetPrimAtPath("/referenceBoxWithModifier")
        
        assert_true (originalBoxPrim.IsInstance()) message:"originalBoxPrim prim should be instanceable"
        assert_true (instanceBoxPrim.IsInstance()) message:"instanceBoxPrim prim should be instanceable"
        assert_true (referenceBoxPrim.IsInstance()) message:"referenceBoxPrim prim should be instanceable"
        assert_false (referenceWithModifierPrim.IsInstance()) message:"referenceWithModifierPrim prim should not be instanceable"
    ),

    function test_space_warp = (
        originalBox1 = Box name:"originalBox1" pos:[0,0,0] width:10 length:10 height:10
        instanceBox1 = create_clone originalBox1 #instance "instanceBox1"
        
        originalBox2 = Box name:"originalBox2" pos:[0,0,0] width:10 length:10 height:10
        instanceBox2 = create_clone originalBox2 #instance "instanceBox2"
        instanceBox3 = create_clone originalBox2 #instance "instanceBox3"
        
        bombSpaceWarp = Bomb name:"bombSpaceWarp"
        bindSpaceWarp originalBox1 bombSpaceWarp
        bindSpaceWarp originalBox2 bombSpaceWarp
        
        exportPath = output_prefix + "test_space_warp.usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        originalBox1Prim = stage.GetPrimAtPath("/originalBox1")
        originalBox2Prim = stage.GetPrimAtPath("/originalBox2")
        instanceBox1Prim = stage.GetPrimAtPath("/instanceBox1")
        instanceBox2Prim = stage.GetPrimAtPath("/instanceBox2")
        instanceBox3Prim = stage.GetPrimAtPath("/instanceBox3")
        
        assert_false (originalBox1Prim.IsInstance()) message:"originalBox1Prim prim should not be instanceable"
        assert_false (instanceBox1Prim.IsInstance()) message:"instanceBox1Prim prim should not be instanceable"
        assert_false (originalBox2Prim.IsInstance()) message:"originalBox2Prim prim should not be instanceable"
        assert_true (instanceBox2Prim.IsInstance()) message:"instanceBox2Prim prim should be instanceable"
        assert_true (instanceBox3Prim.IsInstance()) message:"instanceBox2Prim prim should be instanceable"
    ),
    
    function test_helper_instancing testGeometryHelpers = (
        allHelpers = for o in Helper.classes where o.creatable collect o
        for i = 1 to allHelpers.count do
        (
            -- Instanciate the helper type, and clone it to potentially trigger instancing on export.
            local obj = allHelpers[i]()
            local hasGeometry = canConvertTo obj Editable_mesh
            -- Filter out helpers with / without geometry, depending what we are testing.
            if (testGeometryHelpers and not hasGeometry) or (not testGeometryHelpers and hasGeometry) then (
                delete obj                
                continue
            )    
            -- Some helpers cannot be instanciated... skip those.
            clone =  create_clone obj #instance ("Instance of " + obj.Name)        
            if (clone == undefined) then (
                delete obj
            )
        )
        
        exportPath = output_prefix + "helpers_not_instanced_with_geom_" + (testGeometryHelpers as string) + ".usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local rootPrim = stage.GetPseudoRoot()
        local helperPrims = rootPrim.GetAllChildren()
        
        -- Make sure that helpers are instances, or not, depending on what we are testing.
        for i = 1 to helperPrims.count do (
            local prim = helperPrims[i]
            -- Skip class prims.
            if (prim.GetSpecifier() == pySdf.SpecifierClass) then (
                continue
            )
            
            local isInstance = (prim.IsInstance())
            if (testGeometryHelpers) then (
                assert_true isInstance message:(prim.GetName() + " was exported from a helper node with geometry, it should be instanced.")   
            )
            else (
                assert_false isInstance message:(prim.GetName() + " was exported from a helper node without geometry, it should not be instanced.")   
            )
        )
    ),
            
    function test_helpers_without_geometry_are_not_instanced = (
        -- Test that helpers with geometry can be instanced.        
        test_helper_instancing true
        resetMaxFile #noprompt
        -- Test that helpers without geometry (they are basically exported to Xform prims) are NOT instanced.
        test_helper_instancing false 
    ),

    function perform_transform_test file_postfix = (
        box1 = Box name:"box1" pos:[10,0,0] width:10 length:10 height:10
        box2 = create_clone box1 #instance "box2"
        box2.pos = [-10,0,0]
        
        -- add offset transform on box2
        box2.objectOffsetPos = [0, 0, 10]
        
        exportPath = output_prefix + file_postfix
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        
        box1Prim = stage.GetPrimAtPath("/box1")
        box2Prim = stage.GetPrimAtPath("/box2")
        box1ShapePrim = stage.GetPrimAtPath("/box1/box1_Shape")
        box2ShapePrim = stage.GetPrimAtPath("/box2/box2_Shape")
        box1MeshPrim = stage.GetPrimAtPath("/box1/box1")
        box2MeshPrim = stage.GetPrimAtPath("/box2/box2_Shape/box1")
        
        assert_true (box1Prim.IsInstance()) message:"box1 prim should be instanceable"
        assert_false (pyUsd.Prim.IsValid(box1ShapePrim)) message:"box1_Shape prim should not have been created."
        assert_false (box2Prim.IsInstance()) message:"instanceBox1Prim prim should not be instanceable"
        assert_true (box2ShapePrim.IsInstance()) message:"instanceBox1Prim prim should not be instanceable"

        box1Mesh = pyUsdGeom.Mesh(box1MeshPrim)
        box2Mesh = pyUsdGeom.Mesh(box2MeshPrim)

        box1Points = ((box1Mesh.GetPointsAttr()).Get())
        box2Points = ((box2Mesh.GetPointsAttr()).Get())

        expectedPoints = #(#(-5,-5,0), #(5,-5,0), #(-5,5,0), #(5,5,0), #(-5,-5,10), #(5,-5,10), #(-5,5,10), #(5,5,10))
        assert_true (compare2dMatrix box1Points expectedPoints 8 3)
        assert_true (compare2dMatrix box2Points expectedPoints 8 3)

        box1Xformable = pyUsdGeom.Xformable(box1Prim)
        box2Xformable = pyUsdGeom.Xformable(box2Prim)
        box2ShapeXformable = pyUsdGeom.Xformable(box2ShapePrim)
        box1MeshXformable = pyUsdGeom.Xformable(box1MeshPrim)
        box2MeshXformable = pyUsdGeom.Xformable(box2MeshPrim)
        
        box1Transform = box1Xformable.GetLocalTransformation()
        box2Transform = box2Xformable.GetLocalTransformation()
        box2ShapeTransform = box2ShapeXformable.GetLocalTransformation()
        box1MeshTransform = box1MeshXformable.GetLocalTransformation()
        box2MeshTransform = box2MeshXformable.GetLocalTransformation()
            
        expectedNoTransform = #( #(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(0, 0, 0, 1))
        expectedBox1Transform = #( #(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #( 10, 0, 0, 1) )
        expectedBox2Transform = #( #(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(-10, 0, 0, 1) )
        expectedBox2ShapeTransform = #( #(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(0, 0, 10, 1) )       
            
        assert_true (compare2dMatrix box1Transform expectedBox1Transform 4 4)
        assert_true (compare2dMatrix box2Transform expectedBox2Transform 4 4)
        assert_true (compare2dMatrix box2ShapeTransform expectedBox2ShapeTransform 4 4)
        assert_true (compare2dMatrix box1MeshTransform expectedNoTransform 4 4)
        assert_true (compare2dMatrix box2MeshTransform expectedNoTransform 4 4)
    ),
    
    function test_baked_transform = (
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.BakeObjectOffsetTransform = true
        perform_transform_test "test_baked_transform.usda"
    ),
    
    function test_transform_not_baked = (
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.BakeObjectOffsetTransform = false
        perform_transform_test "test_transform_not_baked.usda"
    ),
    
    function test_naming_conflicts = (
        box1 = Box name:"box" pos:[10,0,0] width:10 length:10 height:10
        create_clone box1 #instance "box"
        
        box2 = Box name:"box" pos:[20,0,0] width:20 length:20 height:20
        create_clone box2 #instance "box"
        
        exportPath = output_prefix + "test_naming_conflicts.usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        
        classBoxPrim = stage.GetPrimAtPath("/_class_box")
        boxPrim = stage.GetPrimAtPath("/box")
        box1Prim = stage.GetPrimAtPath("/box1")
        classBox2Prim = stage.GetPrimAtPath("/_class_box2")
        box2Prim = stage.GetPrimAtPath("/box2")
        box3Prim = stage.GetPrimAtPath("/box3")
        
        assert_true (pyUsd.Prim.IsValid(classBoxPrim)) message:"A box was not exported to a USD prim as expected."
        assert_true (pyUsd.Prim.IsValid(boxPrim)) message:"A box was not exported to a USD prim as expected."
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"A box was not exported to a USD prim as expected."
        assert_true (pyUsd.Prim.IsValid(classBox2Prim)) message:"A box was not exported to a USD prim as expected."
        assert_true (pyUsd.Prim.IsValid(box2Prim)) message:"A box was not exported to a USD prim as expected."
        assert_true (pyUsd.Prim.IsValid(box3Prim)) message:"A box was not exported to a USD prim as expected."  
    ),
    
    -- Instancing can force us to use a *_Shape prim. Indeed, if a node has children, or if the object 
    -- has an offset, we cannot export the node and its attached object (mesh) to a single USD prim. 
    -- Indeed, instanced prims will ignore their children, and we cannot bake the offset into the geometry
    -- if the geometry is to be reused elsewhere.
    function test_instance_with_childrens_or_offset = (
        box1 = Box name:"box1" pos:[10,0,0] width:10 length:10 height:10
        box2 = create_clone box1 #instance "box2"
        box3 = Box name:"box3" pos:[10,0,0] width:10 length:10 height:10
        
        box4 = create_clone box1 #instance "box4"
        box4.objectOffsetPos = [0, 0, 10]
        
        append box2.children box3
        
        exportPath = output_prefix + "test_instance_with_childrens.usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        
        box1Prim = stage.GetPrimAtPath("/box1")
        box2Prim = stage.GetPrimAtPath("/box2")
        box2ShapePrim = stage.GetPrimAtPath("/box2/box2_Shape")
        box3Prim = stage.GetPrimAtPath("/box2/box3")
        box4Prim = stage.GetPrimAtPath("/box4")
        box4ShapePrim = stage.GetPrimAtPath("/box4/box4_Shape")
        
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"box1 was not exported to a USD prim as expected."
        assert_true (pyUsd.Prim.IsValid(box2Prim)) message:"box2 was not exported to a USD prim as expected."
        assert_true (pyUsd.Prim.IsValid(box2ShapePrim)) message:"box2 should have a Shape prim, as the object is instanced and has children."
        assert_true (pyUsd.Prim.IsValid(box3Prim)) message:"box3 was not exported to a USD prim as expected."
        assert_true (pyUsd.Prim.IsValid(box4Prim)) message:"box4 was not exported to a USD prim as expected."
        assert_true (pyUsd.Prim.IsValid(box4ShapePrim)) message:"box4 should have a Shape prim, as the object has an offset."
        
        assert_true (box1Prim.IsInstance()) message:"/box1 should be an instance."
        assert_false (box2Prim.IsInstance()) message:"/box2 should not be instance."
        assert_true (box2ShapePrim.IsInstance()) message:"/box2/box2_Shape should be an instance."
        assert_false (box3Prim.IsInstance()) message:"/box2/box3 should not be instance."
        assert_false (box4Prim.IsInstance()) message:"/box4 should not be instance."
        assert_true (box4ShapePrim.IsInstance()) message:"/box4/box4_Shape should be an instance."
    ),
    
    function test_export_invisible_instance = (
        box1 = Box name:"box1" pos:[10,0,0] width:10 length:10 height:10
        box2 = create_clone box1 #instance "box2"
        
        box2.isHidden = true
        
        exportPath = output_prefix + "test_export_invisible_instance.usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.HiddenObjects = true
        exportOptions.UseUSDVisibility = true
        exportOptions.RootPrimPath = "/"
                
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)

        box2Prim = stage.GetPrimAtPath("/box2")
        imageablePrim = pyUsdGeom.Imageable(box2Prim)
        visibilityAttr = imageablePrim.GetVisibilityAttr()
        visibility = (visibilityAttr.Get())
        assert_equal "invisible" visibility message:"Unexpected visible box, should be invisible."
    ),

    function test_export_instance_reuse_parent_prim = (
        sphere1 = Sphere name:"sphere1" pos:[10,0,0] radius:10 width:10 length:10 height:10
        sphere2 = create_clone sphere1 #instance "sphere2"
        point1 = Point pos:[10,0,0] name:"point1"
        sphere2.parent = point1
        
        exportPath = output_prefix + "test_export_instance_reuse_parent_prim.usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)

        point1Prim = stage.GetPrimAtPath("/point1")
        assert_true (point1Prim.IsInstance()) message:"point1Prim prim should be an instance"
            
        -- Make sure that the exported prims are at the right transform. This was a regression 
        -- following the support for animation.
        local xformable = pyUsdGeom.Xformable(point1Prim)
        assert_true (compareUsdAndMaxTransforms (xformable.ComputeLocalToWorldTransform(0)) sphere2.transform) message:"Incorrect transform when reusing the parent for instancing."
        
        -- Make sure no prim was exported for the instance at "/point1/sphere2"
        -- Disable instancing on point1 just so that any of its children are not pruned when composing.
        point1Prim.SetInstanceable(false)
        local sphere2 = stage.GetPrimAtPath("/point1/sphere2")
        assert_false (sphere2.IsValid()) message:"Parent is reused for instancing, no prim should have been created."
    ),
    
    function test_export_selected_instance_reuse_parent_prim = (
        sphere1 = Sphere name:"sphere1" pos:[10,0,0] radius:10 width:10 length:10 height:10
        sphere2 = create_clone sphere1 #instance "sphere2"
        point1 = Point pos:[10,0,0] name:"point1"
        sphere2.parent = point1
        
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        -- Test when exporting selection and parent is selected. Parent should be reused.
        select sphere1
        selectmore sphere2
        selectmore point1
        exportPath = output_prefix + "test_export_selected_parent_instance_reuse_parent_prim.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected
        stage = pyUsd.Stage.Open(exportPath)
        point1Prim = stage.GetPrimAtPath("/point1")
        assert_true (point1Prim.IsInstance()) message:"point1Prim prim should be an instance"
            
        -- Test when exporting selection and parent is not selected. Parent cant be reused.
        select sphere1
        selectmore sphere2
        exportPath = output_prefix + "test_export_unselected_parent_instance_reuse_parent_prim.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected
        stage = pyUsd.Stage.Open(exportPath)
        shpere2Prim = stage.GetPrimAtPath("/sphere2")
        assert_true (shpere2Prim.IsInstance()) message:"shpere2Prim prim should be an instance"
    ),
    
    function test_export_node_list_instance_reuse_parent_prim = (
        sphere1 = Sphere name:"sphere1" pos:[10,0,0] radius:10 width:10 length:10 height:10
        sphere2 = create_clone sphere1 #instance "sphere2"
        point1 = Point pos:[10,0,0] name:"point1"
        sphere2.parent = point1
        
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        -- Test when a node and it's parent are exported. Parent should be reused.
        exportPath = output_prefix + "test_export_selected_parent_instance_reuse_parent_prim.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:#(sphere1, point1, sphere2)
        stage = pyUsd.Stage.Open(exportPath)
        point1Prim = stage.GetPrimAtPath("/point1")
        assert_true (point1Prim.IsInstance()) message:"point1Prim prim should be an instance"
            
        -- Test when a node's parent is not exported. Parent cant be reused.
        exportPath = output_prefix + "test_export_unselected_parent_instance_reuse_parent_prim.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:#(sphere1, sphere2)
        stage = pyUsd.Stage.Open(exportPath)
        shpere2Prim = stage.GetPrimAtPath("/sphere2")
        assert_true (shpere2Prim.IsInstance()) message:"shpere2Prim prim should be an instance"
    ),
    
    function test_export_instance_not_reuse_parent_prim_when_more_than_one_child = (
        sphere1 = Sphere name:"sphere1" pos:[10,0,0] radius:10 width:10 length:10 height:10
        sphere2 = create_clone sphere1 #instance "sphere2"
        point1 = Point pos:[10,0,0] name:"point1"
        sphere2.parent = point1
        sphere1.parent = point1
        
        exportPath = output_prefix + "test_export_instance_not_reuse_parent_prim_when_more_than_one_child.usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions        
        stage = pyUsd.Stage.Open(exportPath)

        point1Prim = stage.GetPrimAtPath("/point1")
        sphere1Prim = stage.GetPrimAtPath("/point1/sphere1")
        sphere2Prim = stage.GetPrimAtPath("/point1/sphere2")
        assert_false (point1Prim.IsInstance()) message:"point1Prim prim should not be an instance."
        assert_true (sphere1Prim.IsInstance()) message:"sphere1Prim prim should be an instance."
        assert_true (sphere2Prim.IsInstance()) message:"sphere2Prim prim should be an instance."
    ),
    
    function test_export_instance_not_reuse_parent_prim_when_parent_is_not_a_point = (
        sphere1 = Sphere name:"sphere1" pos:[10,0,0] radius:10 width:10 length:10 height:10
        sphere2 = create_clone sphere1 #instance "sphere2"
        dummy1 = Dummy pos:[10,0,0] name:"dummy1"
        sphere2.parent = dummy1
        
        exportPath = output_prefix + "test_export_instance_not_reuse_parent_prim_when_parent_is_not_a_point.usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)

        point1Prim = stage.GetPrimAtPath("/dummy1")
        sphere1Prim = stage.GetPrimAtPath("/sphere1")
        sphere2Prim = stage.GetPrimAtPath("/dummy1/sphere2")
        assert_false (point1Prim.IsInstance()) message:"dummy1 prim should not be an instance."
        assert_true (sphere1Prim.IsInstance()) message:"sphere1Prim prim should be an instance."
        assert_true (sphere2Prim.IsInstance()) message:"sphere2Prim prim should be an instance."
    ),

    function test_export_instance_reuse_parent_prim_should_have_correct_transforms = (
        teapot1 = Teapot name:"teapot1" pos:[1000009,10,0] width:10 length:10 height:10
        teapot2 = create_clone teapot1 #instance "teapot2"
        point1 = Point pos:[9,5,0] name:"point1"
        teapot1.parent = point1
        
        exportPath = output_prefix + "test_export_instance_reuse_parent_prim_should_have_correct_transforms.usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        point1Prim = stage.GetPrimAtPath("/point1")

        assert_true (point1Prim.IsInstance()) message:"point1 prim should be an instance."
        point1XFormPrim = pyUsdGeom.Xformable(point1Prim)
        xformOps = point1XFormPrim.GetOrderedXformOps()
        transform1 = xformOps[1].GetOpTransform(pyUsd.TimeCode.Default())
        transform2 = xformOps[2].GetOpTransform(pyUsd.TimeCode.Default())
        assert_undefined xformOps[3]
        
        expectingOps1 = #(#(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(9, 5, 0, 1))
        expectingOps2 = #(#(1, 0, 0, 0), #(0, 1, 0, 0), #(0, 0, 1, 0), #(1000000, 5, 0, 1))
        assert_true (compare2dMatrix expectingOps1 expectingOps1 4 4) message:"Unexpected transform"
        assert_true (compare2dMatrix expectingOps2 expectingOps2 4 4) message:"Unexpected transform"    
    ),
    
    function test_export_instance_not_reuse_parent_on_metadata_conflict = (
        usdAllCA = attributes USDData
        (
            parameters USD
            (
                usd_purpose type:#string
                usd_hidden type:#boolean
                usd_kind type:#string
            )
        )
        
        teapot1 = Teapot name:"teapot1"
        teapot2 = create_clone teapot1 #instance "teapot2"
        point1 = Point name:"point1"
        teapot1.parent = point1
        
        -- Test different usd_hidden
        custAttributes.add teapot1.baseObject usdAllCA #Unique BaseObject:true
        teapot1.usd_hidden = false
        teapot1.usd_kind = "component"
        teapot1.usd_purpose = "render"

        custAttributes.add point1.baseObject usdAllCA #Unique BaseObject:true
        point1.usd_hidden = true
        point1.usd_kind = "component"
        point1.usd_purpose = "render"
        
        exportPath = output_prefix + "test_export_instance_not_reuse_parent_on_metadata_conflict_hidden.usda"
        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        point1Prim = stage.GetPrimAtPath("/point1")
        teapot1Prim = stage.GetPrimAtPath("/point1/teapot1")
        
        local imageablePoint1 = pyUsdGeom.Imageable(point1Prim)
        local imageableTeapot1 = pyUsdGeom.Imageable(teapot1Prim)       
        local purposePoint1 = imageablePoint1.GetPurposeAttr()
        local purposeTeapot1 = imageableTeapot1.GetPurposeAttr()
        local point1Model = pyUsd.ModelAPI(point1Prim)
        local teapot1Model = pyUsd.ModelAPI(teapot1Prim)
        
        assert_equal point1.usd_purpose (purposeTeapot1.Get())
        assert_equal teapot1.usd_purpose (purposeTeapot1.Get())
        assert_equal point1.usd_kind (point1Model.GetKind())
        assert_equal teapot1.usd_kind (teapot1Model.GetKind())
        assert_equal point1.usd_hidden (point1Prim.IsHidden())
        assert_equal teapot1.usd_hidden (teapot1Prim.IsHidden())
            
        assert_false (point1Prim.IsInstance()) message:"point1 prim should not be an instance."
        assert_true (teapot1Prim.IsInstance()) message:"teapot1 prim should be an instance."
        
        -- Test different usd_kind
        point1.usd_hidden = false
        point1.usd_kind = "model"
            
        exportPath = output_prefix + "test_export_instance_not_reuse_parent_on_metadata_conflict_kind.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        point1Prim = stage.GetPrimAtPath("/point1")
        teapot1Prim = stage.GetPrimAtPath("/point1/teapot1")
        
        local imageablePoint1 = pyUsdGeom.Imageable(point1Prim)
        local imageableTeapot1 = pyUsdGeom.Imageable(teapot1Prim)       
        local purposePoint1 = imageablePoint1.GetPurposeAttr()
        local purposeTeapot1 = imageableTeapot1.GetPurposeAttr()
        local point1Model = pyUsd.ModelAPI(point1Prim)
        local teapot1Model = pyUsd.ModelAPI(teapot1Prim)
        
        assert_equal point1.usd_purpose (purposeTeapot1.Get())
        assert_equal teapot1.usd_purpose (purposeTeapot1.Get())
        assert_equal point1.usd_kind (point1Model.GetKind())
        assert_equal teapot1.usd_kind (teapot1Model.GetKind())
        assert_equal point1.usd_hidden (point1Prim.IsHidden())
        assert_equal teapot1.usd_hidden (teapot1Prim.IsHidden())
            
        assert_false (point1Prim.IsInstance()) message:"point1 prim should not be an instance."
        assert_true (teapot1Prim.IsInstance()) message:"teapot1 prim should be an instance."
            
        -- Test different usd_purpose
        point1.usd_kind = "component"
        point1.usd_purpose = "proxy"
        exportPath = output_prefix + "test_export_instance_not_reuse_parent_on_metadata_conflict_purpose.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        point1Prim = stage.GetPrimAtPath("/point1")
        teapot1Prim = stage.GetPrimAtPath("/point1/teapot1")
        
        local imageablePoint1 = pyUsdGeom.Imageable(point1Prim)
        local imageableTeapot1 = pyUsdGeom.Imageable(teapot1Prim)       
        local purposePoint1 = imageablePoint1.GetPurposeAttr()
        local purposeTeapot1 = imageableTeapot1.GetPurposeAttr()
        local point1Model = pyUsd.ModelAPI(point1Prim)
        local teapot1Model = pyUsd.ModelAPI(teapot1Prim)
        
        assert_equal point1.usd_purpose (purposePoint1.Get())
        assert_equal teapot1.usd_purpose (purposeTeapot1.Get())
        assert_equal point1.usd_kind (point1Model.GetKind())
        assert_equal teapot1.usd_kind (teapot1Model.GetKind())
        assert_equal point1.usd_hidden (point1Prim.IsHidden())
        assert_equal teapot1.usd_hidden (teapot1Prim.IsHidden())
            
        assert_false (point1Prim.IsInstance()) message:"point1 prim should not be an instance."
        assert_true (teapot1Prim.IsInstance()) message:"teapot1 prim should be an instance."
            
        -- Validate if the metadata is the same, we should reuse parent prim
        point1.usd_purpose = "render"
            
        exportPath = output_prefix + "test_export_instance_not_reuse_parent_on_metadata_conflict.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        point1Prim = stage.GetPrimAtPath("/point1")
        
        local imageablePoint1 = pyUsdGeom.Imageable(point1Prim) 
        local purposePoint1 = imageablePoint1.GetPurposeAttr()
        local point1Model = pyUsd.ModelAPI(point1Prim)
        
        assert_equal point1.usd_purpose (purposePoint1.Get())
        assert_equal point1.usd_kind (point1Model.GetKind())
        assert_equal point1.usd_hidden (point1Prim.IsHidden())
            
        assert_true (point1Prim.IsInstance()) message:"point1 prim should be an instance."
            
        -- Should reuse parent prim even if an object doesn't have some or all the metadata.
        usdPurposeOnlyCA = attributes USDData
        (
            parameters USD
            (
                usd_purpose type:#string
            )
        )
        teapot3 = create_clone teapot2 #instance "teapot3"
        point2 = Point name:"point2"
        teapot3.parent = point2
        
        custAttributes.add teapot3.baseObject usdAllCA #Unique BaseObject:true
        teapot3.usd_hidden = false
        teapot3.usd_kind = "component"
        teapot3.usd_purpose = "render"

        custAttributes.add point2.baseObject usdPurposeOnlyCA #Unique BaseObject:true
        point2.usd_purpose = "render"
        
        exportPath = output_prefix + "test_export_instance_not_reuse_parent_on_metadata_conflict_unused_data.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        stage = pyUsd.Stage.Open(exportPath)
        point2Prim = stage.GetPrimAtPath("/point2")
        
        local imageablePoint2 = pyUsdGeom.Imageable(point2Prim) 
        local purposePoint2 = imageablePoint2.GetPurposeAttr()
        local point2Model = pyUsd.ModelAPI(point2Prim)
        
        assert_equal teapot3.usd_kind (point2Model.GetKind())
        assert_equal teapot3.usd_hidden (point2Prim.IsHidden())
        assert_equal teapot3.usd_purpose (purposePoint2.Get())
            
        assert_true (point2Prim.IsInstance()) message:"point2 prim should be an instance."
    ),
    
    function test_export_single_max_instance_does_not_create_usd_instance  = (
        originalBox = Box name:"originalBox" pos:[0,0,0] width:10 length:10 height:10
        instanceBox = create_clone originalBox #instance "instanceBox"

        exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        
        -- 1) Via selection.
        
        -- Make sure that we dont generate instances on the USD side if a single instance is exported.
        select instanceBox
        exportPath = output_prefix + "test_export_single_instance_by_selection.usda"

        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected
        stage = pyUsd.Stage.Open(exportPath)
        originalBoxPrim = stage.GetPrimAtPath("/originalBox")
        instanceBoxPrim = stage.GetPrimAtPath("/instanceBox")
        assert_false (originalBoxPrim.IsValid()) message:"originalBox prim should not have been exported."
        assert_false (instanceBoxPrim.IsInstance())message:"instanceBoxPrim prim should not be instanceable, as it is exported alone."
        
        -- If multiple instances are selected, we should now get instances.
        selectmore originalBox
        
        exportPath = output_prefix + "test_export_two_instances_by_selection.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected
        stage = pyUsd.Stage.Open(exportPath)
        originalBoxPrim = stage.GetPrimAtPath("/originalBox")
        instanceBoxPrim = stage.GetPrimAtPath("/instanceBox")
        assert_true (originalBoxPrim.IsInstance()) message:"originalBox should be an instance."
        assert_true (instanceBoxPrim.IsInstance())message:"instanceBoxPrim prim should be an instance."
        
        clearSelection()
        
        -- 2) Via node list 
            
        -- Make sure that we dont generate instances on the USD side if a single instance is exported.
        exportPath = output_prefix + "test_export_single_instance_by_node_list.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:#(instanceBox)
        stage = pyUsd.Stage.Open(exportPath)
        originalBoxPrim = stage.GetPrimAtPath("/originalBox")
        instanceBoxPrim = stage.GetPrimAtPath("/instanceBox")
        assert_false (originalBoxPrim.IsValid()) message:"originalBox prim should not have been exported."
        assert_false (instanceBoxPrim.IsInstance())message:"instanceBoxPrim prim should not be an instance, as it is exported alone."
            
        -- If multiple instances are selected, we should now get instances.
        
        exportPath = output_prefix + "test_export_two_instances_by_node_list.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:#(instanceBox, originalBox)
        stage = pyUsd.Stage.Open(exportPath)
        originalBoxPrim = stage.GetPrimAtPath("/originalBox")
        instanceBoxPrim = stage.GetPrimAtPath("/instanceBox")
        assert_true (originalBoxPrim.IsInstance()) message:"originalBox prim should be an instance."
        assert_true (instanceBoxPrim.IsInstance())message:"instanceBoxPrim prim should be an instance."
    ),
    
    function teardown = (
    ),

    Tests = #(test_copy_types,
        test_reference_with_modifier,
        test_space_warp, test_baked_transform,
        test_helpers_without_geometry_are_not_instanced,
        test_transform_not_baked,
        test_naming_conflicts,
        test_instance_with_childrens_or_offset,
        test_export_invisible_instance,
        test_export_instance_reuse_parent_prim,
        test_export_selected_instance_reuse_parent_prim,
        test_export_node_list_instance_reuse_parent_prim,
        test_export_instance_not_reuse_parent_prim_when_more_than_one_child,
        test_export_instance_not_reuse_parent_prim_when_parent_is_not_a_point,
        test_export_instance_reuse_parent_prim_should_have_correct_transforms,
        test_export_instance_not_reuse_parent_on_metadata_conflict,
        test_export_single_max_instance_does_not_create_usd_instance
        )
)

runUsdTestFixture export_instance_test (getThisScriptFilename())