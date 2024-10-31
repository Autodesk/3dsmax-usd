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

struct load_materialx_plugin_test
(
    public
    function setup =
    (
        resetmaxfile #noprompt
    ),
    
    function test_material_x_plugin_loaded = (
        -- The materialX 3dsmax plugin is bundled with our USD plugin.
        
        -- We should be able to find its DLL in the registered plugins.
        local materialXDllLoaded = false
        for i=1 to pluginManager.pluginDllCount do
        (
            
            if (pluginManager.isPluginDllLoaded i) and (pluginManager.pluginDllName i) == "materialxio.dlu" do
            (
                materialXDllLoaded = true
                exit 
            )
        )
        assert_true materialXDllLoaded message:"materialxio.dlu is not loaded, the MaterialX plugin is not properly initialized."
        
        -- Also make sure that its scripts parts have been executed.
        assert_defined MaterialXMaterial message:"MaterialXMaterial is not defined, the MaterialX plugin is not properly initialized."
    ),

    function teardown = (
    ),

    tests = #(test_material_x_plugin_loaded)
)

runUsdTestFixture load_materialx_plugin_test (getThisScriptFilename())