//
// Copyright 2020 Autodesk, Inc.  All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which 
// otherwise accompanies this software in either electronic or hard copy form.   
//
//

//Link with the optimesh.lib library to use this in your plugin.

#pragma once

#include "Graphics/IRenderGeometry.h"
#include "Graphics/DrawContext.h"
#include "Materials/Mtl.h"
#include "quat.h"
#include "Graphics/UpdateDisplayContext.h"
#include "Graphics/UpdateNodeContext.h"
#include "Graphics/MaterialRequiredStreams.h"
#include "Graphics/VertexBufferHandle.h"
#include "Graphics/IndexBufferHandle.h"
#include "Noncopyable.h"
#include "Graphics/MaterialConversionHelper.h"
#include "export.h"

namespace MaxSDK { namespace Graphics {

///This macro converts from a color r,g,b in bytes (range 0 to 255) to a combined DWORD used internally by 3ds Max for storing Vertex Colors.
#define INSTANCES_R8G8B8X8_DWORD(r,g,b)((DWORD)((((WORD)((BYTE)(b)) << 8)) | (((DWORD)(BYTE)(g)) << 16) | (((DWORD)(BYTE)(r)) << 24)));

///Declaration for the private implementation of this class
class InstanceRenderGeometryImpl;

/** The struct InstanceData is used to pass instance data in different methods from the InstanceRenderGeometry class.
It is used in InstanceRenderGeometry::CreateInstanceVertexBuffer to create the instance vertex buffer. This has to be called once only unless you want to reset all instance data with something else
Or the number of instances has changed.
It is also used in InstanceRenderGeometry::UpdateInstanceVertexBuffer to update the instance data which was changed. This is only used when there is animation on the instance data.
Like updating the positions at different times.
*/
struct InstanceData : public MaxSDK::Util::Noncopyable
{
	/**numInstances : is the number of instances.
	When creating the instance buffers with InstanceRenderGeometry::CreateInstanceVertexBuffer, this parameter should be non null.
	When updating the instance buffers with InstanceRenderGeometry::UpdateInstanceVertexBuffer, this can be ignored as the number of instances should not have changed.
	Please, keep in mind, that we currently have a limitation of 32768 instances.
	If you want more instances than 32768, you will have to split the instances into several InstanceRenderGeometry.
	*/
	size_t				numInstances					= 0; 

	/**If bTransformationsAreInWorldSpace is true, then all matrices/positions/orientation/scales used in that struct are in world space, so moving the node will not move the instances.
	If bTransformationsAreInWorldSpace is false then all matrices/positions/orientation/scales are relative to the node's transfrom matrix, so moving the node will move all instances.
	When creating the instance buffers with InstanceRenderGeometry::CreateInstanceVertexBuffer, this parameter should be set.
	When updating the instance buffers with InstanceRenderGeometry::UpdateInstanceVertexBuffer, this parameter can be ingored as this should not change during update.
	If so, then use InstanceRenderGeometry::CreateInstanceVertexBuffer to recreate the full buffer.
	*/
	bool				bTransformationsAreInWorldSpace = false;

	/**pMatrices is an array of Matrix3 which is the transform matrix of each instance, this parameter can be null if you use the pPositions array instead but 
	either pMatrices or pPositions should be non null if you want the instances to be at different positions.
	To set if these matrices are in world space or relative to the node's transform, please see bTransformationsAreInWorldSpace.
	*/
	Matrix3*			pMatrices						= nullptr;

	/**numMatrices is the number of elements in pMatrices, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numMatrices						= 0;

	/**pPositions is an array of Point3 which is the position of each instance, this parameter can be null if you use the pMatrices array instead at least one of those two pointers should be non null.
	To set if these positions are in world space or relative to the node's transform, please see bTransformationsAreInWorldSpace.
	Internally in the vertex buffer, this data will be converted to a Matrix3 with pos/orientation/scale combined when put in the instance vertex buffer.
	When updating the instance buffers with InstanceRenderGeometry::UpdateInstanceVertexBuffer, if you are not using pMatrices, but rather position/orientation/scale, 
	always provide the 3 of them (position/orientation/scale) as any missing of those will be replaced by identity or null (for position)
	as we are combining the 3 into a matrix. Even if only one of them is animated.
	*/
	Point3*				pPositions						= nullptr;

