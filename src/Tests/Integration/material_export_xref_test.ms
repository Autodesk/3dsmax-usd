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
pyShade = python.import("pxr.UsdShade")
    
output_prefix = pathConfig.appendPath (getDir #temp) "/USD_EXPORT_XREF_MATERIAL_TEST_"
deleteTestFiles(output_prefix + "*.*")

Struct usd_export_xref_material_test
(
    private
    temp_xref_child = (pathConfig.removePathLeaf (getSourceFileName())) + "/resources/box_with_physical_material.max",
    temp_usd_file = output_prefix + "xref_export.usd",
    exportOptions = USDExporter.CreateOptions(),

    public
    function setup =
    (
        resetMaxFile #noprompt
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        exportOptions.Materials = true
    ),
    
    -- This function validates that xref material value are exported correctly
    function test_material_export_xref =
    (
        -- Add known test file which contains a box and a physical material that was setup with the values inside the setupValuesOnPhysMtl utility function. 
        nodeAdded = xrefs.addNewXrefObject temp_xref_child "XrefBox"
        xref_mat = nodeAdded.material
        
        -- Export to usd
        UsdExporter.ExportFile temp_usd_file exportOptions:exportOptions contentSource:#nodeList nodeList:#($XrefBox)       
        
        -- Validate the correct material value have been exported
        local stage = pyUsd.Stage.Open(temp_usd_file)
        local shaderPrim = pyShade.Shader.Get stage "/mtl/usdPhysTestMtl/usdPhysTestMtl"
        validateValueOnPhysMtl shaderPrim
    ),
    
    function tearDown =
    (
        format "Framework teardown...\n"
    ),

    Tests = #(
        test_material_export_xref
    )
)

runUsdTestFixture usd_export_xref_material_test (getThisScriptFilename())