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

#include <USDComponentVersionNumber.h>

// MaxUsd public namespace string will never change.
#define MAXUSD_NS MaxUsd
// C preprocessor trickery to expand arguments.
#define MAXUSD_CONCAT(A, B) MAXUSD_CONCAT_IMPL(A, B)
#define MAXUSD_CONCAT_IMPL(A, B) A##B
// Versioned namespace includes the major version number.
#define MAXUSD_VERSIONED_NS MAXUSD_CONCAT(MAXUSD_NS, MAXUSD_CONCAT(_v,COMPONENT_VERSION_MAJOR))

namespace MAXUSD_VERSIONED_NS {}

// With a using namespace declaration, pull in the versioned namespace into the
// MaxUsd public namespace, to allow client code to use the plain MaxUsd
// namespace, e.g. MaxUsd::Class.
namespace MAXUSD_NS {
    using namespace MAXUSD_VERSIONED_NS;
}

// Macro to place the MaxUsd symbols in the versioned namespace, which is how
// they will appear in the shared library, e.g. MaxUsd_v1::Class.
#ifdef DOXYGEN
#define MAXUSD_NS_DEF MAXUSD_NS
#else
#define MAXUSD_NS_DEF MAXUSD_VERSIONED_NS
#endif