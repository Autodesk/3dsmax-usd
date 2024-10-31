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

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_BONE_GUIDES_TEST_"
deleteTestFiles(output_prefix + "*.*") 

struct export_bones_as_guides_test
(  
    private 
    exportOptions = USDExporter.CreateOptions(),
    
    public
    function setup = (
        resetmaxfile #noprompt
        exportOptions.FileFormat = #ascii
    ),
    
    function test_export_bone_as_guides = (
        -- Create a hierarchy including some bones...
        --   bone1
        --    - box1 
        --     - bone2 
        bone1 = BoneSys.createBone [0,0,0] [0,0,1] [0,0,1]
        bone1.Name = "bone1"
        box1 = Box name:"box1"
        box1.parent = bone1
        bone2 = BoneSys.createBone [0,0,1] [0,0,2] [0,0,1]
        bone2.Name = "bone2"
        bone2.parent = box1
        
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Skin = false
                
        exportPath = output_prefix + "export_bone_as_guides.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        
        -- Bones should be set as purpose=guide in USD.
        -- Because purposes in USD are inherited, we need to split the bone nodes
        -- into an Xform and UsdGeomMesh, the UsdGeomMesh is the prim which will have the guide purpose.
        -- Any child of the node will be under the Xform, and so the purpose will not be inherited.
        
        -- bone1, should find an xform prim and a shape, the shape should be set as a guide.
        local bone1prim = stage.GetPrimAtPath("/bone1")
        local bone1shapePrim = stage.GetPrimAtPath("/bone1/bone1_Shape")
        assert_true (pyUsd.Prim.IsValid(bone1prim)) message:"bone1 was not exported to the expected prim path."
        assert_true (bone1prim.IsA(pyUsdGeom.Xform)) message:"bone1 should have been exported to a Xform prim."
        assert_true (pyUsd.Prim.IsValid(bone1shapePrim)) message:"bone1 should have an associated shape prim carrying its geometry."
        assert_true (bone1shapePrim.IsA(pyUsdGeom.Mesh)) message:"bone1_shape should be an UsdGeomMesh prim."
        
        local bone1imageable = pyUsdGeom.Imageable(bone1prim)
        assert_equal "default" (bone1imageable.ComputePurpose()) message:"bone1's xform prim should have the default purpose."
        local bone1shapeImageable = pyUsdGeom.Imageable(bone1shapePrim)
        assert_equal "guide" (bone1shapeImageable.ComputePurpose()) message:"bone1's shape prim should have guide purpose."
        
        -- box1 should not be a guide.
        local box1prim = stage.GetPrimAtPath("/bone1/box1")
        assert_true (pyUsd.Prim.IsValid(box1prim)) message:"box1 was not exported to the expected prim path."
        local boxImageable = pyUsdGeom.Imageable(box1prim)
        assert_equal "default" (boxImageable.ComputePurpose()) message:"box1 one should have the default purpose."
            
        -- bone2, should find an xform prim and a shape, the shape should be set as a guide.            
        local bone2prim = stage.GetPrimAtPath("/bone1/box1/bone2")
        local bone2shapePrim = stage.GetPrimAtPath("/bone1/box1/bone2/bone2_Shape")
        assert_true (pyUsd.Prim.IsValid(bone2prim)) message:"bone2 was not exported to the expected prim path."
        assert_true (bone1prim.IsA(pyUsdGeom.Xform)) message:"bone2 should have been exported to a Xform prim."
        assert_true (pyUsd.Prim.IsValid(bone2shapePrim)) message:"bone2 should have an associated shape prim carrying its geometry."
        assert_true (bone1shapePrim.IsA(pyUsdGeom.Mesh)) message:"bone2_shape should be an UsdGeomMesh prim."
        
        local bone2imageable = pyUsdGeom.Imageable(bone2prim)
        assert_equal "default" (bone2imageable.ComputePurpose()) message:"bone2's xform prim should have the default purpose."
        local bone2shapeImageable = pyUsdGeom.Imageable(bone2shapePrim)
        assert_equal "guide" (bone2shapeImageable.ComputePurpose()) message:"bone2's shape prim should have the guide purpose."
    ),
    
    function teardown = (
    ),

    Tests = #(test_export_bone_as_guides)
)

runUsdTestFixture export_bones_as_guides_test (getThisScriptFilename())