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

pyUsd = python.import("pxr.Usd")
pyUsdGeom = python.import("pxr.UsdGeom")
pyGf = python.import("pxr.Gf")
pyShade = python.import("pxr.UsdShade")
pyVt = python.import("pxr.Vt")

pyUsdUtils = python.import("pxr.UsdUtils")

struct import_from_cache_test
(
    initialUnits,
    initialScale,
    
    public
    
    function resetFileAndUnits = (
        resetMaxFile #noprompt
        units.SystemType = #Centimeters
        units.SystemScale = 1
    ),    
    
    function setup =
    (
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        resetFileAndUnits()
    ),
    
    function test_import_from_cache = (
        -- Create a stage in memory.
        local stage = pyUsd.Stage.CreateInMemory()
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z
        local pyramidPrim = pyUsdGeom.Mesh.Define stage "/pyramid"
        local points = #((pyGf.Vec3f -10 0 -10), (pyGf.Vec3f 10 0 -10), (pyGf.Vec3f -10 0 10), (pyGf.Vec3f 10 0 10), (pyGf.Vec3f 0 20 0))
        local vertexCount = #(3,3,3,3,4)
        local vertexIndices = #(0, 1, 4, 0, 2, 4, 3, 1, 4, 3, 2, 4, 0, 1, 3, 2)
        pyramidPrim.CreatePointsAttr(points)
        pyramidPrim.CreateFaceVertexCountsAttr(vertexCount)
        pyramidPrim.CreateFaceVertexIndicesAttr(vertexIndices)
        
        -- Specify different transforms at TimeCodes 1 and 2. We will use this later to make sure 
        -- that importing from cache with explicit options works.
        local translateOp = pyramidPrim.AddTranslateOp()
        translateOp.Set (pyGf.Vec3f 0 0 1) 0
        translateOp.Set (pyGf.Vec3f 0 0 2) 5
        
        -- Add the stage to the cache.
        local stageCache = pyUsdUtils.StageCache.Get()
        stageCache.Insert(stage)
        
        -- Figure out the stage's ID in the cache. This is what we need to import into max.
        local stageId = (stageCache.GetId(stage)).ToLongInt()
        
        -- Using default options.
        USDImporter.ImportFromCache stageId
        
        -- Make sure the pyramind was imported with default options.
        local pyramidNode = $pyramid
        assert_defined pyramidNode
        -- With default should have imported at timeCode #startTime (1)
        assert_equal 1 pyramidNode.position.z message:"Unexpected node position when importing from cache with default options."
    
        resetFileAndUnits()
            
        -- Now specifying the options...
        local importOptions = USDImporter.CreateOptions()
        importOptions.TimeMode = #customRange
        importOptions.StartTimeCode = 5
        
        USDImporter.ImportFromCache stageId importOptions:importOptions
        pyramidNode = $pyramid
        assert_defined pyramidNode
        -- Should now have imported at timeCode 2
        assert_equal 2 pyramidNode.position.z message:"Unexpected node position when importing from cache with specfied options."
        
        resetFileAndUnits()
        -- Test setting an end time code to be less than the start time code still works
        importOptions.EndTimeCode = -1
        assert_float_equal importOptions.startTimeCode importOptions.EndTimeCode message:"EndTimeCode should've been set to be the same as StartTimeCode"
    ),
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    tests = #(test_import_from_cache)
)

runUsdTestFixture import_from_cache_test (getThisScriptFilename())