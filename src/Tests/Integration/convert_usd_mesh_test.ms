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
pyUsdTf = python.import("pxr.Tf")
pyUsdUtils = python.import("pxr.UsdUtils")

output_prefix = pathConfig.appendPath (getDir #temp) "/CONVERT_USD_MESH_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct convert_usd_mesh_test
(  
    public 
    function setup =
    (
        resetmaxfile #noprompt    
    ),
    
    function test = (

        -- Create a box, and export it to USD.
        local boxNode = box()
        boxNode.name = "box"
      
        convertToPoly boxNode
        polyop.defaultMapFaces boxNode 1
        
        local exportPath = output_prefix + "convert_usd_mesh.usd"
        
        local opts = USDExporter.CreateOptions()
        opts.FileFormat = #ascii
        opts.RootPrimPath = "/"
        USDExporter.ExportFile exportPath exportOptions:opts

        -- Open the exported layer, and place it into the cache.
        local stage = pyUsd.Stage.Open(exportPath)
        stageCache = pyUsdUtils.StageCache.Get()
        stageCache.Insert(stage)
        stageId = (stageCache.GetId(stage)).ToLongInt()

        -- Convert the UsdMesh of the box to a Max mesh.
        local meshData = USDImporter.ConvertUsdMesh stageId "/box" 
        local boxMesh = meshData.mesh
            
        assert_equal 8 boxMesh.numVerts message:"Unexpected vertex count, expected 8 for a box."
        assert_equal 12 boxMesh.numFaces message:"Unexpected face count, expected 12 for a box."

        local bindMaterial = meshData.usdGeomSubsetsBindMaterial
        
        -- A box has 6 material Ids, so on export we generated 6 usdGeomSubsets. When we convert it to 
        -- 3dsMax data, we should therefor get a multi-material with 6 slots.
        assert_equal 6 (GetNumSubMtls bindMaterial) message:"Unexpected number of sub materials, expected 6 for a box."
        assert_equal "(1) _1_" (getSubMtlSlotName bindMaterial 1) message:"Unexpected usdGeomSubsetMaterial slot name for slot 1."
        assert_equal "(2) _2_" (getSubMtlSlotName bindMaterial 2) message:"Unexpected usdGeomSubsetMaterial slot name for slot 2."
        assert_equal "(3) _3_" (getSubMtlSlotName bindMaterial 3) message:"Unexpected usdGeomSubsetMaterial slot name for slot 3."
        assert_equal "(4) _4_" (getSubMtlSlotName bindMaterial 4) message:"Unexpected usdGeomSubsetMaterial slot name for slot 4."
        assert_equal "(5) _5_" (getSubMtlSlotName bindMaterial 5) message:"Unexpected usdGeomSubsetMaterial slot name for slot 5."
        assert_equal "(6) _6_" (getSubMtlSlotName bindMaterial 6) message:"Unexpected usdGeomSubsetMaterial slot name for slot 6."
            
        -- Test with specified import options.
        local conversionOptions = USDImporter.CreateOptions()
        conversionOptions.SetPrimvarChannelMapping "st" 5
        local meshData = USDImporter.ConvertUsdMesh stageId "/box" options:conversionOptions
        local boxMesh = meshData.mesh
        assert_true (meshop.getMapSupport boxMesh 5)

        -- Test import mesh with TimeCode values
        local animeFilePath = getFilenamePath (getThisScriptFilename()) + @"data\cube_scale_up_animation.usda"
        local animeStage = pyUsd.Stage.Open(animeFilePath)
        local animeStageCache = pyUsdUtils.StageCache.Get()
        animeStageCache.Insert(animeStage)
        local animeStageId = (animeStageCache.GetId(animeStage)).ToLongInt()
        
        local importTimeOptions = USDImporter.CreateOptions()
        importTimeOptions.SetPrimvarChannelMapping "st" 5
        
        -- Mesh at startTime
        importTimeOptions.TimeMode = #startTime
        local meshSTData = USDImporter.ConvertUsdMesh animeStageId "/Box001" options:importTimeOptions
        local meshST = meshSTData.mesh
        
        -- Mesh at endTime
        importTimeOptions.TimeMode = #endTime
        local meshETData = USDImporter.ConvertUsdMesh animeStageId "/Box001" options:importTimeOptions
        local meshET = meshETData.mesh

        -- Mesh at frame 50
        importTimeOptions.TimeMode = #CustomRange
        importTimeOptions.StartTimeCode = 50
        local mesh50Data = USDImporter.ConvertUsdMesh animeStageId "/Box001" options:importTimeOptions
        local mesh50 = mesh50Data.mesh
        
        -- Mesh at frame 0
        importTimeOptions.TimeMode = #CustomRange
        importTimeOptions.StartTimeCode = 0
        local mesh0Data = USDImporter.ConvertUsdMesh animeStageId "/Box001" options:importTimeOptions
        local mesh0 = mesh0Data.mesh
        
        local vert50 = #(
            [-14.3052, -18.825, 0],
            [14.3052, -18.825, 0],
            [-14.3052, 18.825, 0],
            [14.3052, 18.825, 0],
            [-14.3052, -18.825, 40.7187],
            [14.3052, -18.825, 40.7187],
            [-14.3052, 18.825, 40.7187],
            [14.3052, 18.825, 40.7187]
        )
        local vert0 = #(
            [-14.3052, -18.825, 0],
            [14.3052, -18.825, 0],
            [-14.3052, 18.825, 0],
            [14.3052, 18.825, 0],
            [-14.3052, -18.825, 16.1803],
            [14.3052, -18.825, 16.1803],
            [-14.3052, 18.825, 16.1803],
            [14.3052, 18.825, 16.1803]
        )

        for i = 1 to mesh50.numVerts do
        (
            assert_point3_equal vert0[i] (getVert meshST i) Message: "Wrong vertice at imported at start time"
            assert_point3_equal vert50[i] (getVert meshET i) Message: "Wrong vertice at imported at end time"
            assert_point3_equal vert50[i] (getVert mesh50 i) Message: "Wrong vertice at imported at time 50"
            assert_point3_equal vert0[i] (getVert mesh0 i) Message: "Wrong vertice at imported at time 0"
        )

        -- Testing bad calls to the convert function...
        
        -- Bad option object : 
        local exceptionThrown = false
        try (
            local foo = 10
            local meshData = USDImporter.ConvertUsdMesh stageId "/box" options:foo
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to use an invalid options object should throw."
        
        -- Bad stage ID :
        local exceptionThrown = false
        try (
            local meshData = USDImporter.ConvertUsdMesh 1234 "/box" 
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to use an invalid stage ID should throw."
        
        -- Bad mesh prim path :
        local exceptionThrown = false
        try (
            local meshData = USDImporter.ConvertUsdMesh stageId "/not_a_mesh"
        )
        catch 
        (
            exceptionThrown = true
        )
        assert_true exceptionThrown message:"Attempting to convert a path that does not point to a UsdGeomMesh should throw."
    ),

    function teardown = (
    ),

    Tests = #(test)
)

runUsdTestFixture convert_usd_mesh_test (getThisScriptFilename())