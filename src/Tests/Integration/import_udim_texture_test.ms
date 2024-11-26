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
pyGf = python.import("pxr.Gf")
pyVt = python.import("pxr.Vt")

struct import_udim_texture_test
(

    public 
    function setup =
    (
        resetmaxfile #noprompt
    ),

    function test_valid_udim_data_imported = (
        resetMaxFile #noprompt
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local importPath = (testDataDir + @"data\import_udim_test_data\max-osl-uber-bitmap-udims-plane.usda")

        USDImporter.ImportFile importPath

        importedSceneUberBitmap = sceneMaterials[1].diffusecolor_map.source_map
        listOfFoundUDIMs = importedSceneUberBitmap.Filename_UDIMList

        -- Based on import_udim_test_data/max-osl-uber-bitmap-udims-plane.usda containing 
        -- only 1 Material and 1 UsdUVTexture and set it's field "asset inputs:file" set to:
        --    asset inputs:file = @./data/testUsdImagingGLUsdUdims/texture_<UDIM>.jpg@
        -- Then looking into `./data/testUsdImagingGLUsdUdims/` seeing that those are the
        -- correct UDIMS associated to that texture being found and set to our max texture.
        correctUdimList = "1001 1002 1003 1004 1005 1006 1008 1009 1010 1011 1012 1013"
        assert_equal listOfFoundUDIMs correctUdimList message:"The Filename_UDIMList property did not match the correct values."
    ),
    
    function teardown = (
    ),

    Tests = #(test_valid_udim_data_imported)
)

runUsdTestFixture import_udim_texture_test (getThisScriptFilename())