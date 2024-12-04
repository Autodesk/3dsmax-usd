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

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_DEFAULT_PRIM_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_default_prim_test
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

    function test_export_default_prim = (
        boxA = box()
        boxA.name = "boxA"
        boxB = box()
        boxB.name = "boxB"

        -- Test default prim is set on export selected.
        select #(boxA, boxB)
        local exportPath = (output_prefix + "export_default_prim.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#selected
        stage = pyUsd.Stage.Open(exportPath)

        defaultPrim = stage.GetDefaultPrim()
        assert_true (pyUsd.Prim.IsValid(defaultPrim)) message:"Default prim not valid."

        defaultPrimAtPath = stage.GetPrimAtPath("/" + boxA.name)
        assert_true (defaultPrimAtPath.GetPath() as string == defaultPrim.GetPath() as string) message:"Expected default prim path not correct."

        -- Test default prim is set on export scene.
        local exportPath = (output_prefix + "export_scene_2_boxes.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions contentSource:#all
        stage = pyUsd.Stage.Open(exportPath)

        defaultPrim = stage.GetDefaultPrim()
        assert_true (pyUsd.Prim.IsValid(defaultPrim)) message:"Default prim not valid."
        defaultPrimAtPath = stage.GetPrimAtPath("/" + boxA.name)
        assert_true (defaultPrimAtPath.GetPath() as string == defaultPrim.GetPath() as string) message:"Expected default prim path not correct."
    ),
    
    -- Making sure that prototype (classes) are not considered as default prims. Only 
    -- consider prims that originate directly from a Max node.
    function test_default_prim_instance = (
        -- Create an instances.
        local originalBox = Box name:"originalBox" pos:[0,0,0] width:10 length:10 height:10
        local instanceBox = create_clone originalBox #instance "instanceBox"
        -- When exporting the scene, the first prim will be the class prim that the instances will inherit from.
        local exportPath = output_prefix + "test_prototype_is_not_default.usda"
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
		
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        local defaultPrim = stage.GetDefaultPrim()
        local primPath = defaultPrim.GetPath() as string
        assert_equal "/originalBox" primPath message:"Prototype prims should not be selected. /originalBox should have been selected as default."
    ),
    
    -- When a root prim path is specified, make sure that the top-most ancestor prim of that path is used as default prim.
    function test_specifed_root_default_prim = (
        local box1 = Box name:"Box1"
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.RootPrimPath = "/foo/bar/baz"
        exportOptions.FileFormat = #ascii
        local exportPath = output_prefix + "test_specified_root_is_default_prim.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        -- Make sure the correct prim was selected as default.
        stage = pyUsd.Stage.Open(exportPath)
        defaultPrim = stage.GetDefaultPrim()
        assert_equal "/foo" ((defaultPrim.GetPath()) as string) message:"Incorrect prim selected as default when specifiying a root prim"
    ),
    
    function teardown = (
    ),

    Tests = #(test_export_default_prim, 
        test_default_prim_instance,
        test_specifed_root_default_prim)
)

runUsdTestFixture export_default_prim_test (getThisScriptFilename())