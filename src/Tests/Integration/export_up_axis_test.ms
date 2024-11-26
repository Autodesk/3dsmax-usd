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

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_UP_AXIS_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_up_axis_test
(
    private 
    exportOptions = USDExporter.CreateOptions(),
    
    function resetMaxFileAndSetupUnits = 
    (
        resetmaxfile #noprompt
    ),
    
    public
    function setup =
    (
        resetMaxFileAndSetupUnits()

        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
    ),

    function test_export_y_up_axis = (
        -- Populate the scene
        Box pos:[10,10,10] width:10 length:100 height:1000 name:"Box"
        Box pos:[10,10,10] width:10 length:100 height:1000 name:"ChildBox"
        $ChildBox.parent = $Box
        
        -- Set Export options
        exportOptions.UpAxis = #y
        
        local exportPath = output_prefix + "export_test_y_up_axis.usda"

        -- Export to USD
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        local exportedUpAxis = pyUsdGeom.GetStageUpAxis(stage)

        assert_true(exportedUpAxis == pyUsdGeom.Tokens.y) message: "The wrong upAxis value was exported."
        
        local exportedBoxPrim = stage.GetPrimAtPath("/Box")
        local exportedPrimXformable = pyUsdGeom.Xformable(exportedBoxPrim)
        local exportedTransform = exportedPrimXformable.GetLocalTransformation()
       
        local expectedTransform = (pyGf.Matrix4d 1 0 0 0 0 0 -1 0 0 1 0 0 10 10 -10 1)
        assert_true(compare2dMatrix exportedTransform expectedTransform 4 4)
        
        exportedBoxPrim = stage.GetPrimAtPath("/Box/ChildBox")
        exportedPrimXformable = pyUsdGeom.Xformable(exportedBoxPrim)
        exportedTransform = exportedPrimXformable.GetLocalTransformation()
        expectedTransform = (pyGf.Matrix4d 1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1)
        assert_true(compare2dMatrix exportedTransform expectedTransform 4 4)
    ),

    function test_export_z_up_axis = (
        -- Populate the scene
        Box pos:[10,10,10] width:10 length:100 height:1000 name:"Box"
        
        -- Set Export options
        exportOptions.UpAxis = #z
        
        local exportPath = output_prefix + "export_test_z_up_axis.usda"

        -- Export to USD
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        local stage = pyUsd.Stage.Open(exportPath)
        
        local exportedUpAxis = pyUsdGeom.GetStageUpAxis(stage)
        assert_true(exportedUpAxis == pyUsdGeom.Tokens.z) message: "The wrong upAxis value was exported."
        
        local exportedBoxPrim = stage.GetPrimAtPath("/Box")
        local exportedPrimXformable = pyUsdGeom.Xformable(exportedBoxPrim)
        local exportedTransform = exportedPrimXformable.GetLocalTransformation()
        local expectedTransform = (pyGf.Matrix4d 1 0 0 0 0 1 0 0 0 0 1 0 10 10 10 1)
        assert_true(compare2dMatrix exportedTransform expectedTransform 4 4)
    ),

    function teardown = (
    ),

    tests = #(test_export_y_up_axis, 
        test_export_z_up_axis)
)

runUsdTestFixture export_up_axis_test (getThisScriptFilename())