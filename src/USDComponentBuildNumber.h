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

// For a local build, dummy up the build number to zero; for a proper build in the component pipeline, this will be
// overwritten
#pragma warning(push)
#pragma warning(disable : 4005)  // prevent macro redefinition warning with 'maxsdk\include\buildnumber.h'
#define VERSION_STRING "local build"
#define VERSION_INT 0
#pragma warning(pop)
