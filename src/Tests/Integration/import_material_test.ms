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

input_prefix = pathConfig.appendPath (getDir #temp) "/IMPORT_MATERIAL_TEST_"
deleteTestFiles(input_prefix + "*.*")

struct import_material_test
(  
    private 
    test_python_path,
   
    public 
    function setup =
    (
        resetmaxfile #noprompt

        thisFileName = getSourceFileName()
        test_python_path = pathConfig.removePathLeaf thisFileName
        pysys = python.import("sys")
        if pysys.path.__contains__(test_python_path) then (
            return false
        ) else (
            pysys.path.insert 0 test_python_path
        )
    ),
    
    function test_no_node_created_for_material_and_shader_prim = 
    (
        local importOptions = USDImporter.CreateOptions()
        local filepath = input_prefix + "test_no_node_created_for_material_and_shader_prim.usda"
        local stage = pyUsd.Stage.CreateNew(filepath)
        local materialsPrim = stage.DefinePrim "/Materials"
        local materialPrim = pyShade.Material.Define stage  "/Materials/Material__25"
        local shaderPrim = pyShade.Shader.Define stage "/Materials/Material__25/Material__25"
        (stage.GetRootLayer()).Save()

        USDImporter.ImportFile filepath importOptions:importOptions

        assert_not_equal $Materials undefined
        assert_equal undefined $Materials/Material__25 
        assert_equal undefined $Materials/Material__25/UsdPreviewSurface
    ),

    function test_material_import_pymxs_file = 
    (
        python.Execute "from importlib import reload"
        python.Execute "import import_material_test"
        python.Execute "reload(import_material_test)"
        mat_imp_test = python.import("import_material_test")
        results = mat_imp_test.run_tests()

        processPythonUnittestResults results
    ), 
    function teardown = (
        pysys = python.import("sys")
        if pysys.path.__contains__(test_python_path) then (
            pysys.path.remove(test_python_path)
        )
    ),
    
    Tests = #(
        test_no_node_created_for_material_and_shader_prim,
        test_material_import_pymxs_file
    )
)

runUsdTestFixture import_material_test (getThisScriptFilename())