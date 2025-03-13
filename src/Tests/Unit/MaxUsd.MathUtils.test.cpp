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
#include <gtest/gtest.h>

#include <MaxUsd/Utilities/MathUtils.h>
#include "TestUtils.h"

using namespace MaxUsd;

// Test RoundToSignificantDigit utility function
TEST(MathUtilsTest, RoundToSignificantDigit)
{
	EXPECT_EQ(MathUtils::RoundToSignificantDigit(0, 6), 0.0);
	EXPECT_EQ(MathUtils::RoundToSignificantDigit(10, 1), 10.0);
	EXPECT_EQ(MathUtils::RoundToSignificantDigit(0.12345, 2), 0.12);
	EXPECT_EQ(MathUtils::RoundToSignificantDigit(1234500, 3), 1230000);
	EXPECT_EQ(MathUtils::RoundToSignificantDigit(0.00012345, 4), 0.0001235);
	EXPECT_EQ(MathUtils::RoundToSignificantDigit(1.2345, 5), 1.2345);
	EXPECT_EQ(MathUtils::RoundToSignificantDigit(std::numeric_limits<double>::max(), 6), 1.79769e+308);
	EXPECT_EQ(MathUtils::RoundToSignificantDigit(std::numeric_limits<double>::lowest(), 6), -1.79769e+308);
	EXPECT_EQ(MathUtils::RoundToSignificantDigit(2.22507e-306, 2), 2.20e-306);
	EXPECT_EQ(std::isnan(MathUtils::RoundToSignificantDigit(2.22507e-306, 4)), true);
}

// Test RoundToPrecision utility function
TEST(MathUtilsTest, RoundToPrecision)
{
	EXPECT_EQ(MathUtils::RoundToPrecision(123.1), 123.0);
	EXPECT_EQ(MathUtils::RoundToPrecision(123.99), 124.0);
	EXPECT_EQ(MathUtils::RoundToPrecision(999), 999);
	EXPECT_EQ(MathUtils::RoundToPrecision(-123.1), -123.0);
	EXPECT_EQ(MathUtils::RoundToPrecision(-123.99), -124.0);
	EXPECT_EQ(MathUtils::RoundToPrecision(-999), -999);

	EXPECT_EQ(MathUtils::RoundToPrecision(1234, 10), 1230);

	EXPECT_EQ(MathUtils::RoundToPrecision(-999.9999, 0.0001), -999.9999);
	EXPECT_EQ(MathUtils::RoundToPrecision(999.9999, 0.0001), 999.9999);

	EXPECT_EQ(MathUtils::RoundToPrecision(999.9999, 0.001), 1000);
	EXPECT_EQ(MathUtils::RoundToPrecision(-999.9999, 0.001), -1000);

	EXPECT_EQ(MathUtils::RoundToPrecision(-1000.9999, 0.001), -1001);

	EXPECT_EQ(MathUtils::RoundToPrecision(0.123456789, 0.1), 0.1);
	EXPECT_EQ(MathUtils::RoundToPrecision(0.123456789, 0.01), 0.12);
	EXPECT_EQ(MathUtils::RoundToPrecision(0.123456789, 0.001), 0.123);
	EXPECT_EQ(MathUtils::RoundToPrecision(0.123456789, 0.000001), 0.123457);

	// Zero precision, illegal, returns value as is and assert.
	EXPECT_EQ(MathUtils::RoundToPrecision(1, 0.0), 1);
}

TEST(MathUtilsTest, IsAlmostZero)
{
	EXPECT_TRUE(MathUtils::IsAlmostZero(0.0f));
	EXPECT_TRUE(MathUtils::IsAlmostZero(0.000000005f));
	EXPECT_TRUE(MathUtils::IsAlmostZero(-0.000000005f));
	EXPECT_TRUE(MathUtils::IsAlmostZero(FLT_EPSILON - 0.0000001f));
	EXPECT_TRUE(MathUtils::IsAlmostZero(-FLT_EPSILON + 0.0000001f));
	
	EXPECT_FALSE(MathUtils::IsAlmostZero(-FLT_EPSILON));
	EXPECT_FALSE(MathUtils::IsAlmostZero(FLT_EPSILON));
	EXPECT_FALSE(MathUtils::IsAlmostZero(-FLT_EPSILON));
	EXPECT_FALSE(MathUtils::IsAlmostZero(10.f));
	EXPECT_FALSE(MathUtils::IsAlmostZero(-10.f));
}