	/**numPositions is the number of elements in pPositions, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numPositions					= 0;

	/**pOrientationsAsPoint4 is an array of Point4 which are quaternions. This parameter can be null if the orientation of instances is not overriden, it will be an identity orientation in that case.
	If you prefer to use the Quat class to provide orientations, please see pOrientationsAsQuat.
	To set if these orientations are in world space or relative to the node's transform, please see bTransformationsAreInWorldSpace.
	Internally in the vertex buffer, this data will be converted to a Matrix3 with pos/rot/scale combined when put in the instance vertex buffer.
	When updating the instance buffers with InstanceRenderGeometry::UpdateInstanceVertexBuffer, if you are not using pMatrices, but rather position/orientation/scale, 
	always provide the 3 of them (position/orientation/scale) as any missing of those will be replaced by identity or null (for position)
	as we are combining the 3 into a matrix. Even if only one of them is animated.
	*/
	Point4*				pOrientationsAsPoint4			= nullptr;

	/**numOrientationsAsPoint4 is the number of elements in pOrientationsAsPoint4, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numOrientationsAsPoint4			= 0;


	/**pOrientationsAsQuat is an array of Quat which are quaternions. This parameter can be null if the orientation of instances is not overriden, it will be an identity orientation in that case.
	If you prefer to use the Point4 class to provide orientations instead of Quat, please see pOrientationsAsPoint4.
	To set if these orientations are in world space or relative to the node's transform, please see bTransformationsAreInWorldSpace.
	Internally in the vertex buffer, this data will be converted to a Matrix3 with pos/rot/scale combined when put in the instance vertex buffer.
	When updating the instance buffers with InstanceRenderGeometry::UpdateInstanceVertexBuffer, if you are not using pMatrices, but rather position/orientation/scale, 
	always provide the 3 of them (position/orientation/scale) as any missing of those will be replaced by identity or null (for position)
	as we are combining the 3 into a matrix. Even if only one of them is animated.
	*/
	Quat*				pOrientationsAsQuat				= nullptr;

	/**numOrientationsAsQuat is the number of elements in pOrientationsAsQuat, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numOrientationsAsQuat			= 0;

	/**pScales is an array of Point3 which is the scale of each instance. This parameter can be null if the scale of instances is not overriden, it will be an identity scale (1,1,1) used in that case.
	To set if these scales are in world space or relative to the node's transform, please see bTransformationsAreInWorldSpace.
	Internally in the vertex buffer, this data will be converted to a Matrix3 with pos/rot/scale combined when put in the instance vertex buffer.
	When updating the instance buffers with InstanceRenderGeometry::UpdateInstanceVertexBuffer, if you are not using pMatrices, but rather position/orientation/scale, 
	always provide the 3 of them (position/orientation/scale) as any missing of those will be replaced by identity or null (for position)
	as we are combining the 3 into a matrix. Even if only one of them is animated.
	*/
	Point3*				pScales							= nullptr;

	/**numScales is the number of elements in pScales, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numScales						= 0;

	/**pViewportMaterials is an array of BaseMaterialHandle which is the material of each instance. This parameter can be null if you want to use the original material from the node.
	If you need to convert from a 3ds Max Mtl class to a BaseMaterialHandle, please see MaxSDK::Graphics::MaterialConversionHelper::ConvertMaxToNitrousMaterial
	Warning : if you need to update the materials on instances such as remove/add some materials or change which material is applied to an instance, call CreateInstanceVertexBuffer instead of
	UpdateInstanceVertexBuffer so we recreate the instance vertex buffer from scratch as we store the instance vertex buffer data per material which involves a reordering of the vertex buffer data.
	*/
	BaseMaterialHandle*	pViewportMaterials				= nullptr;

	/**materialStyle is used only when you set pViewportMaterials pointer. When compiling the materials, you can set which type of quality you want for the viewport shader.
	An example is : MaterialConversionHelper::MaterialStyles::MaterialStyle_Simple is for a viewport in standard quality.
	And MaterialConversionHelper::MaterialStyles::MaterialStyle_Realistic is for a viewport in high quality.
	//This parameter is optional and by default is set to MaterialConversionHelper::MaterialStyles::MaterialStyle_Simple
	*/
	MaterialConversionHelper::MaterialStyles	materialStyle = MaterialConversionHelper::MaterialStyles::MaterialStyle_Simple;

