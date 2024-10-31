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
#include "MathUtils.h"

#include "MaxSupportUtils.h"

#include <pxr/usd/usdSkel/utils.h>

#include <math.h>

namespace MAXUSD_NS_DEF {
namespace MathUtils {

double RoundToSignificantDigit(double value, int numberOfSignificantDigit)
{
    if (value == 0.0) {
        return 0.0;
    }

    double scalingFactor = pow(10.0, numberOfSignificantDigit - std::ceil(log10(fabs(value))));
    return std::round(value * scalingFactor) / scalingFactor;
}

double RoundToPrecision(double value, double precision)
{
    if (precision == 0.0) {
        DbgAssert("Zero is not a valid decimal precision.");
        return value;
    }
    return std::round(value / precision) * precision;
}

bool IsAlmostZero(float value) { return abs(value) < FLT_EPSILON; }

void ModifyTransformYToZUp(pxr::GfMatrix4d& transformMatrix)
{
    // -90 degree rotation on the X axis
    pxr::GfMatrix4d axisTransform(1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1);
    transformMatrix *= axisTransform;
}

void ModifyTransformZToYUp(pxr::GfMatrix4d& transformMatrix)
{
    // 90 degree rotation on the X axis
    pxr::GfMatrix4d axisTransform(1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1);
    transformMatrix *= axisTransform;
}

bool IsIdentity(const Matrix3& maxMatrix3) { return maxMatrix3.Equals(Matrix3::Identity); }

void RoundMatrixValues(pxr::GfMatrix4d& matrix, int numberOfSignificantDigit)
{
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            matrix[i][j] = RoundToSignificantDigit(matrix[i][j], numberOfSignificantDigit);
        }
    }
}

void RoundMatrixValues(Matrix3& matrix, int numberOfSignificantDigit)
{
    for (int i = 0; i < 4; ++i) {
        const Point3& p3 = matrix.GetRow(i);
        Point3        roundedPoint;
        roundedPoint.x
            = static_cast<float>(RoundToSignificantDigit(p3.x, numberOfSignificantDigit));
        roundedPoint.y
            = static_cast<float>(RoundToSignificantDigit(p3.y, numberOfSignificantDigit));
        roundedPoint.z
            = static_cast<float>(RoundToSignificantDigit(p3.z, numberOfSignificantDigit));

        matrix.SetRow(i, roundedPoint);
    }
}

bool IsIdentity(const pxr::GfMatrix4d& matrix, float epsilon)
{
    pxr::GfMatrix4d identity(1);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (fabs(matrix[i][j] - identity[i][j]) > epsilon) {
                return false;
            }
        }
    }
    return true;
}

bool FixNonUniformScaling(pxr::GfMatrix4d& transform)
{
    auto isUniformScaling = [](const pxr::GfVec3h& scale) {
        const auto halfEpsilon = std::numeric_limits<float>::epsilon();
        return (abs(scale.data()[0] - scale.data()[1]) <= halfEpsilon)
            && (abs(scale.data()[0] - scale.data()[2]) <= halfEpsilon);
    };

    pxr::GfVec3f translation;
    pxr::GfQuatf rotation;
    pxr::GfVec3h scale;
    pxr::UsdSkelDecomposeTransform(transform, &translation, &rotation, &scale);

    // nothing to do if already uniform
    if (isUniformScaling(scale)) {
        return false;
    }

    const auto scaleX = scale.data()[0];
    const auto scaleY = scale.data()[1];
    const auto scaleZ = scale.data()[2];

    transform.SetScale((scaleX + scaleY + scaleZ) / 3.f);
    return true;
}

} // namespace MathUtils
} // namespace MAXUSD_NS_DEF