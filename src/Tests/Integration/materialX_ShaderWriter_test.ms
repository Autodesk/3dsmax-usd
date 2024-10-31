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
pyGf = python.import("pxr.Gf")

output_prefix = pathConfig.appendPath (getDir #temp) "/MTLX_SHADER_WRITER_TEST_"
deleteTestFiles(output_prefix + "*.*")
deleteTestFiles(output_prefix + "\\materials\\AnotherSubFolder\\*.*")
deleteTestFiles(output_prefix + "\\models\\*.*")

struct materialX_shader_writer_test
( 
    function setup =
    (
        resetmaxfile #noprompt
    ),
    
    function validateUsdMtlx stage = (
        
        local mtlxMatPrim = stage.GetPrimAtPath("/root/mtl/matTeapot001")
        assert_true (mtlxMatPrim.IsValid())
        assert_true (mtlxMatPrim.IsA(pyShade.Material))
        local mtlxShaderPrim = stage.GetPrimAtPath("/root/mtl/matTeapot001/Materials/Iberian_Blue_Ceramic_Tiles/ND_standard_surface_surfaceshader")
        assert_true (mtlxShaderPrim.IsValid())
        assert_true (mtlxShaderPrim.IsA(pyShade.Shader))
    ),
    
    --- Validate that the relative path added to the reference the MaterialX document is valid 
    --- when exporting to a separate material layer not at the same location than the main layer.
    --- Using the Custom Attributes
    function test_export_material_separate_file_different_depth_CA = (

        local teapot001 = teapot name:"teapot001"

        local mtl1 = MaterialXMaterial()
        mtl1.name = "matTeapot001"
        local scriptDir = getFilenamePath (getThisScriptFilename())
        local mtlxDoc = (scriptDir + @"data\Iberian_Blue_Ceramic_Tiles_1k_8b\Iberian_Blue_Ceramic_Tiles.mtlx")
        mtl1.importMaterial mtlxDoc
        teapot001.material = mtl1

        select $*
        macros.run "USD" "AddUsdCustomAttrToObjectSelection"
        custAttrFileName = output_prefix + "\\materials\\AnotherSubFolder\\testExportMaterialSeparateFileMaterial.usda"
        mtl1.UsdMaterialAttributeHolder.pathMethod = 2
        mtl1.UsdMaterialAttributeHolder.separateLayer = true
        mtl1.UsdMaterialAttributeHolder.filePath = custAttrFileName
        mtl1.UsdMaterialAttributeHolder.primPath = "mtl"

        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.AllMaterialTargets = #("MaterialX")

        local exportPath = output_prefix + "\\models\\testExportMaterialSeparateFileModel.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
        validateUsdMtlx stage
    ),
    
    --- Validate that the relative path added to the reference the MaterialX document is valid 
    --- when exporting to a separate layer
    --- Using the Custom Attributes
    function test_export_material_separate_file_CA = (

        local teapot001 = teapot name:"teapot001"

        local mtl1 = MaterialXMaterial()
        mtl1.name = "matTeapot001"
        local scriptDir = getFilenamePath (getThisScriptFilename())
        local mtlxDoc = (scriptDir + @"data\Iberian_Blue_Ceramic_Tiles_1k_8b\Iberian_Blue_Ceramic_Tiles.mtlx")
        mtl1.importMaterial mtlxDoc
        teapot001.material = mtl1

        select $*
        macros.run "USD" "AddUsdCustomAttrToObjectSelection"
        custAttrFileName = output_prefix + "testExportMaterialSeparateFileMaterial.usda"
        mtl1.UsdMaterialAttributeHolder.pathMethod = 2
        mtl1.UsdMaterialAttributeHolder.separateLayer = true
        mtl1.UsdMaterialAttributeHolder.filePath = custAttrFileName
        mtl1.UsdMaterialAttributeHolder.primPath = "mtl"

        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.AllMaterialTargets = #("MaterialX")

        local exportPath = output_prefix + "testExportMaterialSeparateFileModel.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
            
        validateUsdMtlx stage
    ),
    
    --- Export a materialX material to the main layer
    function test_export_material = (
        
        local teapot001 = teapot name:"teapot001"

        local mtl1 = MaterialXMaterial()
        mtl1.name = "matTeapot001"
        local scriptDir = getFilenamePath (getThisScriptFilename())
        local mtlxDoc = (scriptDir + @"data\Iberian_Blue_Ceramic_Tiles_1k_8b\Iberian_Blue_Ceramic_Tiles.mtlx")
        mtl1.importMaterial mtlxDoc
        teapot001.material = mtl1

        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.AllMaterialTargets = #("MaterialX")

        local exportPath = output_prefix + "testExportMaterial.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
            
        validateUsdMtlx stage
    ),
    
    -- Export a materialX material to the separate material layer
    function test_export_material_separate_file = (
        
        local teapot001 = teapot name:"teapot001"

        local mtl1 = MaterialXMaterial()
        mtl1.name = "matTeapot001"
        local scriptDir = getFilenamePath (getThisScriptFilename())
        local mtlxDoc = (scriptDir + @"data\Iberian_Blue_Ceramic_Tiles_1k_8b\Iberian_Blue_Ceramic_Tiles.mtlx")
        mtl1.importMaterial mtlxDoc
        teapot001.material = mtl1

        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii
        exportOptions.AllMaterialTargets = #("MaterialX")
        exportOptions.UseSeparateMaterialLayer = true
        
        local exportPath = output_prefix + "testExportMaterialSeparateFile.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)
            
        validateUsdMtlx stage
    ),

    function teardown = (
    ),

    Tests = #(
        test_export_material_separate_file_CA,
        test_export_material_separate_file_different_depth_CA,
        test_export_material,
        test_export_material_separate_file
        )
)

runUsdTestFixture materialX_shader_writer_test (getThisScriptFilename())