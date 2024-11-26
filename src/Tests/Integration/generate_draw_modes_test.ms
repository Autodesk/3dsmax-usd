--
-- Copyright 2024 Autodesk
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
pyUsdGeom = python.import("pxr.UsdGeom")
pyUsdUtils = python.import("pxr.UsdUtils")
pySdf = python.import("pxr.Sdf")

testDataDir = (getFilenamePath (getThisScriptFilename())) + @"data\"
    
output_prefix = pathConfig.appendPath (getDir #temp) "/GENERATE_DRAW_MODE_TESTS_"
deleteTestFiles(output_prefix + "*.*")
    
struct generate_draw_modes_test
(  
    private 
    initialUnits,
    initialScale,
    
    public
    function setup =
    (
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        resetMaxFile #noprompt
        units.SystemType = #inches
        units.SystemScale = 1
    ),
    
    function point_instancer_generate_draw_modes_test = (
        
        instancer_file = (testDataDir + @"point_instancer_draw_modes.usda")
        
        local stage_object = USDStageObject()
        
        assert_true stage_object.GeneratePointInstancesDrawModes
       
        stage_object.GeneratePointInstancesDrawModes = false
        stage_object.SetRootLayer instancer_file
        
        -- Use the render mesh to inspect the geometry..
        render_mesh = snapshotasmesh stage_object renderMesh:true

        -- Instancer has 2 protoypes, 2 instances of each.
        -- 2 Boxes (shared corner vertices) -> 8 verts * 2 = 16 
        -- 2 Cones -> 31 verts * 2 = 62
        -- Total : 78 verts
        assert_equal 78 render_mesh.numVerts

        -- Enable draw modes...
        stage_object.GeneratePointInstancesDrawModes = true
        render_mesh = snapshotasmesh stage_object renderMesh:true
        
        -- Now all 4 instances are displayed as boxes (unshared verts)
        -- 24 verts * 4 = 96
        assert_equal 96 render_mesh.numVerts
        
        -- Disable again..
        stage_object.GeneratePointInstancesDrawModes = false
        render_mesh = snapshotasmesh stage_object renderMesh:true
        assert_equal 78 render_mesh.numVerts
    ),
    
    function point_instancer_active_draw_modes_test = (
        instancer_file = (testDataDir + @"point_instancer_draw_modes.usda")
        
        local stage_object = USDStageObject()
        
        -- The option defaults to "Box Cards"
        assert_equal 1 stage_object.PointInstancesDrawMode
       
        stage_object.SetRootLayer instancer_file
        
        render_mesh = snapshotasmesh stage_object renderMesh:true

        assert_equal 96 render_mesh.numVerts

        -- "Default" draw mode (the actual meshes)
        stage_object.PointInstancesDrawMode = 0
        render_mesh = snapshotasmesh stage_object renderMesh:true
        
        assert_equal 78 render_mesh.numVerts
        
        -- "Cross Cards" draw mode
        stage_object.PointInstancesDrawMode = 2
        render_mesh = snapshotasmesh stage_object renderMesh:true
        assert_equal 96 render_mesh.numVerts
    ),
    
    function point_instancer_draw_modes_clear_session_test = (
        
        -- Makes sure that the MXS function to "clear the session layer" (which is the same
        -- as clicking the button in the UI, will leave the stage in a state that respects 
        -- the draw mode configuration (i.e. rebuild draw modes after the clear).
        
        instancer_file = (testDataDir + @"point_instancer_draw_modes.usda")
        
        local stage_object = USDStageObject()
        stage_object.SetRootLayer instancer_file
        
        render_mesh = snapshotasmesh stage_object renderMesh:true
        assert_equal 96 render_mesh.numVerts

        stage_object.ClearSessionLayer()
        
        render_mesh = snapshotasmesh stage_object renderMesh:true    
        assert_equal 96 render_mesh.numVerts
        
        stage_object.ClearSessionLayer()
        
        render_mesh = snapshotasmesh stage_object renderMesh:true    
        assert_equal 96 render_mesh.numVerts
        
        -- However, clearing the sesison layer "manually" using USD api, will not.
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stage_object.cacheId))
        
        (stage.GetSessionLayer()).Clear()
            
        render_mesh = snapshotasmesh stage_object renderMesh:true    
        assert_equal 78 render_mesh.numVerts
    ), 
    
    function point_instancer_draw_modes_generate_function = (
        
        -- Test the "GenerateDrawModes()" MXS function.
        
        instancer_file = (testDataDir + @"point_instancer_draw_modes.usda")
        
        local stage_object = USDStageObject()
        stage_object.SetRootLayer instancer_file
        
        -- Manually clear the session layer.
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stage_object.cacheId))
        
        (stage.GetSessionLayer()).Clear()
            
        render_mesh = snapshotasmesh stage_object renderMesh:true    
        assert_equal 78 render_mesh.numVerts
            
        -- Now force update the generated  draw modes.
        stage_object.GenerateDrawModes()
        
        render_mesh = snapshotasmesh stage_object renderMesh:true    
        assert_equal 96 render_mesh.numVerts
            
        -- Do it again, except this time, disable draw mode generation for instancers...
        (stage.GetSessionLayer()).Clear()
        stage_object.GeneratePointInstancesDrawModes = false
        
        stage_object.GenerateDrawModes()
        
        render_mesh = snapshotasmesh stage_object renderMesh:true    
        assert_equal 78 render_mesh.numVerts        
    ),
    
    function point_instancer_draw_modes_no_authored_kinds = (
        -- Kinds need to be authored in the entire hierarchy for draw modes to work, 
        -- this test checks authored kinds...
        
        instancer_file = (testDataDir + @"point_instancer_draw_modes_authored_kinds.usda")
        
        local stage_object = USDStageObject()
        stage_object.SetRootLayer instancer_file
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stage_object.cacheId))
        
        prim = stage.GetPrimAtPath("/Test1")     
        assert_equal "assembly" ((pyUsd.ModelAPI(prim)).GetKind())
        prim = stage.GetPrimAtPath("/Test1/Instancer")
        assert_equal "assembly" ((pyUsd.ModelAPI(prim)).GetKind())            
        prim = stage.GetPrimAtPath("/Test1/Instancer/Protos")
        assert_equal "assembly" ((pyUsd.ModelAPI(prim)).GetKind())
        prim = stage.GetPrimAtPath("/Test1/Instancer/Protos/Proto1")
        assert_equal "component" ((pyUsd.ModelAPI(prim)).GetKind())
        prim = stage.GetPrimAtPath("/Test1/Instancer/Protos/Proto2")
        assert_equal "component" ((pyUsd.ModelAPI(prim)).GetKind())
    ),    
   
    function point_instancer_draw_modes_user_authored_kinds = (
        -- Kinds need to be authored in the entire hierarchy for draw modes to work, 
        -- this test checks authored kinds...
        
        instancer_file = (testDataDir + @"point_instancer_draw_modes_authored_kinds.usda")
        
        local stage_object = USDStageObject()
        stage_object.SetRootLayer instancer_file
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stage_object.cacheId))
        
        prim = stage.GetPrimAtPath("/Test2")     
        assert_equal "assembly" ((pyUsd.ModelAPI(prim)).GetKind()) --> Overruled component->assembly
        prim = stage.GetPrimAtPath("/Test2/Instancer")
        assert_equal "group" ((pyUsd.ModelAPI(prim)).GetKind()) --> User authored kind kept   
        prim = stage.GetPrimAtPath("/Test2/Instancer/Protos")
        assert_equal "group" ((pyUsd.ModelAPI(prim)).GetKind()) -- User authored kind kept
        proto1 = stage.GetPrimAtPath("/Test2/Instancer/Protos/Proto1")
        assert_equal "group" ((pyUsd.ModelAPI(prim)).GetKind()) --> User authored kind kept
        proto2 = stage.GetPrimAtPath("/Test2/Instancer/Protos/Proto2")
        assert_equal "group" ((pyUsd.ModelAPI(prim)).GetKind()) --> User authored kind kept
                    
        -- The two prototypes were left at "kind = group", but make sure we signaled that 
        -- the Draw Modes should be applied at the level anyway
        assert_true (proto1.HasAPI(pyUsdGeom.ModelAPI))
        local attr1 = (pyUsdGeom.ModelAPI(proto1)).GetModelApplyDrawModeAttr();
        
        assert_true (attr1.IsAuthored())
        assert_true (attr1.Get())
            
        assert_true (proto2.HasAPI(pyUsdGeom.ModelAPI))
        local attr2 = (pyUsdGeom.ModelAPI(proto2)).GetModelApplyDrawModeAttr();
        assert_true (attr2.IsAuthored())
        assert_true (attr2.Get())
    ),    
    
    function point_instancer_draw_modes_sublayer_setup = (
        -- Tests that generated draw modes are authored on a sublayer of the session layer.
        
        instancer_file = (testDataDir + @"point_instancer_draw_modes_authored_kinds.usda")
        
        local stage_object = USDStageObject()
        stage_object.SetRootLayer instancer_file
        
        local stageCache = pyUsdUtils.StageCache.Get()
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stage_object.cacheId))
        
        sublayers = (stage.GetSessionLayer()).subLayerPaths
            
        -- Should find one sublayer..
        assert_equal 1 sublayers.count
            
        drawModesLayerPreSave = sublayers[1]
        
        -- Test that the anomymous layer is cleaned/regenerated when reloading the max scene from disk.
        local maxSceneSavePath = output_prefix + "drawModesLayer.max"
        saveMaxFile maxSceneSavePath quiet:true
        
        loadMaxFile maxSceneSavePath
            
        stage_object = $UsdStage001
        local stage = stageCache.Find(pyUsd.StageCache.Id.FromLongInt(stage_object.cacheId))
        sublayers = (stage.GetSessionLayer()).subLayerPaths
            
        assert_equal 1 sublayers.count
        assert_defined (pySdf.Layer.FindOrOpen(sublayers[1]))
    ),        
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(
        point_instancer_generate_draw_modes_test,
        point_instancer_active_draw_modes_test,
        point_instancer_draw_modes_clear_session_test,
        point_instancer_draw_modes_generate_function,
        point_instancer_draw_modes_no_authored_kinds,
        point_instancer_draw_modes_user_authored_kinds,
        point_instancer_draw_modes_sublayer_setup
    )
)

runUsdTestFixture generate_draw_modes_test(getThisScriptFilename())