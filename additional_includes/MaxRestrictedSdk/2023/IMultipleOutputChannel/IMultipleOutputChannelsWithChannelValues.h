//*********************************************************************/
// Copyright (c) 2009-2020 Autodesk, Inc.
// All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
//*********************************************************************/

#pragma once

#define IMULTIPLEOUTPUTCHANNELS_WITH_VALUES_INTERFACE Interface_ID(0x24280bd5, 0x22b1edca)

#include "IMultipleOutputChannels.h"
#include "../../maxsdk/include/paramtype.h"
#include "../../maxsdk/include/strclass.h"
#include "../../maxsdk/include/ifnpub.h"
#include "../../maxsdk/include/interval.h"

// forward declarations
class Point3;
class Point4;
class Color;
class AColor;
class Mtl;
class Texmap;
class PBBitmap;
class INode;
class ReferenceTarget;
class IParamBlock2;
class Matrix3;
namespace MaxSDK
{
	namespace AssetManagement
	{
		class AssetUser;
	}
}

//-------------------------------------------------------------
//! \brief An interface for objects that that expose multiple output channels of various types that can be recognized by 3ds Max.
/*! \sa Class IMultipleOutputChannelsConsumer, REFMSG_MULTIOUTPUT_CHANNEL_NUMBER_CHANGED, imultioutput_interface
This interface provides support for exposing multiple output channels. Each output channel has its own data type. 

This interface is primarily to support MetaSL and MR objects that have multiple outputs (for example, an XYZ and a derivative XYZ). Those objects will expose each output as a Texmap.

Basic ParamBlock2 data types are supported by the interface, but not Tab data types. The interface could be expanded to handle Tab data types.

Note: if you derive from this interface, see notification code REFMSG_MULTIOUTPUT_CHANNEL_NUMBER_CHANGED for information on how this notification should be sent or processed.

Note: Classes that derive from this interface need to manually add the imultioutput_with_values_interface FPInterfaceDesc to their ClassDesc using ClassDesc::AddInterface. This is typically performed in the ClassDesc::Create method.
*/
#pragma warning(push)
#pragma warning(disable:4100) // Unused parameters. 

class IMultipleOutputChannelsWithChannelValues: public IMultipleOutputChannels
{
public:
	//! \brief Indicates whether a REFMSG_CHANGED notification received from the object should be propagated from dependents using just the channel output value.
	/*! The plugin implements this method to optimize REFMSG_CHANGE propagation. An object using an output channel value from this object will receive REFMSG_CHANGE notifications for
	all changes to this object, even if that change does not affect the contents of the value retrieved from an output channel. The REFMSG_CHANGE notification does not need to 
	propagate from that object since its dependents are not affected by the change. If this method returns false, the object can return REF_STOP from its NotifyRefChanged if this object
	is the target object.
	\param[in] index - The index of the output channel.
	\return false if a REFMSG_CHANGE notification from this object can be blocked from propagating from objects using just the specified output channel's data value.
	*/
	virtual bool GetIMultipleOutputChannelValueChanged( int index) const { return true; } 

	//! \brief Returns the output data value for the specified output channel.
	/*! The plugin implements this method to return the output channel's data value as an FPValue.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return The data value for the specified output channel as an FPValue.
	*/
	virtual FPValue GetIMultipleOutputChannelValue( int index, TimeValue t, Interval& ivalid ) const = 0;

	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, float& v, Interval& ivalid ) const { return false; } // TYPE_FLOAT
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, int& 			v, Interval& ivalid ) const { return false; } // TYPE_INT
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, Color&			v, Interval& ivalid ) const { return false; } // TYPE_RGBA
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, Point3&			v, Interval& ivalid ) const { return false; } // TYPE_POINT3
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, const MCHAR*&	v, Interval& ivalid ) const { return false; } // TYPE_STRING
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, const MaxSDK::AssetManagement::AssetUser *&	v, Interval& ivalid ) const { return false; } // TYPE_FILENAME
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, Mtl*& 			v, Interval& ivalid ) const { return false; } // TYPE_MTL
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, Texmap*&		v, Interval& ivalid ) const { return false; } // TYPE_TEXMAP
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, PBBitmap*&		v, Interval& ivalid ) const { return false; } // TYPE_BITMAP
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, INode*&			v, Interval& ivalid ) const { return false; } // TYPE_INODE
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, ReferenceTarget*& v, Interval& ivalid ) const { return false; } // TYPE_REFTARG
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, Matrix3&		v, Interval& ivalid ) const { return false; } // TYPE_MATRIX3
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, IParamBlock2*&	v, Interval& ivalid ) const { return false; } // TYPE_PBLOCK2
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, Point4&			v, Interval& ivalid ) const { return false; } // TYPE_POINT4
	//! \brief Access the output data value for the specified output channel.
	/*! The plugin implements this method to access the output channel's data value as a float.
	\param[in] index - The index of the output channel.
	\param[in] t The time at which to get the value.
	\param[out] v - The value to retrieve is returned here.
	\param[out] ivalid - The validity interval of the value retrieved is returned here.
	\return true if the output value was written to v.
	*/
	virtual bool GetIMultipleOutputChannelValue( int index, TimeValue t, AColor&			v, Interval& ivalid ) const { return false; } // TYPE_FRGBA

	// FPMixinInterface exposure
	// FP-published function IDs 
	enum {  getoutputtype = IMultipleOutputChannels::getoutputname+1, getoutputvalue };
	// FP-published symbolic enumerations
	enum
	{
		outputChannelType,
	};
