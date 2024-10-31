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

--this script adds the USD Stage menu item to the File->Reference menu.

--Finds the "targetMenuName" menu entry inside the "menuToSearch", returns true if the menu was found.
fn findMenuEntry menuToSearch targetMenuName &referenceMenuItemIdx = (
	
	if menuToSearch != undefined then 
	(
		local menuItemCount = menuToSearch.numItems()
		for i = 1 to menuItemCount do
		(
			local curItem = menuToSearch.getItem i
			local curItemTitle = curItem.getTitle()
			
			if ( curItemTitle == targetMenuName ) then
			(
				referenceMenuItemIdx = i
				return true
			)
		)
	)
	
	return false
)

fn addUSDStageReferenceMenuItem = (

	local referenceMenuFound = false
	local referenceMenuItemIdx = -1
	local referenceMenu = undefined
	mainFileMenu = menuMan.findMenu ~FILE_MENU_MAIN~
	referenceMenuFound = findMenuEntry mainFileMenu ~REFERENCE_MENU_ITEM~ &referenceMenuItemIdx
	
	if referenceMenuFound then
	(
		referenceMenu = mainFileMenu.getItem referenceMenuItemIdx
		referenceMenu = referenceMenu.getSubMenu()
		
		local USDStageItemIdx = -1
		local USDStageItemFound = findMenuEntry referenceMenu ~USD_STAGE_MENU_ITEM~ &USDStageItemIdx
		
		if not USDStageItemFound then 
		(
			-- create a menu items that calls the macroscript
			local theMenuItem = menuMan.createActionItem "CreateUSDStage" "USD"
			theMenuItem.setTitle ~USD_STAGE_MENU_ITEM~
			theMenuItem.setUseCustomTitle true
			referenceMenu.addItem theMenuItem -1
			
			-- redraw the menu bar with the new item
			menuMan.updateMenuBar()
		)
	)
)

fn removeUSDStageReferenceMenuItem = (

	local referenceMenuFound = false
	local referenceMenuItemIdx = -1
	local referenceMenu = undefined
	mainFileMenu = menuMan.findMenu ~FILE_MENU_MAIN~
	referenceMenuFound = findMenuEntry mainFileMenu ~REFERENCE_MENU_ITEM~ &referenceMenuItemIdx
	
	if referenceMenuFound then
	(
		referenceMenu = mainFileMenu.getItem referenceMenuItemIdx
		referenceMenu = referenceMenu.getSubMenu()
		
		local USDStageItemIdx = -1
		local USDStageItemFound = findMenuEntry referenceMenu ~USD_STAGE_MENU_ITEM~ &USDStageItemIdx
		
		if USDStageItemFound then 
		(
			referenceMenu.removeItemByPosition USDStageItemIdx
		)
	)
)

fn addUSDMenuItem = (
	toolsMenu = menuMan.findMenu ~TOOLS_MENU_ITEM~
	if toolsMenu != undefined then
	(
		local USDItemIdx = -1
		local USDItemFound = findMenuEntry toolsMenu "USD" &USDItemIdx
	
		if not USDItemFound then 
		(
			usdMenu = menuMan.createMenu "USD"
			
			-- create a menu item that calls the macroscript to open the explorer
			local explorerMenuItem = menuMan.createActionItem "OpenUsdExplorer" "USD"
			explorerMenuItem.setTitle ~USD_EXPLORER_MENU_ITEM~
			explorerMenuItem.setUseCustomTitle true
			usdMenu.addItem explorerMenuItem -1
			
			-- create a menu item that calls the macroscript to open the layer editor
			local layerEditorMenuItem = menuMan.createActionItem "OpenUsdLayerEditor" "USD"
			layerEditorMenuItem.setTitle ~USD_LAYER_EDITOR_MENU_ITEM~
			layerEditorMenuItem.setUseCustomTitle true
			usdMenu.addItem layerEditorMenuItem -1
			
			toolsMenu.addItem (menuMan.createSeparatorItem()) -1
			subMenuItem = menuMan.createsubMenuItem "USD" usdMenu
			
			toolsMenu.addItem subMenuItem -1			
			
			-- redraw the menu bar with the new item
			menuMan.updateMenuBar()
		)
	)	
)

fn removeUSDMenuItem = (
	toolsMenu = menuMan.findMenu ~TOOLS_MENU_ITEM~
	local USDItemIdx = -1
	local USDItemFound = findMenuEntry toolsMenu "USD" &USDItemIdx
	if USDItemFound then 
	(
		toolsMenu.removeItemByPosition USDItemIdx
	)
)

function addUSDMenus = (
	addUSDStageReferenceMenuItem()
	addUSDMenuItem()	
)

function removeUSDMenus = (
	removeUSDStageReferenceMenuItem()
	removeUSDMenuItem()	
)

--add a callback to that whenever a menu file is loaded, such as during theme or workspace loading, we add potentially missing entries
callbacks.addScript #postLoadingMenus "addUSDMenus()" id:#USDMenuEntryPostLoad
--remove ourself from the menu, we don't want to be saved
callbacks.addScript #preSavingMenus "removeUSDMenus()" id:#USDMenuEntryRemoval
--add ourself again when the saving is done
callbacks.addScript #postSavingMenus "addUSDMenus()" id:#USDMenuEntryRemoval
--call the function once to handle first load, because menus are loaded before startup scripts and the callback won't trip
addUSDMenus()