TEST(MathUtilsTest, ModifyTransformYToZUp)
{
	pxr::GfMatrix4d initialMatrix(
		1, 2, 3, 4,
		5, 6, 7, 8,
		9, 10, 11, 12,
		13, 14, 15, 16
	);

	pxr::GfMatrix4d expectedResult(
		1, -3, 2, 4,
		5, -7, 6, 8,
		9, -11, 10, 12,
		13, -15, 14, 16
	);

	MathUtils::ModifyTransformYToZUp(initialMatrix);
	TestUtils::CompareUSDMatrices(initialMatrix, expectedResult);
}

TEST(MathUtilsTest, ModifyTransformZToYUp)
{
	pxr::GfMatrix4d initialMatrix(
		1, 2, 3, 4,
		5, 6, 7, 8,
		9, 10, 11, 12,
		13, 14, 15, 16
	);

	pxr::GfMatrix4d expectedResult(
		1, 3, -2, 4,
		5, 7, -6, 8,
		9, 11, -10, 12,
		13, 15, -14, 16
	);

	MathUtils::ModifyTransformZToYUp(initialMatrix);
	TestUtils::CompareUSDMatrices(initialMatrix, expectedResult);
}

TEST(MathUtilsTest, RoundMatrixValues)
{
	pxr::GfMatrix4d initialMatrix(
		12345, 1.23456, 19999, 0,
		0.12345, 0.000001, 0.0000000019999, -12599,
		12345, 12345, 12345, 12345,
		12345, 12345, 12345, 12345
	);

	pxr::GfMatrix4d expectedResult(
		12300, 1.23, 20000, 0,
		0.123, 0.000001, 0.000000002, -12600,
		12300, 12300, 12300, 12300,
		12300, 12300, 12300, 12300
	);

	MathUtils::RoundMatrixValues(initialMatrix, 3);
	TestUtils::CompareUSDMatrices(initialMatrix, expectedResult);

	const Point3 initialU(12345.0f, 1.23456f, 19999.0f);
	const Point3 initialV(0.12345, 0.000001, 0.0000000019999);
	const Point3 initialN(-12599.0f, 0.0f, 0.0f);
	const Point3 initialT(12345.0f, 1.23456f, 19999.0f);

	const Point3 expectedU(12350.0f, 1.23500f, 20000.0f);
	const Point3 expectedV(0.12350, 0.000001, 0.000000002);
	const Point3 expectedN(-12600.0f, 0.0f, 0.0f);
	const Point3 expectedT(12350.0f, 1.2350f, 20000.0f);

	Matrix3 initialMatrix3(initialU, initialV, initialN, initialT);
	Matrix3 expectedMatrix3(expectedU, expectedV, expectedN, expectedT);
	MathUtils::RoundMatrixValues(initialMatrix3, 4);
	EXPECT_TRUE(initialMatrix3.Equals(expectedMatrix3));
}

TEST(MathUtilsTest, IsIdentity)
{
	pxr::GfMatrix4d identity(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);
	EXPECT_TRUE(MathUtils::IsIdentity(identity));

	pxr::GfMatrix4d equalIdentity(
		1.000000001, 0.00000001, 0, 0,
		0, 1, -0.00000001, 0,
		0, 0, 1, 1e-8,
		0, 0, 0, 1
	);
	EXPECT_TRUE(MathUtils::IsIdentity(equalIdentity));

	pxr::GfMatrix4d notIdentity(
		1, 0.00001, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);
	EXPECT_FALSE(MathUtils::IsIdentity(notIdentity));
}

TEST(MathUtilsTest, clamp)
{
	EXPECT_FLOAT_EQ(MathUtils::clamp(-1.f, 0.f, 10.f), 0.f);
	EXPECT_EQ(MathUtils::clamp(67, 55, 300), 67);
	EXPECT_FALSE(MathUtils::clamp(-1, FALSE, TRUE));
	EXPECT_TRUE(MathUtils::clamp(static_cast<int8_t>(127), std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max()) == std::numeric_limits<int8_t>::max());
	EXPECT_EQ(MathUtils::clamp('p', 'c', 'm'), 'm');
}