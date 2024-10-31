//**************************************************************************/
// Copyright (c) 2008 Autodesk, Inc.
// All rights reserved.
// 
// These coded instructions, statements, and computer programs contain
// unpublished proprietary information written by Autodesk, Inc., and are
// protected by Federal copyright law. They may not be disclosed to third
// parties or copied or duplicated in any form, in whole or in part, without
// the prior written consent of Autodesk, Inc.
//**************************************************************************/
#pragma once

#include <iparamb2.h>

// New types for IParamBlock3
// Notice currently(R13) we don't need support for TYPE_BOOL2_TAB, TYPE_BOOL3_TAB, TYPE_BOOL4_TAB...
// So these are not defined, but they are expected to be defined in the future if needed.
// Should these parameter types be processed in RescaleParam? Currently are.
enum ParamType3 {
	TYPE_BOOL2 = TYPE_UNSPECIFIED + 1, 
	TYPE_BOOL3, 
	TYPE_BOOL4, 
	TYPE_INT2, 
	TYPE_INT3, 
	TYPE_INT4, 
	TYPE_INT2_TAB = TYPE_INT2 + TYPE_TAB, 
	TYPE_INT3_TAB = TYPE_INT3 + TYPE_TAB, 
	TYPE_INT4_TAB = TYPE_INT4 + TYPE_TAB, 
	TYPE_MAX_TYPE3, 
};
