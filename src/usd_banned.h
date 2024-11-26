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

#include <plugapi.h>
#if MAX_RELEASE >= 25900
// C++17 removed std::binary/unary_function
// we need to update our dependencies to be compatible
// in the meantime, we will hide the issue using this trick
namespace std {
template <class Arg1, class Arg2, class Result>
struct binary_function
{
	using first_argument_type = Arg1;
	using second_argument_type = Arg2;
	using result_type = Result;
};
} // namespace std
#endif

// Bunch of headers bringing memcpy calls
#include <atomic>
#include <locale>
#include <boost/functional/hash.hpp>
#include <boost/container/detail/copy_move_algo.hpp>

// Some macro conflicts
#include <boost/random.hpp>
#include <boost/bimap.hpp>

#pragma warning(push)
#pragma warning(disable : 4244 4305 4267 4003 4305 6011 6319 6386 26451 26439 26478 26487)
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec2h.h>
#include <pxr/base/gf/vec2i.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3h.h>
#include <pxr/base/gf/vec3i.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/gf/vec4h.h>
#include <pxr/base/gf/vec4i.h>
#include <pxr/base/gf/quath.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix3f.h>
#include <pxr/base/gf/range1f.h>
#include <pxr/base/gf/matrixData.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/pcp/mapfunction.h>
#include <pxr/base/tf/anyUniquePtr.h>
#include <pxr/base/tf/refPtr.h>
#include <pxr/base/tf/smallVector.h>
#include <pxr/base/tf/diagnosticLite.h>
#if MAX_RELEASE >= 25900
#include <pxr/base/tf/pxrTslRobinMap/robin_growth_policy.h>
#endif
#include <pxr/imaging/pxOsd/meshTopology.h>
#include <pxr/usd/pcp/dynamicFileFormatDependencyData.h>
#include <pxr/base/trace/dataBuffer.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usdImaging/usdImaging/resolvedAttributeCache.h>
#include <pxr/imaging/hdx/taskController.h>
#include <pxr/imaging/pxosd/tokens.h>
#include <pxr/usd/sdf/schema.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdr/shadernode.h>
#include <pxr/usd/usd/primData.h>
#include <pxr/usd/usd/notice.h>
#include <pxr/usd/usdgeom/xformOp.h>
#include <pxr/usd/usdUtils/stageCache.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable : 26451)
#include <spdlog/fmt/bundled/core.h>
#include <spdlog/fmt/bundled/format.h>
#include <spdlog/fmt/bundled/format-inl.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable : 4828)
#include <ILightingUnits.h>
#pragma warning(push)

#pragma warning(push)
#pragma warning(disable : 26451)
#include <mesh.h>
#include <winutil.h>
#pragma warning(push)

// the 3dsmax_banned.h header file is part of Max RTM 2020 (major version 22)
// unless present in the SDK include folder, do not include the header
#if defined(_MSC_VER) && _MSC_VER >= 1910
#if __has_include("<3dsmax_banned.h>") // need VS2017 to use the method
#include <3dsmax_banned.h>
#endif
#else
#include <maxversion.h>
#if MAX_VERSION_MAJOR >= 22
#include <3dsmax_banned.h>
#endif
#endif