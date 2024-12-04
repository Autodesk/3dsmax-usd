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
#include <MaxUsd/USDCore.h>
#include <MaxUsd/Utilities/TranslationUtils.h>

#include <gtest/gtest.h>
#include <max.h>

class MaxUsdTest : public ::testing::Test
{
public:
    void SetUp() override { }
    void TearDown() override { }
};

// Test sanitization of USD file extensions.
TEST(MaxUsdTest, SanitizingUSDFilenamesPutsExtensionsToLowercase)
{
    EXPECT_EQ(USDCore::sanitizedFilename(L"file.USD"), L"file.usd");
    EXPECT_EQ(USDCore::sanitizedFilename(L"file.USDA"), L"file.usda");
    EXPECT_EQ(USDCore::sanitizedFilename(L"file.USDC"), L"file.usdc");
    EXPECT_EQ(USDCore::sanitizedFilename(L"file.USDZ"), L"file.usdz");
    EXPECT_EQ(USDCore::sanitizedFilename(L"file.usD"), L"file.usd");
    EXPECT_EQ(USDCore::sanitizedFilename(L"file.UsDa"), L"file.usda");
    EXPECT_EQ(USDCore::sanitizedFilename(L"file", L".usda"), L"file.usda");
    EXPECT_EQ(USDCore::sanitizedFilename(L"file"), L"file");
}

TEST(MaxUsdTest, TestMockMaxInterfaces)
{
    auto* core_interface = GetCOREInterface();
    EXPECT_TRUE(core_interface != nullptr);
}

TEST(MaxUsdTest, IsValidAbsolutePath)
{
    EXPECT_TRUE(MaxUsd::IsValidAbsolutePath(L"C:/foo/bar/baz.txt"));
    EXPECT_TRUE(MaxUsd::IsValidAbsolutePath(L"Z:/foo.log"));
    EXPECT_TRUE(MaxUsd::IsValidAbsolutePath(L"//foo/baz.log"));
    // Input over 260 chars but this will resolve to an acceptable size.
    EXPECT_TRUE(MaxUsd::IsValidAbsolutePath(
        L"C:/./././././././././././././././././././././././././././././././././././././././././././"
        L"././././././././././././././././././././././././././././././././././././././././././././"
        L"././././././././././././././././././././././././././././././././././././././././././"
        L"not.txt"));
    // Path too long.
    EXPECT_FALSE(MaxUsd::IsValidAbsolutePath(
        L"C:/foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/"
        L"foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/foo/bar/"
        L"baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/foo/bar/baz/"
        L"foo/bar.txt"));
    // Not a file.
    EXPECT_FALSE(MaxUsd::IsValidAbsolutePath(L"C:/foo"));
    EXPECT_FALSE(MaxUsd::IsValidAbsolutePath(L"C:/foo/"));
    // Illegal characters
    EXPECT_FALSE(MaxUsd::IsValidAbsolutePath(L"C:/<>.txt"));
    // Not an absolute path.
    EXPECT_FALSE(MaxUsd::IsValidAbsolutePath(L"not.txt"));
    EXPECT_FALSE(MaxUsd::IsValidAbsolutePath(L"./not.txt"));
}
