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
pySdf = python.import("pxr.Sdf")
pyGf = python.import("pxr.Gf")
pyShade = python.import("pxr.UsdShade")

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_USDZ_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_usdz_test
(  
    public 
    function setup =
    (
        resetmaxfile #noprompt
    ),

    function test_usdz_exported_stages_as_references = (
        -- Create a simple layer to bring in as a USD stage.
        local stage = pyUsd.Stage.CreateInMemory()
        local sphere = pyUsdGeom.Sphere.Define stage "/sphere"
        stage.SetDefaultPrim (sphere.GetPrim())

        local layerName = "simple_layer_sphere"
        local layerPath = output_prefix +  "reference/" + layerName + ".usda"
        stage.Export layerPath

        -- Create a USD Stage Object from the layer.
        stage = UsdStageObject name:"UsdStageObject"
        stage.SetRootLayer layerPath

        -- Export the scene to USDZ.
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.RootPrimPath = "/"
        stageFilename = "usdsphere_sample"
        local exportPath = output_prefix + (stageFilename + ".usdz")
        status = USDExporter.ExportFile exportPath exportOptions:exportOptions

        local exporterLayerStage = pyUsd.Stage.Open exportPath
        sphereRef = exporterLayerStage.GetPrimAtPath ("/UsdStageObject/UsdStageObject_Layer/sphere")
        assert_true (sphereRef.IsValid())
        assert_true (sphereRef.IsA(pyUsdGeom.Sphere))
    ),
    
    function test_usdz_exported_bitmaps = (
        local box001 = Box pos:[0,0,0] name:"box001"
        convertToPoly box001
        polyop.defaultMapFaces box001 1
        local physicalMtl = MaxUsdPreviewSurface()
        box001.material = physicalMtl
        
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local bitmapPath = (testDataDir + @"data\uv_checker.png")
            
        physicalMtl.diffuseColor_map = Bitmaptexture fileName:bitmapPath
        
        local exportPath = output_prefix + "test_usdz_exported_bitmaps.usdz"
                    
        local exportOptions = USDExporter.CreateOptions()
        exportOptions.RootPrimPath = "/"
                
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        expectedFilePath = @"0/uv_checker.png"
            
        -- Inspect the content of the USDZ, making sure our bitmap is found...
        try
        (
            pyZip = python.import("zipfile")
            zipfile = pyZip.ZipFile(exportPath)
            zipStatus = zipfile.testzip()
            assert_undefined zipStatus message:("Exported usdz is a corrupted zip file")
            
            -- verify the presence of the bitmap in the usdz output.
            bitmapPresent = false
                        
            --print(zipfile.namelist() as Array)
            
            local allFilesInZip = (zipfile.namelist() as Array)
            
            -- Find the root layer... usdzip will reproduce the hierarchy, so the folder directory depends on
            -- where the temporary layer was.
            --local usdFile 
            
            local fileName = (getFilenameFile exportPath)
            
            -- Find the root layer. We will then make sure the bitmap is correctly placed
            -- relative to it. hierarchyPrefixLength is size the string in the root layer path which
            -- comes before the layer file name (for ex : foo/bar/bar/test.usd -> sizeOf("foo/bar/bar/")
            local hierarchyPrefixLength
            for item in allFilesInZip do
            (
                posInStr = findString item fileName
                if (posInStr != undefined) do (
                    hierarchyPrefixLength = posInStr
                    break
                )
            )
            
            assert_true (hierarchyPrefixLength != undefined)
            if hierarchyPrefixLength == undefined then return false
            
            for item in allFilesInZip do
            (
                -- Remove the hierarchy up to the root layer, to compare things.
                local relativePath = substring item hierarchyPrefixLength -1
                if pathConfig.pathsResolveEquivalent expectedFilePath relativePath do
                (
                    bitmapPresent = true
                )
            )
            assert_true bitmapPresent message:("Exported usdz should contain the bitmap referenced by the material at " + expectedFilePath)
        )
        catch
        (
            print(getCurrentException())
            assert_true false message:("Exported usdz is not a valid zip file")
        )
        
        -- Finally, make sure the correct relative path is set in the layer for the bitmap.
        local stageHo = pyUsd.Stage.Open exportPath
        local shaderPrim = pyShade.Shader.Get stageHo "/mtl/MaxUsdPreviewSurface/Bitmaptexture/Bitmaptexture"            
        local inputFile = (shaderPrim.GetInput("file")).Get() as string
        assert_equal ("@"+ expectedFilePath + "@") inputFile
    ),
    
    function teardown = (
    ),

    Tests = #(test_usdz_exported_stages_as_references,
        test_usdz_exported_bitmaps
    )
)

runUsdTestFixture export_usdz_test (getThisScriptFilename())