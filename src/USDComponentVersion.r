//
// Copyright 2023 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "UsdComponentVersion.h"
#include <Windows.h>

// Versioning parameters which may be overriden for various binaries in the component by defining the corresponding macros
// before including this file
#ifndef USD_COMPONENT_BINARY_INTERNAL_NAME
#define USD_COMPONENT_BINARY_INTERNAL_NAME "\0"
#endif
 
#ifndef USD_COMPONENT_BINARY_ORIGINAL_FILE_NAME
#define USD_COMPONENT_BINARY_ORIGINAL_FILE_NAME "\0"
#endif

#ifndef USD_COMPONENT_BINARY_FILE_DESCRIPTION
#define USD_COMPONENT_BINARY_FILE_DESCRIPTION "\0"
#endif
 
#ifndef USD_COMPONENT_BINARY_COMMENTS
#define USD_COMPONENT_BINARY_COMMENTS "\0"
#endif

// Versioning parameters which should be consistent for all binaries in the component
#define USD_COMPONENT_BINARY_PRODUCT_NAME "USD for Autodesk 3ds Max\0"
#define USD_COMPONENT_BINARY_COPYRIGHT "© 2021 Autodesk, Inc. All rights reserved.\0"
#define USD_COMPONENT_BINARY_COMPANY_NAME "Autodesk, Inc.\0"

// Information block implementing versioning for each binary in the component
VS_VERSION_INFO VERSIONINFO
FILEVERSION USD_COMPONENT_VERSION_COMMA_SEPARATED
PRODUCTVERSION USD_COMPONENT_VERSION_COMMA_SEPARATED
FILEFLAGSMASK 0x3fL
#ifdef _DEBUG
FILEFLAGS VS_FF_DEBUG
#else
FILEFLAGS 0x0L
#endif
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_DLL
FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName", USD_COMPONENT_BINARY_COMPANY_NAME
            VALUE "FileVersion",USD_COMPONENT_VERSION_STRING
            VALUE "InternalName", USD_COMPONENT_BINARY_INTERNAL_NAME
            VALUE "LegalCopyright", USD_COMPONENT_BINARY_COPYRIGHT
            VALUE "OriginalFilename", USD_COMPONENT_BINARY_ORIGINAL_FILE_NAME
            VALUE "Private Build Data", VERSION_STRING
            VALUE "ProductName", USD_COMPONENT_BINARY_PRODUCT_NAME
            VALUE "ProductVersion", USD_COMPONENT_VERSION_STRING
            VALUE "FileDescription", USD_COMPONENT_BINARY_FILE_DESCRIPTION
            VALUE "Comments", USD_COMPONENT_BINARY_COMMENTS
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
