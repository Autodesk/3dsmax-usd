--
-- Copyright 2025 Autodesk
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
pyUsdGeom = python.import("pxr.UsdGeom")

output_prefix = pathConfig.appendPath (getDir #temp) "/USD_GEOM_OBJECT_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct usd_geom_object_test
(  
    private 
    initialUnits,
    initialScale,
    
    public 
    function setup =
    (
        quickReset()
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        
        units.SystemType = #centimeters
        units.SystemScale = 1.0
    ),
    
    -- Test promoting prim hierarchies to usd geom objects
    function usdgeom_object_from_hierarchy = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_hierarchy_test.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        
        -- Invalid path
        local result = stageObject.PromoteTo3dsMaxObject "foo/bar"
        assert_undefined result
        
        local rootObject = stageObject.PromoteTo3dsMaxObject "/root"
        assert_defined rootObject
        assert_equal "root" rootObject.name        
        
        assert_equal (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0]) rootObject.transform
        assert_box3_equal (box3 [-5,-5,0] [45,45,0.0001]) (nodeLocalBoundingBox  rootObject asBox3:true)    
        
        local aShapeObject = stageObject.PromoteTo3dsMaxObject "/root/A/A_Shape"
        assert_defined aShapeObject
        assert_equal "A_Shape" aShapeObject.name            
        assert_equal (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,0,0]) aShapeObject.transform
        assert_box3_equal (Box3 [-5,-5,0] [5,5,0.0001]) (nodeLocalBoundingBox  aShapeObject asBox3:true)    
        
        local bObject = stageObject.PromoteTo3dsMaxObject "/root/A/B"
        assert_defined bObject
        assert_equal "B" bObject.name
        assert_equal (matrix3 [1,0,0] [0,1,0] [0,0,1] [0,20,0])  bObject.transform
        assert_box3_equal (Box3 [-5,15,0] [45,45,0.0001]) (nodeLocalBoundingBox  bObject asBox3:true)    
        
        local eObject = stageObject.PromoteTo3dsMaxObject "/root/A/B/E"
        assert_defined eObject
        assert_equal "E" eObject.name
        assert_equal (matrix3 [1,0,0] [0,1,0] [0,0,1] [40,40,0]) eObject.transform
        assert_box3_equal (Box3 [35,35,0] [45,45,0.0001]) (nodeLocalBoundingBox  eObject asBox3:true)
    ),    
    
    -- Test transform controller on usd geom objects
    function usdgeom_transform_animation = (
        
        -- Test that the transform anim is correctly handled by the controller
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_transform_animation.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        
        local box1 = stageObject.PromoteTo3dsMaxObject "/root/Box001"
        
        maxver = maxVersion()
        -- 3dsMax 2024 and above have matrix3 list controllers. 
        if maxver[1] >= 25900 then (
            
            assert_equal transform_list (classOf box1.transform.controller)
            -- Subanims are :
            --   #USD_Xformable
            --   #Position_Rotation_Scale
            --   #available
            --   #Weights
            assert_equal 4 box1.transform.controller.numSubs
                
            assert_equal USDXformableController (classOf ((getSubAnim box1.transform.controller 1).object))
            assert_equal prs (classOf ((getSubAnim box1.transform.controller 2).object))
            
            -- Make sure the PRS controller is set active, i.e. that users can move the object by default.
            assert_equal 2 box1.transform.controller.active
        ) else (
            -- In 3dsmax 2023 - we need to use individual PRS controllers to use list controllers.
            -- Overall logic is the same.
            
            assert_equal position_list (classOf box1.position.controller)
            assert_equal scale_list (classOf box1.scale.controller)
            assert_equal rotation_list (classOf box1.rotation.controller)
            
            assert_equal 4 box1.position.controller.numSubs
            assert_equal 4 box1.scale.controller.numSubs
            assert_equal 4 box1.rotation.controller.numSubs
            
            assert_equal 2 box1.position.controller.active
            assert_equal 2 box1.scale.controller.active
            assert_equal 2 box1.rotation.controller.active
        )
        
        at time 0 assert_equal (matrix3 [0,0,-2] [0,2,0] [2,0,0] [0,0,0]) box1.transform
        at time 1 assert_equal (matrix3 [0,0,-3] [0,3,0] [3,0,0] [0,0,100]) box1.transform
        
        -- Test moving PRS, and combined transform.
        move box1 [0,0,5]
        
        at time 0 assert_equal (matrix3 [0,0,-2] [0,2,0] [2,0,0] [0,0,5]) box1.transform
        at time 1 assert_equal (matrix3 [0,0,-3] [0,3,0] [3,0,0] [0,0,105]) box1.transform
    ),
    
    -- Test vertex animation sync.
    function usdgeom_vertex_animation = (
    
        -- Test that the transform anim is correctly handled by the controller
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_vertex_animation.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        
        -- Mesh conversion is tested in greated detail in usd_to_tri_mesh_test
        -- Here, we only test that it is wired up correctly for use when build the 3dsmax geom object,
        -- using the bounding box.
        local planes = stageObject.PromoteTo3dsMaxObject "/root"
        planes.LiveUpdates = true
        at time 0 assert_box3_equal (Box3 [-5,-5,0] [15,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
        at time 1 assert_box3_equal (Box3 [-10,-5,0] [20,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
    ),
    
    -- Test "select" parameter
    function usdgeom_auto_selection = (
    
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_vertex_animation.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        
        local planes = stageObject.PromoteTo3dsMaxObject "/root"
        assert_false planes.isSelected
        local planesSel = stageObject.PromoteTo3dsMaxObject "/root" select:true
        assert_true planesSel.isSelected
    ),
    
    -- Test sync on add/remove prims
    function usdgeom_add_remove_prim = (
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_vertex_animation.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
            
        local planes = stageObject.PromoteTo3dsMaxObject "/root"
        planes.LiveUpdates = true
        planes.ShowSource = false
        assert_box3_equal (Box3 [-5,-5,0] [15,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
            
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
            
        spherePrim = pyUsdGeom.Sphere.Define stage "/root/sphere"
        
        -- Radius 1 sphere extents the box in Z and -Z by one.
        assert_box3_equal (Box3 [-5,-5,-1] [15,5,1]) (nodeLocalBoundingBox  planes asBox3:true)
        spherePrim.CreateRadiusAttr(3)
        -- Increase radius, see effect.
        assert_box3_equal (Box3 [-5,-5,-3] [15,5,3]) (nodeLocalBoundingBox  planes asBox3:true)
        -- Deactive the prim, should no longer be in the usd geom.
        (spherePrim.GetPrim()).SetActive false
        assert_box3_equal (Box3 [-5,-5,0] [15,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
    ),
    
    -- Test live updates option toggle
    function usdgeom_live_updates = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_vertex_animation.usda")
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        
        local planes = stageObject.PromoteTo3dsMaxObject "/root"
        -- Frame 0 values
        assert_box3_equal (Box3 [-5,-5,0] [15,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
        
        planes.LiveUpdates = false
        sliderTime = 1
        -- Still frame 0 values..
        assert_box3_equal (Box3 [-5,-5,0] [15,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
        -- Updated now.
        planes.LiveUpdates = true
        assert_box3_equal (Box3 [-10,-5,0] [20,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
        sliderTime = 0
        -- Back to frame 0 values.
        assert_box3_equal (Box3 [-5,-5,0] [15,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
    ),
    
    -- Test show source option.
    function usdgeom_show_source = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_hidesource_test.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        stageObject.Reload quiet:true
        stageObject.DisplayProxy = true
        stageObject.DisplayRender = false
        stageObject.DisplayGuide = false
            
        local geom = stageObject.PromoteTo3dsMaxObject "/root"
        geom.ViewportDisplayPurposes = true
        geom.LiveUpdates = true
        geom.ShowSource = false
        
        -- Subtree starting at B is not visible because a guide purpose is applied on B.
        -- Only 2 quads visible.        
        assert_box3_equal (Box3 [-5,-5,0] [25,25,0.0001]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 8 (getNumVerts (snapshotAsMesh geom))
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        local root = stage.GetPrimAtPath("/root")
        local rootImg = pyUsdGeom.Imageable(root)
        local rootPurpose = rootImg.GetPurposeAttr()
        assert_equal "geomObjectSource" (rootPurpose.Get())

        local A = stage.GetPrimAtPath("/root/A")
        local Aimg = pyUsdGeom.Imageable(A)
        local APurpose = Aimg.GetPurposeAttr()
        assert_equal "default" (APurpose.Get())
            
        local B = stage.GetPrimAtPath("/root/A/B")
        local Bimg = pyUsdGeom.Imageable(B)
        local Bpurpose = Bimg.GetPurposeAttr()
        assert_equal "geomObjectSkip" (Bpurpose.Get())
                    
        local AShape = stage.GetPrimAtPath("/root/A/A_Shape")
        local AShapeimg = pyUsdGeom.Imageable(AShape)
        local AShapePurpose = AShapeimg.GetPurposeAttr()
        assert_equal "default" (AShapePurpose.Get())

        local C = stage.GetPrimAtPath("/root/A/C")
        local Cimg = pyUsdGeom.Imageable(C)
        local CPurpose = Cimg.GetPurposeAttr()
        assert_equal "default" (CPurpose.Get())
        
        local E = stage.GetPrimAtPath("/root/A/B/E")
        local Eimg = pyUsdGeom.Imageable(E)
        local EPurpose = Eimg.GetPurposeAttr()
        assert_equal "geomObjectSkip" (EPurpose.Get())

        -- If we start display guides, it will be picked up.
        stageObject.DisplayGuide = true
        
        assert_equal "geomObjectSource" (Bpurpose.Get())
        assert_box3_equal (Box3 [-5,-5,0] [45,45,0.0001]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 24 (getNumVerts (snapshotAsMesh geom))
        
        -- Hide guides again...
        stageObject.DisplayGuide = false
        assert_box3_equal (Box3 [-5,-5,0] [25,25,0.0001]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 8 (getNumVerts (snapshotAsMesh geom))
            
        -- Make sure that changes to the stage are considered.
        Bpurpose.Set("default")
            
        assert_box3_equal (Box3 [-5,-5,0] [25,45,0.0001]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 20 (getNumVerts (snapshotAsMesh geom))
        
        -- Test adding a new prim under a guide, should not change output
            
        Bpurpose.Set("guide")
        local beforeBb = (nodeLocalBoundingBox  geom asBox3:true)
        local beforeVcount = (getNumVerts (snapshotAsMesh geom))
        spherePrim = pyUsdGeom.Sphere.Define stage "/root/A/B/Sphere"        
        assert_box3_equal beforeBb (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal beforeVcount (getNumVerts (snapshotAsMesh geom))
                    
        (Bimg.GetPurposeAttr()).Set("proxy")
        sphereImg = pyUsdGeom.Imageable(spherePrim)
        
        (sphereImg.GetPurposeAttr()).Set("proxy")
        
        -- Now the sphere will be converted.
        assert_box3_equal (Box3 [-5,-5,-1] [25,45,1]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 112 (getNumVerts (snapshotAsMesh geom))        
        
        assert_equal 1 (((stage.GetSessionLayer()).subLayerPaths).count)
        assert_true (findString (stage.GetSessionLayer()).subLayerPaths[0] "MaxUsd_USDGeomObjects_Reserved" != undefined)
        
        -- Test live updates the purpose metadata.
        Bpurpose.Set("proxy")
        assert_equal "geomObjectSource" (Bpurpose.Get())
        Bpurpose.Set("guide")
        assert_equal "geomObjectSkip" (Bpurpose.Get())
        Bpurpose.Set("proxy")
        assert_equal "geomObjectSource" (Bpurpose.Get())
        
        -- Finally, toggle the option.
        -- Currently, the option is enabled - it is by default.        
        geom.ShowSource = true
        assert_equal 0 (((stage.GetSessionLayer()).subLayerPaths).count)
        assert_equal "proxy" (Bpurpose.Get())
        
        geom.ShowSource = false
        assert_equal 1 (((stage.GetSessionLayer()).subLayerPaths).count)
        assert_equal "geomObjectSource" (Bpurpose.Get())
    ),
    
    -- Test that toggling back on LiveUpdates forces an immediate update.
    function live_updates_refresh = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_vertex_animation.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
            
        local planes = stageObject.PromoteTo3dsMaxObject "/root"
        
        assert_box3_equal (Box3 [-5,-5,0] [15,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
        planes.LiveUpdates = false
            
        sliderTime = 1
        -- no change
        assert_box3_equal (Box3 [-5,-5,0] [15,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
        planes.LiveUpdates = true
        
        -- updated
        assert_box3_equal (Box3 [-10,-5,0] [20,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
    ),
    
    function force_refresh = (
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_vertex_animation.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
            
        local planes = stageObject.PromoteTo3dsMaxObject "/root"
        planes.LiveUpdates = false
            
        assert_box3_equal (Box3 [-5,-5,0] [15,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
        
        sliderTime = 1
        -- no change
        assert_box3_equal (Box3 [-5,-5,0] [15,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
        
        planes.Refresh()
        
        -- updated
        assert_box3_equal (Box3 [-10,-5,0] [20,5,0.0001]) (nodeLocalBoundingBox  planes asBox3:true)
    ),
    
    -- Test promoting to different geom objects within the same hierarchy.
    -- Make sure only the root most prim is used.
    function multi_promote_ancestors = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_hidesource_test.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        stageObject.Reload quiet:true
        stageObject.DisplayGuide = true
        
        -- Promote a few things in the same hierarchy. Purposefully do not order 
        -- the promote ops in any meaningfull way.
        local geomA = stageObject.PromoteTo3dsMaxObject "/root/A"
        geomA.LiveUpdates = true
        geomA.ShowSource = false
        local geomRoot = stageObject.PromoteTo3dsMaxObject "/root"
        geomRoot.LiveUpdates = true
        geomRoot.ShowSource = false
        local geomF = stageObject.PromoteTo3dsMaxObject "/root/A/B/F"
        geomF.LiveUpdates = true
        geomF.ShowSource = false
        local geomB = stageObject.PromoteTo3dsMaxObject "/root/A/B"
        geomB.LiveUpdates = true
        geomB.ShowSource = false
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        -- Only the root-most promoted prim is used when setting up the purposes.
            
        local root = stage.GetPrimAtPath("/root")
        local rootImg = pyUsdGeom.Imageable(root)
        local rootPurpose = rootImg.GetPurposeAttr()
        assert_equal "geomObjectSource" (rootPurpose.Get())
            
        local A = stage.GetPrimAtPath("/root/A")
        local Aimg = pyUsdGeom.Imageable(A)
        local APurpose = Aimg.GetPurposeAttr()
        assert_equal "default" (APurpose.Get())
            
        local B = stage.GetPrimAtPath("/root/A/B")
        local Bimg = pyUsdGeom.Imageable(B)
        local Bpurpose = Bimg.GetPurposeAttr()
        assert_equal "geomObjectSource" (Bpurpose.Get())
            
        local F = stage.GetPrimAtPath("/root/A/B/F")
        local Fimg = pyUsdGeom.Imageable(F)
        local Fpurpose = Fimg.GetPurposeAttr()
        assert_equal "default" (Fpurpose.Get())
            
        assert_box3_equal (Box3 [-5,-5,0] [45,45,0.0001]) (nodeLocalBoundingBox  geomRoot asBox3:true)
        assert_equal 24 (getNumVerts (snapshotAsMesh geomRoot))
        
        assert_box3_equal (Box3 [-5,-5,0] [45,45,0.0001]) (nodeLocalBoundingBox  geomA asBox3:true)
        assert_equal 24 (getNumVerts (snapshotAsMesh geomA))
            
        assert_box3_equal (Box3 [-5,15,0] [45,45,0.0001]) (nodeLocalBoundingBox  geomB asBox3:true)
        assert_equal 16 (getNumVerts (snapshotAsMesh geomB))
            
        assert_box3_equal (Box3 [15,35,0] [25,45,0.0001]) (nodeLocalBoundingBox  geomF asBox3:true)
        assert_equal 4 (getNumVerts (snapshotAsMesh geomF))
            
        assert_equal 1 (((stage.GetSessionLayer()).subLayerPaths).count)
        
        -- Test that deleting object recovers the correct state.
        
        delete geomRoot
        assert_equal 1 (((stage.GetSessionLayer()).subLayerPaths).count)
        assert_equal "default" (rootPurpose.Get()) -- No longer in a geom object
        assert_equal "geomObjectSource" (APurpose.Get())
        assert_equal "geomObjectSource" (Bpurpose.Get())
        assert_equal "default" (Fpurpose.Get())
            
        delete geomA        
        assert_equal 1 (((stage.GetSessionLayer()).subLayerPaths).count)
        assert_equal "default" (rootPurpose.Get())
        assert_equal "default" (APurpose.Get()) -- No longer in a geom object
        assert_equal "geomObjectSource" (Bpurpose.Get())
        assert_equal "default" (Fpurpose.Get())
        
        delete geomB
        assert_equal 1 (((stage.GetSessionLayer()).subLayerPaths).count)
        assert_equal "default" (rootPurpose.Get())
        assert_equal "default" (APurpose.Get())
        assert_equal "guide" (Bpurpose.Get())  -- No longer in a geom object
        assert_equal "geomObjectSource" (Fpurpose.Get())

        delete geomF
        assert_equal 0 (((stage.GetSessionLayer()).subLayerPaths).count) -- No need for the usdGeom sublayer.
        assert_equal "default" (rootPurpose.Get())
        assert_equal "default" (APurpose.Get())
        assert_equal "guide" (Bpurpose.Get())  
        assert_equal "default" (Fpurpose.Get()) -- No longer in a geom object            
    ),
    
    -- Promote sibling prims to geom objects, make sure all root most prims are used.
    function multi_promote_siblings = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_hidesource_test.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        stageObject.Reload quiet:true
        stageObject.DisplayGuide = true
        
        local geomAShape = stageObject.PromoteTo3dsMaxObject "/root/A/A_Shape"
        geomAShape.LiveUpdates = true
        geomAShape.ShowSource = false
        local geomB = stageObject.PromoteTo3dsMaxObject "/root/A/B"
        geomB.LiveUpdates = true
        geomB.ShowSource = false
        local geomF = stageObject.PromoteTo3dsMaxObject "/root/A/B/F"
        geomF.LiveUpdates = true
        geomF.ShowSource = false
            
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        -- Only root-most promoted prims are driving setup of purposes.
        
        local AShape = stage.GetPrimAtPath("/root/A/A_Shape")
        local AShapeImg = pyUsdGeom.Imageable(AShape)
        local AShapePurpose =AShapeImg.GetPurposeAttr()
        assert_equal "geomObjectSource" (AShapePurpose.Get())
            
        local B = stage.GetPrimAtPath("/root/A/B")
        local Bimg = pyUsdGeom.Imageable(B)
        local Bpurpose = Bimg.GetPurposeAttr()
        assert_equal "geomObjectSource" (Bpurpose.Get())
            
        local F = stage.GetPrimAtPath("/root/A/B/F")
        local Fimg = pyUsdGeom.Imageable(F)
        local Fpurpose = Fimg.GetPurposeAttr()
        assert_equal "default" (Fpurpose.Get())
            
        assert_box3_equal (Box3 [-5,-5,0] [5,5,0.0001]) (nodeLocalBoundingBox  geomAShape asBox3:true)
        assert_equal 4 (getNumVerts (snapshotAsMesh geomAShape))
                    
        assert_box3_equal (Box3 [-5,15,0] [45,45,0.0001]) (nodeLocalBoundingBox  geomB asBox3:true)
        assert_equal 16 (getNumVerts (snapshotAsMesh geomB))
            
        assert_box3_equal (Box3 [15,35,0] [25,45,0.0001]) (nodeLocalBoundingBox  geomF asBox3:true)
        assert_equal 4 (getNumVerts (snapshotAsMesh geomF))
            
        assert_equal 1 (((stage.GetSessionLayer()).subLayerPaths).count)
    ),
    
    function promote_undo_redo = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_hidesource_test.usda")
        
        undo on (
            local stageObject = USDStageObject()
            stageObject.SetRootLayer sample
            stageObject.Reload quiet:true
        )
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        local geomD = stageObject.PromoteTo3dsMaxObject "/root/A/B/D"
        geomD.LiveUpdates = true
        geomD.ShowSource = false
        local geomF = stageObject.PromoteTo3dsMaxObject "/root/A/B/F"
        geomF.LiveUpdates = true
        geomF.ShowSource = false
            
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        local D = stage.GetPrimAtPath("/root/A/B/D")
        local Dimg = pyUsdGeom.Imageable(D)
        local Dpurpose = Dimg.GetPurposeAttr()
        assert_equal "geomObjectSource" (Dpurpose.Get())
        
        local F = stage.GetPrimAtPath("/root/A/B/F")
        local Fimg = pyUsdGeom.Imageable(F)
        local Fpurpose = Fimg.GetPurposeAttr()
        assert_equal "geomObjectSource" (Fpurpose.Get())
                
        max undo
        assert_false (isDeleted geomD)
        assert_equal "geomObjectSource" (Dpurpose.Get())
        assert_true (isDeleted geomF)
        assert_equal "default" (Fpurpose.Get())
            
        max undo
            
        assert_true (isDeleted geomD)
        assert_equal "default" (Dpurpose.Get())
        assert_true (isDeleted geomF)
        assert_equal "default" (Fpurpose.Get())            
        
        -- Undo stage creation
        max undo
        assert_true (isDeleted stageObject)
            
        max redo 
        assert_false (isDeleted stageObject)    
        
        max redo
        assert_false (isDeleted geomD)
        assert_equal "geomObjectSource" (Dpurpose.Get())
        assert_true (isDeleted geomF)
        assert_equal "default" (Fpurpose.Get())
    
        max redo
        assert_false (isDeleted geomD)
        assert_equal "geomObjectSource" (Dpurpose.Get())
        assert_false (isDeleted geomF)
        assert_equal "geomObjectSource" (Fpurpose.Get())
    ),
    
    function delete_stage_test = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_hidesource_test.usda")
        
        undo on (
            local stageObject = USDStageObject()
            stageObject.SetRootLayer sample
            stageObject.Reload quiet:true
            stageObject.DisplayGuide = true
        )
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        local geomD = stageObject.PromoteTo3dsMaxObject "/root/A/B/D"
        
        assert_box3_equal (Box3 [-5,35,0] [5,45,0.0001]) (nodeLocalBoundingBox  geomD asBox3:true)
        assert_equal 4 (getNumVerts (snapshotAsMesh geomD))
        
        undo on (            
            delete stageObject
        )
        
        -- Connection to stage broken - empty object.
        assert_box3_equal (Box3 [-0.0001,-0.0001,-0.0001] [0.0001,0.0001,0.0001]) (nodeLocalBoundingBox  geomD asBox3:true)
        assert_equal 0 (getNumVerts (snapshotAsMesh geomD))
            
        -- Test connection rebuilt after undo
        max undo
        assert_box3_equal (Box3 [-5,35,0] [5,45,0.0001]) (nodeLocalBoundingBox  geomD asBox3:true)
        assert_equal 4 (getNumVerts (snapshotAsMesh geomD))
        assert_equal stageObject geomD.parent        
    ),
    
    function save_load_test = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_hidesource_test.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        stageObject.Reload quiet:true
        stageObject.DisplayGuide = true
        
        local geomD = stageObject.PromoteTo3dsMaxObject "/root/A/B/D"
        assert_box3_equal (Box3 [-5,35,0] [5,45,0.0001]) (nodeLocalBoundingBox  geomD asBox3:true)
        assert_equal 4 (getNumVerts (snapshotAsMesh geomD))
                    
        local maxSceneSavePath = output_prefix + "save_load_test.max"
        saveMaxFile maxSceneSavePath quiet:true
        
        quickReset()
            
        assert_true (isDeleted geomD)
            
        -- Load the scene from disk...
        loadMaxFile maxSceneSavePath
        
        local reloadedD = $D
            
        assert_defined reloadedD
            
        assert_box3_equal (Box3 [-5,35,0] [5,45,0.0001]) (nodeLocalBoundingBox  reloadedD asBox3:true)
        assert_equal 4 (getNumVerts (snapshotAsMesh reloadedD))
    ),
    
    function promote_instances = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\promote_test_cube_instance_refs.usda")
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        stageObject.Reload quiet:true
        
        local pathBox001 = "/root/UsdStage002/UsdStage001/root/Box002/Box001"
        local Box001 = stageObject.PromoteTo3dsMaxObject pathBox001
        Box001.LiveUpdates = true
        Box001.ShowSource = false
            
        assert_equal 8 (getNumVerts (snapshotAsMesh Box001))
            
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        local Box001Prim = stage.GetPrimAtPath(pathBox001)
        local Box001PrimImg = pyUsdGeom.Imageable(Box001Prim)
        local Box001PrimPurpose = Box001PrimImg.GetPurposeAttr()
        assert_equal "geomObjectSource" (Box001PrimPurpose.Get())
        
        -- Check that instancing was broken (and nested instancing) : 
        local UsdStage002 = stage.GetPrimAtPath("/root/UsdStage002")
        assert_false (UsdStage002.IsInstanceable())            
        local Box002 = stage.GetPrimAtPath("/root/UsdStage002/UsdStage001/root/Box002")
        assert_false (Box002.IsInstanceable())
            
        quickReset()
        
        -- Test that instancing is broken bellow promotion point as well.
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        stageObject.Reload quiet:true
        stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        local pathRoot = "/root"
        local root = stageObject.PromoteTo3dsMaxObject pathRoot
        root.LiveUpdates = true
        root.ShowSource = false
        assert_equal 32 (getNumVerts (snapshotAsMesh root))
            
        local rootPrim = stage.GetPrimAtPath(pathRoot)
        local rootPrimImg = pyUsdGeom.Imageable(rootPrim)
        local rootPrimPurpose = rootPrimImg.GetPurposeAttr()
        assert_equal "geomObjectSource" (rootPrimPurpose.Get())

        local stage1 = stage.GetPrimAtPath("/root/UsdStage001")
        assert_false (stage1.IsInstanceable())            
        local box1 = stage.GetPrimAtPath("/root/UsdStage001/UsdStage001/root/Box002")
        assert_false (box1.IsInstanceable())
        local stage2 = stage.GetPrimAtPath("/root/UsdStage002")
        assert_false (stage2.IsInstanceable())            
        local box2 = stage.GetPrimAtPath("/root/UsdStage002/UsdStage001/root/Box002")
        assert_false (box2.IsInstanceable())
    ),
    
    -- Draw modes should not be promoted to 3ds Max.
    function disable_draw_modes = (
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\point_instancer_draw_modes.usda")
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        stageObject.Reload quiet:true
            
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        assert_true (findString (stage.GetSessionLayer()).subLayerPaths[0] "MaxUsd_DrawModes_Reserved" != undefined)
        
        -- Before promoting, cards draw mode enabled.
        local proto1 = stage.GetPrimAtPath("/Instancer/Protos/Proto1")
        local attr1 = (pyUsdGeom.ModelAPI(proto1)).GetModelDrawModeAttr()
        assert_equal (attr1.Get()) "cards"
        local proto2 = stage.GetPrimAtPath("/Instancer/Protos/Proto2")
        local attr2 = (pyUsdGeom.ModelAPI(proto2)).GetModelDrawModeAttr()
        assert_equal (attr2.Get()) "cards"
            
        local promotedInstancer = stageObject.PromoteTo3dsMaxObject "/Instancer"
        promotedInstancer.LiveUpdates = true
        promotedInstancer.ShowSource = false
        
        -- Make sure the USDGeomObject layer is stronger than the draw modes layer.
        -- Hack : use strings, indexing on subLayerPaths doesnt return the strenght order, 
        -- the USD api provides iterators in C++, that would give the order, but unclear how
        -- to do it from mxs wraping python wrapping c++... 
        local layers = ((stage.GetSessionLayer()).subLayerPaths) as string
        local posDrawModes = (findString layers "MaxUsd_DrawModes_Reserved")
        local posGeom = (findString layers "MaxUsd_USDGeomObjects_Reserved")    
        assert_defined posDrawModes
        assert_defined posGeom
        assert_true (posGeom < posDrawModes)
            
        -- Force regenerate draw modes, make sure the geom object layer is still stronger.
        stageObject.GenerateDrawModes()

        layers = ((stage.GetSessionLayer()).subLayerPaths) as string
        posDrawModes = (findString layers "MaxUsd_DrawModes_Reserved")
        posGeom = (findString layers "MaxUsd_USDGeomObjects_Reserved")
        assert_defined posDrawModes
        assert_defined posGeom
        assert_true (posGeom < posDrawModes)
        
        -- Cards now disabled.
        local proto1 = stage.GetPrimAtPath("/Instancer/Protos/Proto1")
        local attr1 = (pyUsdGeom.ModelAPI(proto1)).GetModelDrawModeAttr()
        assert_equal (attr1.Get()) "default"
        local proto2 = stage.GetPrimAtPath("/Instancer/Protos/Proto2")
        local attr2 = (pyUsdGeom.ModelAPI(proto2)).GetModelDrawModeAttr()
        assert_equal (attr2.Get()) "default"
        
        assert_equal 78 (getNumVerts (snapshotAsMesh promotedInstancer))
        assert_box3_equal (Box3 [-1,-13,-0.951056] [13,1,13]) (nodeLocalBoundingBox  promotedInstancer asBox3:true)
    ),
    
    function custom_purpose_includes = (
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_hidesource_test.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        stageObject.Reload quiet:true
            
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        local geom = stageObject.PromoteTo3dsMaxObject "/root"
        geom.LiveUpdates = true
        geom.ShowSource = false
        
        -- Test using currently displayed purposes.
        geom.ViewportDisplayPurposes = true
        geom.IncludeProxy = true
        geom.IncludeRender = false
        geom.IncludeGuide = false
            
        -- Display guides in vp...
        stageObject.DisplayGuide = true
        
        local root = stage.GetPrimAtPath("/root")
        local rootImg = pyUsdGeom.Imageable(root)
        local rootPurpose = rootImg.GetPurposeAttr()
        assert_equal "geomObjectSource" (rootPurpose.Get())    
            
        local B = stage.GetPrimAtPath("/root/A/B")
        local Bimg = pyUsdGeom.Imageable(B)
        local Bpurpose = Bimg.GetPurposeAttr()
        assert_equal "geomObjectSource" (Bpurpose.Get())    
        
        local E = stage.GetPrimAtPath("/root/A/B/E")
        local Eimg = pyUsdGeom.Imageable(E)
        local Epurpose = Eimg.GetPurposeAttr()
        assert_equal "geomObjectSource" (Epurpose.Get())
            
        assert_box3_equal (Box3 [-5,-5,0] [45,45,0.0001]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 24 (getNumVerts (snapshotAsMesh geom))
        
        -- Test using custom purposes.
        geom.ViewportDisplayPurposes = false
            
        -- Now guides should not be displayed.
        assert_equal "geomObjectSource" (rootPurpose.Get())    
        assert_equal "geomObjectSkip" (Bpurpose.Get())
        assert_equal "geomObjectSkip" (Epurpose.Get())
        
        assert_box3_equal (Box3 [-5,-5,0] [25,25,0.0001]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 8 (getNumVerts (snapshotAsMesh geom))
        
        -- Add a new prim, proxy, currently included.
        spherePrim = pyUsdGeom.Sphere.Define stage "/root/sphere_proxy"
            
        local sphereImg = pyUsdGeom.Imageable(spherePrim)
        local spherePurpose =sphereImg.GetPurposeAttr()
        spherePurpose.Set("proxy")
        
        assert_equal "geomObjectSource" (rootPurpose.Get())    
        assert_equal "geomObjectSkip" (Bpurpose.Get())
        assert_equal "geomObjectSkip" (Epurpose.Get())
        assert_equal "geomObjectSource" (spherePurpose.Get())
            
        assert_box3_equal (Box3 [-5,-5,-1] [25,25,1]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 100 (getNumVerts (snapshotAsMesh geom))
        
        -- Exlude proxy..
        geom.IncludeProxy = false
        
        assert_equal "geomObjectSource" (rootPurpose.Get())    
        assert_equal "geomObjectSkip" (Bpurpose.Get())
        assert_equal "geomObjectSkip" (Epurpose.Get())
        assert_equal "geomObjectSkip" (spherePurpose.Get())
            
        assert_box3_equal (Box3 [-5,-5,0] [25,25,0.0001]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 8 (getNumVerts (snapshotAsMesh geom))
        
        -- Add a new prim, render, currently not included.
        sphereRenderPrim = pyUsdGeom.Sphere.Define stage "/root/sphere_render"
            
        local sphereRenderImg = pyUsdGeom.Imageable(sphereRenderPrim)
        local sphereRenderPurpose =sphereRenderImg.GetPurposeAttr()
        sphereRenderPurpose.Set("render")
        
        assert_equal "geomObjectSource" (rootPurpose.Get())    
        assert_equal "geomObjectSkip" (Bpurpose.Get())
        assert_equal "geomObjectSkip" (Epurpose.Get())
        assert_equal "geomObjectSkip" (spherePurpose.Get())
        assert_equal "geomObjectSkip" (sphereRenderPurpose.Get())
            
        assert_box3_equal (Box3 [-5,-5,0] [25,25,0.0001]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 8 (getNumVerts (snapshotAsMesh geom))
        
        -- Include render..
        geom.IncludeRender = true
            
        assert_equal "geomObjectSource" (rootPurpose.Get())    
        assert_equal "geomObjectSkip" (Bpurpose.Get())
        assert_equal "geomObjectSkip" (Epurpose.Get())
        assert_equal "geomObjectSkip" (spherePurpose.Get())
        assert_equal "geomObjectSource" (sphereRenderPurpose.Get())
        
        assert_box3_equal (Box3 [-5,-5,-1] [25,25,1]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 100 (getNumVerts (snapshotAsMesh geom))
            
        -- Include guides..
        geom.IncludeGuide = true
        -- All quads should show + the render sphere.
        
        assert_equal "geomObjectSource" (rootPurpose.Get())    
        assert_equal "geomObjectSource" (Bpurpose.Get())
        assert_equal "geomObjectSource" (Epurpose.Get())
        assert_equal "geomObjectSkip" (spherePurpose.Get())
        assert_equal "geomObjectSource" (sphereRenderPurpose.Get())
            
        assert_box3_equal (Box3 [-5,-5,-1] [45,45,1]) (nodeLocalBoundingBox  geom asBox3:true)
        assert_equal 116 (getNumVerts (snapshotAsMesh geom))
    ),
    
    function include_invisible = (
                
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_hierarchy_test.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        stageObject.Reload quiet:true
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
            
        local rootObject = stageObject.PromoteTo3dsMaxObject "/root"
        rootObject.IncludeInvisible = false
        rootObject.LiveUpdates = true
        
        -- Entire stage in mesh.
        assert_box3_equal (box3 [-5,-5,0] [45,45,0.0001]) (nodeLocalBoundingBox  rootObject asBox3:true)    
            
        BPrim = stage.GetPrimAtPath("/root/A/B")
        imageablePrim = pyUsdGeom.Imageable(BPrim)
        visibilityAttr = imageablePrim.GetVisibilityAttr()
        visibilityAttr.Set("invisible")
        
        -- Removed invisible B.
        assert_box3_equal (Box3 [-5,-5,0] [25,25,0.0001]) (nodeLocalBoundingBox  rootObject asBox3:true)
            
        rootObject.IncludeInvisible = true
        -- B back in the mesh.
        assert_box3_equal (box3 [-5,-5,0] [45,45,0.0001]) (nodeLocalBoundingBox  rootObject asBox3:true)
            
        -- Remove again to then test interaction with live updates config.
        rootObject.IncludeInvisible = false
        assert_box3_equal (Box3 [-5,-5,0] [25,25,0.0001]) (nodeLocalBoundingBox  rootObject asBox3:true)
        
        rootObject.LiveUpdates = false
        
        visibilityAttr.Set("inherited")
        
        -- Mesh not updated yet.
        assert_box3_equal (Box3 [-5,-5,0] [25,25,0.0001]) (nodeLocalBoundingBox  rootObject asBox3:true)
        
        -- Toggling the option, or enabling live updates, will force updates.
        
        -- Toggle include invisible.
        rootObject.IncludeInvisible = true
        rootObject.IncludeInvisible = false
        assert_box3_equal (box3 [-5,-5,0] [45,45,0.0001]) (nodeLocalBoundingBox  rootObject asBox3:true)
        visibilityAttr.Set("invisible")
        
        -- Toggle live updates.        
        assert_box3_equal (box3 [-5,-5,0] [45,45,0.0001]) (nodeLocalBoundingBox  rootObject asBox3:true)
        rootObject.LiveUpdates = true
        assert_box3_equal (Box3 [-5,-5,0] [25,25,0.0001]) (nodeLocalBoundingBox  rootObject asBox3:true)
    ),
    
    -- Testing that the usd geom object being detached from the node is handled
    -- correctly, that is, the USD prims are displayed again in the stage.
    function convert_to_mesh_detach = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_hidesource_test.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        stageObject.Reload quiet:true
                
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        local geomD = stageObject.PromoteTo3dsMaxObject "/root/A/B/D"
        geomD.LiveUpdates = true
        geomD.ShowSource = false
                    
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        local D = stage.GetPrimAtPath("/root/A/B/D")
        local Dimg = pyUsdGeom.Imageable(D)
        local Dpurpose = Dimg.GetPurposeAttr()
        assert_equal "geomObjectSource" (Dpurpose.Get())
        
        convertToMesh geomD
        
        -- Should detect the conversion and show again in the stage.
        local Dpurpose = Dimg.GetPurposeAttr()
        assert_equal "default" (Dpurpose.Get())
        
        -- Test undo redo handling..
        max undo
        
        local Dpurpose = Dimg.GetPurposeAttr()
        assert_equal "geomObjectSource" (Dpurpose.Get())
        max redo 
        local Dpurpose = Dimg.GetPurposeAttr()
        assert_equal "default" (Dpurpose.Get())        
    ),
    
    -- Adding modifiers onto the meshes should not show again prims in the stage on its own.
    function modifier_no_detach = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\usdgeom_hidesource_test.usda")
        
        local stageObject = USDStageObject()
        stageObject.SetRootLayer sample
        stageObject.Reload quiet:true
                
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        
        local geomD = stageObject.PromoteTo3dsMaxObject "/root/A/B/D"
        geomD.LiveUpdates = true
        geomD.ShowSource = false
                    
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stageObject.cacheId))
        local D = stage.GetPrimAtPath("/root/A/B/D")
        local Dimg = pyUsdGeom.Imageable(D)
        local Dpurpose = Dimg.GetPurposeAttr()
        assert_equal "geomObjectSource" (Dpurpose.Get())
        
        bendModifier = BendMod ()
        addModifier geomD bendModifier
        
        -- Make sure the connection was preserved.
        local Dpurpose = Dimg.GetPurposeAttr()
        assert_equal "geomObjectSource" (Dpurpose.Get())        
            
        delete geomD 
            
        local Dpurpose = Dimg.GetPurposeAttr()
        assert_equal "default" (Dpurpose.Get())        
    ),
    
    function update_stage_and_path = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        
        -- In the first stage : 
        -- A -> Sphere -> 92 verts
        -- B -> Cube -> 8 verts
        -- C -> Cone -> 82 verts
        local sample_1 = (testDataDir + @"data\usdgeom_update_stage_and_path_1.usda")
        local stageObject_1 = USDStageObject()
        stageObject_1.SetRootLayer sample_1
        stageObject_1.Reload quiet:true
        
        -- In the second stage : 
        -- A -> Cone -> 31 verts
        -- B -> Capsule -> 82 verts
        local sample_2 = (testDataDir + @"data\usdgeom_update_stage_and_path_2.usda")
        local stageObject_2 = USDStageObject()
        stageObject_2.SetRootLayer sample_2
        stageObject_2.Reload quiet:true    
        
        local geom = stageObject_1.PromoteTo3dsMaxObject "/root/A"
        assert_equal 92 (getNumVerts (snapshotAsMesh geom))
        
        geom.PrimPath = "/root/B"
        assert_equal 8 (getNumVerts (snapshotAsMesh geom))
            
        geom.USDStage = stageObject_2
        assert_equal 82 (getNumVerts (snapshotAsMesh geom))
            
        geom.PrimPath = "/root/C" -- doesnt exist in the stage
        assert_equal 0 (getNumVerts (snapshotAsMesh geom))

        geom.USDStage = stageObject_1 -- now C will exist
        assert_equal 31 (getNumVerts (snapshotAsMesh geom))
            
        geom.PrimPath = "#$@()$*" -- bad path
        assert_equal 0 (getNumVerts (snapshotAsMesh geom))
        
        geom.PrimPath = "/root/A" -- fix the path
        assert_equal 92 (getNumVerts (snapshotAsMesh geom))
        
        geom.USDStage = undefined -- clear stage
        assert_equal 0 (getNumVerts (snapshotAsMesh geom)) 
        
        geom.USDStage = stageObject_2 -- fix stage
        assert_equal 31 (getNumVerts (snapshotAsMesh geom))
    ),    
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(usdgeom_object_from_hierarchy,
        usdgeom_transform_animation,
        usdgeom_vertex_animation,
        usdgeom_auto_selection,
        usdgeom_add_remove_prim,
        usdgeom_live_updates,
        live_updates_refresh,
        force_refresh,
        usdgeom_show_source,
        multi_promote_ancestors,
        multi_promote_siblings,
        promote_undo_redo,
        delete_stage_test,
        save_load_test,
        promote_instances,
        disable_draw_modes,
        custom_purpose_includes,
        include_invisible,
        convert_to_mesh_detach,
        modifier_no_detach,
        update_stage_and_path
        )
)

runUsdTestFixture usd_geom_object_test (getThisScriptFilename())