(
    -- Look for the usd plug-in
   findUSD = false
   for i = 1 to pluginManager.pluginDllCount do ( 
       if (pluginManager.pluginDllName i) == "usdexport.dle" then findUSD = true
   )

   if findUSD then
   (
       pyUsdPlug = python.import("pxr.Plug")
       
       function addMatXShaderWriterPlugin = 
       (
           pysys = python.import("sys")
           pyos = python.import("os")
       
           scriptPath = getThisScriptFilename()
           pluginPath = pathConfig.removePathLeaf scriptPath
           pluginPath = pluginPath + "\\plugInfo.json"
           -- The plugin script location must part of the Python path
           pysys.path.insert 0 (pathConfig.removePathLeaf pluginPath)
       
           plugRegistry = pyUsdPlug.Registry()
           plugRegistry.RegisterPlugins pluginPath
       )
       
       addMatXShaderWriterPlugin()
   )
)