(
    -- Look for the usd plug-in
    findUSD = false
    for i = 1 to pluginManager.pluginDllCount do ( 
        if (pluginManager.pluginDllName i) == "usdexport.dle" then findUSD = true
    )

    if findUSD then
    (
        pyMaxUsd = python.import("maxUsd")
        pyMaxUsd.ShadingModeRegistry.RegisterExportConversion "MaterialX" "mtlx" "MaterialX" "Export USD assets referencing MaterialX material targets."
    )
)