#pragma warning(push)
#pragma warning(disable:4238)
	BEGIN_FUNCTION_MAP_PARENT(IMultipleOutputChannels)
		FN_1( getoutputtype,			TYPE_ENUM,			MXS_GetIMultipleOutputChannelType,	TYPE_INDEX );
		FN_2( getoutputvalue,			TYPE_FPVALUE_BV,	MXS_GetIMultipleOutputChannelValue,	TYPE_INDEX, TYPE_TIMEVALUE );
	END_FUNCTION_MAP
#pragma warning(pop)

	FPInterfaceDesc* GetDesc();    // <-- must implement 

private:
	DWORD MXS_GetIMultipleOutputChannelType( int index )  const 
	{
		ValidateIMultipleOutputChannelIndexValue(index);
		return GetIMultipleOutputChannelType(index);
	}
	FPValue MXS_GetIMultipleOutputChannelValue( int index, TimeValue t )  const 
	{
		ValidateIMultipleOutputChannelIndexValue(index);
		Interval valid;
		return GetIMultipleOutputChannelValue(index, t, valid);
	}
};


#pragma warning(pop)
//-------------------------------------------------------------
//! \brief Function Publishing descriptor for Mixin interface on iMultipleOutputChannelsWithValues-derived classes
/*! \sa Class IMultipleOutputChannels 
This interface needs to be manually added to the ClassDesc for iMultipleOutputChannelsWithValues-derived objects using ClassDesc::AddInterface. This is typically performed in the ClassDesc::Create method.

*/
static FPInterfaceDesc imultioutput_with_values_interface(
	IMULTIPLEOUTPUTCHANNELS_WITH_VALUES_INTERFACE, _T("iMultipleOutputChannelsWithValues"), 0, NULL, FP_MIXIN,
	IMultipleOutputChannels::getlocalizedoutputname, _T("getIMultipleOutputChannelLocalizedName"), 0, TYPE_TSTR_BV, FP_NO_REDRAW, 1,
		_T("index"),		0,	TYPE_INDEX,  
	IMultipleOutputChannels::getoutputname, _T("getIMultipleOutputChannelName"), 0, TYPE_TSTR_BV, FP_NO_REDRAW, 1,
		_T("index"),		0,	TYPE_INDEX,  
	IMultipleOutputChannelsWithChannelValues::getoutputtype, _T("getIMultipleOutputChannelType"), 0, TYPE_ENUM, IMultipleOutputChannelsWithChannelValues::outputChannelType, FP_NO_REDRAW, 1,
		_T("index"),		0,	TYPE_INDEX,  
	IMultipleOutputChannelsWithChannelValues::getoutputvalue, _T("getIMultipleOutputChannelValue"), 0, TYPE_FPVALUE_BV, FP_NO_REDRAW, 2,
		_T("index"),		0,	TYPE_INDEX,  
		_T("time"),			0,	TYPE_TIMEVALUE,  f_keyArgDefault, 0,

	properties,
	IMultipleOutputChannels::getnumoutputs, FP_NO_FUNCTION, _T("numIMultipleOutputChannels"),FP_NO_REDRAW,TYPE_INT,

	// symbolic enumerations
	enums,
	IMultipleOutputChannelsWithChannelValues::outputChannelType, 15,
		_T("float"),		TYPE_FLOAT,
		_T("integer"),		TYPE_INT,
		_T("rgb"),			TYPE_RGBA,
		_T("point3"),		TYPE_POINT3,
		_T("string"),		TYPE_STRING,
		_T("filename"),		TYPE_FILENAME,
		_T("material"),		TYPE_MTL,
		_T("texturemap"),	TYPE_TEXMAP,
		_T("bitmap"),		TYPE_BITMAP,
		_T("node"),			TYPE_INODE,
		_T("maxObject"),	TYPE_REFTARG,
		_T("matrix3"),		TYPE_MATRIX3,
		_T("paramblock2"),	TYPE_PBLOCK2,
		_T("point4"),		TYPE_POINT4,
		_T("frgba"),		TYPE_FRGBA,
	p_end
);
