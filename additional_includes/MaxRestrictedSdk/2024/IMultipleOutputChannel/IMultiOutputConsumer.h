//*********************************************************************/
// Copyright (c) 2009-2020 Autodesk, Inc.
// All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
//*********************************************************************/

#pragma once

#include "../../maxsdk/include/ifnpub.h"
#include <iparamb3.h>
#define IMULTIOUTPUT_CONSUMER_INTERFACE Interface_ID( 0x6d4a30ed, 0x61024d74 )

#define IMULTIOUTPUT_CONSUMER_NO_OUTPUT_INDEX ( -1 )

#define REFMSG_MULTIOUTPUT_CONSUMER_NEEDUPDATE REFMSG_USER + 0x13654852

// forward declarations
class ReferenceTarget;
class ReferenceMaker;

//! \brief An interface for objects that reference other objects that implement IMultipleOutputChannels and acquire output channel data from those objects.
/*! \sa Class IMultipleOutputChannels, REFMSG_MULTIOUTPUT_CHANNEL_NUMBER_CHANGED
	An object can acquire output channel data from another object by requesting the IMultipleOutputChannels interface from that object, and then calling
	GetOutputChannelValue(int index) on that interface. This interface provides read/write access to the specification of which source objects implementing 
	IMultipleOutputChannels are being used and which output channel indices to use on those objects. 
	This interface is used by Schematic Material Editor to help create, modify, and display objects deriving from this interface.
*/

class IMultiOutputConsumer: public FPMixinInterface
{
public:
	
	virtual int  GetNumInputs() const = 0; 
	virtual bool SetOutputToInput( int input_index, ReferenceTarget* output_rtarg, int output_index ) = 0;
	virtual bool GetOutputFromInput( int input_index, ReferenceTarget*& output_rtarg, int& output_index ) const = 0;
	virtual bool CanAssignOutputToInput( int input_index, ReferenceTarget* output_rtarg, int output_index ) const = 0;
	virtual MSTR GetInputName( int input_index ) const = 0;
	virtual MSTR GetInputLocalizedName( int input_index ) const = 0;
	virtual ParamType3 GetInputType( int input_index ) const = 0;
	
	FPInterfaceDesc* GetDesc() override;    // <-- must implement 
private:

	ReferenceTarget* MXS_GetOutputRefTargetFromInput( int input_index )
	{
		ReferenceTarget* rtarg = nullptr; 
		int output_index = IMULTIOUTPUT_CONSUMER_NO_OUTPUT_INDEX; 
		if( GetOutputFromInput( input_index, rtarg, output_index ) )
		{
			return rtarg; 
		}
		return nullptr; 
	}

	int MXS_GetOutputIndexFromInput( int input_index )
	{
		ReferenceTarget* rtarg = nullptr; 
		int output_index = IMULTIOUTPUT_CONSUMER_NO_OUTPUT_INDEX; 
		if( GetOutputFromInput( input_index, rtarg, output_index ) )
		{
			return output_index; 
		}
		return IMULTIOUTPUT_CONSUMER_NO_OUTPUT_INDEX; 
	}

public:

	// FPMixinInterface exposure
	// FP-published function IDs 
	enum {  
		imultioutputconsumer_getnuminputs, 
		imultioutputconsumer_setoutputtoinput, 
		imultioutputconsumer_getoutputrtargfrominput, 
		imultioutputconsumer_getoutputindexfrominput, 
		imultioutputconsumer_canassignoutputtoinput, 
		imultioutputconsumer_getinputname,
		imultioutputconsumer_getinputlocalizedname
	};

