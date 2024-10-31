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
pyUsdTf = python.import("pxr.Tf")
pyShade = python.import("pxr.UsdShade")
pysdf = python.import("pxr.Sdf")

struct import_export_non_renderable_test
(  
    private 
    output_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_EXPORT_NON_RENDERABLE_TESTS_",
    input_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_EXPORT_NON_RENDERABLE_TESTS_",
    exportOptions = USDExporter.CreateOptions(),
    
    public 
    function setup =
    (
        deleteTestFiles(output_prefix + "*.*")
        resetmaxfile #noprompt
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.UsdStagesAsReferences = false
    ),

    -- Test that non-renderable mesh exports as guide purpose
    function export_non_renderable_mesh = (
        -- given
        myBox = Box name:"Box001"
        myBox.renderable = false

        -- when
        local exportPath = output_prefix + "export_non_renderable_mesh.usd"
        USDExporter.ExportFile exportPath exportOptions:exportOptions 	

        -- then
        local stage = pyUsd.Stage.Open(exportPath)
        local boxPrim = stage.GetPrimAtPath("/Box001/Box001_Shape")
        local mesh = pyUsdGeom.Mesh(boxPrim)
        assert_equal "guide" ((mesh.GetPurposeAttr()).Get())  
    ),
	
    --Test that mesh set to guide in USD is set to non-renderable when imported to Max
    function import_geometry_with_guide_purpose = (
        -- given
        local importOptions = USDImporter.CreateOptions()
        local filepath = input_prefix + "test_import_geometry_with_guide_purpose.usd"
        local stage = pyUsd.Stage.CreateNew(filepath)
        local prim001 = pyUsdGeom.Mesh.Define stage "/Box001"
        local purposeAttr = (pyUsdGeom.Imageable(prim001)).CreatePurposeAttr()
        purposeAttr.Set(pyUsdGeom.Tokens.guide)
        (stage.GetRootLayer()).Save()

        -- when
        USDImporter.ImportFile filepath importOptions:importOptions

        -- then
        assert_equal false $Box001.renderable
    ),

    function teardown = (
    ),

    Tests = #(export_non_renderable_mesh,
        import_geometry_with_guide_purpose
    )
)

runUsdTestFixture import_export_non_renderable_test (getThisScriptFilename())