	/**numViewportMaterials is the number of elements in pViewportMaterials, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numViewportMaterials			= 0;

	/**pUVWMapChannel1 is an array of Point3 which is the UVW value of each instance in map channel 1, we override the UVWs from the whole geometry with a single UVW value per 
	instance on that map channel.
	This parameter can be null if the UVWs in map channel 1 of each instance are not overriden.
	*/
	Point3*				pUVWMapChannel1					= nullptr;

	/**numUVWMapChannel1 is the number of elements in pUVWMapChannel1, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numUVWMapChannel1				= 0;

	/**pUVWMapChannel2 is an array of Point3 which is the UVW value of each instance in map channel 2, we override the UVWs from the whole geometry with a single UVW value per 
	instance on that map channel.
	This parameter can be null if the UVWs in the map channel 2 of each instance are not overriden.
	*/
	Point3*				pUVWMapChannel2					= nullptr;

	/**numUVWMapChannel2 is the number of elements in pUVWMapChannel2, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numUVWMapChannel2				= 0;

	/**pUVWMapChannel3 is an array of Point3 which is the UVW value of each instance in map channel 3, we override the UVWs from the whole geometry with a single UVW value per 
	instance on that map channel.
	This parameter can be null if the UVWs in the map channel 3 of each instance are not overriden.
	*/	
	Point3*				pUVWMapChannel3					= nullptr;

	/**numUVWMapChannel3 is the number of elements in pUVWMapChannel3, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numUVWMapChannel3				= 0;

	/**pUVWMapChannel4 is an array of Point3 which is the UVW value of each instance in map channel 4, we override the UVWs from the whole geometry with a single UVW value per 
	instance on that map channel.
	This parameter can be null if the UVWs in the map channel 4 of each instance are not overriden.
	*/
	Point3*				pUVWMapChannel4					= nullptr;

	/**numUVWMapChannel4 is the number of elements in pUVWMapChannel4, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numUVWMapChannel4				= 0;

	/**pUVWMapChannel5 is an array of Point3 which is the UVW value of each instance in map channel 5, we override the UVWs from the whole geometry with a single UVW value per 
	instance on that map channel.
	This parameter can be null if the UVWs in the map channel 5 of each instance are not overriden.
	*/
	Point3*				pUVWMapChannel5					= nullptr;

	/**numUVWMapChannel5 is the number of elements in pUVWMapChannel5, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numUVWMapChannel5				= 0;

	/**pUVWMapChannel6 is an array of Point3 which is the UVW value of each instance in map channel 6, we override the UVWs from the whole geometry with a single UVW value per 
	instance on that map channel.
	This parameter can be null if the UVWs in the map channel 6 of each instance are not overriden.
	*/
	Point3*				pUVWMapChannel6					= nullptr;

	/**numUVWMapChannel6 is the number of elements in pUVWMapChannel6, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numUVWMapChannel6				= 0;

	/**pUVWMapChannel7 is an array of Point3 which is the UVW value of each instance in map channel 7, we override the UVWs from the whole geometry with a single UVW value per 
	instance on that map channel.
	This parameter can be null if the UVWs in the map channel 7 of each instance are not overriden.
	*/
	Point3*				pUVWMapChannel7					= nullptr;

	/**numUVWMapChannel7 is the number of elements in pUVWMapChannel7, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numUVWMapChannel7				= 0;

	/**pUVWMapChannel8 is an array of Point3 which is the UVW value of each instance in map channel 8, we override the UVWs from the whole geometry with a single UVW value per 
	instance on that map channel.
	This parameter can be null if the UVWs in the map channel 8 of each instance are not overriden.
	*/
	Point3*				pUVWMapChannel8					= nullptr;

	/**numUVWMapChannel8 is the number of elements in pUVWMapChannel8, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numUVWMapChannel8				= 0;

	/**pColors is an array of AColor which is the rgba color of each instance. This will result in the viewport as a flat shading color applied on each instance (no lighting effect added).  
	AColor has r,g,b,a components usually with values from 0.0 to 1.0, though we don't do any check on this. 
	You can set a non zero alpha value to use transparency.
	This parameter can be null if you don't want to use a color per instance.
	Using a non-null pColors value with a non-null pViewportMaterials or pMaxMaterials is not advised as we will use only either the material or the colors but not both.
	*/
	AColor*				pColors							= nullptr;

