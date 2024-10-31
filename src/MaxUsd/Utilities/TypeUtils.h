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

#include "MaxSupportUtils.h"

#include <MaxUsd.h>

#ifdef IS_MAX2025_OR_GREATER
#include <Geom/ipoint2.h>
#include <Geom/matrix3.h>
#include <Geom/point3.h>
#else
#include <ipoint2.h>
#include <matrix3.h>
#include <point3.h>
#endif
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec2i.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>

#include <Graphics/Matrix44.h>

namespace MAXUSD_NS_DEF {

inline pxr::GfVec2i ToUsd(IPoint2 p) { return pxr::GfVec2i(p.x, p.y); }

inline pxr::GfVec3d ToUsd(Point3 p) { return pxr::GfVec3d(p.x, p.y, p.z); }

inline pxr::GfMatrix4d ToUsd(Matrix3 m)
{
    pxr::GfMatrix4d r;

    for (int i = 0; i < 4; ++i) {
        r.SetRow3(i, ToUsd(m.GetRow(i)));
    }

    r.SetColumn(3, pxr::GfVec4d(0, 0, 0, 1));
    return r;
}

inline pxr::GfMatrix4d ToUsd(MaxSDK::Graphics::Matrix44 m)
{
    pxr::GfMatrix4d r;
    for (int i = 0; i < 4; ++i) {
        r.SetRow(i, pxr::GfVec4d(pxr::GfVec4f(m.m[i])));
    }

    return r;
}

inline Point3 ToMax(pxr::GfVec3d p) { return Point3(p[0], p[1], p[2]); }

inline Point3 ToMax(pxr::GfVec3f p) { return Point3(p[0], p[1], p[2]); }

inline Point4 ToMax(pxr::GfVec4f p) { return Point4(p[0], p[1], p[2], p[3]); }

inline MaxSDK::Graphics::Matrix44 ToMax(const pxr::GfMatrix4d& m)
{
    MaxSDK::Graphics::Matrix44 r;
#pragma warning(push)
#pragma warning(disable : 4244)
    std::copy_n(m.data(), 16, static_cast<float*>(&r.m[0][0]));
#pragma warning(pop)
    return r;
}

template <class QuatType> bool ToMaxQuat(pxr::VtValue& quatValue, Quat& quat)
{
    if (!quatValue.CanCast<QuatType>()) {
        return false;
    }
    const auto  usdQuat = quatValue.Cast<QuatType>().Get<QuatType>();
    const auto& imaginary = usdQuat.GetImaginary();
    quat.x = static_cast<float>(imaginary[0]);
    quat.y = static_cast<float>(imaginary[1]);
    quat.z = static_cast<float>(imaginary[2]);
    // Inverse the quat to account for the difference in conventions (right-hand VS left-hand).
    quat.w = -static_cast<float>(usdQuat.GetReal());
    return true;
}

inline Matrix3 ToMaxMatrix3(const MaxSDK::Graphics::Matrix44& mat)
{
    Matrix3    matrix3;
    const auto row1 = mat.m[0];
    matrix3.SetRow(0, Point3(row1[0], row1[1], row1[2]));
    const auto row2 = mat.m[1];
    matrix3.SetRow(1, Point3(row2[0], row2[1], row2[2]));
    const auto row3 = mat.m[2];
    matrix3.SetRow(2, Point3(row3[0], row3[1], row3[2]));
    const auto row4 = mat.m[3];
    matrix3.SetRow(3, Point3(row4[0], row4[1], row4[2]));
    return matrix3;
}

inline Matrix3 ToMaxMatrix3(const pxr::GfMatrix4d& mat)
{
    Matrix3    matrix3;
    const auto row1 = mat.GetRow3(0).data();
    matrix3.SetRow(0, Point3(row1[0], row1[1], row1[2]));
    const auto row2 = mat.GetRow3(1).data();
    matrix3.SetRow(1, Point3(row2[0], row2[1], row2[2]));
    const auto row3 = mat.GetRow3(2).data();
    matrix3.SetRow(2, Point3(row3[0], row3[1], row3[2]));
    const auto row4 = mat.GetRow3(3).data();
    matrix3.SetRow(3, Point3(row4[0], row4[1], row4[2]));
    return matrix3;
}

} // namespace MAXUSD_NS_DEF