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

pyUsd = python.import("pxr.Usd")

struct load_usd_materialx_plugin_test
(
    public
    function setup =
    (
        resetmaxfile #noprompt
    ),
    
    function test_usd_material_x_plugin_loaded = (
        local testDataDir = getFilenamePath (getThisScriptFilename())
        local sampleMtlXFile = (testDataDir + @"data\simpleMtlxRef.usda")
        
        stage = pyUsd.Stage.Open(sampleMtlXFile)
        
        -- Make sure the materialX format plugin was loaded, and the standard materialX libs where found.
        -- Do this by looking for a translated shader prim. "ND_standard_surface_surfaceshader" is a shader from 
        -- the standard library.
        prim = stage.GetPrimAtPath("/root/mtl/Material__43/Materials/Iberian_Blue_Ceramic_Tiles/ND_standard_surface_surfaceshader")
        assert_true (prim.IsValid())
    ),

    function teardown = (
    ),

    tests = #(test_usd_material_x_plugin_loaded)
)

runUsdTestFixture load_usd_materialx_plugin_test (getThisScriptFilename())