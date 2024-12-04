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
 * Appends usd python bindings to max's python path
 * so that max users can use the usd python out of the box.
*/
(

function getUsdImporterPluginIndex =
(
	for i = 1 to pluginManager.pluginDllCount do ( 
		if (pluginManager.pluginDllName i) == "usdimport.dli" then return i
	)
	return -1;
)

function addUsdToPythonPath = 
(
	pluginIndex = getUsdImporterPluginIndex()
	pluginPath = pluginManager.pluginDllDirectory pluginIndex
	-- In versions < 2023, pluginManager.pluginDllDirectory would return the plugin's dll 
	-- path and not the directory, this issue was since fixed.
	if (getFilenameType pluginPath) != "" do (
		pluginPath = pathConfig.removePathLeaf pluginPath
	)
	usdPythonPath = pathConfig.appendPath pluginPath "python"

	--need to add the pluginPath itself (bin folder) for the python imports to work properly
	pysys = python.import("sys")
	pyos = python.import("os")

	if pysys.version_info[1] == 3 and not pysys.version_info[2] < 8 then (
		pyos.add_dll_directory(pluginPath)
	) else (
		pyos.environ["PATH"] = pluginPath + pyos.pathsep + pyos.environ["PATH"]
	)

	--need to add usdPluginsPath to PATH for usd plugins dlls to load properly
	usdPluginsPath = pathConfig.appendPath (pathConfig.removePathLeaf pluginPath) "plugin\\usd"
	if pysys.version_info[1] == 3 and not pysys.version_info[2] < 8 then (
		pyos.add_dll_directory(usdPluginsPath)
	) else (
		pyos.environ["PATH"] = usdPluginsPath + pyos.pathsep + pyos.environ["PATH"]
	)

	if pysys.path.__contains__(usdPythonPath) then (
		return false
	) else (
		if doesFileExist usdPythonPath then (
			--favor prepend because we currently do not want users to override the path by mistake when setting PYTHONPATH
			pysys.path.insert 0 usdPythonPath
		) else (
			logsystem.logEntry ("Could not find usd python path at " + usdPythonPath) broadcast:true warning:true
		)
	)
	
	local mtlxLibPathEnvVar = undefined
	maxver = maxversion()
	if maxver[1] >= 26000 then (  -- 3ds Max 2024 and up use USD 22.11+ env var name has changed in later versions.
		mtlxLibPathEnvVar = "PXR_MTLX_STDLIB_SEARCH_PATHS"
	) else (
		mtlxLibPathEnvVar = "PXR_USDMTLX_STDLIB_SEARCH_PATHS"
	)

	usdMtlXLibPath = pathConfig.appendPath pluginPath "..\\libraries"
	if pyos.environ[mtlxLibPathEnvVar] == undefined then (
		pyos.environ[mtlxLibPathEnvVar] = usdMtlXLibPath
	) else (
		pyos.environ[mtlxLibPathEnvVar] = usdMtlXLibPath + pyos.pathsep + pyos.environ[mtlxLibPathEnvVar]
	)
)

addUsdToPythonPath()

)