	/**numColors is the number of elements in pColors, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numColors						= 0;

	/**pVertexColorsAsColor is an array of Color, i.e : an rgb color with no alpha. With r,g,b usually with values from 0.0 to 1.0. There is no alpha on vertex colors.
	We override the Vertex Colors from the whole geometry with a single Vertex Color value per instance.
	At this time, there is a limitation : Vertex Colors override will work only on map channels which have not been overriden when you show both in the viewport.
	This parameter can be null if the Vertex Colors of each instance are not overriden.
	This data will be converted when written in the vertex buffer to DWORD R8G8B8X8 color as this is what 3ds Max uses internally for Vertex Colors in vertex buffers. 
	If you prefer to use directly DWORD for Vertex Colors as it's faster, please see pVertexColorsAsDWORD.
	*/
	Color*				pVertexColorsAsColor					= nullptr;

	/**numVertexColorsAsColor is the number of elements in pVertexColorsAsColor, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numVertexColorsAsColor					= 0;

	/**pVertexColorsAsDWORD is an array of 32 bits DWORD R8G8B8X8, i.e : an rgb color with no alpha. With r,g,b usually with values from 0.0 to 1.0. There is no alpha on vertex colors.
	We override the Vertex Colors from the whole geometry with a single Vertex Color value per instance.
	At this time, there is a limitation : Vertex Colors override will work only on map channels which have not been overriden when you show both in the viewport.
	This parameter can be null if the Vertex Colors of each instance are not overriden.
	If you prefer to use Color for Vertex Colors, please see pVertexColorsAsColor. This will be slower as DWORD is the format that 3ds Max uses natively for the Vertex Color 
	in vertex buffers so we will convert from Color to DWORD.
	As an helper, to convert from r,g,b being bytes, with values from 0 to 255, to 32 bits DWORD R8G8B8X8, please use the macro declared above INSTANCES_R8G8B8X8_DWORD.
	*/
	DWORD*				pVertexColorsAsDWORD			= nullptr;