	#pragma warning(push)
	#pragma warning(disable:4238)
		BEGIN_FUNCTION_MAP
			RO_PROP_FN( imultioutputconsumer_getnuminputs,						GetNumInputs, TYPE_INT );
			FN_3( imultioutputconsumer_setoutputtoinput,		 TYPE_bool,		SetOutputToInput,	TYPE_INDEX, TYPE_REFTARG, TYPE_INDEX );
			FN_1( imultioutputconsumer_getoutputrtargfrominput,  TYPE_REFTARG,	MXS_GetOutputRefTargetFromInput,	TYPE_INDEX );
			FN_1( imultioutputconsumer_getoutputindexfrominput,  TYPE_INDEX,	MXS_GetOutputIndexFromInput,	TYPE_INDEX );
			FN_3( imultioutputconsumer_canassignoutputtoinput,	 TYPE_bool,		CanAssignOutputToInput,	TYPE_INDEX, TYPE_REFTARG, TYPE_INDEX );
			FN_1( imultioutputconsumer_getinputname,			 TYPE_TSTR_BV,	GetInputName,	TYPE_INDEX );
			FN_1( imultioutputconsumer_getinputlocalizedname,	 TYPE_TSTR_BV,	GetInputLocalizedName,	TYPE_INDEX );
		END_FUNCTION_MAP
	#pragma warning(pop)
};


// ----------------------------------------------------------------------------
/** \brief Function Publishing descriptor for Mixin interface on
 * IMultipleOutputChannels-derived classes  
 * 
 * This interface needs to be manually added to the ClassDesc for
 * IMultipleOutputChannels-derived objects using ClassDesc::AddInterface. This
 * is typically performed in the ClassDesc::Create method.  
 * 
 * \see IMultipleOutputChannels 
 */
static FPInterfaceDesc imultioutput_consumer_interface(
	IMULTIOUTPUT_CONSUMER_INTERFACE, _T("iMultiOutputConsumer"), 0, nullptr, FP_MIXIN,

	IMultiOutputConsumer::imultioutputconsumer_setoutputtoinput, _T("SetOutputToInput"), 0, TYPE_bool, 0, 3,
		_T("input_index"),		0,	TYPE_INDEX,  
		_T("output_rtarg"),		0,	TYPE_REFTARG,  
		_T("output_index"),		0,	TYPE_INDEX,  

	IMultiOutputConsumer::imultioutputconsumer_getoutputrtargfrominput, _T("GetOutputRefTargetFromInput"), 0, TYPE_REFTARG, 0, 1,
		_T("input_index"),		0,	TYPE_INDEX,  
		
	IMultiOutputConsumer::imultioutputconsumer_getoutputindexfrominput, _T("GetOutputIndexFromInput"), 0, TYPE_INDEX, 0, 1,
		_T("input_index"),		0,	TYPE_INDEX,  

	IMultiOutputConsumer::imultioutputconsumer_canassignoutputtoinput, _T("CanAssignOutputToInput"), 0, TYPE_bool, 0, 3,
		_T("input_index"),		0,	TYPE_INDEX,  
		_T("output_rtarg"),		0,	TYPE_REFTARG,  
		_T("output_index"),		0,	TYPE_INDEX,  

	IMultiOutputConsumer::imultioutputconsumer_getinputname, _T("GetInputName"), 0, TYPE_TSTR_BV, FP_NO_REDRAW, 1,
		_T("input_index"),		0,	TYPE_INDEX,  

	IMultiOutputConsumer::imultioutputconsumer_getinputlocalizedname, _T("GetInputLocalizedName"), 0, TYPE_TSTR_BV, FP_NO_REDRAW, 1,
		_T("input_index"),		0,	TYPE_INDEX,  

	properties,

	IMultiOutputConsumer::imultioutputconsumer_getnuminputs, FP_NO_FUNCTION, _T("numInputs"), FP_NO_REDRAW, TYPE_INT,

	p_end
);

// ----------------------------------------------------------------------------
/** Convenience helper function to retrieve the IMultiOutputConsumer interface
 * from an ReferenceTarget.  
 * \param [in] rtarg  The ReferenceTarget to be asked for the interface. Will be
 *                    internally checked for nullptr, so there is no need to
 *                    check outside.  
 * \returns A pointer to the IMultiOutputConsumer interface of the given
 *          ReferenceTarget, if it supports it, or a nullptr.  
 * \see IMultiOutputConsumer, imultioutput_consumer_interface
 */
template <typename T>
inline IMultiOutputConsumer* GetIMultiOutputConsumer(T* rtarg)
{
	return rtarg ? static_cast<IMultiOutputConsumer*>(rtarg->GetInterface(IMULTIOUTPUT_CONSUMER_INTERFACE)) : nullptr;
}