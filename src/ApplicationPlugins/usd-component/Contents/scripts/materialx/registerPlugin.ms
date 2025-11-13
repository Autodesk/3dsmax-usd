(	
    -- Look for the USD plug-in
    findUSD = false
    for i = 1 to pluginManager.pluginDllCount do
    (
        if (pluginManager.pluginDllName i) == "usdexport.dle" then
            findUSD = true
    )
    
    if findUSD then
    (
        pyUsdPlug = python.import "pxr.Plug"
        
        function addMatXShaderWriterPlugin =
        (
            pysys = python.import "sys"
            scriptPath = getThisScriptFilename()
            pluginPath = pathConfig.removePathLeaf scriptPath
            pluginPath = pluginPath + "\\writer\\plugInfo.json"
            
            -- The plugin script location must be part of the Python path
            pysys.path.insert 0 (pathConfig.removePathLeaf pluginPath)
            plugRegistry = pyUsdPlug.Registry()
            plugRegistry.RegisterPlugins pluginPath
        )
        
        function addMatXShaderReaderPlugin =
        (
            pysys = python.import "sys"
            scriptPath = getThisScriptFilename()
            pluginPath = pathConfig.removePathLeaf scriptPath
            pluginPath = pluginPath + "\\reader\\plugInfo.json"
            
            -- The plugin script location must be part of the Python path
            pysys.path.insert 0 (pathConfig.removePathLeaf pluginPath)
            plugRegistry = pyUsdPlug.Registry()
            plugRegistry.RegisterPlugins pluginPath
        )
        
        addMatXShaderWriterPlugin()
        addMatXShaderReaderPlugin()
    )
)