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

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_SELECTED_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_selected_test
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
    
    function test_export_selected = (
        /* Build a hierarchy to test various cases with : 

                           |-- box3  |-- box5A    
               |-- box2A --|         |
        box1 --|           |-- box4--|-- box5B
               |-- box2B             |
                                     |-- box5C         
        */

        box1 = box()
        box1.name = "box1"
        box2A = box()
        box2A.name = "box2A"
        box2A.parent = box1
        box2B = box()
        box2B.name = "box2B"
        box2B.parent = box1
        box3 = box()
        box3.name = "box3"
        box3.parent = box2A
        box4 = box()
        box4.name = "box4"
        box4.parent = box2A
        box5A = box()
        box5A.name = "box5A"
        box5A.parent = box4
        box5B = box()
        box5B.name = "box5B"
        box5B.parent = box4
        box5C = box()
        box5C.name = "box5C"
        box5C.parent = box4
        
        -- Test that the children of selected nodes are not exported.
        select box1
        local exportPath = (output_prefix + "children_not_exported.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected

        stage = pyUsd.Stage.Open(exportPath)
        
        box1Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box1.name)
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"box1 was not properly exported."
        local bindingAPI = pyShade.MaterialBindingAPI(box1Prim)
        mtlSubsets = bindingAPI.GetMaterialBindSubsets()
        numberOfChildren = mtlSubsets.count - (box1Prim.GetAllChildren()).count
        assert_equal numberOfChildren 0 message:"The children of the selected node should not have been exported."
        clearSelection()

        -- Test that selected nodes that do not have their parent selected are exported at the root level.
        select box2A
        exportPath = (output_prefix + "parent_not_selected.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected
        
        stage = pyUsd.Stage.Open(exportPath)
        box2APrim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box2A.name)
        assert_true (pyUsd.Prim.IsValid(box2APrim)) message:"box2A was not properly exported to the expected path."
        clearSelection()

        -- Test complex multi-selection.
        selectmore  box1 -- Should be exported at /box1
        selectmore  box4 -- Should be exported at /box4 because /Box2A is not selected
        selectmore  box5A -- Should be exported at /box4/box5A
        selectmore  box5C -- Should be exported at /box4/box5C

        exportPath = (output_prefix + "multiselection.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected
        stage = pyUsd.Stage.Open(exportPath)
        
        box1Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box1.name)
        assert_true (pyUsd.Prim.IsValid(box1Prim)) message:"box1 was not properly exported to the expected path."
        
        box4Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box4.name)
        assert_true (pyUsd.Prim.IsValid(box4Prim)) message:"box4 was not properly exported to the expected path."
        
        box5APrim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box4.name + "/" + box5A.name)
        assert_true (pyUsd.Prim.IsValid(box5APrim)) message:"box5A was not properly exported to the expected path."

        -- Make sure the unselected child was not exported 
        box5BPrim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box4.name + "/" + box5B.name)
        assert_false (pyUsd.Prim.IsValid(box5BPrim)) message:"box5B was not selected and should not have been exported."
        
        box5CPrim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + box4.name + "/" + box5C.name)
        assert_true (pyUsd.Prim.IsValid(box5CPrim)) message:"box5C was not properly exported to the expected path."
    ),
    
    function test_name_conflicts = (
        /*
        By design, when exporting selected nodes only, nodes that have their parents unselected will export at the 
        root level in the USD file. Therefor, node names that would not conflict when going to USD if the whole scene was 
        exported will now conflict. In the example bellow, both "boxA" nodes will attempt to export at the USD path /boxA
        if box1 and box2 are not selected.
        
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
        
        selectmore  box1 
        selectmore  box2
        selectmore  boxA1
        selectmore  boxA2
        exportPath = (output_prefix + "no_name_conflict.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected
        
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
        deselect box1 
        deselect box2
        selectmore boxA1
        selectmore boxA2
        exportPath = (output_prefix + "name_conflict.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected
        
        stage = pyUsd.Stage.Open(exportPath)
        boxA1Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + boxA1.name)
        assert_true (pyUsd.Prim.IsValid(boxA1Prim)) message:"boxA1 was not properly exported to the expected path."
        boxA2Prim = stage.GetPrimAtPath(exportOptions.RootPrimPath + "/" + boxA2.name + "1") -- renamed because of conflict at the root level.
        assert_true (pyUsd.Prim.IsValid(boxA2Prim)) message:"boxA2 was not properly exported to the expected path."
    ),
    
    function teardown = (
    ),

    Tests = #(test_export_selected, test_name_conflicts)
)

runUsdTestFixture export_selected_test (getThisScriptFilename())