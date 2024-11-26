/**********************************************************************
// Copyright 2022 Autodesk, Inc.  All rights reserved. 
**********************************************************************/
--this script adds the macro action for USD Stage menu item.

macroScript CreateUSDStage category:"USD" buttonText:~USD_STAGE_MENU_ITEM~ tooltip:~USD_STAGE_TOOLTIP~
(
	result = USDStageObject.SelectRootLayerAndPrim filterMode:#exclude filteredTypes:#( "Material" , "Shader" , "GeomSubset" ) useUserSettings:true
	if result != undefined then 
	(
		stageObject = USDStageObject()
		stageObject.SetRootLayer result[1] stageMask:result[2] payloadsLoaded:result[3]
		if (result[4]) do (
			stageObject.OpenInUsdExplorer()
		)
	)
)

macroScript OpenUsdExplorer category:"USD" buttonText:~USD_EXPLORER_MENU_ITEM~ tooltip:~USD_EXPLORER_MENU_ITEM~
(
	USDStageObject.OpenUsdExplorer()
)