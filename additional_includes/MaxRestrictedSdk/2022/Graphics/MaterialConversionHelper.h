//
// Copyright 2020 Autodesk, Inc.  All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which 
// otherwise accompanies this software in either electronic or hard copy form.   
//
//

#pragma once

#include "Graphics/GraphicsExport.h"
#include "Graphics/BaseMaterialHandle.h"
#include "Materials/Mtl.h"
#include "maxTypes.h"

namespace MaxSDK { namespace Graphics {

/** 
* MaterialConversionHelper is an helper class to convert from a 3ds Max material (Mtl class) into a Viewport material (BaseMaterialHandle class)
* This viewport material (say BaseMaterialHandle physMatHandle )can be assigned with RenderItemHandle::SetCustomMaterial(physMatHandle) or if you get access to an 
* UpdateNodeContext& updateNodeContext which can be found in UpdatePerNodeItems, you can do nodeContext.GetRenderNode().SetSolidMaterial(physMatHandle);
* 
* Examples of usage :
* 
*	//Create a the Nitrous equivalent of a physical material with a bitmap texture as its base color.
*	MSTR texturePath;
*	if (GetSpecDir(APP_MAP_DIR, _T("Maps"), texturePath)){
*		texturePath += TSTR(_T("/uv-grid.png")); //Use UV grid to get something
*	}
*	//Create a BitmapTex which holds the bitmap
*	BitmapTex *pBitmapTex = NewDefaultBitmapTex();
*   pBitmapTex->SetMapName(texturePath);
*	pBitmapTex->SetMtlFlag(MTL_TEX_DISPLAY_ENABLED, TRUE);//Active its display in the viewport
*	pBitmapTex->ActivateTexDisplay(TRUE); //activate it
*	pBitmapTex->NotifyDependents(FOREVER, PART_ALL, REFMSG_CHANGE);
*
*	Interval valid;
*	valid.SetInfinite();
*	
*	//In realistic mode, like in high quality viewport
*	const MaterialConversionHelper::MaterialStyles matStyle = MaterialConversionHelper::MaterialStyles::MaterialStyle_Realistic;
*
*	//Create a physical material and get its Nitrous material equivalent
*	BaseMaterialHandle physMatHandle ;
*	Mtl* pPhysicalMaterial = (Mtl*)GetCOREInterface()->CreateInstance(MATERIAL_CLASS_ID, PHYSICALMATERIAL_CLASS_ID);
*	pPhysicalMaterial->SetName(_T("PhysMat1"));
	const int mapSlot = GetPrimaryMapSlot(pPhysicalMaterial);
	pPhysicalMaterial->SetSubTexmap(mapSlot, pBitmapTex);
*	pPhysicalMaterial->Update(t, valid);
*	//debug if needed, put the created material to compact material editor slot #0
*   GetCOREInterface()->PutMtlToMtlEditor(pPhysicalMaterial, 0);
*	//Convert to Nitrous the physical material
*	physMatHandle = MaxSDK::Graphics::MaterialConversionHelper::ConvertMaxToNitrousMaterial(*pPhysicalMaterial, t, matStyle);
*
*	//Another example :
*	//Create a physical material from a preset
*	BaseMaterialHandle physicalMaterialHandleFromGoldPreset;
*	const MSTR presetName (_T("Gold Polished")); //another example is const MSTR presetName (_T("Aluminium Matte"));
*	MaxSDK::Graphics::MaterialConversionHelper::GetNitrousMaterialFromPhysicalMaterialPreset(physicalMaterialHandleFromGoldPreset, presetName, t, matStyle);
*/
namespace MaterialConversionHelper
{
	/**
	The enum MaterialStyles is the quality of the desired material from the viewport, when being in high quality viewport, you should use MaterialStyle_Realistic 
	to enableBump/Normal mapping.
	*/
    enum class MaterialStyles
	{
		MaterialStyle_Default,			///The default from the material
		MaterialStyle_Simple,			///Simple which is what is used in standard quality viewport
		MaterialStyle_Realistic,		///Is with normal/bump mapping
		MaterialStyle_Flat,				///No lighting used
		MaterialStyle_HiddenLine,		///To show hidden lines
		MaterialStyle_MaterialDecide,	///Let the material decide
		MaterialStyle_Count, 
		MaterialStyleExt_FastShader = MaterialStyle_Count,	///Is the override material style with fast shader
		MaterialStyleExt_UVChecker,		///Is the override material style with a UV checker map
		MaterialStyleExt_RenderSetting, ///Is used internally
	};

	/** Convert a 3ds Max Mtl to a viewport BaseMaterialHandle
	\param[in] mtl : the 3ds Max material.
	\param[in] t : the time at which you want to conversion to happen (could be different from the current time).
	\param[in] matStyle : the style used for the conversion. It is the quality of the desired material from the viewport, when being in high quality viewport, you should use MaterialStyle_Realistic 
	to enableBump/Normal mapping.
	\return a BaseMaterialHandle which can be used with RenderItemHandle::SetCustomMaterial()
	*/
	MaxGraphicsObjectAPI  BaseMaterialHandle ConvertMaxToNitrousMaterial(Mtl& mtl, TimeValue t, MaterialStyles matStyle);


	/** Create a viewport BaseMaterialHandle from a Physical material preset
	\param[out] outBaseMaterialHandle : a BaseMaterialHandle which can be used with RenderItemHandle::SetCustomMaterial()
	\param[in] presetName : the name used in the preset, you can find the preset names from en-US\Plugcfg\PhysicalMaterialTemplates.ini 
		** BUT ** when there is a composite name like Polished Gold" it's usually the reverse order like "Gold Polished", in the .ini, 
		the actual name is what is after PhysicalTemplate_ActiveMaterial
	\param[in] matStyle : the style used for the conversion. It is the quality of the desired material from the viewport, when being in high quality viewport, you should use MaterialStyle_Realistic 
	to enableBump/Normal mapping.
	\return true if the preset name was found and the BaseMaterialHandle correctly filled.
	*/
    MaxGraphicsObjectAPI bool GetNitrousMaterialFromPhysicalMaterialPreset(	BaseMaterialHandle& outBaseMaterialHandle, const MSTR& presetName, TimeValue t, MaterialConversionHelper::MaterialStyles matStyle);
}

} } // namespace
