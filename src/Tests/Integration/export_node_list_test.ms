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
pyShade = python.import("pxr.UsdShade")

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_NODE_LIST_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_node_list_test
(  
    private 
    exportOptions = USDExporter.CreateOptions(),
    
    public
    function setup =
    (
        resetmaxfile #noprompt
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/foo"
        exportOptions.AllowNestedGprims = true
    ),
    
    function test_export_node_list = (
        /* Build a hierarchy to test various cases with : 

                           |-- box3A  |-- box4A    
               |-- box2A --|          |
        box1 --|           |-- box3B--|-- box4B
               |-- box2B              |
                                      |-- box4C         
        */

        box1 = box name:"box1"
        box2A = box name:"box2A" parent:box1
        box2B = box name:"box2B" parent:box1
        box3A = box name:"box3A" parent:box2A
        box3B = box name:"box3B" parent:box2A
        box4A = box name:"box4A" parent:box3B
        box4B = box name:"box4B" parent:box3B
        box4C = box name:"box4C" parent:box3B

        -- test nodeList argument option. When a valid non-empty list is provided to 'nodeList', contentSource:#nodeList must be set
        local nodeToExport = #(box1, box2A)
        local exportPath = (output_prefix + "wrong_args_nodelist_not_exported.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions nodeList:nodeToExport
        assert_equal 0 (getfiles exportPath).count message:"Stage should not have been created when providing a node list but no contentSource:#nodeList argument"

        -- test contentSource argument option. When contentSource:#nodeList a list must be valid and not empty
        nodeToExport = #()
        exportPath = (output_prefix + "wrong_args_contentsource_not_exported.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList  nodeList:nodeToExport
        assert_equal 0 (getfiles exportPath).count message:"Stage should not have been created when providing contentSource:#nodeList, but not valid/non-empty nodeList"

        -- Test that the children of nodes in the list of nodes to export are not exported.
        nodeToExport = #(box1)
        exportPath = (output_prefix + "children_not_exported.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:nodeToExport

        stage = pyUsd.Stage.Open(exportPath)
        
        box1Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box1.name)
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"box1 was not properly exported."
        local bindingAPI = pyShade.MaterialBindingAPI(box1Prim)
        mtlSubsets = bindingAPI.GetMaterialBindSubsets()
        numberOfChildren = mtlSubsets.count - (box1Prim.GetAllChildren()).count
        assert_equal numberOfChildren 0 message:"The children of the node box1 should not have been exported."

        -- Test that nodes in the list of nodes to export that do not have their parent in that list too are exported at the root level.
        nodeToExport = #(box2A)
        exportPath = (output_prefix + "parent_not_in_node_list.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:nodeToExport
        
        stage = pyUsd.Stage.Open(exportPath)
        box2APrim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box2A.name)
        assert_true (pyUsd.Prim.IsValid(box2APrim)) message:"box2A was not properly exported to the expected path."

        -- Test complex list of nodes to export.
        nodeToExport = #(box1,-- Should be exported at /box1
                        box3B, -- Should be exported at /box3B because /Box2A is not in the list of nodes to export
                        box4A, -- Should be exported at /box3B/box4A
                        box4C) -- Should be exported at /box3B/box4C

        exportPath = (output_prefix + "multiselection.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:nodeToExport
        stage = pyUsd.Stage.Open(exportPath)
        
        box1Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box1.name)
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"box1 was not properly exported to the expected path."
        
        box3BPrim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box3B.name)
        assert_true (pyUsd.Prim.IsValid(box3BPrim)) message:"box3B was not properly exported to the expected path."
        
        box4APrim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box3B.name + "/" + box4A.name)
        assert_true (pyUsd.Prim.IsValid(box4APrim)) message:"box4A was not properly exported to the expected path."

        -- Make sure the child not in the list of node to export was not exported 
        box4BPrim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box3B.name + "/" + box4B.name)
        assert_false (pyUsd.Prim.IsValid(box4BPrim)) message:"box4B was not added to the list of nodes to export and should not have been exported."
        
        box4CPrim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box3B.name + "/" + box4C.name)
        assert_true (pyUsd.Prim.IsValid(box4CPrim)) message:"box4C was not properly exported to the expected path."
    ),
    
    function test_name_conflicts = (
        /*
        By design, when exporting a node list, nodes that have their parents not part of the list will export at the 
        root level in the USD file. Therefor, node names that would not conflict when going to USD if the whole scene was 
        exported will now conflict. In the example bellow, both "boxA" nodes will attempt to export at the USD path /boxA
        if box1 and box2 are not part of the export list.
        
               |-- box1 --|--boxA
        root --|           
               |-- box2 --|--boxA
        
        This test makes sure that this conflict is handled.
        */
        
        -- Test no name conflict.
        box1 = box()
        box1.name = "box1"
        box2 = box()
        box2.name = "box2"
        boxA1 = box()
        boxA1.name = "boxA"
        boxA1.parent = box1
        boxA2 = box()
        boxA2.name = "boxA"
        boxA2.parent = box2
        
        exportPath = (output_prefix + "no_name_conflict.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:#(box1,box2,boxA1,boxA2)
        
        stage = pyUsd.Stage.Open(exportPath)
        
        box1Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box1.name)
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"box1 was not properly exported to the expected path."
        box2Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box2.name)
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"box1 was not properly exported to the expected path."
        boxA1Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box1.name + "/" + boxA1.name)
        assert_true (pyUsd.Prim.IsValid(boxA1Prim)) message:"boxA1 was not properly exported to the expected path."
        boxA2Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box2.name + "/" + boxA2.name)
        assert_true (pyUsd.Prim.IsValid(boxA2Prim)) message:"boxA2 was not properly exported to the expected path."
        
        -- Test with name conflict.
        exportPath = (output_prefix + "name_conflict.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#nodeList nodeList:#(boxA1,boxA2)
        
        stage = pyUsd.Stage.Open(exportPath)
        boxA1Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + boxA1.name)
        assert_true (pyUsd.Prim.IsValid(boxA1Prim)) message:"boxA1 was not properly exported to the expected path."
        boxA2Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + boxA2.name + "1") -- renamed because of conflict at the root level.
        assert_true (pyUsd.Prim.IsValid(boxA2Prim)) message:"boxA2 was not properly exported to the expected path."
    ),    
        
    function teardown = (
    ),

    Tests = #(test_export_node_list , test_name_conflicts)
)

runUsdTestFixture export_node_list_test (getThisScriptFilename())