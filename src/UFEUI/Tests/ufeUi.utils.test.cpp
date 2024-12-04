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
#include "testUfeRuntime.h"
#include "ufeUiTestBase.h"
#include "utils.h"

#include <UFEUI/utils.h>

#include <ufe/hierarchy.h>
#include <ufe/path.h>
#include <ufe/selection.h>

#include <gtest/gtest.h>

TEST(UtilsTest, SelectionsAreEquivalent_Empty_Both)
{
    Ufe::Selection a, b;
    EXPECT_EQ(a.empty(), true);
    EXPECT_EQ(b.empty(), true);

    EXPECT_TRUE(UfeUi::Utils::selectionsAreEquivalent(a, b));
}

TEST(UtilsTest, SelectionsAreEquivalent_Empty_First)
{
    Ufe::Selection a, b;
    EXPECT_TRUE(
        b.append(Ufe::Hierarchy::createItem(Ufe::Path::Path(Ufe::PathSegment("test_01", 1, '/')))));

    EXPECT_FALSE(UfeUi::Utils::selectionsAreEquivalent(a, b));
}

TEST(UtilsTest, SelectionsAreEquivalent_Empty_Second)
{
    Ufe::Selection a, b;
    EXPECT_TRUE(
        a.append(Ufe::Hierarchy::createItem(Ufe::Path::Path(Ufe::PathSegment("test_01", 1, '/')))));

    EXPECT_FALSE(UfeUi::Utils::selectionsAreEquivalent(a, b));
}

TEST(UtilsTest, SelectionsAreEquivalent_Identical_Ptrs)
{
    Ufe::Selection a, b;

    auto ptr = Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_01", 1, '/')));
    EXPECT_TRUE(a.append(ptr));
    EXPECT_TRUE(b.append(ptr));

    EXPECT_TRUE(UfeUi::Utils::selectionsAreEquivalent(a, b));
}

TEST(UtilsTest, SelectionsAreEquivalent_Identical_Paths)
{
    Ufe::Selection a, b;

    EXPECT_TRUE(a.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_01", 1, '/')))));
    EXPECT_TRUE(b.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_01", 1, '/')))));

    EXPECT_TRUE(UfeUi::Utils::selectionsAreEquivalent(a, b));
}

TEST(UtilsTest, SelectionsAreEquivalent_Identical_Paths_Different_Order)
{
    Ufe::Selection a, b;

    EXPECT_TRUE(a.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_01", 1, '/')))));
    EXPECT_TRUE(a.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_02", 1, '/')))));

    EXPECT_TRUE(b.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_02", 1, '/')))));
    EXPECT_TRUE(b.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_01", 1, '/')))));

    EXPECT_TRUE(UfeUi::Utils::selectionsAreEquivalent(a, b));
}

TEST(UtilsTest, SelectionsAreEquivalent_Different_Paths)
{
    Ufe::Selection a, b;

    EXPECT_TRUE(a.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_01", 1, '/')))));
    EXPECT_TRUE(b.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_02", 1, '/')))));

    EXPECT_FALSE(UfeUi::Utils::selectionsAreEquivalent(a, b));
}

TEST(UtilsTest, SelectionsAreEquivalent_Identical_Ptrs_Paths_Mixed)
{
    Ufe::Selection a, b;

    auto ptr = Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_01", 1, '/')));
    EXPECT_TRUE(a.append(ptr));
    EXPECT_TRUE(b.append(ptr));

    EXPECT_TRUE(a.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_02", 1, '/')))));
    EXPECT_TRUE(b.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_02", 1, '/')))));

    EXPECT_TRUE(UfeUi::Utils::selectionsAreEquivalent(a, b));
}

TEST(UtilsTest, SelectionsAreEquivalent_Different_Ptrs_Paths_Mixed)
{
    Ufe::Selection a, b;

    auto ptr = Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_01", 1, '/')));
    EXPECT_TRUE(a.append(ptr));
    EXPECT_TRUE(b.append(ptr));

    EXPECT_TRUE(a.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_02", 1, '/')))));
    EXPECT_TRUE(b.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_03", 1, '/')))));

    EXPECT_FALSE(UfeUi::Utils::selectionsAreEquivalent(a, b));
}

TEST(UtilsTest, SelectionsAreEquivalent_Different_WorstCase)
{
    Ufe::Selection a, b;
    for (int x = 0; x < 99; ++x) {
        for (int y = 0; y < 99; ++y) {
            std::string path = QString("test_root/test_folder_%1/test_%2")
                                   .arg(x, 2, QChar('0'))
                                   .arg(y, 2, QChar('0'))
                                   .toStdString();
            auto ptr = Ufe::Hierarchy::createItem(Ufe::Path::Path(Ufe::PathSegment(path, 1, '/')));
            EXPECT_TRUE(a.append(ptr));
            EXPECT_TRUE(b.append(ptr));
        }
    }

    // still valid...
    EXPECT_TRUE(UfeUi::Utils::selectionsAreEquivalent(a, b));

    EXPECT_TRUE(a.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_folder_99/test_100", 1, '/')))));
    EXPECT_TRUE(b.append(Ufe::Hierarchy::createItem(
        Ufe::Path::Path(Ufe::PathSegment("test_root/test_folder_99/test_101", 1, '/')))));
    // not equal any more
    EXPECT_FALSE(UfeUi::Utils::selectionsAreEquivalent(a, b));
}