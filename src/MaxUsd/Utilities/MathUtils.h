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

#include <MaxUsd/MaxUSDAPI.h>

#include <MaxUsd.h>

class Matrix3;

namespace MAXUSD_NS_DEF {
namespace MathUtils {

/**
 * \brief Round value to the number of significant digit specified.
 *  Will return nan if numberOfSignificantDigit - order of magnitude of value > 309
 */
MaxUSDAPI double RoundToSignificantDigit(double value, int numberOfSignificantDigit);

/**
 * \brief Round value to the specified precision.
 */
MaxUSDAPI double RoundToPrecision(double value, double precision = 1.0);

/**
 * \brief Returns whether the given value is within FLT_EPSILON of zero.
 * \param value The value to test.
 * \return True if the value is almost zero, false otherwise.
 */
MaxUSDAPI bool IsAlmostZero(float value);

/**
 * \brief Modify the transform matrix to convert from Y up axis to Z up axis.
 * \param transformMatrix The base transform matrix on which to add the axis transform operation
 */
MaxUSDAPI void ModifyTransformYToZUp(pxr::GfMatrix4d& transformMatrix);

/**
 * \brief Modify the transform matrix to convert from Z up axis to Y up axis.
 * \param transformMatrix The base transform matrix on which to add the axis transform operation
 */
MaxUSDAPI void ModifyTransformZToYUp(pxr::GfMatrix4d& transformMatrix);

/**
 * \brief return true if the provided matrix is equal to identity.
 * \param maxMatrix3 max Matrix3
 * \return true if Matrix3 is identity
 */
MaxUSDAPI bool IsIdentity(const Matrix3& maxMatrix3);

/**
 * \brief round every value in the matrix to the desired number of significant digits.
 * \param matrix for which to round the values.
 * \param numberOfSignificantDigit number of significant digits to use for rounding.
 */
MaxUSDAPI void RoundMatrixValues(pxr::GfMatrix4d& matrix, int numberOfSignificantDigit);

/**
 * \brief round every value in the matrix to the desired number of significant digits.
 * \param matrix for which to round the values.
 * \param numberOfSignificantDigit number of significant digits to use for rounding.
 */
MaxUSDAPI void RoundMatrixValues(Matrix3& matrix, int numberOfSignificantDigit);

/**
 * \brief return true if the provided matrix is equal to identity.
 * \param matrix to compare against identity
 * \param epsilon value to be used for comparision
 * \return True if the matrix is equal to identity
 */
MaxUSDAPI bool
IsIdentity(const pxr::GfMatrix4d& matrix, float epsilon = std::numeric_limits<float>::epsilon());

/**
 * \brief If the given transform matrix has non-uniform scaling. This function make it uniform by averaging the scale components.
 * If the matrix already has uniform scaling, the function won't do anything.
 * \param transform the transform matrix be checked and modified, if necessary.
 * \return true if a fix was applied to the transform; false if transform was already uniform.
 */
MaxUSDAPI bool FixNonUniformScaling(pxr::GfMatrix4d& transform);

/*
 *	\brief std::clamp is available only C++17 and up, so for now, this function replaces it.
 *	\param val the value to clamp
 *	\param lo the lower boundary to clamp val to
 *	\param hi the upper boundary to clamp val to
 */
template <class T> const T& clamp(const T& val, const T& lo, const T& hi)
{
    assert(!(hi < lo));
    return (val < lo) ? lo : (hi < val) ? hi : val;
};

} // namespace MathUtils
} // namespace MAXUSD_NS_DEF