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

output_prefix = pathConfig.appendPath (getDir #temp) "/EXPORT_MATERIAL_ID_TEST_"
deleteTestFiles(output_prefix + "*.*")

struct export_material_id_test
(
    private
    exportOptions = USDExporter.CreateOptions(),

    public
    function setup =
    (
        exportOptions.FileFormat = #ascii
        exportOptions.RootPrimPath = "/"
        resetmaxfile #noprompt
    ),
    
    function test_export_material_id = (
        -- Populate the scene
        Box pos:[0,0,0] width:10 length:10 height:10 name:"Box"
        convertto $Box editable_poly
        numFaces = (getNumFaces $Box)
        for i = 1 to numFaces do
        (
            facelist = #(i)
            polyop.SetFaceMatID $Box facelist i
        )
            
        -- export file
        exportPath = (output_prefix + "export_mat_test_id.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions

        -- validate exported data
        local stage = pyUsd.Stage.Open(exportPath)
        local exportedBoxPrim = stage.GetPrimAtPath("/Box")
        local bindingAPI = pyShade.MaterialBindingAPI(exportedBoxPrim)
        subsets = bindingAPI.GetMaterialBindSubsets()
        assert_equal subsets.count 6
        for i = 1 to 6 do
        (
            subset = subsets[i]
            att = subset.GetIndicesAttr()
            indices = att.Get()
            assert_equal indices.count 1
            assert_equal indices[0] (i - 1)
            assert_equal i ((subset.GetPrim()).GetCustomDataByKey "3dsmax:matId") message:"The Max matId should have been written to CustomData."
        )
    ),
    
    function test_export_material_with_only_one_id = (
        -- Populate the scene
        myBox = Box pos:[0,0,0] width:10 length:10 height:10 name:"Box"
        convertto myBox editable_poly
        numFaces = (getNumFaces $Box)
        polyop.SetFaceMatID $Box #(1,2,3,4,5,6) 3
  
        -- export file
        exportPath = (output_prefix + "export_mat_test_only_one_id.usda")
        USDExporter.ExportFile exportPath exportOptions:exportOptions
        
        -- validate exported data doesn't have material bind subset
        local stage = pyUsd.Stage.Open(exportPath)
        local exportedBoxPrim = stage.GetPrimAtPath("/Box")
        local bindingAPI = pyShade.MaterialBindingAPI(exportedBoxPrim)
        subsets = bindingAPI.GetMaterialBindSubsets()
        
        assert_equal subsets.count 0
        assert_equal 3 (exportedBoxPrim.GetCustomDataByKey "3dsmax:matId") message:"The 3ds Max matId should have been written to CustomData."
    ),
    function teardown = (
    ),

    tests = #(test_export_material_id, test_export_material_with_only_one_id)
)

runUsdTestFixture export_material_id_test (getThisScriptFilename())