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
    
output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_NODE_NAME_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_node_name_test
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
    
    function test_node_name_conflict = (
        -- Simple node name conflict.
        local box1 = box()
        box1.name = "name"
        local box2 = box()
        box2.name = "name"
        local exportPath = (output_prefix + "simple_conflict.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        
        box1Prim = stage.GetPrimAtPath("/name")
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"box1 was not exported to the expected path"
        box2Prim = stage.GetPrimAtPath("/name1") -- was renamed because of the name conflict
        assert_true (pyUsd.Prim.IsValid(box2Prim)) message:"box2 was not exported to the expected path"    
            
        -- Name conflict where the next generated name would also conflict.
        local dummyNode = dummy()
        dummyNode.name = "name1" -- box2 should not "steal" this name when being renamed.
        
        local exportPath = (output_prefix + "chain_conflict.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
            
        box1Prim = stage.GetPrimAtPath("/name")
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"box1 was not exported to the expected path"
        xformPrim = stage.GetPrimAtPath("/name1")
        assert_true (pyUsd.Prim.IsValid(xformPrim)) message:"dummyNode was not exported to the expected path."
        assert_true (xformPrim.IsA(pyUsdGeom.Xform)) message:("dummyNode should have been exported to an Xform prim")
        box2prim = stage.GetPrimAtPath("/name2")
        assert_true (pyUsd.Prim.IsValid(box2prim)) message:"box2 was not exported to the expected path."
        assert_true (box2prim.IsA(pyUsdGeom.Mesh)) message:("box2 should have been exported to an Mesh prim")

        -- Tests that reusing the same name at different levels in the hierarchy is supported.
        resetMaxFile #noprompt
        local level1 = box()
        level1.name = "name"
        local level2 = box()
        level2.name = "name"
        level2.parent = level1
        local level3 = box()
        level3.name = "name"
        level3.parent = level2

        local exportPath = (output_prefix + "reuse_across_hierarchy.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        level3Prim = stage.GetPrimAtPath("/name/name/name") 
        assert_true (pyUsd.Prim.IsValid(level3Prim)) message:"level3 was not exported to the expected path"
    ),

    function test_invalid_usd_node_name = (
        -- Makes sure that Max node names that are not valid in USD are transformed to be valid upon export.
        local box1 = box()
        box1.name = "box:1"
        local exportPath = (output_prefix + "simple_conflict.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        stage = pyUsd.Stage.Open(exportPath)
        box1Prim = stage.GetPrimAtPath("/box_1")
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"box1 was not exported to the expected path"
    ),
        
    function teardown = (
    ),
    Tests = #(test_node_name_conflict, test_invalid_usd_node_name)
)

runUsdTestFixture export_node_name_test (getThisScriptFilename())