	/**numVertexColorsAsDWORD is the number of elements in pVertexColorsAsDWORD, it can be less than numInstances, in that case, we will loop through the data when filling the vertex buffer.
	*/
	size_t				numVertexColorsAsDWORD			= 0;
};

/** InstanceRenderGeometry is an extension of IRenderGeometry dealing with instancing. 

	Link with the optimesh.lib library to use this in your plugin.

	You can replace any call using a IRenderGeometry by this class, so the geometry is filled the usual way (vertex buffers, stream etc.).
	IRenderGeometry and InstanceRenderGeometry have identical methods related to the geometry except SetStreamRequirement where we fixed the typo on SetSteamRequirement.

	And you can instantiate this geometry by using the methods below.
	You can override matrices, positions, orientations, scales, material, UVs map channel (any up to 8), Vertex Colors or a color per instance.
	The number of instances and the number of elements passed from an array to be applied on instances can be different.
	Say, you have 100 instances. You are setting 100 positions for instances and 50 colors for instances, it's possible.
	We will loop through the data that is lower than the number of instances, say if we have 50 colors, instance 51 
	will use color#0, ... Looping.
	
	For instances to be visible, at least an array of matrices or positions must be provided to differentiate the instances positions visually.
	
	About materials, there are different situation where this class can be used :
	- You want to keep the original node's material. In that case don't set the members InstanceData::numColors or InstanceData::pColors and don't set InstanceData::numViewportMaterials 
		or InstanceData::pViewportMaterials and don't set InstanceData::numMaxMaterials or InstanceData::pMaxMaterials and we will use by default the node's material on all instances.
	- You want the instances to use a flat color shading, use the members InstanceData::numColors and InstanceData::pColors and it will display a flat color per instance ignoring the original 
		node's material. Transparency can be applied on this flat colors shading.
	- You want to use a (possibly) different material per instance ignoring the node's original material, then use the members InstanceData::numViewportMaterials 
		and InstanceData::pViewportMaterials or InstanceData::numMaxMaterials and InstanceData::pMaxMaterials.

	Performance considerations in the InstanceData struct :
	- Using InstanceData::pMatrices directly is faster than using InstanceData::pPositions/InstanceData::pOrientationsAsPoint4/InstanceData::pOrientationsAsQuat/InstanceData::pScales 
		as we will convert them into a Matrix3 in the vertex buffer by combining the position/orientation and scale. Though the conversion is not very expansive.
	
	About InstanceData members pointers ownership : we never take the ownership of the arrays provided and we immediately build the instance vertex buffer from that data.
	Except for the colors (InstanceData::pColors member) and materials where local copies of the arrays are done.

	When overriding the materials per instance we do a for loop on each instance and apply the material on it before drawing it. So it's slower than using DrawInstances which we use with 
	either the original material or the color per instance override.
	So changing the materials per instance often in an animation is not advised in term of performance. 

	an example of usage is :
	\code
	//From the header file :
	class InstanceObject : public SimpleObject2, public MaxSDK::Graphics::IObjectDisplay2:
	{
		...
		//From MaxSDK::Graphics::IObjectDisplay2
		virtual bool PrepareDisplay(const MaxSDK::Graphics::UpdateDisplayContext& prepareDisplayContext) override;
		virtual bool UpdatePerNodeItems(const MaxSDK::Graphics::UpdateDisplayContext& updateDisplayContext, MaxSDK::Graphics::UpdateNodeContext& nodeContext, MaxSDK::Graphics::IRenderItemContainer& targetRenderItemContainer)override;

		bool												mInstanceDataWasUpdated = false;
		MaxSDK::Graphics::InstanceRenderGeometry*			mpShadedInstanceRenderGeometry   = nullptr;
		MaxSDK::Graphics::InstanceRenderGeometry*			mpWireframeInstanceRenderGeometry= nullptr;
		//Instance data stored
		std::vector<Point3>                                 mInstancesPositions;
		std::vector<Point3>                                 mInstancesScales;
		std::vector<Point4>                                 mInstancesOrientations;
		std::vector<AColor>                                 mInstancesColors;
		std::vector<MaxSDK::Graphics::BaseMaterialHandle>   mInstancesMaterials;
	};

	//From the cpp file
	bool InstanceObject::PrepareDisplay(const MaxSDK::Graphics::UpdateDisplayContext& displayContext)
	{
		//Update instance data so it gets animated if the time has changed
		const TimeValue t = displayContext.GetDisplayTime();
		if (mLastTimeInstanceDataUpdated != t){
			Instancing::UpdateInstanceMatricesData		(mInstancesMatrixAndUVData		, t);//This updates all position/orientation/scale from the matrices using current time
			Instancing::UpdateInstancePositionData      (mInstancesPositions			, t);//This updates all positions using current time
			Instancing::UpdateInstanceScaleData         (mInstancesScales				, t);//This updates all scales using current time
			Instancing::UpdateInstanceOrientationData   (mInstancesOrientations			, t);//This updates all orientations using current time
			Instancing::UpdateInstanceColorData         (mInstancesColors				, t);//This updates all colors using current time
			Instancing::UpdateInstanceVertexColorData   (mInstancesVertexColorsAsColor	, t);//This updates all VertexColors using current time
		
			mInstanceDataWasUpdated		 = true;
			mLastTimeInstanceDataUpdated = t;
		}

		return true;
	}

	//Create the instance vertex buffer, this has to be called once per InstanceRenderGeometry
	void InstanceObject::CreateInstanceData(InstanceRenderGeometry* pInstanceRenderGeometry)
	{
		DbgAssert(pInstanceRenderGeometry);
		if (nullptr == pInstanceRenderGeometry){
			return;
		}

		InstanceData data;

		data.bTransformationsAreInWorldSpace	= false;//Are relative to the node's TM
		data.numInstances						= mInstancesPositions.size();

		data.pMatrices							= mInstancesMatrixAndUVData.mMat.data(); //Give matrices with pos/orientation/scale for each instance
		data.numMatrices						= mInstancesMatrixAndUVData.mMat.size();

		data.pUVWMapChannel1					= mInstancesMatrixAndUVData.mUV1.data();//Override map channel 1 for each instance
		data.numUVWMapChannel1					= mInstancesMatrixAndUVData.mUV1.size();

		data.pUVWMapChannel3					= mInstancesMatrixAndUVData.mUV3.data();//Override map channel 3 for each instance
		data.numUVWMapChannel3					= mInstancesMatrixAndUVData.mUV3.size();
	
		data.pVertexColorsAsColor				= mInstancesVertexColorsAsColor.data();//Override VertexColors for each instance
		data.numVertexColorsAsColor				= mInstancesVertexColorsAsColor.size();

		//For creation
		pInstanceRenderGeometry->CreateInstanceVertexBuffer(data);
	}

	//Update the instance vertex buffer, this has to be called each time the instance data is updated
	void InstanceObject::UpdateInstanceData(InstanceRenderGeometry* pInstanceRenderGeometry)
	{
		DbgAssert(pInstanceRenderGeometry);
		if (nullptr == pInstanceRenderGeometry){
			return;
		}

		InstanceData data;

		//We are ignoring data.bTransformationsAreInWorldSpace and data.numInstances which should not have changed when doing an animation
		//if so, use InstanceRenderGeometry::CreateInstanceVertexBuffer

		data.pMatrices					= mInstancesMatrixAndUVData.mMat.data();//Update position/orientation/scale from the matrices array
		data.numMatrices				= mInstancesMatrixAndUVData.mMat.size();
	
		//Map channel 1 is not animated so ignored (it will be kept as it is from the InstanceRenderGeometry::CreateInstanceVertexBuffer call)
		//No need to provide its data again

		//Update map channel 3 
		data.pUVWMapChannel3			= mInstancesMatrixAndUVData.mUV3.data();
		data.numUVWMapChannel3			= mInstancesMatrixAndUVData.mUV3.size();

		//Update VertexColors
		data.pVertexColorsAsColor		= mInstancesVertexColorsAsColor.data();
		
		//For updates
		pInstanceRenderGeometry->UpdateInstanceVertexBuffer(data);
	}

	bool InstanceObject::UpdatePerNodeItems(const UpdateDisplayContext& updateDisplayContext, UpdateNodeContext& nodeContext, IRenderItemContainer& targetRenderItemContainer)
	{
		const TimeValue t						= updateDisplayContext.GetDisplayTime();
		const unsigned long requirementFlags	= updateDisplayContext.GetRequiredComponents();
		const bool bRequireSolidMesh			= (requirementFlags & ObjectComponentSolidMesh) != 0;
		const bool bRequireWireframe			= (requirementFlags & ObjectComponentWireframe) != 0;
	
		const MaterialRequiredStreams& materialRequiredStreams = updateDisplayContext.GetRequiredStreams();
		
		const bool bNeedToRecreateGeometryVertexBuffers = (mLastMaterialRequiredStreams != materialRequiredStreams);
		if (bNeedToRecreateGeometryVertexBuffers){
			mLastMaterialRequiredStreams		= materialRequiredStreams;
		}

		if (bRequireSolidMesh){
			static const bool sbWireFrame = false;//For clarity

			bool bInstanceVertexBufferWasCreated = false;
			if((nullptr == mpShadedInstanceRenderGeometry) || bNeedToRecreateGeometryVertexBuffers){
				mLastMaterialRequiredStreams	= materialRequiredStreams;//store this material required stream
				mpShadedInstanceRenderGeometry	= new InstanceRenderGeometry;
				CreateGeometry(mpShadedInstanceRenderGeometry, sbWireFrame, bForceAddingUVs, materialRequiredStreams);
				CreateInstanceData(mpShadedInstanceRenderGeometry);
				bInstanceVertexBufferWasCreated = true;
			}

			if (mInstanceDataWasUpdated && false == bInstanceVertexBufferWasCreated){
				//Update instance data when there is an animation
				UpdateInstanceData(mpShadedInstanceRenderGeometry);//Update instance vertex buffer
			}

			//Always add the render items
			mpShadedInstanceRenderGeometry->GenerateInstances(sbWireFrame, updateDisplayContext, nodeContext, targetRenderItemContainer);
		}

		if (bRequireWireframe){
			static const bool sbWireFrame = true;//For clarity
		
			bool bInstanceVertexBufferWasCreated = false;

			if  ((nullptr == mpWireframeInstanceRenderGeometry) || bNeedToRecreateGeometryVertexBuffers){
				mpWireframeInstanceRenderGeometry = new InstanceRenderGeometry;
				CreateGeometry(mpWireframeInstanceRenderGeometry, sbWireFrame, bForceAddingUVs, materialRequiredStreams);
				CreateInstanceData(mpWireframeInstanceRenderGeometry);
				bInstanceVertexBufferWasCreated = true;
			}

			if (mInstanceDataWasUpdated && false == bInstanceVertexBufferWasCreated){
				//Update instance data when there is an animation
				UpdateInstanceData(mpWireframeInstanceRenderGeometry);//Update instance vertex buffer
			}

			//Always add the render items
			mpWireframeInstanceRenderGeometry->GenerateInstances(sbWireFrame, updateDisplayContext, nodeContext, targetRenderItemContainer);
		}
    
		return true;
	}

	\endcode
*/

class InstanceRenderGeometry : public IRenderGeometry, public MaxSDK::Util::Noncopyable
{
public:
	DllExport InstanceRenderGeometry();
	DllExport virtual ~InstanceRenderGeometry();

