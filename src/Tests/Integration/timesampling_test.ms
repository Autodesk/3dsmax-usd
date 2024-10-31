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
pyVt = python.import("pxr.Vt")
pySdf = python.import("pxr.Sdf")
pyUsdLux = python.import("pxr.UsdLux")

output_prefix = pathConfig.appendPath (getDir #temp) "/TIMESPAMPLING_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct timesampling_test
(
    private 
    initialUnits,
    initialScale,
    
    public
    function setup =
    (
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        resetmaxfile #noprompt
    ),

    -- We test the conversion of time-sampled data in depth in the C++ tests. 
    -- Just testing that everything is wired up correctly here.
    function test_import_at_timecode = (
        local stagePath = (output_prefix + "TIMCODE_IMPORT.usda")
        local stage = pyUsd.Stage.CreateNew(stagePath)
        pyUsdGeom.SetStageUpAxis stage pyUsdGeom.Tokens.z
        stage.SetStartTimeCode(0)
        stage.SetEndTimeCode(1)
        
        -- Create prims and configure them differently at the default timecode 
        -- and timecode 1, then import at timeCode 1.
            
        -- Create a simple mesh.
        local fooPrim = pyUsdGeom.Mesh.Define stage "/foo"
        local pointsAtt = fooPrim.CreatePointsAttr()
        pointsAtt.Set (pyVt.Vec3fArray (#((pyGf.Vec3f 1 1 1), (pyGf.Vec3f 2 2 2), (pyGf.Vec3f 3 3 3) , (pyGf.Vec3f 4 4 4)))) -- default
        pointsAtt.Set (pyVt.Vec3fArray (#((pyGf.Vec3f 0 0 0), (pyGf.Vec3f 0 1 0), (pyGf.Vec3f 0 0 1)))) 1 -- 1
        local vertexCounts = fooPrim.CreateFaceVertexCountsAttr()
        vertexCounts.Set (pyVt.IntArray #(4)) -- default
        vertexCounts.Set (pyVt.IntArray #(3)) 1
        local vertexIndidces = fooPrim.CreateFaceVertexIndicesAttr()
        vertexIndidces.Set (pyVt.IntArray #(0, 1, 2, 3)) -- default
        vertexIndidces.Set (pyVt.IntArray #(2, 1, 0)) 1
        
        -- Add an animated transform..
        local translateOp = fooPrim.AddTranslateOp()
        translateOp.Set (pyGf.Vec3f 0 0 5) -- default
        translateOp.Set (pyGf.Vec3f 0 0 15) 1
        
        -- Add an animated purpose
        local purposeAttr = (pyUsdGeom.Imageable(fooPrim)).CreatePurposeAttr()
        purposeAttr.Set "render" -- default
        purposeAttr.Set "proxy" 1
        
        -- Create a light.
        local lightPrim = pyUsdLux.DiskLight.Define stage "/light"
        local radiusAttr = lightPrim.CreateRadiusAttr()
        radiusAttr.Set 5 -- default
        radiusAttr.Set 25 1
            
        -- Create a camera.
        local cameraPrim = pyUsdGeom.Camera.Define stage "/camera"
        local clippingAttr = cameraPrim.CreateClippingRangeAttr()
        clippingAttr.Set (pyGf.Vec2f 25 75) -- default
        clippingAttr.Set (pyGf.Vec2f 50 100) 1
        (stage.GetRootLayer()).Save()

        units.SystemType = #Centimeters
        units.SystemScale = 1
            
        -- Import the file in Max at timecode 1.
        local importOptions = USDImporter.CreateOptions()
        importOptions.TimeMode = #customRange
        importOptions.StartTimeCode = 1
        USDImporter.ImportFile stagePath importOptions:importOptions
        
        -- Check the imported geometry...
        local fooNode = getNodeByName "foo"
        in coordsys local (
            local badVertexMsg = "Incorrect vertex position for timecode 1."
            assert_point3_equal (point3 0.f 0.f 0.f) (polyop.getVert fooNode 1) message:badVertexMsg
            assert_point3_equal (point3 0.f 1.f 0.f) (polyop.getVert fooNode 2) message:badVertexMsg
            assert_point3_equal (point3 0.f 0.f 1.f) (polyop.getVert fooNode 3) message:badVertexMsg
        )
        local badIndexMsg = "Incorrect face index timecode 1."
        assert_equal 3 (fooNode.GetFaceVertex 1 1) message:badIndexMsg
        assert_equal 2 (fooNode.GetFaceVertex 1 2) message:badIndexMsg
        assert_equal 1 (fooNode.GetFaceVertex 1 3) message:badIndexMsg
            
        -- Node position...
        assert_equal 15 fooNode.position.z message:"Incorrect node position for timecode 1."
        
        -- Purpose metadata...
        assert_equal "proxy" fooNode.usd_purpose
        
        -- Light...
        local lightNode = getNodeByName "light"
        assert_equal 25 lightNode.radius message:"Incorrect light radius for timecode 1."
        
        -- Cameras... 
        local cameraNode = getNodeByName "camera" message:"Incorrect camera clipping range for timecode 1."
        assert_equal 50 cameraNode.nearclip
        assert_equal 100 cameraNode.farclip
   ),
        
    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    tests = #(test_import_at_timecode)
)

runUsdTestFixture timesampling_test (getThisScriptFilename())