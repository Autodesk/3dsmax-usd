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
pyShade = python.import("pxr.UsdShade")
pyVt = python.import("pxr.Vt")
    
output_prefix = pathConfig.appendPath (getDir #temp) "/MATERIAL_OPACITY_THRESHOLD_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct material_opacity_threshold_test
(
    private
    exportOptions = USDExporter.CreateOptions(),
    importOptions = USDImporter.CreateOptions(),
    public
    function setup =
    (
        exportOptions.FileFormat = #ascii
        exportOptions.Materials = true
        resetmaxfile #noprompt
    ),

    -- This function validates that opacity threshold value is saved at export
    function test_opacity_threshold =
    (
        local exportPath = output_prefix + "opacity_threshold_test.usda"
        -- Populate the scene
        local myBox = Box pos:[0,0,0] width:50 length:50 height:50 name: "myBox"
        
        -- Assign same matId to every face
        convertto myBox editable_poly
        numFaces = (getNumFaces myBox)
        polyop.SetFaceMatID myBox #(1,2,3,4,5,6) 3
        
        -- Assign material
        myBox.material = MaxUsdPreviewSurface ()
        myBox.material.opacityThreshold = 0.5

        -- Export file
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        resetMaxFile #noprompt 

        -- Import file
        USDImporter.ImportFile exportPath importOptions:importOptions

        -- Validation
        assert_equal 0.5 $myBox.material.opacityThreshold message: "Expected opacity threshold to be 0.5."
    ),

    function teardown = (
        exportOptions.Reset()
    ),

    tests = #(
        test_opacity_threshold
        )
)

runUsdTestFixture material_opacity_threshold_test (getThisScriptFilename())