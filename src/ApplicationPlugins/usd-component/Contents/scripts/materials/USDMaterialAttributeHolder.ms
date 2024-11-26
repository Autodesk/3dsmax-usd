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

global UsdMaterialAttributeHolder = attributes UsdMaterialAttributeHolder
version:1
attribID:#(0x37a5e0e8, 0x723f032d)
(
	function replaceUnicode val = 
	(
		local updatedStr = ""
		for i = 1 to val.count do
		(
			local char = val[i]
			if bit.charAsInt char > 127 then
			(
				updatedStr += "_"
			)
			else 
			(
				updatedStr += char
			)
		)
		updatedStr
	)
	
	local enableFunc, UsdAttributes, enableLayerPath, validatePrimPath
	parameters USD rollout:UsdAttributes
	(
		pathMethod type:#integer default:1 ui:rdo_buttons
		primPath type:#string default:"mtl" ui:edt_primPath
		separateLayer type:#boolean default:false ui:ckb_separateLayer
		filePath type:#string ui:edt_filePath

		on pathMethod set val do 
		(
			if enableFunc != undefined then enableFunc()
		)
		on separateLayer set val do 
		(
			if enableLayerPath != undefined then enableLayerPath()
		)
		on filePath set val do 
		(
			if val != undefined then 
			(
				filePath = replaceUnicode val
			)
		)
	)
	
	rollout UsdAttributes "USD Export Options" autoLayoutOnResize:true 
	(
		radiobuttons rdo_buttons "" labels:#("Use defaults defined in the USD Exporter", "Specify New Path") default:1 columns:1
		editText edt_primPath "Scope" enabled:(pathMethod != 1) tooltip:"The path in the USD hierarchy where this prim will reside."
		checkbox ckb_separateLayer "Separate USD Layer" enabled:(pathMethod != 1) tooltip:"Enable to have materials export to a separate USD file at the File Path specified. When disabled, used the value defined in the USD Exporter."
		editText edt_filePath "File Path" enabled:(pathMethod !=1) tooltip:"The path to the file where the material is going to be exported."
		
		on UsdAttributes open do 
		(
			enableFunc()
		)

		on edt_filePath entered val do
		(
			edt_filePath.text = replaceUnicode val
		)
	)
	
	
	function enableFunc = 
	(
		UsdAttributes.edt_primPath.enabled = (pathMethod != 1)
		UsdAttributes.ckb_separateLayer.enabled = (pathMethod != 1)
		UsdAttributes.edt_filePath.enabled = ((pathMethod != 1) and separateLayer)
	)

	function enableLayerPath = 
	(
		UsdAttributes.edt_filePath.enabled = separateLayer
	)
)