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
#pragma once

#include "USDComponentBuildNumber.h"
#include "USDComponentVersionNumber.h"

// Helper macros to concatenate components of a version number, and to convert to a string
#define MAKE_VERSION_NUMBER(major, minor, micro, build) major##.##minor##.##micro##.##build
#define MAKE_VERSION_NUMBER_COMMA_SEPARATED(major, minor, micro, build) major,minor,micro,build
#define MAKE_STRING(str) #str
#define MAKE_VERSION_STRING(verStr) MAKE_STRING(verStr)

// Concatenated version for the USD component
#define USD_COMPONENT_VERSION MAKE_VERSION_NUMBER(COMPONENT_VERSION_MAJOR, COMPONENT_VERSION_MINOR, COMPONENT_VERSION_MICRO, VERSION_INT)
#define USD_COMPONENT_VERSION_STRING MAKE_VERSION_STRING(USD_COMPONENT_VERSION)
#define USD_COMPONENT_VERSION_COMMA_SEPARATED MAKE_VERSION_NUMBER_COMMA_SEPARATED(COMPONENT_VERSION_MAJOR, COMPONENT_VERSION_MINOR, COMPONENT_VERSION_MICRO, VERSION_INT)
