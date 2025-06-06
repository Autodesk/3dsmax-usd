--
-- Copyright 2025 Autodesk
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

output_prefix = pathConfig.appendPath (getDir #temp) "/OpenPBR_TO_USDPS_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct openPBR_to_usdps_test
( 
    function setup =
    (
        quickReset()
    ),

    function test_export_OpenPBR_material = (
        maxver = maxVersion()
        -- Don't test version before 2026
        -- OpenPBR is only 2025.3 and above
        if maxver[1] < 27900 then (
            return 0
        )

        local scriptDir = getFilenamePath (getThisScriptFilename())
        local uvCheckerPath = (scriptDir + @"data\uv_checker.png")

        local teapot001 = teapot name:"teapot001"
        local mtl1 = OpenPBR()
        mtl1.name = "matTeapot001"
        checker = BitmapTexture()
        checker.filename = uvCheckerPath
        mtl1.base_color_map = checker

        teapot001.material = mtl1
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.FileFormat = #ascii

        local exportPath = output_prefix + "testOpenPBRMaterial.usda"
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        local stage = pyUsd.Stage.Open(exportPath)

        -- Validate Material Prim
        local mtlPrim = stage.GetPrimAtPath("/root/mtl/matTeapot001")
        assert_true (mtlPrim.IsValid())
        assert_true (mtlPrim.IsA(pyShade.Material))

        -- Validate Shader Prim
        local shaderPrim = stage.GetPrimAtPath("/root/mtl/matTeapot001/matTeapot001")
        assert_true (shaderPrim.IsValid())
        assert_true (shaderPrim.IsA(pyShade.Shader))
        local usdShader = pyShade.Shader(shaderPrim)
        assert_equal ((shaderPrim.GetAttribute "info:id").Get()) "UsdPreviewSurface"
        assert_equal ((((usdShader.GetInput "diffuseColor").GetConnectedSource())[1].GetPath())) (pysdf.Path "/root/mtl/matTeapot001/Bitmaptexture")
        assert_equal ((usdShader.GetInput "metallic").Get()) 0
        assert_equal ((usdShader.GetInput "roughness").Get()) 0
        assert_equal ((usdShader.GetInput "specularColor").Get()) (pyGf.Vec3f 1 1 1)

        -- Validate NodeGraph Prim
        local nodeGraphPrim = stage.GetPrimAtPath("/root/mtl/matTeapot001/Bitmaptexture")
        assert_true (nodeGraphPrim.IsValid())
        assert_true (nodeGraphPrim.IsA(pyShade.NodeGraph))
		local nodeGraph = pyShade.NodeGraph(nodeGraphPrim)
        assert_equal ((nodeGraphPrim.GetAttribute "inputs:frame:st").Get()) "st"
        assert_equal ((((nodeGraph.GetOutput "rgb").GetConnectedSource())[1].GetPath())) (pysdf.Path "/root/mtl/matTeapot001/Bitmaptexture/Bitmaptexture")

        -- Validate BitmapTexture Shader
        local bitmapShaderPrim = stage.GetPrimAtPath("/root/mtl/Bitmaptexture/Bitmaptexture")
        assert_true (bitmapShaderPrim.IsValid())
        assert_true (bitmapShaderPrim.IsA(pyShade.Shader))
        local bitmapShader = pyShade.Shader(bitmapShaderPrim)
        assert_equal ((bitmapShaderPrim.GetAttribute "info:id").Get()) "UsdUVTexture"
        assert_equal ((((bitmapShader.GetInput "st").GetConnectedSource())[1].GetPath())) (pysdf.Path "/root/mtl/Bitmaptexture/PrimvarReader_st")
        assert_equal ((bitmapShader.GetInput "wrapS").Get()) "repeat"
        assert_equal ((bitmapShader.GetInput "wrapT").Get()) "repeat"

        -- Validate PrimvarReader Shader
        local primvarReaderPrim = stage.GetPrimAtPath("/root/mtl/Bitmaptexture/PrimvarReader_st")
        assert_true (primvarReaderPrim.IsValid())
        assert_true (primvarReaderPrim.IsA(pyShade.Shader))
        local primvarReader = pyShade.Shader(primvarReaderPrim)
        assert_equal ((primvarReaderPrim.GetAttribute "info:id").Get()) "UsdPrimvarReader_float2"
        assert_equal ((((primvarReader.GetInput "varname").GetConnectedSource())[1].GetPath())) (pysdf.Path "/root/mtl/Bitmaptexture")

    ),

    function teardown = (
    ),

    Tests = #(
        test_export_OpenPBR_material
        )
)

runUsdTestFixture openPBR_to_usdps_test (getThisScriptFilename())