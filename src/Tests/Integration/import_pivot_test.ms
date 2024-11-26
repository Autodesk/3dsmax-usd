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
pyUsdUtils = python.import("pxr.UsdUtils")

struct import_pivot_test
(
    private
    initialUnits,
    initialScale,
    
    public
    
    function reset = (
        resetmaxfile #noprompt
        units.SystemType = #meters
        units.SystemScale = 1
    ),    
    
    function setup =
    (
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        reset()
    ),
        
    function test_import_pivot  = (
        
        -- Create a simple stage to test with...add it to the cache.
        local stage = pyUsd.Stage.CreateInMemory()        
        local stageCache = pyUsdUtils.StageCache.Get()
        stageCache.Insert(stage)
        local stageId = (stageCache.GetId(stage)).ToLongInt()
        
        pyUsdGeom.SetStageMetersPerUnit stage 1
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z
        local pyramidPrim = pyUsdGeom.Mesh.Define stage "/pyramid"
        local points = #((pyGf.Vec3f -10 0 -10), (pyGf.Vec3f 10 0 -10), (pyGf.Vec3f -10 0 10), (pyGf.Vec3f 10 0 10), (pyGf.Vec3f 0 20 0))
        local vertexCount = #(3,3,3,3,4)
        local vertexIndices = #(0, 1, 4, 0, 2, 4, 3, 1, 4, 3, 2, 4, 0, 1, 3, 2)
        pyramidPrim.CreatePointsAttr(points)
        pyramidPrim.CreateFaceVertexCountsAttr(vertexCount)
        pyramidPrim.CreateFaceVertexIndicesAttr(vertexIndices)
                    
        -- No pivot op, objectOffsetPos remains empty.
        local fooOp = pyramidPrim.AddRotateXYZOp pyUsdGeom.XformOp.PrecisionDouble "foo" false
        fooOp.Set (pyGf.Vec3f 10 20 30)
        local barOp = pyramidPrim.AddTranslateOp pyUsdGeom.XformOp.PrecisionDouble "bar" false
        barOp.Set (pyGf.Vec3f 1 1 1)
            
        USDImporter.ImportFromCache stageId
    
        foo = getNodeByName "pyramid"
        assert_equal [0, 0, 0] foo.objectOffsetPos

        reset()    
            
        -- Pivot defined, but it has no equivalent !inverse! op, can't be used as offset as it wouldnt be inherited by children.
        local pivotOp = pyramidPrim.AddTranslateOp pyUsdGeom.XformOp.PrecisionDouble "pivot" false
        pivotOp.Set (pyGf.Vec3f 0 0 10) 
        USDImporter.ImportFromCache stageId
            
        foo = getNodeByName "pyramid"
        assert_equal [0, 0, 0] foo.objectOffsetPos
    
        reset()
        
        -- Now with an !inverse! op defined, the typical case.
        local pivotInverseOp = pyramidPrim.AddTranslateOp pyUsdGeom.XformOp.PrecisionDouble "pivot" true
        USDImporter.ImportFromCache stageId
        foo = getNodeByName "pyramid"
        assert_equal [0, 0, -10] foo.objectOffsetPos
        xRotationZup = (foo.rotation as EulerAngles).x
        
        reset()

        -- Make sure it still works if the stage's up axis isn't Z.
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.y
        USDImporter.ImportFromCache stageId
        foo = getNodeByName "pyramid"
        assert_equal [0, 0, -10] foo.objectOffsetPos
        -- Expect an additional 90 degree rotation around X when source is Y up.
        xRotationYup = (foo.rotation as EulerAngles).x
        assert_float_equal -90.0f (xRotationYup - xRotationZup)
    ),
    
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    Tests = #(test_import_pivot)
)

runUsdTestFixture import_pivot_test (getThisScriptFilename())