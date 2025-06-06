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

struct usd_to_tri_mesh_test
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
        
        units.SystemType = #inches
        units.SystemScale = 1.0
        
    ),
    
    -- Test conversion with multiple transformed prims.
    function two_cube_prims_trimesh = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\2_cubes_render_mesh.usda")
        
        local stageObject = USDStageObject()
        
        stageObject.SetRootLayer sample
        
        usdMesh = Mesh()
        usdMesh.mesh = (snapshotasmesh stageObject renderMesh:true)
        m = usdMesh.mesh;
        
        -- Test verts
            
        local numVerts = getNumVerts m
        assert_equal 16 numVerts
        allVerts = meshop.getVerts m #all
        
        -- Two 10x10x10 cubes.
        -- Offset by z = 20.
        local expectedVerts = #(
            [-5,-5,0], [-5,5,0], [5,5,0], [5,-5,0],
            [-5,-5,10],[5,-5,10],[5,5,10],[-5,5,10],
            [-5,-5,20],[-5,5,20],[5,5,20],[5,-5,20],
            [-5,-5,30],[5,-5,30],[5,5,30],[-5,5,30])

        assert_array_equal expectedVerts allVerts

        -- Test faces
            
        local numFaces = meshop.getNumFaces m
        assert_equal 24 numfaces
        
        local allFaces = meshop.getFaces m #all
        local expectedFaces = #([1,2,3],[1,3,4],[5,6,7],[5,7,8],[1,4,6],[1,6,5],[4,3,7],
            [4,7,6],[3,2,8],[3,8,7],[2,1,5],[2,5,8],[9,10,11],[9,11,12],[13,14,15],[13,15,16],
            [9,12,14],[9,14,13],[12,11,15],[12,15,14],[11,10,16],[11,16,15],[10,9,13],[10,13,16])
            
        assert_array_equal expectedFaces allFaces
            
        for i = 1 to numFaces do 
        (
            -- All faces on the same material
            assert_equal 1 (getFaceMatID m i)
        )
            
        -- Test normals 
            
        -- Need an Edit_Normals to be able to read specified normals. 
        select usdMesh
        local normals = Edit_Normals()
        modPanel.addModToSelection (normals) ui:on -- maxscript voodoo...otherwise normals arent loaded correctly...
        local numNormals = normals.GetNumNormals()        
        
        local expectedNormals = #([0,0,-1],[0,0,-1],[0,0,-1],[0,0,-1],[0,0,1],[0,0,1],[0,0,1],
            [0,0,1],[0,-1,0],[0,-1,0],[0,-1,0],[0,-1,0],[1,0,0],[1,0,0],[1,0,0],[1,0,0],[0,1,0],
            [0,1,0],[0,1,0],[0,1,0],[-1,0,0],[-1,0,0],[-1,0,0],[-1,0,0],[0,0,-1],[0,0,-1],[0,0,-1],[0,0,-1],
            [0,0,1],[0,0,1],[0,0,1],[0,0,1],[0,-1,0],[0,-1,0],[0,-1,0],[0,-1,0],[1,0,0],[1,0,0],[1,0,0],
            [1,0,0],[0,1,0],[0,1,0],[0,1,0],[0,1,0],[-1,0,0],[-1,0,0],[-1,0,0],[-1,0,0])        
            
        for i = 1 to numNormals do 
        (
            assert_point3_equal expectedNormals[i] (normals.GetNormal i)
        )
        
        local expectedNormalIndices = #(1,2,3,1,3,4,5,6,7,5,7,8,9,10,11,
            9,11,12,13,14,15,13,15,16,17,18,19,17,19,20,21,22,23,21,23,24,
            25,26,27,25,27,28,29,30,31,29,31,32,33,34,35,33,35,36,37,38,39,
            37,39,40,41,42,43,41,43,44,45,46,47,45,
            47,48)
        
        local count = 1
        for i = 1 to (normals.GetNumFaces()) do
        (
            for j = 1 to (normals.GetFaceDegree i) do
            (
                assert_equal expectedNormalIndices[count] (normals.GetNormalID i j)
                count = count +1
            )                
        )
        
        -- Test primvars
        
        assert_equal 3 (meshop.getNumMaps m)
        
        assert_true (meshop.getMapSupport m 0) -- vertex color from display color
        assert_true (meshop.getMapSupport m 1) -- 2 Uv primvars defined..
        assert_true (meshop.getMapSupport m 2)
        
        local numVerts0 = meshop.getNumMapVerts m 0
        assert_equal 48 numVerts0
        local numFaces0 = meshop.getNumMapFaces m 0
        assert_equal 24 numFaces0        
        for i = 1 to numVerts0 do 
        (
            -- from USD displayColor.
            assert_point3_equal [0.364706,0.0705882,0.388235] (meshop.getMapVert m 0 i)
        )
        
        local expectedMapFaces = #([1,2,3],[1,3,4],[5,6,7],[5,7,8],[9,10,11],[9,11,12],
            [13,14,15],[13,15,16],[17,18,19],[17,19,20],[21,22,23],[21,23,24],[25,26,27],[25,27,28],
            [29,30,31],[29,31,32],[33,34,35],[33,35,36],[37,38,39],[37,39,40],[41,42,43],[41,43,44],
            [45,46,47],[45,47,48])
        local numMap0faces = meshop.getNumMapFaces m 0
        for i = 1 to numMap0faces do 
        (
            assert_point3_equal expectedMapFaces[i] (meshop.getMapFace m 0 i)
        )
        
        local numVerts1 = meshop.getNumMapVerts m 1
        assert_equal 48 numVerts1
        local numFaces1 = meshop.getNumMapFaces m 1
        assert_equal 24 numFaces1
        local expectedMap1Verts = #([1,2.38419e-07,0],[1,1,0],[0,1,0],[2.38419e-07,0,0],[0,2.38419e-07,0],
            [1,0,0],[1,1,0],[2.68221e-07,1,0],[0,0,0],[1,0,0],[1,1,0],[0,1,0],[0,0,0],[1,0,0],[1,1,0],
            [0,1,0],[0,0,0],[1,0,0],[1,1,0],[0,1,0],[0,0,0],[1,0,0],[1,1,0],[0,1,0],[1,2.38419e-07,0],
            [1,1,0],[0,1,0],[2.38419e-07,0,0],[0,2.38419e-07,0],[1,0,0],[1,1,0],[2.68221e-07,1,0],[0,0,0],
            [1,0,0],[1,1,0],[0,1,0],[0,0,0],[1,0,0],[1,1,0],[0,1,0],[0,0,0],[1,0,0],[1,1,0],[0,1,0],
            [0,0,0],[1,0,0],[1,1,0],[0,1,0])
        for i = 1 to numVerts1 do 
        (
            assert_point3_equal expectedMap1Verts[i] (meshop.getMapVert m 1 i)
        )
        
        local numMap1faces = meshop.getNumMapFaces m 1
        for i = 1 to numMap1faces do 
        (
            assert_point3_equal expectedMapFaces[i] (meshop.getMapFace m 1 i)
        )
        
        local numVerts2 = meshop.getNumMapVerts m 2
        assert_equal 48 numVerts2
        local numFaces2 = meshop.getNumMapFaces m 2
        assert_equal 24 numFaces2
        local expectedMapVerts2 = #([-0.625,0.304087,0],[0.125,0.304087,0],[-0.125,0.304087,0],
            [-0.375,0.304087,0],[1.375,0.695913,0],[0.625,0.695913,0],[0.875,0.695913,0],
            [1.125,0.695913,0],[0.375,0.304087,0],[0.625,0.304087,0],[0.625,0.695913,0],
            [0.375,0.695913,0],[0.625,0.304087,0],[0.875,0.304087,0],[0.875,0.695913,0],
            [0.625,0.695913,0],[0.875,0.304087,0],[1.125,0.304087,0],[1.125,0.695913,0],
            [0.875,0.695913,0],[0.125,0.304087,0],[0.375,0.304087,0],[0.375,0.695913,0],
            [0.125,0.695913,0],[-0.625,0.304087,0],[0.125,0.304087,0],[-0.125,0.304087,0],
            [-0.375,0.304087,0],[1.375,0.695913,0],[0.625,0.695913,0],[0.875,0.695913,0],
            [1.125,0.695913,0],[0.375,0.304087,0],[0.625,0.304087,0],[0.625,0.695913,0],
            [0.375,0.695913,0],[0.625,0.304087,0],[0.875,0.304087,0],[0.875,0.695913,0],
            [0.625,0.695913,0],[0.875,0.304087,0],[1.125,0.304087,0],[1.125,0.695913,0],
            [0.875,0.695913,0],[0.125,0.304087,0],[0.375,0.304087,0],[0.375,0.695913,0],
            [0.125,0.695913,0])
        for i = 1 to numVerts2 do 
        (
            assert_point3_equal expectedMapVerts2[i] (meshop.getMapVert m 2 i)
        )
                
        local numMap2faces = meshop.getNumMapFaces m 2
        for i = 1 to numMap1faces do 
        (
            assert_point3_equal expectedMapFaces[i] (meshop.getMapFace m 1 i)
        )
    ),    
    
    -- Test generation of the material ids.
    function cube_mat_ids = (
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\box_6_subsets_materials.usda")
        
        local stageObject = USDStageObject()
        
        stageObject.SetRootLayer sample
        
        usdMesh = Mesh()
        usdMesh.mesh = (snapshotasmesh stageObject renderMesh:true)
        m = usdMesh.mesh;
        
        assert_equal "#{1..2}" ((meshop.getFacesByMatId m 1) as string)
        assert_equal "#{3..4}" ((meshop.getFacesByMatId m 2) as string)
        assert_equal "#{5..6}" ((meshop.getFacesByMatId m 3) as string)
        assert_equal "#{7..8}" ((meshop.getFacesByMatId m 4) as string)
        assert_equal "#{9..10}" ((meshop.getFacesByMatId m 5) as string)
        assert_equal "#{11..12}" ((meshop.getFacesByMatId m 6) as string)
            
        local sample2 = (testDataDir + @"data\box_3_subsets_materials.usda")
        
        stageObject.SetRootLayer sample2
        
        usdMesh = Mesh()
        usdMesh.mesh = (snapshotasmesh stageObject renderMesh:true)
        m = usdMesh.mesh;
        
        assert_equal "#{1..4}" ((meshop.getFacesByMatId m 1) as string)
        assert_equal "#{5..8}" ((meshop.getFacesByMatId m 2) as string)
        assert_equal "#{9..12}" ((meshop.getFacesByMatId m 3) as string)
    ),
    
    -- Test with animated prims.
    function animated_prim_to_trimesh = (
        
        -- Test at frame 0
        sliderTime = 0
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sample = (testDataDir + @"data\diamond_anim.usda")
        
        local stageObject = USDStageObject()
        
        stageObject.SetRootLayer sample
        
        usdMesh = Mesh()
        usdMesh.mesh = (snapshotasmesh stageObject renderMesh:true)
        m = usdMesh.mesh;
        
        local numVerts = getNumVerts m
        assert_equal 6 numVerts
        allVerts0 = meshop.getVerts m #all
        
        local expectedVerts0 = #([0,0,5],[0,5,0],[-5,0,0],
            [0,-5,0],[5,0,0],[0,0,-5])
            
        assert_array_equal expectedVerts0 allVerts0
                   
        select usdMesh
        local normals = Edit_Normals()
        modPanel.addModToSelection (normals) ui:on
        local numNormals = normals.GetNumNormals()        
        
        local expectedNormals0 = #([0,0,1],[0,1,0],[-1,0,0],[0,0,1],[-1,0,0],[0,-1,0],[0,0,1],
            [0,-1,0],[1,0,0],[0,0,1],[1,0,0],[0,1,0],[0,0,-1],[-1,0,0],[0,1,0],[0,0,-1],[0,-1,0],
            [-1,0,0],[0,0,-1],[1,0,0],[0,-1,0],[0,0,-1],[0,1,0],[1,0,0])        
                        
        for i = 1 to numNormals do 
        (
            assert_point3_equal expectedNormals0[i] (normals.GetNormal i)
        )
        
        -- Test at frame 5
        sliderTime = 5        
        
        usdMesh1 = Mesh()
        usdMesh1.mesh = (snapshotasmesh stageObject renderMesh:true)
        m1 = usdMesh1.mesh;
        allVerts1 = meshop.getVerts m1 #all
        
        local expectedVerts1 = #([3.75677,0,6.4913],[-4.47957e-07,7.5,-1.1953e-07],[-6.4913,-6.55671e-07,3.75677],
            [0,-7.5,-3.28543e-07],[6.4913,1.31134e-06,-3.75677],[-3.75677,0,-6.4913])
       
        assert_array_equal expectedVerts1 allVerts1
                        
        select usdMesh1
        local normals1 = Edit_Normals()
        modPanel.addModToSelection (normals1) ui:on
        local numNormals1 = normals1.GetNumNormals()        
        local expectedNormals1 = #([0.500901,0,0.865504],[0,1,0],[-0.865504,0,0.500901],[0.500901,0,0.865504],
            [-0.865504,0,0.500901],[0,-1,0],[0.500901,0,0.865504],[0,-1,0],[0.865504,0,-0.500901],[0.500901,0,0.865504],
            [0.865504,0,-0.500901],[0,1,0],[-0.500901,0,-0.865504],[-0.865504,0,0.500901],[0,1,0],[-0.500901,0,-0.865504],
            [0,-1,0],[-0.865504,0,0.500901],[-0.500901,0,-0.865504],[0.865504,0,-0.500901],[0,-1,0],[-0.500901,0,-0.865504],
            [0,1,0],[0.865504,0,-0.500901])        
                        
        for i = 1 to numNormals do 
        (
            assert_point3_equal expectedNormals1[i] (normals1.GetNormal i)
        )
    ),
    
    -- Test primvar -> channel mapping (using the stage's primvar mapping)
    function primvar_mapping_trimesh = (
        
        local stageObject = USDStageObject name:"stageFright"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\2_cubes_render_mesh.usda")
        stageObject.SetRootLayer sampleFile
        
        usdMesh = Mesh()
        usdMesh.mesh = (snapshotasmesh stageObject renderMesh:true)
            
        -- Asset has st & st1 primvars, by default mapped to channels 1 and 2
        assert_true (meshop.getMapSupport usdMesh.mesh 1) -- 2 Uv primvars defined..
        assert_true (meshop.getMapSupport usdMesh.mesh 2)
        for i = 3 to 99 do 
        (
           assert_false (meshop.getMapSupport usdMesh.mesh i)
        )
            
        -- Change mapping, make sure it is used.
        -- The stage's primvar mapping api itself is tested elsewhere.
        
        stageObject.SetPrimvarChannelMapping "st" 98
        stageObject.SetPrimvarChannelMapping "st1" 99
        
        usdMesh2 = Mesh()
        usdMesh2.mesh = (snapshotasmesh stageObject renderMesh:true)
                
        for i = 1 to 97 do 
        (
           assert_false (meshop.getMapSupport usdMesh2.mesh i)
        )
        assert_true (meshop.getMapSupport usdMesh2.mesh 98)
        assert_true (meshop.getMapSupport usdMesh2.mesh 99)
    ),
    
    -- Test the fallback when no display color is specified.
    function vertex_colors_with_default = (
        local stageObject = USDStageObject name:"stageFright"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleFile = (testDataDir + @"data\4_quads_vert_color.usda")
        stageObject.SetRootLayer sampleFile
            
        local m = (snapshotasmesh stageObject renderMesh:true)
        
        assert_true (meshop.getMapSupport m 0)
            
        local numVerts0 = meshop.getNumMapVerts m 0
        assert_equal 16 numVerts0
        local numFaces0 = meshop.getNumMapFaces m 0
        assert_equal 8 numFaces0        

        -- First quad : fallback grey color.
        assert_point3_equal [0.8,0.8,0.8] (meshop.getMapVert m 0 1)
        assert_point3_equal [0.8,0.8,0.8] (meshop.getMapVert m 0 2)
        assert_point3_equal [0.8,0.8,0.8] (meshop.getMapVert m 0 3)
        assert_point3_equal [0.8,0.8,0.8] (meshop.getMapVert m 0 4)
        -- Second quad : red, from data    
        assert_point3_equal [1,0,0] (meshop.getMapVert m 0 5)
        assert_point3_equal [1,0,0] (meshop.getMapVert m 0 6)
        assert_point3_equal [1,0,0] (meshop.getMapVert m 0 7)
        assert_point3_equal [1,0,0] (meshop.getMapVert m 0 8)
        -- Third quad : fallback grey color
        assert_point3_equal [0.8,0.8,0.8] (meshop.getMapVert m 0 9)
        assert_point3_equal [0.8,0.8,0.8] (meshop.getMapVert m 0 10)
        assert_point3_equal [0.8,0.8,0.8] (meshop.getMapVert m 0 11)
        assert_point3_equal [0.8,0.8,0.8] (meshop.getMapVert m 0 12)
        -- Fourth quad : red, from data        
        assert_point3_equal [1,0,0] (meshop.getMapVert m 0 13)
        assert_point3_equal [1,0,0] (meshop.getMapVert m 0 14)
        assert_point3_equal [1,0,0] (meshop.getMapVert m 0 15)
        assert_point3_equal [1,0,0] (meshop.getMapVert m 0 16)
    ),
    
    -- Testing the generated edge visibility. This is important, for example, when 
    -- later converting to polys.
    function edge_vis = (
        
        local stageObject = USDStageObject name:"stageFright"        
        local testDataDir = getFilenamePath (getThisScriptFilename())
            
        -- The sample covers 3 scenarios.
        -- 1) A box - the edges "inside" each quads are hidden.
        -- 2) A skinned box (just making sure skinning doesnt impact the above)
        -- 3) A USD "Sphere" primitive (tesselation generated on the fly)
        local sampleFile = (testDataDir + @"data\usd_to_tri_edge_vis.usda")
        stageObject.SetRootLayer sampleFile
        
        -- Usd triangulation has changed over versions. Triangulated indices expected in Max 2023 are a bit differnt.
        maxver = maxversion()
        if maxver[1] > 25000 then (
            -- At frame 0
            local expectedVis = #(
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true)
            -- At frame 5
            local expectedVis5 = #(
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true
            )
        ) else (
            -- At frame 0
            local expectedVis = #(
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true)
            
            -- At frame 5
            local expectedVis5 = #(
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false,
                false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false,
                true,  true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,
                true,  true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,
                true,  true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,
                true,  false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,
                false, false, true,  true,  true,  true,  false, false, true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,  true,
                true,  true,  true,  true,  true)
        )
            
        -- Test at frame 0
        sliderTime = 0    
        
        local m = (snapshotasmesh stageObject renderMesh:true)
        local numFaces = meshop.getNumFaces m
        
        for i = 1 to numFaces do 
        (
            assert_equal expectedVis[(i-1)*3 + 1] (getEdgeVis m i 1)
            assert_equal expectedVis[(i-1)*3 + 2] (getEdgeVis m i 2)
            assert_equal expectedVis[(i-1)*3 + 3] (getEdgeVis m i 3)
        )
    
        -- Test at frame 5. The box and skinned box are animated.
        sliderTime = 5

        local m2 = (snapshotasmesh stageObject renderMesh:true)        
        local numFaces = meshop.getNumFaces m2
        for i = 1 to numFaces do 
        (            
            assert_equal expectedVis5[(i-1)*3 + 1] (getEdgeVis m2 i 1)
            assert_equal expectedVis5[(i-1)*3 + 2] (getEdgeVis m2 i 2)
            assert_equal expectedVis5[(i-1)*3 + 3] (getEdgeVis m2 i 3)
        )
    ),    
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(animated_prim_to_trimesh,
        two_cube_prims_trimesh,
        cube_mat_ids,
        primvar_mapping_trimesh,
        vertex_colors_with_default,
        edge_vis)
)

runUsdTestFixture usd_to_tri_mesh_test (getThisScriptFilename())