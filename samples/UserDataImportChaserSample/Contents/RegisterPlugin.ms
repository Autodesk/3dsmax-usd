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

/*
 * Registers the User Data Import Chaser to USD Plug Registry
 * This script is ran as part of the "post-start-up scripts parts"
 * This is necessary to ensure that USD can register the plugin properly, and load the associated dll or python script when the time is right.
 * Loading the USD plugin's DLL before the USD plugin object Load() is called can cause issues/crash.
 * This script registers either the python or the c++ import chaser. Comment/uncomment the file accordingly to the one you want to load.
 * Both could be registered at the same time without problem, but only one would ever be executed.
 * For the purpose of this sample we demonstrate how to load a c++ and a python chaser.
*/
(

pyUsdPlug = python.import("pxr.Plug")

function addMyMaxUsdPlugin = 
(
	pysys = python.import("sys")
	pyos = python.import("os")
	
	scriptPath = getThisScriptFilename()
	pluginPath = pathConfig.removePathLeaf scriptPath

	-- note: either use the Python (default) or the C++ version of the plugin
	-- You must comment/uncomment the respective maxscript code in the sections below

	-- [section python - start]
	-- Load the python prim reader
	pluginPath = pluginPath + "\\python\\plugInfo.json"
	-- The plugin script location must part of the Python path
	pysys.path.insert 0 (pathConfig.removePathLeaf pluginPath)
	-- [section python - end]
	
	-- [section c++ - start]
	-- Load the c++ prim reader
	/**
	 * pluginPath = pluginPath + "\\plugInfo.json"
	 */
	-- [section c++ - end]

	plugRegistry = pyUsdPlug.Registry()
	plugRegistry.RegisterPlugins pluginPath
)

addMyMaxUsdPlugin()

)