	/** Create the vertex buffer with instances data. We don't do any local copy of the data you pass (except colors and materials), we use it directly to build the vertex buffer for instances.
	This does a full rebuild of the instance vertex buffer. So you should call this the first time you pass instance data or when the number of instances has changed.
	\param[in] data : a const reference on InstanceData
	*/
	DllExport virtual void CreateInstanceVertexBuffer(const InstanceData& data);

	/** Update the instances data. We don't do any local copy of the data (except colors and materials), we use it directly to update the vertex buffer for instances.
	Each time the data has been updated on your side, say the positions and colors of instances have been updated, then you should call this method to update the instance vertex buffer.
	In the data parameter of this method, only what needs to be updated should be non-null pointers and non zero for the number of elements.
	When calling that function, we update the data provided at the right place into the instance vertex buffer directly. We don't fully rebuild the vertex buffer.
	Say you are updating only the positions but not the colors, you should do :
	\code
	Point3* my_updated_Point3_positions_array = initialized_somewhere_else;
	InstanceData data;/by default the pColors and numcolors members will be 0.
	data.pPositions   = my_updated_Point3_positions_array;
	data.numPositions = numPositions;//numPositions is the number of elements of my_updated_Point3_positions_array
	MyInstanceRenderGeometry.UpdateInstanceVertexBuffer(data);//This will update only the positions and leave unchanged the others data set previously in a call to CreateInstanceVertexBuffer.
	\endcode
	\param[in] data : a const reference on InstanceData
	*/
	DllExport virtual void UpdateInstanceVertexBuffer(const InstanceData& data);
	
