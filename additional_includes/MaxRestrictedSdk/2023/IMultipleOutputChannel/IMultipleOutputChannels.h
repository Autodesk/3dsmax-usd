//*********************************************************************/
// Copyright (c) 2009-2020 Autodesk, Inc.
// All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
//*********************************************************************/

#pragma once

#define IMULTIPLEOUTPUTCHANNELS_INTERFACE Interface_ID(0x43147cc9, 0x600e29ff)

#include "../../maxsdk/include/strclass.h"
#include "../../maxsdk/include/ifnpub.h"
#include <iparamb3.h>

//! \brief This notification is sent to dependents when a IMultipleOutputChannels's output channel list changes.
/*!  It is sent by IMultipleOutputChannels-derived objects to tell dependents when the number or ordering 
of output channels changes, so those objects can keep pointing at the correct output channel.  
The PartID is a pointer to a Tab of MultiOutputChannelNumberChanged structure instances (defined in 
IMultipleOutputChannels.h) in which each element contains an old-to-new mapping. A new channel index 
of -1 implies the channel was removed. A old channel index of -1 implies the channel was added.
IMultipleOutputChannelsConsumerWrapper-derived objects typically consume this notification.
NOTE: If you send this message, the 'propagate' argument of NotifyDependents must be false. 
Otherwise, dependents of dependents think that their ref's output channel list is changing.*/
#define REFMSG_MULTIOUTPUT_CHANNEL_NUMBER_CHANGED 				REFMSG_USER+0x13654850

#define REFMSG_MULTIOUTPUT_CHANNEL_NEEDUPDATE 					REFMSG_USER+0x13654851

//-------------------------------------------------------------
//! \brief An interface for objects that that expose multiple output channels of various types that can be recognized by 3ds Max.
/*! \sa Class IMultipleOutputChannelsConsumer, REFMSG_MULTIOUTPUT_CHANNEL_NUMBER_CHANGED, imultioutput_interface
This interface provides support for exposing multiple output channels. Each output channel has its own data type. 

This interface is primarily to support MetaSL and MR objects that have multiple outputs (for example, an XYZ and a derivative XYZ). Those objects will expose each output as a Texmap.

Basic ParamBlock2 data types are supported by the interface, but not Tab data types. The interface could be expanded to handle Tab data types.

Note: if you derive from this interface, see notification code REFMSG_MULTIOUTPUT_CHANNEL_NUMBER_CHANGED for information on how this notification should be sent or processed.

Note: Classes that derive from this interface need to manually add the imultioutput_interface FPInterfaceDesc to their ClassDesc using ClassDesc::AddInterface. This is typically performed in the ClassDesc::Create method.
*/
#pragma warning(push)
#pragma warning(disable:4100) // Unused parameters. 

class IMultipleOutputChannels: public FPMixinInterface
{
public:
	//! \brief Returns the number of output channels the object exposes.
	/*! The plugin implements this method to indicate the number of output channels it exposes.
	\return The number of output channels the object exposes.
	*/
	virtual int GetNumIMultipleOutputChannels() const = 0;

	//! \brief Returns the localized name for the specified output channel.
	/*! The plugin implements this method to provide the localized names of the output channels it exposes. These names will be used in the UI.
	\param[in] index - The index of the output channel.
	\return The localized name for the specified output channel.
	*/
	virtual MSTR GetIMultipleOutputChannelLocalizedName( int index ) const = 0;

	//! \brief Returns the non-localized name for the specified output channel.
	/*! The plugin implements this method to provide the non-localized names of the output channels it exposes. These would typically be used in scripts to provide locale independence.
	\param[in] index - The index of the output channel.
	\return The non-localized name for the specified output channel.
	*/
	virtual MSTR GetIMultipleOutputChannelName( int index ) const = 0;

	//! \brief Returns the Parameter type for the specified output channel.
	/*! The plugin implements this method to provide the parameter type of the output channels it exposes. The type can be used for parameter validation
	between input and output channels.
	\param[in] index - The index of the output channel.
	\return The parameter type of the output channel.
	Note: the data type will correspond to one of the data types supported by the ParamBlock2 system, tabs are not supported
	*/
	virtual ParamType3 GetIMultipleOutputChannelType( int index) const =0;

	//! \brief Data structure for REFMSG_MULTIOUTPUT_CHANNEL_NUMBER_CHANGED notifications
	/*! A REFMSG_MULTIOUTPUT_CHANNEL_NUMBER_CHANGED notification sends a Tab<MultiOutputChannelNumberChanged>*, which provides old-to-new output channel indexing. 
	An old index value of -1 means the output channel is new, a new output index of -1 means the old output channel was removed.
	*/
	struct MultiOutputChannelNumberChanged
	{
		int oldIndex;
		int newIndex;
	};

	// FPMixinInterface exposure
	// FP-published function IDs 
	enum {  getnumoutputs, getlocalizedoutputname, getoutputname };
	// FP-published symbolic enumerations

#pragma warning(push)
#pragma warning(disable:4238)
	BEGIN_FUNCTION_MAP
		RO_PROP_FN(getnumoutputs, GetNumIMultipleOutputChannels, TYPE_INT);
		FN_1( getlocalizedoutputname,	TYPE_TSTR_BV,		MXS_GetIMultipleOutputChannelLocalizedName,	TYPE_INDEX );
		FN_1( getoutputname,			TYPE_TSTR_BV,		MXS_GetIMultipleOutputChannelName,	TYPE_INDEX );
	END_FUNCTION_MAP
#pragma warning(pop)

	FPInterfaceDesc* GetDesc() override;    // <-- must implement 

protected:
	// FPS exposure stub methods
	void ValidateIMultipleOutputChannelIndexValue(int index)  const 
	{
		if (index < 0 || index >= GetNumIMultipleOutputChannels())
			throw MAXException(_M("Invalid IMultiOutput channel index"));
	}
private:
	MSTR MXS_GetIMultipleOutputChannelLocalizedName( int index )  const 
	{
		ValidateIMultipleOutputChannelIndexValue(index);
		return GetIMultipleOutputChannelLocalizedName(index);
	}
	MSTR MXS_GetIMultipleOutputChannelName( int index )  const 
	{
		ValidateIMultipleOutputChannelIndexValue(index);
		return GetIMultipleOutputChannelName(index);
	}
};


#pragma warning(pop)
//-------------------------------------------------------------
//! \brief Function Publishing descriptor for Mixin interface on IMultipleOutputChannels-derived classes
/*! \sa Class IMultipleOutputChannels 
This interface needs to be manually added to the ClassDesc for IMultipleOutputChannels-derived objects using ClassDesc::AddInterface. This is typically performed in the ClassDesc::Create method.

*/
static FPInterfaceDesc imultioutput_interface(
	IMULTIPLEOUTPUTCHANNELS_INTERFACE, _T("iMultipleOutputChannels"), 0, nullptr, FP_MIXIN,
	IMultipleOutputChannels::getlocalizedoutputname, _T("getIMultipleOutputChannelLocalizedName"), 0, TYPE_TSTR_BV, FP_NO_REDRAW, 1,
		_T("index"),		0,	TYPE_INDEX,  
	IMultipleOutputChannels::getoutputname, _T("getIMultipleOutputChannelName"), 0, TYPE_TSTR_BV, FP_NO_REDRAW, 1,
		_T("index"),		0,	TYPE_INDEX,  

	properties,
	IMultipleOutputChannels::getnumoutputs, FP_NO_FUNCTION, _T("numIMultipleOutputChannels"),FP_NO_REDRAW,TYPE_INT,

	p_end
);
