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

path_prefix = pathConfig.appendPath (getDir #temp) "/IO_POINT_HELPERS_TEST_"
deleteTestFiles(path_prefix + "*.*")

struct io_point_helpers_test
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

    function test_basic_io_point_helpers = (
        -- Make sure Point Helpers are exported as Xform
        Point()

        -- Groups are dummies that will be exported as Xform
        Box()
        Sphere()
        DataPair "Group" (group #($Box001, $Sphere001))

        -- Export 3ds max scene
        local stagePath = path_prefix + "basic_io_point_helpers_test.usda"
        USDExporter.ExportFile stagePath exportOptions:exportOptions

        -- Make sure everything was exported properly
        local stage = pyUsd.Stage.Open(stagePath)

        -- Validate the export of Point Helpers 
        local point1Prim = stage.GetPrimAtPath("/Point001")
        assert_true (pyUsd.Prim.IsValid(point1Prim)) message:"Point001 was not properly exported."
        assert_true (point1Prim.IsA(pyUsdGeom.Xform)) message:("Wrong prim type for Point001, it should have been a Xform.")

        -- Validate the export of groups
        local group1Prim = stage.GetPrimAtPath("/Group001")
        assert_true (pyUsd.Prim.IsValid(group1Prim)) message:"Group001 was not properly exported."
        assert_true (group1Prim.IsA(pyUsdGeom.Xform)) message:("Wrong prim type for Group001, it should have been a Xform. ")
        local box1Prim = stage.GetPrimAtPath("/Group001/Box001")
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"Box001 was not properly exported."
        assert_true (box1Prim.IsA(pyUsdGeom.Mesh)) message:("Wrong prim type for Box001, it should have been a Mesh.")
        local sphere1Prim = stage.GetPrimAtPath("/Group001/Sphere001")
        assert_true (pyUsd.Prim.IsValid(sphere1Prim)) message:"Sphere001 was not properly exported."
        assert_true (sphere1Prim.IsA(pyUsdGeom.Mesh)) message:("Wrong prim type for Sphere001, it should have been a Mesh.")
        
        -- Clean 3ds max scene and reimport USD stage
        resetmaxfile #noprompt
        USDImporter.ImportFile stagePath

        -- Make sure Xform were imported as Point Helpers
        assert_true (classOf $Point001 == Point) message:"Point001 was not properly imported."
        assert_true (classOf $Group001 == Point) message:"Group001 was not properly imported."

        -- Their cross should be invisible
        assert_false ($Point001.cross) message:"Point001's cross should be invisible (set to false) by default on import."
        assert_false ($Group001.cross) message:"Group001's cross should be invisible (set to false) by default on import."
    ),

    function teardown = (
    ),

    Tests = #(test_basic_io_point_helpers)
)

runUsdTestFixture io_point_helpers_test (getThisScriptFilename())