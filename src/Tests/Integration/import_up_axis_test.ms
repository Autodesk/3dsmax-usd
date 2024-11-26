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

output_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_UP_AXIS_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct import_up_axis_test
(
    private 
    initialUnits,
    initialScale,
    
    function resetMaxFileAndSetupUnits = 
    (
        resetmaxfile #noprompt
        initialUnits = units.SystemType
        initialScale = units.SystemScale
        units.SystemType = #Centimeters
        units.SystemScale = 1
    ),
    
    public
    function setup =
    (
        resetMaxFileAndSetupUnits()
    ),

    function create_usd_pyramid_file filePath upAxis = (
        local stage = pyUsd.Stage.CreateNew(filePath)
        pyUsdGeom.SetStageUpAxis stage upAxis
        local prim001 = pyUsdGeom.Mesh.Define stage "/Prim001"
        local points = #((pyGf.Vec3f -10 0 -10), (pyGf.Vec3f 10 0 -10), (pyGf.Vec3f -10 0 10), (pyGf.Vec3f 10 0 10), (pyGf.Vec3f 0 20 0))
        local vertexCount = #(3,3,3,3,4)
        local vertexIndices = #(0, 1, 4, 0, 2, 4, 3, 1, 4, 3, 2, 4, 0, 1, 3, 2)
        prim001.CreatePointsAttr(points)
        prim001.CreateFaceVertexCountsAttr(vertexCount)
        prim001.CreateFaceVertexIndicesAttr(vertexIndices)
        (stage.GetRootLayer()).Save()
    ),
    
    function test_import_y_up_axis = (
        local importPath = output_prefix + "import_y_up_axis_test.usda"
        -- Populate usd scene file
        create_usd_pyramid_file importPath pyUsdGeom.Tokens.y
            
        -- Import file
        USDImporter.ImportFile importPath
        
        -- Validate everything is where it should
        assert_point3_equal [-10, 10, 0] $prim001.vertices[1].pos
        assert_point3_equal [10, 10, 0] $prim001.vertices[2].pos
        assert_point3_equal [-10, -10, 0] $prim001.vertices[3].pos
        assert_point3_equal [10, -10, 0] $prim001.vertices[4].pos
        assert_point3_equal [0, 0, 20] $prim001.vertices[5].pos
    ),

    function test_import_z_up_axis = (
        local importPath = output_prefix + "import_z_up_axis_test.usda"
        -- Populate usd scene file
        create_usd_pyramid_file importPath pyUsdGeom.Tokens.z
        
        -- Import file
        USDImporter.ImportFile importPath
        
        -- Validate everything is where it should
        assert_point3_equal [-10, 0, -10] $prim001.vertices[1].pos
        assert_point3_equal [10, 0, -10] $prim001.vertices[2].pos
        assert_point3_equal [-10, 0, 10] $prim001.vertices[3].pos
        assert_point3_equal [10, 0, 10] $prim001.vertices[4].pos
        assert_point3_equal [0, 20, 0] $prim001.vertices[5].pos
    ),

    function teardown = (
        units.SystemType = initialUnits
        units.SystemScale = initialScale
    ),

    tests = #(test_import_y_up_axis, test_import_z_up_axis)
)

runUsdTestFixture import_up_axis_test (getThisScriptFilename())