	/** Get if the matrices / positions / orientations / scales on instances are in world space or relative to the node's transform matrix.
	\return true if they are in world space, false if they are relative to the node's transform.
	*/
	DllExport virtual bool GetTransformationsAreInWorldSpace(void)const;

	/** Generates the GeometryRenderItemHandle and adds it to the targetRenderItemContainer. 
	This is an helper function when you are in an INode's method UpdatePerNodeItems(const UpdateDisplayContext& updateDisplayContext, UpdateNodeContext& nodeContext, IRenderItemContainer& targetRenderItemContainer)
	//so it creates and adds the instance render items to the container.
	\param[in] if bWireframe, is true to generate the render items for a wireframe display, false means for a solid mesh display
	\param[in] updateDisplayContext is an UpdateDisplayContext which has to be passed from UpdatePerNodeItems
	\param[in] nodeContext is UpdateNodeContext which has to be passed from UpdatePerNodeItems
	\param[in] targetRenderItemContainer is an IRenderItemContainer which has to be passed from UpdatePerNodeItems
	*/
	DllExport virtual bool GenerateInstances(bool bWireframe, const UpdateDisplayContext& updateDisplayContext, UpdateNodeContext& nodeContext, IRenderItemContainer& targetRenderItemContainer);
	
	/** Get the instance vertex buffer.
		\return the instance vertex buffer.
	*/
	DllExport virtual VertexBufferHandle GetInstanceVertexBuffer(void) const;

	/** Get the instance stream format from vertex buffer.
		\return the stream instance data from vertex buffer.
	*/
	DllExport virtual const MaterialRequiredStreams& GetInstanceStream(void) const;
	
	/** Get the geometry stream format from vertex buffer.
		\return the geometry stream format from vertex buffer.
	*/
	DllExport virtual const MaterialRequiredStreams& GetGeometryStream(void) const;
	
	/** This function might be called multiple times in a frame. Inherited classes need to
	use in the pipeline context to render the geometry. It's recommend to prepare geometry
	data in another function, and only do rendering tasks in the Display() function.
	Furthermore, sub-classes are not allowed to change current material parameters. So for
	multiple material instance, you should use multiple render items to store them.
	Note: the vertex buffers' format must match current stream requirement in pipeline context.
	/param drawContext the context for display
	/param start start primitive to render
	/param count primitive count to render
	/param lod current lod value from the adaptive degradation system
	*/
	DllExport virtual void Display(DrawContext& drawContext, int start, int count, int lod) override;

