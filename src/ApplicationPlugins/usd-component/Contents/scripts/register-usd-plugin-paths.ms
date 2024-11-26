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
 * Registers the 3ds Max usd plugin paths to USD Plug Registry
 * so that max users can use the usd plugins out of the box.
*/
(

pyUsdPlug = python.import("pxr.Plug")

function addUsdPluginPaths = 
(
	scriptPath = getThisScriptFilename()
	componentPath = pathConfig.removePathLeaf (pathConfig.removePathLeaf scriptPath)
	plugRegistry = pyUsdPlug.Registry()

	pluginPaths = #(
		"plugin\\MaxUsd_Translators",
		"plugin\\BasePxrUsdPreviewSurface",
		"scripts"
	)
	for pluginPath in pluginPaths do (
		fullPluginPath = pathConfig.appendPath componentPath pluginPath
		plugRegistry.RegisterPlugins fullPluginPath
	)
)

addUsdPluginPaths()

)