	/** Get the type of primitives in the geometry.
	/return the geometry's primitive type
	*/
	DllExport virtual PrimitiveType GetPrimitiveType() override;
		
	/** Sets type of primitives in the geometry.
	/param type the geometry's primitive type
	*/
	DllExport virtual void SetPrimitiveType(PrimitiveType type) override;

	/** Number of primitives the mesh represents.
	/return geometry's primitive count
	*/
	DllExport virtual size_t GetPrimitiveCount() override;

	/** Set the number of primitives in the geometry.
		\param[in] count the number of primitives
	*/
	DllExport virtual void SetPrimitiveCount(size_t count);

	/** Number of vertices in the mesh.
	/return number of vertices in the mesh.
	*/
	DllExport virtual size_t GetVertexCount() override;
		
	/** Get start primitive of this geometry.
	\return The index of the start primitive.
	*/
	DllExport virtual int GetStartPrimitive() const override;

	/** Set the start primitive offset for drawing.
	\param[in] offset this offset will pass to Display() function
	*/
	DllExport virtual void SetStartPrimitive(int offset);
	
	/** Get the stream requirement of this render geometry.
	To optimize performance, it's better to create a requirement-geometry mapping. 
	And make the render geometry read-only after created.
	\return the stream requirement which this geometry built with.
	*/
	DllExport virtual MaterialRequiredStreams& GetSteamRequirement()override;

	/** Set the stream requirement of this render geometry.
		\param[in] streamFormat the stream requirement which this geometry built with.
	*/
	DllExport virtual void SetStreamRequirement(const MaterialRequiredStreams& streamFormat);

	/** Get the vertex streams of this geometry
	\return vertex streams of this geometry
	*/
	DllExport virtual VertexBufferHandleArray& GetVertexBuffers()override;

	/** Get index buffer of this geometry.
	\return index buffer of this geometry. Might be invalid if the geometry doesn't need index buffer
	*/
	DllExport virtual IndexBufferHandle& GetIndexBuffer()override;

	/** Set index buffer of this geometry.
		\param[in] indexBuffer index buffer of this geometry. 
	*/
	DllExport virtual void SetIndexBuffer(const IndexBufferHandle& indexBuffer);

	/** Add a vertex buffer to this geometry.
		\param[in] vertexBuffer : the vertex buffer to be added. 
	*/
	DllExport virtual void AddVertexBuffer(const VertexBufferHandle& vertexBuffer);

	/** Remove the index-th geometry vertex buffer.
		\param[in] index the index of the geometry vertex buffer to be removed
	*/
	DllExport virtual void RemoveVertexBuffer(size_t index);

	/** Get the number of geometry vertex buffers.
		\return	the number of geometry vertex buffers.
	*/
	DllExport virtual size_t GetVertexBufferCount() const;

	/** Get the index-th vertex buffer from the geometry.
		\param[in] index the index of the geoemtry vertex buffer
		\return the index-th geometry vertex buffer.
	*/
	DllExport virtual VertexBufferHandle GetVertexBuffer(size_t index) const;

	/** Retrieve an InterfaceID, is reserved for internal usage only.
	*/
	DllExport virtual BaseInterface* GetInterface(Interface_ID id);
		
private:
	///Private implementation of this class.
	InstanceRenderGeometryImpl* mpImpl;
};

/** Generate the instance render item from a tuple mesh which is an InstanceRenderGeometry class.
	This is an extension of the method above with more overridable data for instances. The method above could only
	override the transform matrix in world space and UV from the map channel #1.
	The InstanceRenderGeometry class lets you override more data per instance. For more details, please 
	see the header file maxsdk\include\Graphics\InstanceRenderGeometry.h

	\param hInstanceRenderItem the instance render item handle which can replace original tuple mesh render item.
	\param hTupleMeshHandle handle of a tuple mesh render item
	\param pInstanceRenderGeometry : an InstanceRenderGeometry* 
	\return true if successful created the instance render item.
*/
	DllExport bool GenerateInstanceRenderItem(
	RenderItemHandle& hInstanceRenderItem,
	const RenderItemHandle& hTupleMeshHandle,
	const InstanceRenderGeometry* pInstanceRenderGeometry);
}

} // namespace
